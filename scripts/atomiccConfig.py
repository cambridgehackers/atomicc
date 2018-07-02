#!/usr/bin/python
## Copyright (c) 2013-2014 Quanta Research Cambridge, Inc.

## Permission is hereby granted, free of charge, to any person
## obtaining a copy of this software and associated documentation
## files (the "Software"), to deal in the Software without
## restriction, including without limitation the rights to use, copy,
## modify, merge, publish, distribute, sublicense, and/or sell copies
## of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:

## The above copyright notice and this permission notice shall be
## included in all copies or substantial portions of the Software.

## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
## EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
## MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
## NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
## BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
## ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
## CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
import os, sys, json

verilatorProjectString = '''
#ifndef _ConnectalProjectConfig_h
#define _ConnectalProjectConfig_h

#define MainClockPeriod 20
#define SIMULATION ""
#define BOARD_verilator ""

#endif // _ConnectalProjectConfig_h
'''

zyboProjectString = '''
#ifndef _ConnectalProjectConfig_h
#define _ConnectalProjectConfig_h

#define TRACE_PORTAL ""
#define MainClockPeriod 10
#define XILINX 1
#define ZYNQ ""
#define BOARD_zybo ""

#endif // _ConnectalProjectConfig_h
'''

if __name__=='__main__':
    print 'new', sys.argv
    if len(sys.argv) != 2 or (sys.argv[1] != 'verilator' and sys.argv[1] != 'zybo'):
        print 'atomiccConfig.py <targetDirectoryName>'
        sys.exit(-1)
    #filename = 'generatedDesignInterfaceFile.json'
    #if (len(sys.argv) > 1):
    #    filename = sys.argv[1];
    #jsondata = json.loads(open(filename).read())
    fd = open(sys.argv[1] + '/ConnectalProjectConfig.h', 'w')
    if sys.argv[1] == 'zybo':
        fd.write(zyboProjectString)
    else:
        fd.write(verilatorProjectString)

