

TCLDIR=/home/jca/git/fpgamake/tcl
BUILDCACHE=/home/jca/git/buildcache/buildcache
CACHEDIR = /home/jca/git/connectal/examples/echo/zybo/Cache
FLOORPLAN=
FPGAMAKE_PARTNAME=xc7z010clg400-1
FPGAMAKE_BOARDNAME=zybo
FPGAMAKE_TOPMODULE=mkZynqTop
FPGAMAKE_FAMILY="Virtex7"
VERILOG_DEFINES=""
PRESERVE_CLOCK_GATES?=0
REPORT_NWORST_TIMING_PATHS?=
include $(TCLDIR)/Makefile.fpgamake.common

mkZynqTop_HEADERFILES = 
mkZynqTop_VFILES = verilog/mkZynqTop.v verilog/mkZynqTop.v /home/jca/git/connectal/verilog/CONNECTNET2.v /scratch/bluespec/Bluespec-2015.09.beta2/lib/Verilog/ResetInverter.v verilog/mkEchoIndicationOutputPipes.v verilog/mkEchoRequestInput.v /scratch/bluespec/Bluespec-2015.09.beta2/lib/Verilog.Vivado/SizedFIFO.v /scratch/bluespec/Bluespec-2015.09.beta2/lib/Verilog/FIFO2.v /scratch/bluespec/Bluespec-2015.09.beta2/lib/Verilog/FIFO1.v verilog/mkConnectalTop.v verilog/mkEchoIndicationOutput.v
mkZynqTop_VHDFILES = 
mkZynqTop_VHDL_LIBRARIES = 
mkZynqTop_STUBS = 
mkZynqTop_IP = 
mkZynqTop_SUBINST = 
mkZynqTop_PATH = verilog/mkZynqTop.v
mkZynqTop_USER_TCL_SCRIPT = /home/jca/git/connectal/constraints/xilinx/cdc.tcl
mkZynqTop_XDC = 

$(eval $(call SYNTH_RULE,mkZynqTop))

TopDown_XDC = /home/jca/git/connectal/constraints/xilinx/xc7z010clg400.xdc /home/jca/git/connectal/constraints/xilinx/zybo.xdc
TopDown_NETLISTS = 
TopDown_RECONFIG = 
TopDown_SUBINST = 
TopDown_PRTOP = 

$(eval $(call TOP_RULE,top,mkZynqTop,hw/mkTop.bit,/home/jca/git/connectal/examples/echo/zybo/hw))

everything: hw/mkTop.bit

