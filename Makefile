#
#   Copyright (C) 2018 The Connectal Project
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of version 2 of the GNU General Public License as
#    published by the Free Software Foundation.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

SOURCES = main.cpp verilog.cpp util.cpp interfaces.cpp \
    expr.cpp filegen.cpp readIR.cpp software.cpp metaGen.cpp preprocessIR.cpp kami.cpp
KAMI_SOURCES = kmain.cpp kami.cpp util.cpp readIR.cpp expr.cpp preprocessIR.cpp
LINKER_SOURCES = atomiccLinker.cpp util.cpp readIR.cpp expr.cpp
CUDDINC = -I../cudd/cudd
CUDDLIB = ../cudd/cudd/.libs/libcudd.a

all: veriloggen kamigen atomiccImport atomiccLinker

veriloggen: $(SOURCES) *.h
	@clang++ -o veriloggen -g -std=c++11 \
            -fblocks -fno-exceptions -fno-rtti -fvisibility-inlines-hidden -fPIC \
            -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
            -pedantic \
            -Wall -W \
            -Werror=date-time -Wno-long-long -Wno-unused-parameter \
            -Wwrite-strings -Wcovered-switch-default -Wcast-qual \
            -Wmissing-field-initializers -Wstring-conversion    \
            -Wnon-virtual-dtor -Wdelete-non-virtual-dtor \
            -I. $(CUDDINC) $(SOURCES) -lBlocksRuntime $(CUDDLIB)

kamigen: $(KAMI_SOURCES) *.h
	@clang++ -o kamigen -g -std=c++11 \
            -fblocks -fno-exceptions -fno-rtti -fvisibility-inlines-hidden -fPIC \
            -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
            -pedantic \
            -Wall -W \
            -Werror=date-time -Wno-long-long -Wno-unused-parameter \
            -Wwrite-strings -Wcovered-switch-default -Wcast-qual \
            -Wmissing-field-initializers -Wstring-conversion    \
            -Wnon-virtual-dtor -Wdelete-non-virtual-dtor \
            -I. $(CUDDINC) $(KAMI_SOURCES) -lBlocksRuntime $(CUDDLIB)

atomiccLinker: $(LINKER_SOURCES) *.h
	@clang++ -o atomiccLinker -g -std=c++11 \
            -fblocks -fno-exceptions -fno-rtti -fvisibility-inlines-hidden -fPIC \
            -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
            -pedantic \
            -Wall -W \
            -Werror=date-time -Wno-long-long -Wno-unused-parameter \
            -Wwrite-strings -Wcovered-switch-default -Wcast-qual \
            -Wmissing-field-initializers -Wstring-conversion    \
            -Wnon-virtual-dtor -Wdelete-non-virtual-dtor \
            -I. $(CUDDINC) $(LINKER_SOURCES) -lBlocksRuntime $(CUDDLIB)

atomiccImport: atomiccImport.cpp
	clang++ -g -std=c++11 -o atomiccImport atomiccImport.cpp

clean:
	rm -f veriloggen atomiccImport
