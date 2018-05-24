

##    run: run the program
##         pass parameters to software via 'make RUN_ARGS= run'
RUN_ARGS?=

export DTOP=$(PWD)
CONNECTALDIR=$(DTOP)/../connectal

BOARD=zybo
MKTOP=mkZynqTop
OS=android
#TOOLCHAIN?=
DUT=mkzynqtop

#TRACE_PORTAL=TRACE_PORTAL
#NumberOfUserTiles=1
#SlaveDataBusWidth=32
#SlaveControlAddrWidth=5
#BurstLenSize=10
#project_dir=$(DTOP)
#MainClockPeriod=10
#DerivedClockPeriod=5.000000
XILINX=1
ZYNQ=ZYNQ
ZynqHostInterface=ZynqHostInterface
PhysAddrWidth=32
#NUMBER_OF_LEDS=4
CONNECTAL_BITS_DEPENDENCES=hw/mkTop.bit
CONNECTAL_RUN_SCRIPT=$(CONNECTALDIR)/scripts/run.android
CONNECTAL_EXENAME=android.exe
BOARD_zybo=BOARD_zybo


include $(CONNECTALDIR)/scripts/Makefile.connectal.build

#BLUESPEC_VERILOG=/scratch/bluespec/Bluespec-2015.09.beta2/lib/Verilog
export VERILOG_PATH=verilog $(CONNECTALDIR)/verilog/xilinx $(CONNECTALDIR)/verilog $(BLUESPEC_VERILOG)
FPGAMAKE=$(CONNECTALDIR)/../../fpgamake/fpgamake

synth.%:fpgamake.mk
	$(MAKE) -f fpgamake.mk Synth/$*/$*-synth.dcp

hw/mkTop.bit: prepare_bin_target  fpgamake.mk
	$(Q)mkdir -p hw
	$(Q)$(MAKE) -f fpgamake.mk

