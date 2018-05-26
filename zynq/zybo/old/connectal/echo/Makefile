
export DTOP=$(PWD)
CONNECTALDIR=$(PWD)/..

VERILOG_PATH=./verilog ../verilog ../bsvverilog
VERILATOR_ARGS= -O3 -CFLAGS "-I$(CONNECTALDIR)/cpp -I$(DTOP)/jni -O " -LDFLAGS "-O " \
    -Wno-fatal -exe --profile-cfuncs --output-split 20000 $(VERILOG_PATH:%=-y %) \
    -DDerivedClockPeriod=4 -DMainClockPeriod=4

SIM_CXX_COMMON = $(addprefix $(CONNECTALDIR)/cpp/, sock_utils.c portalPrintf.c XsimTop.cpp poller.cpp \
    transportSocket.c transportHardware.c transportXsim.c portal.c)
SIM_CXX_LOCAL = $(SIM_CXX_COMMON) $(DTOP)/jni/XsimMsgRequest.c \
    $(DTOP)/jni/XsimMsgIndication.c $(DTOP)/jni/GeneratedCppCallbacks.cpp
SIM_OBJECTS = $(addprefix $(DTOP)/lib/, $(addsuffix .o, $(basename $(notdir $(SIM_CXX_LOCAL)))))

all: prepare_dir ubuntu.exe bin/libconnectal-sim.so vlsim

define SIM_CXX_RULE
$1/$(notdir $(basename $3)).o: $3
	$(CXX) -c $2 -o $1/$(notdir $(basename $3)).o $3
endef
SIM_CXXFLAGS= $(CXXFLAGS_PROJECT) -fPIC -Ijni -I $(CONNECTALDIR)/cpp -I $(CONNECTALDIR) -O
$(foreach src, $(SIM_CXX_LOCAL), $(eval $(call SIM_CXX_RULE, $(DTOP)/lib, $(SIM_CXXFLAGS), $(src))))

run:
	./bin/ubuntu.exe

prepare_dir:
	mkdir -p bin lib obj

ubuntu.exe:
	cd jni; $(MAKE) --no-print-directory -f Ubuntu.mk ubuntu.exe
	cp -v jni/ubuntu.exe bin

bin/libconnectal-sim.so: $(SIM_OBJECTS)
	$(CXX) -O -g -Ijni -shared -fpic $(SIM_CXXFLAGS) -g -o bin/libconnectal-sim.so $(SIM_OBJECTS)

vlsim:  obj_dir/vlsim

obj_dir/vlsim.mk: bin/libconnectal-sim.so verilog
	rm -fr obj_dir
	verilator -o vlsim --prefix vlsim $(VERILATOR_ARGS) -cc \
	    --top-module mkXsimTop $(DTOP)/verilog/mkXsimTop.v \
	    $(CONNECTALDIR)/cpp/verilatortop.cpp -LDFLAGS -L$(DTOP)/bin

obj_dir/vlsim: obj_dir/vlsim.mk
	+$(MAKE) USER_LDLIBS="-lconnectal-sim -lpthread" VM_PARALLEL_BUILDS=1 -C obj_dir -f vlsim.mk
	cp obj_dir/vlsim bin/vlsim

clean:
	rm -rf obj_dir/ xx xxb */*.o SWSOCK0 */ubuntu.exe* */libconnectal-sim.so bin/vlsim
