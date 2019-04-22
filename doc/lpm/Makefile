
include ../../makefile.in

all:
	mkdir -p generated
	$(CPP) -c $(CPPFLAGS) -S $(LLVMFLAGS) -o generated/lpm.ll lpm.cpp
