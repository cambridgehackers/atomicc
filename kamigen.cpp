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
    std::string baseDir = OutputDir;
    int ind = baseDir.rfind('/');
    if (ind > 0)
        baseDir = baseDir.substr(0, ind+1);
    for (auto IR : irSeq) {
        static std::list<ModData> modLineTop;
        modLineTop.clear();
        generateModuleDef(IR, modLineTop);         // Collect/process kami info

        FILE *OStrV = fopen((baseDir + IR->name + ".v").c_str(), "w");
        if (!OStrV) {
            printf("kamigen: unable to open '%s'\n", (baseDir + IR->name + ".v").c_str());
            exit(-1);
        }
        fprintf(OStrV, "`include \"%s.generated.vh\"\n\n", myName.c_str());
        fprintf(OStrV, "`default_nettype none\n");
        generateModuleHeader(OStrV, modLineTop);
        if (IR->genvarCount) {
            std::string genstr;
            for (int i = 1; i <= IR->genvarCount; i++)
                genstr = ", " GENVAR_NAME + autostr(i);
            fprintf(OStrV, "    genvar %s;\n", genstr.substr(1).c_str());
        }
        generateVerilogOutput(OStrV);
        generateVerilogGenerateOutput(OStrV, IR);
        fprintf(OStrV, "endmodule \n\n`default_nettype wire    // set back to default value\n");
        fclose(OStrV);
    }
    if (printfFormat.size()) {
    FILE *OStrP = fopen((OutputDir + ".printf").c_str(), "w");
    for (auto item: printfFormat) {
        fprintf(OStrP, "%s ", item.format.c_str());
        for (auto witem: item.width)
            fprintf(OStrP, "%d ", witem);
        fprintf(OStrP, "\n");
    }
    fclose(OStrP);
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
    //generateKami(irSeq, myName, OutputDir);
printf("[%s:%d]DONE\n", __FUNCTION__, __LINE__);
    return 0;
}
