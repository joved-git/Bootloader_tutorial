
/**************************************************

file: ota_update.c
purpose: -
  -host application that updates STM32 with bootloader level 2.
  -use open(), read() and write() commands to acces UART.

compile with the command: 
$ gcc ota_update.c -Wall -Wextra -o2 -o ota_update

or simple type; 
$ make

**************************************************/

// Linux headers
#include <fcntl.h>   // Contains file controls like O_RDWR
#include <errno.h>   // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h>  // write(), read(), close()
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

//#define DEBUG         /* If you want to debug the code  */
#define VERSION   "1.3.4"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "ota_update.h"

#define RS232_PORTNR 38

uint8_t DATA_BUF[ETX_OTA_PACKET_MAX_SIZE];
uint8_t APP_BIN[ETX_OTA_MAX_FW_SIZE];

const char *comports[RS232_PORTNR] = {"/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3", "/dev/ttyS4", "/dev/ttyS5",
                                      "/dev/ttyS6", "/dev/ttyS7", "/dev/ttyS8", "/dev/ttyS9", "/dev/ttyS10", "/dev/ttyS11",
                                      "/dev/ttyS12", "/dev/ttyS13", "/dev/ttyS14", "/dev/ttyS15", "/dev/ttyUSB0",
                                      "/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyUSB3", "/dev/ttyUSB4", "/dev/ttyUSB5",
                                      "/dev/ttyAMA0", "/dev/ttyAMA1", "/dev/ttyACM0", "/dev/ttyACM1",
                                      "/dev/rfcomm0", "/dev/rfcomm1", "/dev/ircomm0", "/dev/ircomm1",
                                      "/dev/cuau0", "/dev/cuau1", "/dev/cuau2", "/dev/cuau3",
                                      "/dev/cuaU0", "/dev/cuaU1", "/dev/cuaU2", "/dev/cuaU3"};

void delay(uint32_t us)
{
  us *= 10;
#ifdef _WIN32
  // Sleep(ms);
  __int64 time1 = 0, time2 = 0, freq = 0;

  QueryPerformanceCounter((LARGE_INTEGER *)&time1);
  QueryPerformanceFrequency((LARGE_INTEGER *)&freq);

  do
  {
    QueryPerformanceCounter((LARGE_INTEGER *)&time2);
  } while ((time2 - time1) < us);
#else
  usleep(us);
#endif
}

/* read the response */
bool is_ack_resp_received(int comport)
{
#ifdef DEBUG
  printf("***  debug 5.1");
#endif

  bool is_ack = false;
  uint8_t *pBuf=NULL;

  // Clear the memory
  memset(DATA_BUF, 0, ETX_OTA_PACKET_MAX_SIZE);

  uint16_t len = 0;
  uint16_t i = 0;
  bool removeZero=true;

#ifdef DEBUG
  printf("***  debug 5.2");
#endif

  /* Read the answer (ACK or NACK)  */
  do
  {
    do
    {
      len = read(comport, DATA_BUF + i, sizeof(ETX_OTA_RESP_));
    } while (!len);

    i += len;
  } 
  while (DATA_BUF[i - 1] != 0xBB);

#ifdef DEBUG
  printf("***  debug 5.3");
#endif

  /* Remove zero in front of the received bytes */
  pBuf=DATA_BUF;

  while (removeZero)
  {
    if (pBuf[0]==0x00)
    {
      pBuf++;
      i--;
    } 
    else
    {
      removeZero=false;
    }
  }

#ifdef DEBUG
  printf("***  debug 5.4");
#endif

#ifdef DEBUG
  printf("\n");

  for (int j=0; j<i; j++)
  {
    printf("%02X.", pBuf[j]);
  }
  printf("\n");
#endif
  
  /* Check the answer, ACK or NACK  */
  if (len > 0)
  {
    ETX_OTA_RESP_ *resp = (ETX_OTA_RESP_ *) pBuf;
    if (resp->packet_type == ETX_OTA_PACKET_TYPE_RESPONSE)
    {
      // TODO: Add CRC check
      if (resp->status == ETX_OTA_ACK)
      {
        // ACK received
        is_ack = true;
        printf("<<< ACK received...\n");
      }
      else
      {
        // NACK received
        printf("<<< NACK received...\n");
      }
    }
  }

#ifdef DEBUG
  printf("***  debug 5.8");
#endif

  return is_ack;
}

/* Build the OTA START command */
int send_ota_start(int comport)
{
  int ex = 0;
  ////printf("[send_ota_start(): send 1\n");

  uint16_t len;
  ETX_OTA_COMMAND_ *ota_start = (ETX_OTA_COMMAND_ *) DATA_BUF;


  ////printf("[send_ota_start(): send 2\n");

  memset(DATA_BUF, 0, ETX_OTA_PACKET_MAX_SIZE);

  ota_start->sof = ETX_OTA_SOF;
  ota_start->packet_type = ETX_OTA_PACKET_TYPE_CMD;
  ota_start->data_len = 1;
  ota_start->cmd = ETX_OTA_CMD_START;
  ota_start->crc = 0x11223344; // TODO: Add CRC
  ota_start->eof = ETX_OTA_EOF;

  len = sizeof(ETX_OTA_COMMAND_);


  // send OTA START
 
  for (int i = 0; i < len; i++)
  {
    delay(1);

    if (write(comport, &DATA_BUF[i], 1)==0)
    {
      // some data missed.
      printf("OTA START : Send Err\n");
      ex = -1;
      break;
    }

    //xxx int res=write(comport, &DATA_BUF[i], 1);
    //xx printf("%02X (%d).", DATA_BUF[i], res);

    /* //xx//
    if (RS232_SendByte(comport, DATA_BUF[i]))
    {
      // some data missed.
      printf("OTA START : Send Err\n");
      ex = -1;
      break;
    }
    */
  }

  //printf("\n");

  if (ex >= 0)
  {
    if (!is_ack_resp_received(comport))
    {
      // Received NACK
      printf("OTA START : NACK\n");
      ex = -1;
    }
  }

  //printf("OTA START [ex = %d]\n", ex);
  return ex;
}

/* Build and Send the OTA END command */
uint16_t send_ota_end(int comport)
{
  uint16_t len;
  ETX_OTA_COMMAND_ *ota_end = (ETX_OTA_COMMAND_ *) DATA_BUF;
  int ex = 0;

  memset(DATA_BUF, 0, ETX_OTA_PACKET_MAX_SIZE);

  ota_end->sof = ETX_OTA_SOF;
  ota_end->packet_type = ETX_OTA_PACKET_TYPE_CMD;
  ota_end->data_len = 1;
  ota_end->cmd = ETX_OTA_CMD_END;
  ota_end->crc = 0x66778899; // TODO: Add CRC
  ota_end->eof = ETX_OTA_EOF;

  len = sizeof(ETX_OTA_COMMAND_);

  // send OTA END
  for (int i = 0; i < len; i++)
  {
    delay(1);
    
    if (write(comport, &DATA_BUF[i], 1)==0)
    {
      // some data missed.
      printf("OTA END : Send Err\n");
      ex = -1;
      break;
    }

    /* //xx//
    if (RS232_SendByte(comport, DATA_BUF[i]))
    {
      // some data missed.
      printf("OTA END : Send Err\n");
      ex = -1;
      break;
    }
    */
  }

  if (ex >= 0)
  {
    if (!is_ack_resp_received(comport))
    {
      // Received NACK
      printf("OTA END : NACK\n");
      ex = -1;
    }
  }
  printf("OTA END [ex = %d]\n", ex);
  return ex;
}

/* Build and send the OTA Header */
int send_ota_header(int comport, meta_info *ota_info)
{
  uint16_t len;
  ETX_OTA_HEADER_ *ota_header = (ETX_OTA_HEADER_ *) DATA_BUF;
  int ex = 0;

  memset(DATA_BUF, 0, ETX_OTA_PACKET_MAX_SIZE);

  ota_header->sof = ETX_OTA_SOF;
  ota_header->packet_type = ETX_OTA_PACKET_TYPE_HEADER;
  ota_header->data_len = sizeof(meta_info);
  ota_header->crc = 0x00; // TODO: Add CRC
  ota_header->eof = ETX_OTA_EOF;

  memcpy(&ota_header->meta_data, ota_info, sizeof(meta_info));

  len = sizeof(ETX_OTA_HEADER_);
  //printf("--- sending HEADER (len=%d)....\n", len);

  // send OTA Header
  for (int i = 0; i < len; i++)
  {
    delay(1);

    if (write(comport, &DATA_BUF[i], 1)==0)
    {
      // some data missed.
      printf("OTA HEADER : Send Err\n");
      ex = -1;
      break;
    }
  
    //printf("%02X.", DATA_BUF[i]);

    /* //xx//
    if (RS232_SendByte(comport, DATA_BUF[i]))
    {
      // some data missed.
      printf("OTA HEADER : Send Err\n");
      ex = -1;
      break;
    }
    */
  }

  //printf("\n");

  if (ex >= 0)
  {
    if (!is_ack_resp_received(comport))
    {
      // Received NACK
      printf("OTA HEADER : NACK\n");
      ex = -1;
    }
  }
  // printf("OTA HEADER [ex = %d]\n", ex);
  return ex;
}

/* Build and send the OTA Data */
int send_ota_data(int comport, uint8_t *data, uint16_t data_len)
{
  uint16_t len;
  ETX_OTA_DATA_ *ota_data = (ETX_OTA_DATA_ *) DATA_BUF;
  int ex = 0;

  // Clean the buffer
  memset(DATA_BUF, 0, ETX_OTA_PACKET_MAX_SIZE);

  ota_data->sof = ETX_OTA_SOF;
  ota_data->packet_type = ETX_OTA_PACKET_TYPE_DATA;
  ota_data->data_len = data_len;

  len = 4;

  // Copy the data
  memcpy(&DATA_BUF[len], data, data_len);
  len += data_len;
  uint32_t crc = 0u; // TODO: Add CRC

  // Copy the crc
  memcpy(&DATA_BUF[len], (uint8_t *)&crc, sizeof(crc));
  len += sizeof(crc);

  // Add the EOF
  DATA_BUF[len] = ETX_OTA_EOF;
  len++;

  // send OTA Data
  for (int i = 0; i < len; i++)
  {
    delay(1);

    if (write(comport, &DATA_BUF[i], 1)==0)
    {
      // some data missed.
      printf("OTA DATA : Send Err\n");
      ex = -1;
      break;
    }
  }

#ifdef DEBUG
  printf("*** debug 5a [ex=%d]", ex);
#endif

  if (ex >= 0)
  {

#ifdef DEBUG
    printf("*** debug 5b");
#endif

    if (!is_ack_resp_received(comport))
    {
      // Received NACK
      printf("OTA DATA : NACK\n");
      ex = -1;
    }
  }

#ifdef DEBUG
  printf("***  debug 6");
#endif

  // printf("OTA DATA [ex = %d]\n", ex);
  return ex;
}

int main(int argc, char *argv[])
{
  int comport=0;
  int serial_port=0;
  //int bdrate = 115200;              /* 115200 baud */
  //char mode[] = {'8', 'N', '1', 0}; /* *-bits, No parity, 1 stop bit */
  char bin_name[1024];
  int ex = 0;
  FILE *Fptr = NULL;

  printf("OTA update v%s\n\n", VERSION);

  do
  {
    if (argc <= 2)
    {
      printf("Please feed the COM PORT number and the Application Image....!!!\n");
      printf("Example: .\\etx_ota_app.exe 8 ..\\..\\debug\\blinky.bin\n");

      printf("\nAvailable ports:\n");

      for (int i=0; i<RS232_PORTNR/2; i++)
      {
        printf("%2d:   %s\t%2d: %s\n", i, comports[i], i+RS232_PORTNR/2, comports[i+RS232_PORTNR/2]);
      }

      printf("\n");

      ex = -1;
      break;
    }

    // get the COM port Number
    comport = atoi(argv[1]);
    strcpy(bin_name, argv[2]);

    printf("Opening COM%d [%s]...\n", comport, comports[comport]);

    /*if (RS232_OpenComport(comport, bdrate, mode, 0))
    {
      printf("Can not open comport\n");
      ex = -1;
      break;
    }*/

    // Open the serial port. Change device path as needed (currently set to an standard FTDI USB-UART cable type device)
    serial_port = open(comports[comport], O_RDWR);
    //printf("res=%d\n", serial_port);

    if (serial_port<0)
    {
      printf("Can not open comport\n");
      ex = -1;
      break;
    }

    comport=serial_port;            /* The opening port is stored into comport  */

    // If all os OK, configure the port.
    // Create new termios struct, we call it 'tty' for convention
    struct termios tty;

    // Read in existing settings, and handle any error
      if (tcgetattr(serial_port, &tty) != 0) {
          printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
          return 1;
      }

      tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
      tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
      tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
      tty.c_cflag |= CS8; // 8 bits per byte (most common)
      tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
      tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

      tty.c_lflag &= ~ICANON;
      tty.c_lflag &= ~ECHO; // Disable echo
      tty.c_lflag &= ~ECHOE; // Disable erasure
      tty.c_lflag &= ~ECHONL; // Disable new-line echo
      tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
      tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
      tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

      tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
      tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
      // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
      // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

      tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
      tty.c_cc[VMIN] = 0;

      // Set in/out baud rate to be 9600
      //cfsetispeed(&tty, B9600);
      //cfsetospeed(&tty, B9600);
      cfsetispeed(&tty, B115200);
      cfsetospeed(&tty, B115200);

      // Save tty settings, also checking for error
      if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
          printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
          return 1;
      }
    ////////////////////////////////////////////////////////////////////////////////////

    // flush the port
    // RS232_flushRXTX(comport);

    // send OTA Start command

    printf("\n>>> sending OTA Start...\n");

    ex = send_ota_start(comport);

    if (ex < 0)
    {
      printf("send_ota_start Err\n");
      break;
    }

    printf("\nOpening Binary file : %s\n", bin_name);

    Fptr = fopen(bin_name, "rb");

    if (Fptr == NULL)
    {
      printf("Can not open %s\n", bin_name);
      ex = -1;
      break;
    }

    fseek(Fptr, 0L, SEEK_END);
    uint32_t app_size = ftell(Fptr);
    fseek(Fptr, 0L, SEEK_SET);

    printf("File size = %d\n", app_size);

    // Send OTA Header
    meta_info ota_info;
    ota_info.package_size = app_size;
    ota_info.package_crc = 0; // TODO: Add CRC

    printf("\n>>> sending OTA Header...\n");

    ex = send_ota_header(comport, &ota_info);

    if (ex < 0)
    {
      printf("send_ota_header Err\n");
      break;
    }

    // read the full image
    if (fread(APP_BIN, 1, app_size, Fptr) != app_size)
    {
      printf("App/FW read Error\n");
      ex = -1;
      break;
    }

    // usleep(1000000);
    uint16_t size = 0;
    uint8_t pack=1;

    delay(100);

    for (uint32_t i = 0; i < app_size; )
    {
      if ((app_size - i) >= ETX_OTA_DATA_MAX_SIZE)
      {
        size = ETX_OTA_DATA_MAX_SIZE;
      }
      else
      {
        size = app_size - i;
      }

      printf("\n>>> sending OTA Data (tot=%d size=%d i=%d)\n", app_size, size, i+size);
      // printf("[%d/%d]\r\n", i/ETX_OTA_DATA_MAX_SIZE, app_size/ETX_OTA_DATA_MAX_SIZE);
      //printf("\n>>> Sending Data #%d [%d bytes]\n", pack, size);

      ex = send_ota_data(comport, &APP_BIN[i], size);

      if (ex < 0)
      {
        printf("send_ota_data Err [i=%d]\n", i);
        break;
      }

      i += size;
      pack++;

      delay(300);
    }

    if (ex < 0)
    {
      break;
    }

    // send OTA END command
    printf("\n>>> sending OTA End...\n");

    ex = send_ota_end(comport);

    if (ex < 0)
    {
      printf("send_ota_end Err\n");
      break;
    }

  } while (false);

  if (Fptr)
  {
    fclose(Fptr);
  }

/*  if (ex < 0 && argc<2)
  {
    printf("OTA ERROR\n");
  }
*/
  return (ex);
}
