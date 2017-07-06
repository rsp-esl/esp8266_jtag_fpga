Arduino-ESP8266 code for accessing the JTAG port of an FPGA device <br>
(e.g., Xilinx Spartan-6 and Altera Cyclone IV). 

- esp8266_jtag_spartan6:
-- This sketch shows how to use an ESP8266 module to <br>
configure the Xilinx Spartan-6 FPGA device using the JTAG port. <br>
-- The bitstream file ("top.bin") and its associated MD5 checksum file ("top.md5") <br>
are stored in a microSD attached to the ESP8266 via the SPI bus.
-- The MD5 checksum calculation is performed first before loading the bitstream.
-- This sketch can successfully load the bitstream into the Xilinx Spartan 6SLX9 <br>
FPGA device on the Mojo v3 board.
