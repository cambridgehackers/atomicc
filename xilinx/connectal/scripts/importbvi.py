#!/usr/bin/python
# Copyright (c) 2013 Quanta Research Cambridge, Inc.
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use, copy,
# modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from __future__ import print_function
import copy, json, optparse, os, sys, re, tokenize
#names of tokens: tokenize.tok_name

masterlist = []
parammap = {}
paramnames = []
conditionalcf = {}
clock_names = []
deleted_interface = []
commoninterfaces = {}
tokgenerator = 0
clock_params = []
reset_params = []
toknum = 0
tokval = 0
modulename = ''

class PinType(object):
    def __init__(self, mode, type, name, origname):
        self.mode = mode
        self.type = type
        self.name = name
        self.origname = origname
        self.comment = ''
        self.separator = ''
        #print('PNN', self.mode, self.type, self.name, self.origname)
#
# parser for .lib files
#
def parsenext():
    global toknum, tokval
    while True:
        toknum, tokval, _, _, _ = tokgenerator.next()
        if toknum != tokenize.NL and toknum != tokenize.NEWLINE:
            break
    #print('Token:', toknum, tokval)
    if toknum == tokenize.ENDMARKER:
        return None, None
    return toknum, tokval

def validate_token(testval):
    global toknum, tokval
    if not testval:
        print('Error:Got:', toknum, tokval)
        sys.exit(1)
    parsenext()

def parseparam():
    paramstr = ''
    validate_token(tokval == '(')
    while tokval != ')' and toknum != tokenize.ENDMARKER:
        paramstr = paramstr + tokval
        parsenext()
    validate_token(tokval == ')')
    validate_token(tokval == '{')
    return paramstr

def parse_item():
    global masterlist, modulename
    paramlist = {}
    while tokval != '}' and toknum != tokenize.ENDMARKER:
        paramname = tokval
        validate_token(toknum == tokenize.NAME)
        if paramname == 'default_intrinsic_fall' or paramname == 'default_intrinsic_rise':
            validate_token(tokval == ':')
            validate_token(toknum == tokenize.NUMBER)
            continue
        if paramname == 'bus_type':
            validate_token(tokval == ':')
            validate_token(toknum == tokenize.NAME)
            continue
        if tokval == '(':
            paramlist['attr'] = []
            while True:
                paramstr = parseparam()
                plist = parse_item()
                if paramstr != '' and paramname != 'fpga_condition':
                    if plist == {}:
                        paramlist['attr'].append([paramstr])
                    else:
                        paramlist['attr'].append([paramstr, plist])
                if paramname == 'cell' and paramstr == options.cell:
                    #print('CC', paramstr)
                    modulename = paramstr
                    pinlist = {}
                    for item in plist['attr']:
                        tname = item[0]
                        tlist = item[1]
                        tdir = 'unknowndir'
                        if tlist.get('direction'):
                            tdir = tlist['direction']
                            del tlist['direction']
                        tsub = ''
                        ind = tname.find('[')
                        if ind > 0:
                            tsub = tname[ind+1:-1]
                            tname = tname[:ind]
                        titem = [tdir, tsub, tlist]
                        ttemp = pinlist.get(tname)
                        if not ttemp:
                            pinlist[tname] = titem
                        elif ttemp[0] != titem[0] or ttemp[2] != titem[2]: 
                            print('different', tname, ttemp, titem)
                        elif ttemp[1] != titem[1]: 
                            if int(titem[1]) > int(ttemp[1]):
                                ttemp[1] = titem[1]
                            else:
                                print('differentindex', tname, ttemp, titem)
                    for k, v in sorted(pinlist.items()):
                        if v[1] == '':
                            ptemp = '1'
                        else:
                            ptemp = str(int(v[1])+1)
                        ttemp = PinType(v[0], ptemp, k, k)
                        if v[2] != {}:
                            ttemp.comment = v[2]
                        masterlist.append(ttemp)
                paramname = tokval
                if toknum != tokenize.NAME:
                    break
                parsenext()
                if tokval != '(':
                    break
        else:
            validate_token(tokval == ':')
            if paramname not in ['fpga_arc_condition', 'function', 'next_state']:
                paramlist[paramname] = tokval
            if toknum == tokenize.NUMBER or toknum == tokenize.NAME or toknum == tokenize.STRING:
                parsenext()
            else:
                validate_token(False)
            if tokval != '}':
                validate_token(tokval == ';')
    validate_token(tokval == '}')
    if paramlist.get('attr') == []:
        del paramlist['attr']
    return paramlist

def parse_lib(filename):
    global tokgenerator, masterlist
    tokgenerator = tokenize.generate_tokens(open(filename).readline)
    parsenext()
    if tokval != 'library':
        sys.exit(1)
    validate_token(toknum == tokenize.NAME)
    parseparam()
    parse_item()
    searchlist = []
    for item in masterlist:
        ind = item.name.find('1')
        if ind > 0:
            searchstr = item.name[:ind]
            #print('II', item.name, searchstr)
            if searchstr not in searchlist:
                for iitem in masterlist:
                    #print('VV', iitem.name, searchstr + '0')
                    if iitem.name.startswith(searchstr + '0'):
                        searchlist.append(searchstr)
                        break
    for item in masterlist:
        for sitem in searchlist:
            tname = item.name
            if tname.startswith(sitem):
                tname = tname[len(sitem):]
                ind = 0
                while tname[ind] >= '0' and tname[ind] <= '9' and ind < len(tname) - 1:
                    ind = ind + 1
                item.name = sitem + tname[:ind] + item.separator + tname[ind:]
                break

def generate_interface(interfacename, paramlist, paramval, ilist, cname):
    global clock_names, deleted_interface
    if interfacename in options.notdef:
        return
    methodfound = False
    for item in ilist:
        #print("GG", item.name, item.type, item.mode)
        if item.mode == 'input' and (item.type != 'Clock' and item.type != 'Reset'):
            methodfound = True
        elif item.mode == 'output':
            methodfound = True
        elif item.mode == 'inout':
            methodfound = True
        elif item.mode == 'interface':
            methodfound = True
    if not methodfound:
        deleted_interface.append(interfacename)
        return
    print(interfacename + paramlist + ' {', file=options.outfile)
    for item in ilist:
        if item.mode != 'input' and item.mode != 'output' and item.mode != 'inout' and item.mode != 'interface':
            continue
        typ = item.type
        outp = ' '
        if typ[0] >= '0' and typ[0] <= '9':
            typ = '__int(' + typ + ')'
        if item.mode == 'inout':
            typ = '__INOUT(' + typ + ')'
        if item.mode == 'output':
            outp = '*'
        print('    ' + typ.ljust(16) + outp + item.name + ';', file=options.outfile)
    print('};', file=options.outfile)

def regroup_items(masterlist):
    global paramnames, commoninterfaces
    paramnames.sort()
    masterlist = sorted(masterlist, key=lambda item: item.type if item.mode == 'parameter' else item.name)
    newlist = []
    currentgroup = ''
    prevlist = []
    for item in masterlist:
        if item.mode != 'input' and item.mode != 'output' and item.mode != 'inout':
            newlist.append(item)
            #print("DD", item.name)
        else:
            litem = item.origname
            titem = litem
            #print('OA', titem)
            separator = '_'
            indexname = ''
            skipParse = False;
            if prevlist != [] and not litem.startswith(currentgroup):
                print('UU', currentgroup, litem, prevlist, file=sys.stderr)
            if options.factor:
                for tstring in options.factor:
                    if len(litem) > len(tstring) and litem.startswith(tstring):
                        groupname = tstring
                        m = re.search('(\d*)(_?)(.+)', litem[len(tstring):])
                        indexname = m.group(1)
                        separator = m.group(2)
                        fieldname = m.group(3)
                        m = None
                        skipParse = True
                        #print('OM', titem, groupname, fieldname, separator)
                        break
            if separator != '' and skipParse != True:
                m = re.search('(.+?)_(.+)', litem)
                if not m:
                    newlist.append(item)
                    #print('OD', item.name)
                    continue
                if len(m.group(1)) == 1: # if only 1 character prefix, get more greedy
                    m = re.search('(.+)_(.+)', litem)
                print('OJ', item.name, m.groups(), file=sys.stderr)
                fieldname = m.group(2)
                groupname = m.group(1)
            skipcheck = False
            for checkitem in options.notfactor:
                if litem.startswith(checkitem):
                    skipcheck = True
            if skipcheck:
                newlist.append(item)
                #print('OI', item.name, file=sys.stderr)
                continue
            itemname = (groupname + indexname)
            interfacename = options.ifprefix + groupname.lower()
            if not commoninterfaces.get(interfacename):
                commoninterfaces[interfacename] = {}
            if not commoninterfaces[interfacename].get(indexname):
                commoninterfaces[interfacename][indexname] = []
                t = PinType('interface', interfacename, itemname, groupname+indexname+separator)
                #print('OZ', interfacename, itemname, groupname+indexname+separator, file=sys.stderr)
                t.separator = separator
                newlist.append(t)
            #print('OH', itemname, separator, file=sys.stderr)
            foo = copy.copy(item)
            foo.origname = fieldname
            lfield = fieldname
            foo.name = lfield
            commoninterfaces[interfacename][indexname].append(foo)
    return newlist

def generate_cpp():
    global paramnames, modulename, clock_names
    global clock_params, reset_params, options
    global commoninterfaces
    # generate output file
    paramlist = ''
    for item in paramnames:
        paramlist = paramlist + ', numeric type ' + item
    if paramlist != '':
        paramlist = '#(' + paramlist[2:] + ')'
    paramval = paramlist.replace('numeric type ', '')
    for k, v in sorted(commoninterfaces.items()):
        #print('interface', k, file=sys.stderr)
        for kuse, vuse in sorted(v.items()):
            if kuse == '' or kuse == '0':
                generate_interface('__verilog ' + k, paramlist, paramval, vuse, [])
            #else:
                #print('     ', kuse, json.dumps(vuse), file=sys.stderr)
    generate_interface('__emodule ' + modulename, paramlist, paramval, masterlist, clock_names)

if __name__=='__main__':
    parser = optparse.OptionParser("usage: %prog [options] arg")
    parser.add_option("-o", "--output", dest="filename", help="write data to FILENAME")
    parser.add_option("-p", "--param", action="append", dest="param")
    parser.add_option("-f", "--factor", action="append", dest="factor")
    parser.add_option("-e", "--export", action="append", dest="export")
    parser.add_option("--notdef", action="append", dest="notdef")
    parser.add_option("-n", "--notfactor", action="append", dest="notfactor")
    parser.add_option("-C", "--cell", dest="cell")
    parser.add_option("-P", "--ifprefix", dest="ifprefix")
    parser.add_option("-I", "--ifname", dest="ifname")
    (options, args) = parser.parse_args()
    print('KK', options, args, file=sys.stderr)
    if options.filename is None or len(args) == 0 or options.ifname is None or options.ifprefix is None:
        print('Missing "--o" option, missing input filenames, missing ifname or missing ifprefix.  Run " importbvi.py -h " to see available options')
        sys.exit(1)
    options.outfile = open(options.filename, 'w')
    if options.notfactor == None:
        options.notfactor = []
    if options.notdef == None:
        options.notdef = []
    if options.param:
        for item in options.param:
            item2 = item.split(':')
            if len(item2) == 1:
                if item2[0] not in paramnames:
                    paramnames.append(item2[0])
            else:
                parammap[item2[0]] = item2[1]
                if item2[1] not in paramnames:
                    paramnames.append(item2[1])
    if len(args) != 1:
        print("incorrect number of arguments", file=sys.stderr)
    else:
        parse_lib(args[0])
        masterlist = regroup_items(masterlist)
        generate_cpp()
