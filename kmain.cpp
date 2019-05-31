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
#include "AtomiccIR.h"
#include "common.h"

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
    flagErrorsCleanup = 1;
    generateKami(irSeq, myName, OutputDir);
    return 0;
}

