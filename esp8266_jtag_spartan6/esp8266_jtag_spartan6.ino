//////////////////////////////////////////////////////////////////////////
// Author: RSP @ Embedded Systems Lab (ESL), KMUTNB, Bangkok / Thailand
// Date: 2017-07-06
// Arduino IDE: v1.8.2 + esp8266 v2.3.0
// MCU Boards with ESP-12E
// Objective: This sketch shows how to use an ESP8266 module to
//   configure the Xilinx Spartan-6 FPGA device using the JTAG port.
//   The bitstream file ("TOP.BIT") and its associated MD5 checksum file ("TOP.MD5")
//   are stored in a microSD attached to the ESP8266 via the SPI bus.
//   The MD5 checksum calculation is performed first before loading the bitstream.
//   This sketch can successfully load the bitstream into the Xilinx Spartan 6SLX9
//   FPGA device on the Mojo v3 board.
//////////////////////////////////////////////////////////////////////////

#include <SD.h>          // see: https://github.com/esp8266/Arduino/blob/master/libraries/SD/src/SD.h
#include <MD5Builder.h>  // see: https://github.com/esp8266/Arduino/blob/master/cores/esp8266/MD5Builder.h

const char     *config_file = "TOP.BIN"; // the file stored in the microSD 
const char *config_file_md5 = "TOP.MD5"; // the file that contains the MD5 hex string of the file

MD5Builder  md5;   // used to calculate the MD5sum of the bitstream file

// ESP8266 pins for microSD: 
//   D8 = #SS  / GPIO-15
//   D7 = MOSI / GPIO-13
//   D6 = MISO / GPIO-12
//   D5 = SCLK / GPIO-14

const int SD_CS_PIN = 15; // GPIO-15 / D8
const int BTN_PIN   = 16; // GPIO-16 / D0

// ESP8266 Pins for JTAG: 
const int TCK_PIN = 5; // D1 / GPIO-5 (output)
const int TDO_PIN = 4; // D2 / GPIO-4 (input)
const int TDI_PIN = 0; // D3 / GPIO-0 (output)
const int TMS_PIN = 2; // D4 / GPIO-2 (output)

#define XILINX_SPARTAN6
#define USE_FAST_IO
#define MAX_BUF_SIZE   (2048)

// global variables
char sbuf[64];
boolean config_fpga = false;
char buf[MAX_BUF_SIZE];

#ifdef XILINX_SPARTAN6
 // see: Spartan-6 FPGA Configuration User Guide UG380 (v2.10) March 31, 2017
 #define XILINX_IR_LEN             (6)
 #define XILINX_USER1_INSTR        (0x02)  // 000010
 #define XILINX_USER2_INSTR        (0x03)  // 000011
 #define XILINX_USER3_INSTR        (0x1A)  // 011010
 #define XILINX_USER4_INSTR        (0x1B)  // 011011
 #define XILINX_CFG_OUT_INSTR      (0x04)  // 000100
 #define XILINX_CFG_IN_INSTR       (0x05)  // 000101
 #define XILINX_BYPASS_INSTR       (0x1F)  // 111111
 #define XILINX_IDCODE_INSTR       (0x09)  // 001001
 #define XILINX_USERCODE_INSTR     (0x08)  // 001000
 #define XILINX_JPROGRAM_INSTR     (0x0B)  // 001011
 #define XILINX_JSTART_INSTR       (0x0C)  // 001100
 #define XILINX_JSHUTDOWN_INSTR    (0x0D)  // 001101 
 #define XILINX_S6LX9_IDCODE       (0x04001093)
 #define XILINX_DEV_IDCODE         XILINX_S6LX9_IDCODE
#endif

///////////////////////////////////////////////////////////////////////
#ifdef USE_FAST_IO

inline void jtag_clk( int tms ) {
  GPOC = (1<<TCK_PIN);
  if ( tms ) 
    GPOS = (1<<TMS_PIN);
  else
    GPOC = (1<<TMS_PIN);
  GPOS = (1<<TCK_PIN);
}

inline int jtag_clk_data( int tms, int tdi ) {
  GPOC = (1<<TCK_PIN);  
  if ( tdi ) 
    GPOS = (1<<TDI_PIN);
  else
    GPOC = (1<<TDI_PIN);
  if ( tms ) 
    GPOS = (1<<TMS_PIN);
  else
    GPOC = (1<<TMS_PIN);
  GPOS = (1<<TCK_PIN);  
  return (GPI >> TDO_PIN) & 1;
}

void jtag_goto_runtest_idle() {
  GPOC = (1<<TCK_PIN);  
  for ( int i=0; i < 8; i++ ) {
     jtag_clk(1);
  }
  jtag_clk(0);  // goto Run-Test/Idle
}

#else

inline void jtag_clk( int tms ) {
  digitalWrite( TCK_PIN, 0 );
  digitalWrite( TMS_PIN, tms );
  digitalWrite( TCK_PIN, 1 );
}

inline int jtag_clk_data( int tms, int tdi ) {
  int tdo;
  digitalWrite( TCK_PIN, 0 );
  digitalWrite( TDI_PIN, tdi );
  digitalWrite( TMS_PIN, tms ); // TMS must be stable before the rising edge of TCK
  digitalWrite( TCK_PIN, 1 );
  tdo = digitalRead( TDO_PIN );
  return tdo;
}

void jtag_goto_runtest_idle() {
  digitalWrite( TCK_PIN, 0 );
  for ( int i=0; i < 10; i++ ) {
     jtag_clk(1);
  }
  jtag_clk(0);  // goto Run-Test/Idle
}
#endif
///////////////////////////////////////////////////////////////////////

void jtag_load_ir( uint32_t instr, uint32_t ir_len ) {
  int tdi, tms;

  // start from Run-Test/Idle state
  jtag_clk(1);  // goto Select-DR-Scan
  jtag_clk(1);  // goto Select-IR-Scan
  jtag_clk(0);  // goto Capture-IR
  jtag_clk(0);  // goto shift-IR

  for ( int i=0; i < ir_len; i++ ) {
     tdi = (instr & 1);
     tms = (i==(ir_len-1)) ? 1 : 0;
     jtag_clk_data( tms, tdi ); // goto Exit1-IR or shift-DR
     instr = (instr >> 1);
  }
  jtag_clk(1);  // goto Update-IR
  jtag_clk(0);  // goto Run-Test/Idle
}

uint32_t jtag_read_idcode( ) {
  int tdi, tms, tdo;
  uint32_t idcode = 0;

  // start from Run-Test/Idle state
  jtag_clk(1);  // goto Select-DR-Scan
  jtag_clk(0);  // goto Capture-DR
  jtag_clk(0);  // goto Shift-DR

  // now in Shift-DR state
  tdi = 0;
  for ( int i=0; i < 32; i++ ) {
     tms = (i==31) ? 1 : 0;
     tdo = jtag_clk_data( tms, tdi ); // goto Exit1-DR or Shift-DR
     idcode = (tdo << 31) | (idcode >> 1);
  }
  // now in Exit1-DR state
  jtag_clk(1);  // goto Update-DR
  jtag_clk(0);  // goto Run-Test/Idle
  return idcode;
}

boolean check_bitstream_file() {
  String str;
  String md5_hex_str = "";
  boolean md5_valid = true;

  if ( SD.exists( (char *)config_file ) ) {
     File f = SD.open( config_file, O_READ );
     if ( f ) {
       size_t bytes_read_total = 0, bytes_read;
       str = "File size: ";
       str += f.size();
       Serial.println( str );
       md5.begin();
       while ( f.available() ) {
         bytes_read = f.read( buf, MAX_BUF_SIZE-1 );
         bytes_read_total += bytes_read;
         md5.add( (uint8_t *)buf, (uint16_t)bytes_read );
       }
       f.close();

       str = "Number of bytes read: ";
       str += bytes_read_total;
       Serial.println( str );
       
       md5.calculate(); // calculate the MD5 checksum 
       str = "MD5 Hex String (calculated): ";
       md5_hex_str = md5.toString();  // get the MD5 hex string
       str += md5_hex_str;
       Serial.println( str );
     }
  } 
  else {
    str = "Cannot open file (no existing): ";
    str += config_file;
    Serial.println( str );
  }
  
  if ( SD.exists( (char *)config_file_md5 ) ) {
     File f = SD.open( config_file_md5, FILE_READ );
     String expected_md5_hex_str = "";
     str = "MD5 Hex string   (expected): ";
     while ( f.available() ) {
        char ch = f.read();
        if ( ch == '\n' || ch == '\r' ) break;
        expected_md5_hex_str += ch;
     }
     str += expected_md5_hex_str;
     Serial.println( str );
     if ( expected_md5_hex_str.equals( md5_hex_str) ) {
        Serial.println("MD5 checksum OK..." );
     } else {
        Serial.println("MD5 checksum mismatch..." );
        Serial.println( expected_md5_hex_str );
        Serial.println( expected_md5_hex_str.length() );
        Serial.println( md5_hex_str );
        Serial.println( md5_hex_str.length() );
        md5_valid = false;
     }
  }
  else {
    str = "Cannot open file (no existing): ";
    str += config_file_md5;
    Serial.println( str );
 
    File f = SD.open( config_file_md5, O_WRITE | O_CREAT ); // create new file for write
    f.println( md5_hex_str );
    f.close();
  }
  return md5_valid;
}

void configure() {
  size_t num_read_total = 0;
  String str;
  uint32_t ts = millis();
  
  File f = SD.open( config_file, O_READ );
  if (!f) {
    str = "Cannot open bitstream file: ";
    str += config_file;
    Serial.println( str );
    return;
  }
  size_t file_size = f.size();  // get file size (in bytes)

  // goto RunTest/Idle
  jtag_goto_runtest_idle();

  Serial.println( "send JSHUTDOWN instruction" );
  jtag_load_ir( XILINX_JSHUTDOWN_INSTR, XILINX_IR_LEN );
  for ( int i=0; i < 32; i++ ) {
     jtag_clk(0);  // stay at Run-Test/Idle state
  }

  Serial.println( "send CFG_IN instruction" );
  jtag_load_ir( XILINX_CFG_IN_INSTR, XILINX_IR_LEN );

  // start from Run-Test/Idle state
  jtag_clk(1);  // goto Select-DR-Scan
  jtag_clk(0);  // goto Capture-DR
  jtag_clk(0);  // goto Shift-DR

  // now in Shift-DR state
  int tdi, tms;

  Serial.print( "Progress: " );
  while ( f.available() ) {
     size_t chunk_size = f.read( (void *)buf, MAX_BUF_SIZE-1 );
     num_read_total += chunk_size;
     boolean last_bit;
     boolean last_chunk = (num_read_total==file_size);
     for ( int j=0; j < chunk_size; j++ ) {    // for each byte of the chunk data
       boolean last_byte = last_chunk && (j==chunk_size-1);
       uint8_t data = buf[j];                  // get the next byte
       for ( int i=0; i < 8; i++ ) {           // for each bit of the byte data
         tdi = (data & 0x80) ? 1 : 0;          // get the MSB bit
         data = data << 1;                     // shift-to-left
         last_bit = last_byte && (i==7);       // check for the last bit
         tms = ((i==7) && last_bit ) ? 1 : 0;  // goto Exit1-DR (1) or Shift-DR (0)
         jtag_clk_data( tms , tdi );
       }
     }
     if ( ((100*num_read_total)/file_size % 5) == 0 ) {
        Serial.print(".");
     }
  }
  Serial.println("[100%]");

  // now in Exit1-DR state
  jtag_clk(1);  // goto Update-DR
  jtag_clk(0);  // goto Run-Test/Idle

  Serial.println( "send JSTART instruction" );
  jtag_load_ir( XILINX_JSTART_INSTR, XILINX_IR_LEN );

  // toggle TCK for startup sequence
  for ( int i=0; i < 32; i++ ) {
     jtag_clk(0);  // stay at Run-Test/Idle state
  }
 
  f.close();
  
  str = "Bitstream size: ";
  str += num_read_total;
  str += " bytes";
  Serial.println( str );
  str = "Configuration done in " ;
  str += millis() - ts;
  str += " msec";
  Serial.println( str );
}

void show_idcode() {
  jtag_load_ir( XILINX_IDCODE_INSTR, XILINX_IR_LEN );
  uint32_t idcode = jtag_read_idcode();
  sprintf( sbuf, "IDCODE: %08X (%08X) for Xilinx FPGA device", idcode, XILINX_DEV_IDCODE );  
  Serial.println( sbuf );
}

void setup() {
  pinMode( BTN_PIN, INPUT ); // need to connect a pull-up resistor to this pin
  Serial.begin( 115200 );
  Serial.println( "\n\n\n" );
  delay(100);
  
  while ( !SD.begin(SD_CS_PIN, 32000000 /* SPI speed*/) ) {
    Serial.println( "Card failed or not present" );
    delay(1000);
  }
  Serial.println( "Card initialized....\n" );

  check_bitstream_file(); // read the bitstream file and calculate MD5sum

  // configure GPIO pins for JTAG link 
  pinMode( TCK_PIN, OUTPUT );
  pinMode( TMS_PIN, OUTPUT );
  pinMode( TDI_PIN, OUTPUT );
  delay(1000);

  jtag_goto_runtest_idle();

  jtag_load_ir( XILINX_IDCODE_INSTR, XILINX_IR_LEN );
  uint32_t idcode = jtag_read_idcode();
  sprintf( sbuf, "IDCODE: %08X (%08X) for Xilinx FPGA device", idcode, XILINX_DEV_IDCODE );  
  Serial.println( sbuf );

}

void loop() { // perform IDCODE reading loop
  config_fpga = false;
  Serial.println( "Press button to configure the FPGA" );
  Serial.print("Waiting");
  for ( int i=0; i < 30; i++ ) {
    Serial.print(".");
    if ( digitalRead( BTN_PIN ) == LOW ) {
       config_fpga = true;
       break;
    }
    delay(200);
  }
  Serial.print("\n");
  
  if ( config_fpga ) {
     Serial.println( "Start configuring FPGA ..." );
     pinMode( TCK_PIN, OUTPUT );
     pinMode( TMS_PIN, OUTPUT );
     pinMode( TDI_PIN, OUTPUT );
     config_fpga = false;
     configure();
     show_idcode();
  }

  pinMode( TCK_PIN, INPUT );
  pinMode( TMS_PIN, INPUT );
  pinMode( TDI_PIN, INPUT );
  delay(5000);
}
//////////////////////////////////////////////////////////////////////////

