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
//#include <string.h>
//#include <assert.h>
#include "AtomiccIR.h"
#include "common.h"

static void generateKami(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir)
{
    FILE *OStrV = fopen((OutputDir + ".kami").c_str(), "w");
    if (!OStrV) {
        printf("kamigen: unable to open '%s'\n", (OutputDir + ".kami").c_str());
        exit(-1);
    }
    for (auto IR : irSeq) {
        //static std::list<ModData> modLineTop;
        //modLineTop.clear();
        //generateModuleDef(IR, modLineTop);         // Collect/process kami info
printf("[%s:%d] module '%s'\n", __FUNCTION__, __LINE__, IR->name.c_str());

        //generateModuleHeader(OStrV, modLineTop);
        //generateVerilogOutput(OStrV);
        //generateVerilogGenerateOutput(OStrV, IR);
        fclose(OStrV);
    }
}

int main(int argc, char **argv)
{
    std::list<ModuleIR *> irSeq;
printf("[%s:%d] KAMIGEN\n", __FUNCTION__, __LINE__);
    int argIndex = 1;
    if (argc - 1 != argIndex) {
        printf("[%s:%d] kamigen <outputFileStem>\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    std::string myName = argv[argIndex];
    std::string OutputDir = myName + ".generated";
    int ind = myName.rfind('/');
    if (ind > 0)
        myName = myName.substr(ind+1);

    readIR(irSeq, OutputDir);
printf("[%s:%d] AFTERREAD\n", __FUNCTION__, __LINE__);
    //cleanupIR(irSeq);
    flagErrorsCleanup = 1;
    //preprocessIR(irSeq);
    generateKami(irSeq, myName, OutputDir);
printf("[%s:%d]DONE\n", __FUNCTION__, __LINE__);
    return 0;
}
