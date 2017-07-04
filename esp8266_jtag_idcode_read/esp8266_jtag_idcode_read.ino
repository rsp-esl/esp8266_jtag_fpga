//////////////////////////////////////////////////////////////////////////
// Author: RSP @ Embedded Systems Lab (ESL), KMUTNB, Bangkok / Thailand
// Date: 2017-07-04
// Arduino IDE: v1.8.2 + esp8266 v2.3.0
// MCU Boards with ESP-12E
// Note:
//  - This Arduino sketch demonstrates how to use ESP8266 
//    as a JTAG link for reading the IDCODE of an FPGA device
//    (e.g. Spartan-6 and Cyclone IV). 
//  - It is assumed that there is only one device in the JTAG chain.
//////////////////////////////////////////////////////////////////////////

const int TCK_PIN = 5; // D1 / GPIO-5 (output)
const int TDO_PIN = 4; // D2 / GPIO-4 (input)
const int TDI_PIN = 0; // D3 / GPIO-0 (output)
const int TMS_PIN = 2; // D4 / GPIO-2 (output)

#define XILINX_SPARTAN6
//#define ALTERA_CYCLONE4

#ifdef XILINX_SPARTAN6
 // see: Spartan-6 FPGA Configuration User Guide UG380 (v2.10) March 31, 2017
 #define XILINX_IR_LEN             (6)
 #define XILINX_BYPASS_INSTR       (0x1F)
 #define XILINX_IDCODE_INSTR       (0x09)
 #define XILINX_USERCODE_INSTR     (0x08)
 #define XILINX_JPROGRAM_INSTR     (0x0B)
 #define XILINX_JSHUTDOWN_INSTR    (0x0D)
 #define XILINX_6SLX9_IDCODE       (0x04001093)
#endif

#ifdef ALTERA_CYCLONE4
 #define ALTERA_IR_LEN             (10)
 #define ALTERA_BYPASS_INSTR       (0x3FF)
 #define ALTERA_IDCODE_INSTR       (0x006)
 #define ALTERA_USERCODE_INSTR     (0x007)
 #define ALTERA_C4E6_IDCODE        (0x020F10DD)
#endif

// global variable
char sbuf[64];

inline void jtag_clk( int tms ) {
  digitalWrite( TCK_PIN, 0 );
  digitalWrite( TMS_PIN, tms );
  digitalWrite( TCK_PIN, 1 );
}

inline int jtag_clk_data( int tms, int tdi ) {
  int data;
  digitalWrite( TCK_PIN, 0 );
  digitalWrite( TDI_PIN, tdi );
  digitalWrite( TMS_PIN, tms ); // TMS must be stable before the rising edge of TCK
  digitalWrite( TCK_PIN, 1 );
  data = digitalRead( TDO_PIN );
  return data;
}

void jtag_goto_runtest_idle() {
  digitalWrite( TCK_PIN, 0 );
  for ( int i=0; i < 8; i++ ) {
     jtag_clk(1);
  }
  jtag_clk(0);  // goto Run-Test/Idle
}

void jtag_load_ir_idcode( uint32_t instr, uint32_t ir_len ) {
  // start from Run-Test/Idle state
  jtag_clk(1);  // goto Select-DR-Scan
  jtag_clk(1);  // goto Select-IR-Scan
  jtag_clk(0);  // goto Capture-IR
  jtag_clk(0);  // goto shift-IR

  int data;
  for ( int i=0; i < ir_len; i++ ) {
     data = (instr & 1);
     jtag_clk_data( (i==(ir_len-1)) ? 1 : 0, data ); // goto Exit1-IR or shift-DR
     instr = (instr >> 1);
  }
  jtag_clk(1);  // goto Update-IR
  jtag_clk(0);  // goto Run-Test/Idle
}

uint32_t jtag_read_idcode( ) {
  int data;
  uint32_t idcode = 0;

  // start from Run-Test/Idle state
  jtag_clk(1);  // goto Select-DR-Scan
  jtag_clk(0);  // goto Capture-DR
  jtag_clk(0);  // goto Shift-DR

  for ( int i=0; i < 32; i++ ) {
     data = jtag_clk_data( (i==31) ? 1 : 0, 0 ); // goto Exit1-DR or Shift-DR
     idcode = (data << 31) | (idcode >> 1);
  }
  jtag_clk(1);  // goto Update-DR
  jtag_clk(0);  // goto Run-Test/Idle
  return idcode;
}

void setup() {
  Serial.begin( 115200 );
  Serial.println( "\n\n\n" );
  delay(100);
  // configure GPIO pins for JTAG link 
  pinMode( TCK_PIN, OUTPUT );
  pinMode( TMS_PIN, OUTPUT );
  pinMode( TDI_PIN, OUTPUT );
  delay(1000);

  Serial.println( "ESP8266 JTAG Test" );
  Serial.println( "Reading IDCODE of an FPGA device" );
  Serial.println( "Assume that there is only one FPGA device in the JTAG chain!!!\n\n" );
  jtag_goto_runtest_idle();
}

void loop() { // perform IDCODE reading loop

#ifdef ALTERA_CYCLONE4
  jtag_load_ir_idcode( ALTERA_IDCODE_INSTR, ALTERA_IR_LEN );
  uint32_t idcode = jtag_read_idcode();
  sprintf( sbuf, "IDCODE: %08X (%08X) for Cyclone IV EP4CE6E22", idcode, ALTERA_C4E6_IDCODE );  
#endif 

#ifdef XILINX_SPARTAN6
  jtag_load_ir_idcode( XILINX_IDCODE_INSTR, XILINX_IR_LEN );
  uint32_t idcode = jtag_read_idcode();
  sprintf( sbuf, "IDCODE: %08X (%08X) for Xilinx Spartan-6", idcode, XILINX_6SLX9_IDCODE );  
#endif

  Serial.println( sbuf );
  delay(2000);
}
//////////////////////////////////////////////////////////////////////////