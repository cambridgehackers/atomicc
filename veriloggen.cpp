/*
   Copyright (C) 2018 John Ankcorn

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
#include "AtomiccExpr.h"
#include "AtomiccReadIR.h"
#include "AtomiccVerilog.h"
#include "AtomiccMetaGen.h"

int main(int argc, char **argv) {
printf("[%s:%d] VERILOGGGEN\n", __FUNCTION__, __LINE__);
    if (argc != 2) {
        printf("[%s:%d] veriloggen <outputFileStem>\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    std::string OutputDir = argv[1];
printf("[%s:%d] stem %s\n", __FUNCTION__, __LINE__, OutputDir.c_str());
    FILE *OStrIRread = fopen((OutputDir + ".generated.IR").c_str(), "r");
    FILE *OStrV = fopen((OutputDir + ".generated.v").c_str(), "w");
    FILE *OStrVH = fopen((OutputDir + ".generated.vh").c_str(), "w");
    fprintf(OStrV, "`include \"%s.generated.vh\"\n\n", OutputDir.c_str());
    std::string myName = OutputDir;
    int ind = myName.rfind('/');
    if (ind > 0)
        myName = myName.substr(0, ind);
    myName += "_GENERATED_";
    fprintf(OStrVH, "`ifndef __%s_VH__\n`define __%s_VH__\n\n", myName.c_str(), myName.c_str());
    std::list<ModuleIR *> irSeq;
    readModuleIR(irSeq, OStrIRread);
    generateModuleIR(irSeq, OStrVH, OStrV);
    fprintf(OStrVH, "`endif\n");
    return 0;
}
