#
#   Copyright (C) 2018 John Ankcorn
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

#/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/
all:
	@clang++  \
    -g \
    -fblocks \
    -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
    -fPIC \
    -fvisibility-inlines-hidden \
    -Werror=date-time \
    -std=c++11 \
    -Wall \
    -W \
    -Wno-unused-parameter \
    -Wwrite-strings \
    -Wcast-qual \
    -Wmissing-field-initializers \
    -pedantic \
    -Wno-long-long \
    -Wcovered-switch-default \
    -Wnon-virtual-dtor \
    -Wdelete-non-virtual-dtor \
    -Wstring-conversion    \
    -fno-exceptions \
    -fno-rtti \
    -I. -I../llvm/lib/Target/Atomicc \
    -lBlocksRuntime \
    -o veriloggen veriloggen.cpp
