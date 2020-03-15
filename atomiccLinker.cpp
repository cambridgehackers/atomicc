/*
   Copyright (C) 2020, The Connectal Project

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <stdio.h>
#include "AtomiccIR.h"
#include "common.h"

static std::map<std::string, std::list<std::string>> externMap;

#define MUX_PORT "forward"

static void dumpMap()
{
    for (auto item: externMap) {
        std::string arg = item.first;
        size_t ind = arg.find("$");
        if (ind != std::string::npos)
            arg = MUX_PORT + arg.substr(ind);
        printf("    .%s({", arg.c_str());
        std::string sep;
        for (auto ref: item.second) {
            ref = ref.substr(10);
            printf("%s%s", sep.c_str(), (ref + item.first).c_str());
            sep = ", ";
        }
        printf("}),\n");
    }
}

static void recurseObject(std::string name, ModuleIR *IR, std::string vecCount)
{
    if (name != "")
        name += ".";
    for (auto item: IR->fields) {
        if (item.isExternal)
            externMap[item.fldName].push_back(name);
        if (auto IIR = lookupIR(item.type)) {
            recurseObject(name + item.fldName, IIR, item.vecCount);
        }
    }
}

int main(int argc, char **argv)
{
    std::list<ModuleIR *> irSeq;
printf("[%s:%d] VERILOGGGEN\n", __FUNCTION__, __LINE__);
    int argIndex = 1;
    if (argc - 1 != argIndex) {
        printf("[%s:%d] veriloggen <outputFileStem>\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    std::string myName = argv[argIndex];
    std::string OutputDir = myName + ".generated";
    int ind = myName.rfind('/');
    if (ind > 0)
        myName = myName.substr(ind+1);

    readIR(irSeq, OutputDir);
    std::string baseDir = OutputDir;
    ind = baseDir.rfind('/');
    if (ind > 0)
        baseDir = baseDir.substr(0, ind+1);
    for (auto IR : irSeq) {
        recurseObject("", IR, "");
        break;
    }
    dumpMap();
    return 0;
}
