Arduino-ESP8266 code for accessing the JTAG port of an FPGA device <br>
(e.g., Xilinx Spartan-6 and Altera Cyclone IV). 

- esp8266_jtag_idcode_read: <br>
This sketch is used to read the IDCODE of an FPGA device via the JTAG port. <br>
Note that it works with a JTAG chain with a single device only.

- esp8266_jtag_spartan6: <br>
This sketch shows how to use an ESP8266 module to configure the Xilinx Spartan-6 <br>
FPGA device using the JTAG port. <br>
The bitstream file ("top.bin") and its associated MD5 checksum file ("top.md5") <br>
are stored in a microSD attached to the ESP8266 via the SPI bus.<br>
The MD5 checksum calculation is performed first before loading the bitstream.<br>
This sketch can successfully load the test bitstream into the Xilinx Spartan 6SLX9 <br>
FPGA device on the Mojo v3 board. <br>
Note that it works with a JTAG chain with a single device only.
