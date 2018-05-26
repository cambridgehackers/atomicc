
CONNECTALDIR?=$(PWD)/../..
DTOP?=$(PWD)/..

CC=$(TOOLCHAIN)gcc
CXX=$(TOOLCHAIN)g++
CFLAGS_COMMON = -O -g -I$(DTOP)/jni -I$(CONNECTALDIR)/cpp
CFLAGS = $(CFLAGS_COMMON)

include $(CONNECTALDIR)/scripts/Makefile.connectal.application
SOURCES = ../testecho.cpp ../../cpp/transportXsim.c $(PORTAL_SRC_FILES)
XSOURCES = $(CONNECTALDIR)/cpp/XsimTop.cpp $(PORTAL_SRC_FILES)
LDLIBS :=    -lpthread

ubuntu.exe: $(SOURCES)
	$(CXX) $(CFLAGS) -o ubuntu.exe $(SOURCES) $(LDLIBS)
