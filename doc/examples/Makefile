include ../../../atomicc-examples/makefile.in

#CPPFLAGS += -emit-llvm
#CPPFLAGS += -mllvm -dtrace
#CPPFLAGS += -mllvm -rgentrace
#CPPFLAGS += -mllvm -mgentrace

all:
	mkdir -p generated
	$(CPP) -c $(CPPFLAGS) -S $(LLVMFLAGS) -o generated/example1.ll example1.cpp
	$(CPP) -c $(CPPFLAGS) -S $(LLVMFLAGS) -o generated/gcd.ll gcd.cpp

