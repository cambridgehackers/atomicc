

##    run: run the program
##         pass parameters to software via 'make RUN_ARGS= run'
RUN_ARGS?=

export DTOP=/home/jca/git/connectal/examples/echo/zybo
CONNECTALDIR=/home/jca/git/connectal
BSVPATH = /home/jca/git/connectal/examples/echo:/home/jca/git/connectal/bsv:/home/jca/git/connectal/examples/echo/zybo/generatedbsv:/home/jca/git/connectal/lib/bsv:/home/jca/git/connectal/generated/xilinx:/home/jca/git/connectal/generated/altera

BOARD=zybo
PROJECTDIR=/home/jca/git/connectal/examples/echo/zybo
MKTOP=mkZynqTop
OS=android
TOOLCHAIN?=
DUT=mkzynqtop

export INTERFACES = MemServerRequest MMURequest MemServerIndication MMUIndication EchoRequest EchoIndication
BSVFILES = /home/jca/git/connectal/examples/echo/Echo.bsv /home/jca/git/connectal/bsv/ConnectalMemory.bsv /home/jca/git/connectal/examples/echo/zybo/generatedbsv/IfcNames.bsv /home/jca/git/connectal/bsv/DisplayInd.bsv
XCIFILES = 

BSCFLAGS_PROJECT = 
SIM_CXX_PROJECT = 
CFLAGS_PROJECT = 
CXXFLAGS_PROJECT = 
XELABFLAGS = 
XSIMFLAGS  = -R
TOPBSVFILE = /home/jca/git/connectal/bsv/ZynqTop.bsv
BSVDEFINES =  -D TRACE_PORTAL -D ConnectalVersion=18.04.1 -D NumberOfMasters=1 -D PinType=Empty -D PinTypeInclude=Misc -D NumberOfUserTiles=1 -D SlaveDataBusWidth=32 -D SlaveControlAddrWidth=5 -D BurstLenSize=10 -D project_dir=$(DTOP) -D MainClockPeriod=10 -D DerivedClockPeriod=5.000000 -D XILINX=1 -D ZYNQ -D ZynqHostInterface -D PhysAddrWidth=32 -D NUMBER_OF_LEDS=4 -D CONNECTAL_BITS_DEPENDENCES=hw/mkTop.bit -D CONNECTAL_RUN_SCRIPT=$(CONNECTALDIR)/scripts/run.android -D CONNECTAL_EXENAME=android.exe -D CONNECTAL_EXENAME2=android.exe2 -D BOARD_zybo
QTUSED = 
export BSVDEFINES_LIST = TRACE_PORTAL ConnectalVersion=18.04.1 NumberOfMasters=1 PinType=Empty PinTypeInclude=Misc NumberOfUserTiles=1 SlaveDataBusWidth=32 SlaveControlAddrWidth=5 BurstLenSize=10 project_dir=$(DTOP) MainClockPeriod=10 DerivedClockPeriod=5.000000 XILINX=1 ZYNQ ZynqHostInterface PhysAddrWidth=32 NUMBER_OF_LEDS=4 CONNECTAL_BITS_DEPENDENCES=hw/mkTop.bit CONNECTAL_RUN_SCRIPT=$(CONNECTALDIR)/scripts/run.android CONNECTAL_EXENAME=android.exe CONNECTAL_EXENAME2=android.exe2 BOARD_zybo
export DUT_NAME = mkZynqTop





TRACE_PORTAL=TRACE_PORTAL
ConnectalVersion=18.04.1
NumberOfMasters=1
PinType=Empty
PinTypeInclude=Misc
NumberOfUserTiles=1
SlaveDataBusWidth=32
SlaveControlAddrWidth=5
BurstLenSize=10
project_dir=$(DTOP)
MainClockPeriod=10
DerivedClockPeriod=5.000000
XILINX=1
ZYNQ=ZYNQ
ZynqHostInterface=ZynqHostInterface
PhysAddrWidth=32
NUMBER_OF_LEDS=4
CONNECTAL_BITS_DEPENDENCES=hw/mkTop.bit
CONNECTAL_RUN_SCRIPT=$(CONNECTALDIR)/scripts/run.android
CONNECTAL_EXENAME=android.exe
CONNECTAL_EXENAME2=android.exe2
BOARD_zybo=BOARD_zybo


include $(CONNECTALDIR)/scripts/Makefile.connectal.build


export VERILOG_PATH=verilog /home/jca/git/connectal/verilog/xilinx /home/jca/git/connectal/verilog $(BLUESPEC_VERILOG)
MODELSIM_FILES= 
FPGAMAKE=$(CONNECTALDIR)/../fpgamake/fpgamake
fpgamake.mk: $(VFILE) Makefile prepare_bin_target
	$(Q)if [ -f ../synth-ip.tcl ]; then vivado -mode batch -source ../synth-ip.tcl; fi
	$(Q)$(FPGAMAKE) $(FPGAMAKE_VERBOSE) -o fpgamake.mk --board=zybo --part=xc7z010clg400-1  --floorplan= --implconstraint=/home/jca/git/connectal/constraints/xilinx/xc7z010clg400.xdc --implconstraint=/home/jca/git/connectal/constraints/xilinx/zybo.xdc  --tcl=/home/jca/git/connectal/constraints/xilinx/cdc.tcl   -t $(MKTOP)   -b hw/mkTop.bit   $(VERILOG_PATH)

synth.%:fpgamake.mk
	$(MAKE) -f fpgamake.mk Synth/$*/$*-synth.dcp

hw/mkTop.bit: prepare_bin_target  fpgamake.mk
	$(Q)mkdir -p hw
	$(Q)$(MAKE) -f fpgamake.mk
ifneq ($(XILINX),)
	$(Q)rsync -rav --include="*/" --include="*.rpt" --exclude="*" Impl/ bin
else ifneq ($(ALTERA),)
	$(Q)cp -f $(MKTOP).sof bin
endif




