/*
 * etx_ota_update.c
 *
 *  Created on: 26-Jul-2021
 *      Author: EmbeTronicX
 */

#include <stdio.h>
#include "etx_ota_update.h"
#include "main.h"
#include <string.h>
#include <stdbool.h>

/* Buffer to hold the received data */
static uint8_t Rx_Buffer[ ETX_OTA_PACKET_MAX_SIZE ];

/* OTA State */
static ETX_OTA_STATE_ ota_state = ETX_OTA_STATE_IDLE;

/* Firmware Total Size that we are going to receive */
static uint32_t ota_fw_total_size;
/* Firmware image's CRC32 */
static uint32_t ota_fw_crc;
/* Firmware Size that we have received */
static uint32_t ota_fw_received_size;

static uint16_t etx_receive_chunk( uint8_t *buf, uint16_t max_len );
static ETX_OTA_EX_ etx_process_data( uint8_t *buf, uint16_t len );
static void etx_ota_send_resp( uint8_t type );
static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,
                                        uint16_t data_len, bool is_full_image );

/**
  * @brief Download the application from UART and flash it.
  * @param None
  * @retval ETX_OTA_EX_
  */
ETX_OTA_EX_ etx_ota_download_and_flash( void )
{
  ETX_OTA_EX_ ret  = ETX_OTA_EX_OK;
  uint16_t    len;

  printf("Waiting for the OTA data...\r\n");

  /* Reset the variables */
  ota_fw_total_size    = 0u;
  ota_fw_received_size = 0u;
  ota_fw_crc           = 0u;
  ota_state            = ETX_OTA_STATE_START;
  char txt[48];

  do
  {
    //clear the buffer
    memset( Rx_Buffer, 0, ETX_OTA_PACKET_MAX_SIZE );

    //printf("wait for data...\r\n");
    len = etx_receive_chunk( Rx_Buffer, ETX_OTA_PACKET_MAX_SIZE );

    sprintf(txt, "len_1=%d\n", len);
    printd(txt);

    for (int i=0; i<len; i++)
    {
    	sprintf(txt, "%02X.", Rx_Buffer[i]);
    	printd(txt);
    }
    sprintf(txt, "\n");
    printd(txt);

    if( len != 0u )
    {
      ret = etx_process_data( Rx_Buffer, len );
      //sprintf(txt, "toto 2\n");
      //printd(txt);
      //printf(".... end of process... ret=%d\n", ret);
    }
    else
    {
      //didn't received data. break.
      ret = ETX_OTA_EX_ERR;
    }

    //Send ACK or NACK
    if( ret != ETX_OTA_EX_OK )
    {
      //printf("Sending NACK\r\n");
      sprintf(txt, "Sending NACK\n");
      printd(txt);
      etx_ota_send_resp( ETX_OTA_NACK );
      break;
    }
    else
    {
      //printf("Sending ACK\r\n");
      sprintf(txt, "Sending ACK\n");
      printd(txt);
      etx_ota_send_resp( ETX_OTA_ACK );
    }

  } while( ota_state != ETX_OTA_STATE_IDLE );

  return ret;
}

/**
  * @brief Process the received data from UART4.
  * @param buf buffer to store the received data
  * @param max_len maximum length to receive
  * @retval ETX_OTA_EX_
  */
static ETX_OTA_EX_ etx_process_data( uint8_t *buf, uint16_t len )
{
  ETX_OTA_EX_ ret = ETX_OTA_EX_ERR;
  char txt[256];

  do
  {
    if( ( buf == NULL ) || ( len == 0u) )
    {
      break;
    }

    //Check we received OTA Abort command
    ETX_OTA_COMMAND_ *cmd = (ETX_OTA_COMMAND_*)buf;
    if( cmd->packet_type == ETX_OTA_PACKET_TYPE_CMD )
    {
      if( cmd->cmd == ETX_OTA_CMD_ABORT )
      {
        //received OTA Abort command. Stop the process
        break;
      }
    }

    sprintf(txt, "state=%d\n", ota_state);
    printd(txt);

    switch( ota_state )
    {
      case ETX_OTA_STATE_IDLE:
      {
        printf("ETX_OTA_STATE_IDLE...\r\n");
        ret = ETX_OTA_EX_OK;
      }
      break;

      case ETX_OTA_STATE_START:
      {
        ETX_OTA_COMMAND_ *cmd = (ETX_OTA_COMMAND_*)buf;

        if( cmd->packet_type == ETX_OTA_PACKET_TYPE_CMD )
        {
          if( cmd->cmd == ETX_OTA_CMD_START )
          {
            //printf("Received OTA START Command\r\n");
            ota_state = ETX_OTA_STATE_HEADER;
            ret = ETX_OTA_EX_OK;
          }
        }
      }
      break;

      case ETX_OTA_STATE_HEADER:
      {
        ETX_OTA_HEADER_ *header = (ETX_OTA_HEADER_*)buf;
        if( header->packet_type == ETX_OTA_PACKET_TYPE_HEADER )
        {
          ota_fw_total_size = header->meta_data.package_size;
          ota_fw_crc        = header->meta_data.package_crc;
          //printf("Received OTA Header. FW Size = %ld\r\n", ota_fw_total_size);
          ota_state = ETX_OTA_STATE_DATA;
          ret = ETX_OTA_EX_OK;
        }
      }
      break;

      case ETX_OTA_STATE_DATA:
      {
        ETX_OTA_DATA_     *data     = (ETX_OTA_DATA_*) buf;
        uint16_t          data_len = data->data_len;
        HAL_StatusTypeDef ex;

        if ( data->packet_type == ETX_OTA_PACKET_TYPE_DATA )
        {
          /* write the chunk to the Flash (App location) */
          sprintf(txt, "   > write data [%d]\n", data_len);
          printd(txt);

          ex = write_data_to_flash_app( buf+4, data_len, ( ota_fw_received_size == 0) );

          if ( ex == HAL_OK )
          {
        	sprintf(txt, "   > HAL_OK\n");
        	printd(txt);

            sprintf(txt, "   > [%ld/%ld]\n", ota_fw_received_size, ota_fw_total_size);
            printd(txt);

            if ( ota_fw_received_size >= ota_fw_total_size )
            {
              //received the full data. So, move to end
              ota_state = ETX_OTA_STATE_END;
              sprintf(txt, "   > switch to state end\n");
              printd(txt);
            }
            ret = ETX_OTA_EX_OK;
          }
        }
      }
      break;

      case ETX_OTA_STATE_END:
      {
    	sprintf(txt, "   > state end\n");
    	printd(txt);

        ETX_OTA_COMMAND_ *cmd = (ETX_OTA_COMMAND_*) buf;

        if( cmd->packet_type == ETX_OTA_PACKET_TYPE_CMD )
        {
          sprintf(txt, "   > CMD packet\n");
          printd(txt);

          if( cmd->cmd == ETX_OTA_CMD_END )
          {
        	sprintf(txt, "   > END CMD\n");
        	printd(txt);

            printf("Received OTA END Command\r\n");

            //TODO: Very full package CRC

            ota_state = ETX_OTA_STATE_IDLE;
            ret = ETX_OTA_EX_OK;
          }
        }
      }
      break;

      default:
      {
        /* Should not come here */
        ret = ETX_OTA_EX_ERR;
      }
      break;
    };
  }while( false );

  return ret;
}

/**
  * @brief Receive a one chunk of data.
  * @param buf buffer to store the received data
  * @param max_len maximum length to receive
  * @retval ETX_OTA_EX_
  */
static uint16_t etx_receive_chunk( uint8_t *buf, uint16_t max_len )
{
  int16_t  ret;
  uint16_t index     = 0u;
  uint16_t data_len;


  char txt[48];
#ifdef READ_ALL_10
  //printf("into chunk...\n");

  ret = HAL_UART_Receive( &huart6, &buf[index], 10, HAL_MAX_DELAY );
  sprintf(txt, "ret10=%d", ret);
  printd(txt);

  for (int j=0; j<10; j++)
  {
	  sprintf(txt, "%02X.", buf[j]);
	  printd(txt);
  }

  sprintf(txt, "");
  printd(txt);
#endif

  //sprintf(txt, "ENTER_CHUNKIE");
  //printd(txt);

  do
  {
    //receive SOF byte (1 byte)
    ret = HAL_UART_Receive( &huart6, &buf[index], 1, HAL_MAX_DELAY );
    //sprintf(txt, "ret1=%d i=%d (%02X)", ret, index, buf[index]);
    //printd(txt);

    if( ret != HAL_OK )
    {
      break;
    }

    if( buf[index] != ETX_OTA_SOF )
    {
      //Not received start of frame
      ret = ETX_OTA_EX_ERR;
      break;
    }

    index++;						/* Next zone		*/

    //printf("after SOF !!!!!!!!!!!!\n");
    //Receive the packet type (1 byte).
    ret = HAL_UART_Receive( &huart6, &buf[index], 1, HAL_MAX_DELAY );
    //sprintf(txt, "ret2=%d i=%d (%02X)", ret, index, buf[index]);
    //printd(txt);

    index++;						/* Next zone		*/

    if( ret != HAL_OK )
    {
      break;
    }

    //Get the data length (2 bytes).
    ret = HAL_UART_Receive( &huart6, &buf[index], 2, HAL_MAX_DELAY );
    //sprintf(txt, "ret3=%d i=%d (%02X)", ret, index, buf[index]);
    //printd(txt);
    //sprintf(txt, "ret3=%d i=%d (%02X)", ret, index+1, buf[index+1]);
    //printd(txt);

    if( ret != HAL_OK )
    {
      break;
    }
    data_len = *(uint16_t *) &buf[index];
    //index += 2u;

    index+=2;						/* Next zone		*/

    //sprintf(txt, "datalen=%d", data_len);
    //printd(txt);

    uint16_t begin=index;

    for( uint16_t i = 0u; i < data_len; i++ )
    {
      ret = HAL_UART_Receive( &huart6, &buf[index], 1, HAL_MAX_DELAY );
      //sprintf(txt, "ret4=%d i=%d (%02X)", ret, index, buf[index]);
      //printd(txt);
      index++;

      if( ret != HAL_OK )
      {
    	//sprintf(txt, "break #4");
    	//printd(txt);
        break;
      }
    }

    //sprintf(txt, "datalen=%d", data_len);
    //printd(txt);

    //for( uint16_t i = 0; i < data_len; i++ )
    //{
    //	sprintf(txt, "%d (%02X)", i, buf[begin+i]);
    //	printd(txt);
    //}

    //sprintf(txt, "after #4");
    //printd(txt);

    //Get the CRC.
    ret = HAL_UART_Receive( &huart6, &buf[index], 4, HAL_MAX_DELAY );

    if( ret != HAL_OK )
    {
      break;
    }

    index += 4u;

    //TODO: Add CRC verification

    //receive EOF byte (1 byte)
    ret = HAL_UART_Receive( &huart6, &buf[index], 1, HAL_MAX_DELAY );
    //sprintf(txt, "ret6=%d i=%d (%02X)", ret, index, buf[index]);
    //printd(txt);

    if( ret != HAL_OK )
    {
      break;
    }

    if ( buf[index] != ETX_OTA_EOF )
    {
      //Not received end of frame
      ret = ETX_OTA_EX_ERR;
      break;
    }

    index++;

  } while( false );

  //sprintf(txt, "RET_CHUNKIE");
  //printd(txt);

  if( ret != HAL_OK )
  {
    //clear the index if error
    index = 0u;
  }

  if ( max_len < index )
  {
    sprintf(txt, "Received more data than expected. Expected = %d, Received = %d\r\n",
                                                              max_len, index );
    printdln(txt);

    index = 0u;
  }

  //sprintf(txt, "tot len=%d", index);
  //printdln(txt);

  /*
  for (int j=0; j<index; j++)
  {
    sprintf(txt, "%02X.", buf[j]);
    printd(txt);
  }

  sprintf(txt, "");
  printdln(txt);
  */


  return index;
}

/**
  * @brief Send the response.
  * @param type ACK or NACK
  * @retval none
  */
static void etx_ota_send_resp( uint8_t type )
{
  ETX_OTA_RESP_ rsp =
  {
    .sof         = ETX_OTA_SOF,
    .packet_type = ETX_OTA_PACKET_TYPE_RESPONSE,
    .data_len    = 1u,
    .status      = type,
    .crc         = 0u,                //TODO: Add CRC
    .eof         = ETX_OTA_EOF
  };

  //send response
  HAL_UART_Transmit(&huart6, (uint8_t *)&rsp, sizeof(ETX_OTA_RESP_), HAL_MAX_DELAY);
}

/**
  * @brief Write data to the Application's actual flash location.
  * @param data data to be written
  * @param data_len data length
  * @is_first_block true - if this is first block, false - not first block
  * @retval HAL_StatusTypeDef
  */
static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,
                                        uint16_t data_len, bool is_first_block )
{
  HAL_StatusTypeDef ret;
  uint32_t pos=ota_fw_received_size;
  char txt[64];

  do
  {
    ret = HAL_FLASH_Unlock();
    if( ret != HAL_OK )
    {
      break;
    }

    //No need to erase every time. Erase only the first time.
    if( is_first_block )
    {

      printf("Erasing the Flash memory...\r\n");
      //Erase the Flash
      FLASH_EraseInitTypeDef EraseInitStruct;
      uint32_t SectorError;

      EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
      EraseInitStruct.Sector        = FLASH_SECTOR_5;
      EraseInitStruct.NbSectors     = 2;                    //erase 2 sectors(5,6)
      EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;

      ret = HAL_FLASHEx_Erase( &EraseInitStruct, &SectorError );
      if( ret != HAL_OK )
      {
        break;
      }
    }

    for (int i = 0; i < data_len; i++ )
    {
      /*
      ret = HAL_FLASH_Program( FLASH_TYPEPROGRAM_BYTE,
                               (ETX_APP_FLASH_ADDR + ota_fw_received_size),
                               data[4+i]
                             );
      */

	ret = HAL_FLASH_Program( FLASH_TYPEPROGRAM_BYTE,
								   (ETX_APP_FLASH_ADDR + ota_fw_received_size),
								   data[i]
								 );

      if( ret == HAL_OK )
      {
        //update the data count
        ota_fw_received_size += 1;
      }
      else
      {
        printf("Flash Write Error\r\n");
        break;
      }
    }

    sprintf(txt, "   >>> write %d bytes at %08X\n", data_len, ETX_APP_FLASH_ADDR+pos );
    printd(txt);

    if( ret != HAL_OK )
    {
      break;
    }

    ret = HAL_FLASH_Lock();
    if( ret != HAL_OK )
    {
      break;
    }
  } while(false);

  return ret;
}
