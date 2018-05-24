#ifndef _ConnectalProjectConfig_h
#define _ConnectalProjectConfig_h

#define TRACE_PORTAL ""
#define ConnectalVersion "18.04.1"
#define NumberOfMasters 1
#define PinType "Empty"
#define PinTypeInclude "Misc"
#define NumberOfUserTiles 1
#define SlaveDataBusWidth 32
#define SlaveControlAddrWidth 5
#define BurstLenSize 10
#define project_dir "$(DTOP)"
#define MainClockPeriod 10
#define DerivedClockPeriod 5.000000
#define XILINX 1
#define ZYNQ ""
#define ZynqHostInterface ""
#define PhysAddrWidth 32
#define NUMBER_OF_LEDS 4
#define CONNECTAL_BITS_DEPENDENCES "hw/mkTop.bit"
#define CONNECTAL_RUN_SCRIPT "$(CONNECTALDIR)/scripts/run.android"
#define CONNECTAL_EXENAME "android.exe"
#define CONNECTAL_EXENAME2 "android.exe2"
#define BOARD_zybo ""

#endif // _ConnectalProjectConfig_h
