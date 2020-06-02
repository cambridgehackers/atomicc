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
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + '/../../connectal/scripts')
import cppgen

if __name__=='__main__':
    print 'new', sys.argv
    filename = "generatedDesignInterfaceFile.json"
    if (len(sys.argv) > 1):
        filename = sys.argv[1];
    jsondata = json.loads(open(filename).read())
    # for examples/gray
    jsondata['globaldecls'].append(
        { "dtype": "TypeDef",
            "tdtype": { "type": "Type", "name": "10", "params": [ {"name": "32"} ] },
            "tname": "width"
        }
     )
    cppgen.generateJson = False
    #cppgen.generatePacketOnly = True
    cppgen.synchronousInvoke = True
    cppgen.generate_cpp(".", False, jsondata)
    open('jni/driver_signature_file.h', 'w')
