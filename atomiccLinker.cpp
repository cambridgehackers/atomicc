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
#include <unistd.h>
#include "AtomiccIR.h"
#include "common.h"

#define MAX_FILENAME 1000

typedef struct {
    std::string            type;
    std::list<std::string> name;
} MapInfo;
static std::map<std::string, MapInfo> externMap;
static char filenameBuffer[MAX_FILENAME];
static std::string topModule;

//#define MUX_PORT "in"

static void dumpMap()
{
    for (auto item: externMap) {
        ModuleIR *temp = lookupIR(item.second.type);
        if (!temp && item.second.type == "Printf")
            continue;     // ignore this 'fake' module
        assert(temp);
        ModuleIR *IIR = lookupInterface(temp->interfaceName);
        assert(IIR);
        std::list<std::string> portName;
        for (auto fitem: IIR->fields) {
            if (fitem.fldName != "CLK" && fitem.fldName != "nRST") {
                portName.push_back(fitem.fldName);
                std::string size = convertType(fitem.type);
                if (size != "1")
                    printf("FunnelBase#(.funnelWidth(%d), .dataWidth(%s)) printFunnel (.CLK(CLK), .nRST(nRST),\n",
                        (int)item.second.name.size() + 1, size.c_str());
            }
        }
        for (auto arg: portName) {
            printf("    .in$%s('{", arg.c_str());
            for (auto ref: item.second.name) {
                printf("%s, ", (ref + item.first + PERIOD + arg).c_str());
            }
            printf("ind$%s}),\n", arg.c_str());
        }
        printf("    .out$enq__ENA(indication$enq__ENA),.out$enq$v(indication$enq$v),.out$enq__RDY(indication$enq__RDY)\n     );\n");
    }
}

static void recurseObject(std::string name, ModuleIR *IR, std::string vecCount)
{
    if (name != "")
        name += PERIOD;
    for (auto item: IR->fields) {
        if (item.type == "Printf" || startswith(item.type, "Trace") ) {
printf("[%s:%d] NAMEMEM %s FIELD %s\n", __FUNCTION__, __LINE__, name.c_str(), item.fldName.c_str());
            std::string fieldName = item.fldName;
            //if (startswith(fieldName, "printfp"))
                //fieldName = fieldName.substr(8);
            externMap[fieldName].name.push_back(name);
            //if (externMap[fieldName].type != "" && externMap[fieldName].type != item.type)
                 //printf("[%s:%d] prevtype %s nexttype %s\n", __FUNCTION__, __LINE__, externMap[fieldName].type.c_str(), item.type.c_str());
            externMap[fieldName].type = item.type;
        }
        if (auto IIR = lookupIR(item.type)) {
//printf("[%s:%d] JNAMEMEM %s FIELD %s\n", __FUNCTION__, __LINE__, name.c_str(), item.fldName.c_str());
            recurseObject(name + item.fldName, IIR, item.vecCount);
        }
    }
}

std::string getRelativePath(std::string path)
{
    std::string cwd = getcwd(filenameBuffer, sizeof(filenameBuffer));
    char *ccwd = realpath(cwd.c_str(), filenameBuffer);
    if (!ccwd) {
        printf("%s: ERROR: path does not exist '%s' or '%s' errno %d\n", __FUNCTION__, cwd.c_str(), filenameBuffer, errno);
        exit(-1);
    }
    cwd = ccwd;
    char *cpath = realpath(path.c_str(), filenameBuffer);
    if (!cpath) {
        printf("%s: ERROR: path does not exist '%s' or '%s' errno %d\n", __FUNCTION__, path.c_str(), filenameBuffer, errno);
        exit(-1);
    }
    path = cpath;
    unsigned ind = 0, lastslash = 0;
    while (ind < cwd.length() && ind < path.length() && cwd[ind] == path[ind]) {
        if (cwd[ind] == '/')
            lastslash = ind;
        ind++;
    }
    if (!lastslash)
        return path;
    path = ".." + path.substr(lastslash);
    while (cwd[ind]) {
        if (cwd[ind] == '/')
            path = "../" + path;
        ind++;
    }
    return path;
}

static void generateYosys(std::string dirName, std::list<std::string> &fileList)
{
static const char *yosys_template = 
    "[options]\n"
    "mode bmc\n"
    "\n"
    "[engines]\n"
    "smtbmc\n"
    "\n"
    "[script]\n"
    "read_verilog -sv -formal ";
static const char *yosys_template2 = "\n\nprep -top ";
static const char *yosys_template3 = "\n\n[files]\n";

    FILE *ofile = fopen((dirName + "/" + "yosys.sby").c_str(), "w");
    fprintf(ofile, "%s", yosys_template);
    for (auto item: fileList) {
        int ind = item.rfind("/");
        if (ind != -1)
            item = item.substr(ind+1);
        if (endswith(item, ".v") || endswith(item, ".sv"))
            fprintf(ofile, "%s ", item.c_str());
    }
    fprintf(ofile, "%s", (yosys_template2 + topModule + yosys_template3).c_str());
    for (auto item: fileList)
        fprintf(ofile, "%s\n", item.c_str());
    fclose(ofile);
}

int main(int argc, char **argv)
{
    std::list<ModuleIR *> irSeq;
    std::list<std::string> fileList;
printf("[%s:%d] atomiccLinker\n", __FUNCTION__, __LINE__);
    int argIndex = 1;
    if (argc - 1 != 2) {
        printf("[%s:%d] atomiccLinker <moduleName> <outputFileStem>\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    topModule = argv[argIndex++];
    std::string myName = argv[argIndex];
    std::string OutputDir = myName + ".generated";
    std::string dirName;
    int ind = myName.rfind('/');
    if (ind > 0) {
        dirName = myName.substr(0, ind);
        myName = myName.substr(ind+1);
    }

    std::string commandLine = getExecutionFilename(filenameBuffer, sizeof(filenameBuffer));
    ind = commandLine.rfind("/");
    std::string atomiccDir = commandLine.substr(0, ind);
    commandLine = atomiccDir + "/../verilator/bin/verilator_bin";
    commandLine += " -Mdir " + dirName + " ";
    commandLine += " --atomicc -y " + dirName;
    commandLine += " -y " + getRelativePath(atomiccDir + "/../atomicc-examples/lib/generated/");
    commandLine += " -y " + getRelativePath(atomiccDir + "/../atomicc-examples/lib/verilog/");
    commandLine += " -y " + getRelativePath(atomiccDir + "/simulation/verilog/");
    commandLine += " --top-module " + topModule + " " + topModule + ".sv";
    int ret = system(commandLine.c_str());
    printf("[%s:%d] return %d from running '%s'\n", __FUNCTION__, __LINE__, ret, commandLine.c_str());
    readIR(irSeq, fileList, OutputDir);
    std::string baseDir = OutputDir;
    ind = baseDir.rfind('/');
    if (ind > 0)
        baseDir = baseDir.substr(0, ind+1);
    for (auto IR : irSeq) {
        recurseObject("", IR, "");
        break;
    }
    dumpMap();
    generateYosys(dirName, fileList);
    //for (auto item: fileList)
        //printf("[%s:%d] file '%s'\n", __FUNCTION__, __LINE__, item.c_str());
    return 0;
}
