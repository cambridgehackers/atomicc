/*
   Copyright (C) 2018, The Connectal Project

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
#include <stdlib.h> // atol
#include <string.h>
#include <assert.h>
#include "AtomiccIR.h"
#include "common.h"

int main(int argc, char **argv)
{
    std::list<ModuleIR *> irSeq;
    bool noVerilator = false;
noVerilator = true;
printf("[%s:%d] VERILOGGGEN\n", __FUNCTION__, __LINE__);
    int argIndex = 1;
    if (argc == 3 && !strcmp(argv[argIndex], "-n")) {
        argIndex++;
        noVerilator = true;
    }
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
    if (int ret = generateSoftware(irSeq, argv[0], OutputDir))
        return ret;
    processInterfaces(irSeq);
    preprocessIR(irSeq);
    generateVerilog(irSeq, myName, OutputDir);
    generateMeta(irSeq, myName, OutputDir);

    if (!noVerilator) {
        std::string commandLine = "verilator --cc " + OutputDir + ".v";
        int ret = system(commandLine.c_str());
        printf("[%s:%d] RETURN from '%s' %d\n", __FUNCTION__, __LINE__, commandLine.c_str(), ret);
        if (ret)
            return -1; // force error return to be propagated
    }
    return 0;
}
