# Host flash soft
It is located in the ota_update directory.
Compile it with:
$ cd ota_update
$ make

Use it:
Find your serial port (those pointing to USART6 on the board, see bootloader source code).
$ ./ota_update 

Flash the binary
1. Check that the bootloader is flashed onto the card (see README.md into XXXXX).
2. Connect the card via USB.
3. Launch minicom pointing to the USB UART (USART2 on the board, see bootloader source code).
3. Launch the card and push the button to switch to the L2 Bootloader (you should see messages from the Bootloader into the minicom)
4. Flash the bin code using the bootloader:
	$ ./ota_update 24 <binary to flash.bin>
