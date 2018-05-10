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

typedef struct {
    FieldElement field;
    ModuleIR *IR; // containing Module
    ModuleIR *inter; // interface
} SoftwareItem;

static const char *jsonPrefix = 
    "{\n"
    "    \"bsvdefines\": [ ],\n"
    "    \"globaldecls\": [\n"
    "        { \"dtype\": \"TypeDef\",\n"
    "            \"tdtype\": { \"name\": \"Bit\", \"params\": [ { \"name\": \"32\" } ] },\n"
    "            \"tname\": \"SpecialTypeForSendingFd\"\n"
    "        },\n"
    "        { \"dtype\": \"TypeDef\",\n"
    "            \"tdtype\": {\n"
    "                \"elements\": [ %s ],\n"
    "                \"name\": \"IfcNames\",\n"
    "                \"type\": \"Enum\"\n"
    "            },\n"
    "            \"tname\": \"IfcNames\"\n"
    "        }\n"
    "    ],\n"
    "    \"interfaces\": [";

static std::map<std::string, SoftwareItem> softwareNameList;
static std::string jsonSep;
static void jsonGenerate(FILE *OStrJ, std::string aname, SoftwareItem &swInfo)
{
    fprintf(OStrJ, "%s\n        { \"cdecls\": [", jsonSep.c_str());
    std::map<std::string, MethodInfo *> reorderList;
    std::string msep;
    for (auto FI : swInfo.inter->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        if (!endswith(methodName, "__ENA"))
            continue;
        reorderList[methodName.substr(0, methodName.length()-5)] = MI;
    }
    for (auto item: reorderList) {
        MethodInfo *MI = item.second;
        std::string methodName = item.first; // reorderList, not method!!
        std::string psep;
        fprintf(OStrJ, "%s\n                { \"dname\": \"%s\", \"dparams\": [",
             msep.c_str(), methodName.c_str());
        for (auto pitem: MI->params) {
             fprintf(OStrJ, "%s\n                        { \"pname\": \"%s\", "
                 "\"ptype\": { \"name\": \"Bit\", \"params\": [ { "
                 "\"name\": \"%d\" } ] } }",
                 psep.c_str(), pitem.name.c_str(), (int)convertType(pitem.type));
             psep = ",";
        }
        fprintf(OStrJ, "\n                    ] }");
        msep = ",";
    }
    fprintf(OStrJ, "\n            ], \"direction\": \"%d\", \"cname\": \"%s\" }",
        swInfo.field.isPtr, aname.c_str());
    jsonSep = ",";
}

int generateSoftware(std::list<ModuleIR *> &irSeq, const char *exename, std::string outName)
{
#if 0
< EMODULE l_module_OC_M2P {
<     INTERFACE l_ainterface_OC_EchoIndication method
<     INTERFACE/Ptr l_ainterface_OC_PipeIn pipe
< }
< EMODULE l_module_OC_P2M {
<     INTERFACE l_ainterface_OC_PipeIn pipe
<     INTERFACE/Ptr l_ainterface_OC_EchoRequest method
< }
<     INTERFACECONNECT lERI$method lEcho$request l_ainterface_OC_EchoRequest
<     INTERFACECONNECT lEcho$indication lEIO$method l_ainterface_OC_EchoIndication
<     INTERFACECONNECT request lERI$pipe l_ainterface_OC_PipeIn
<     INTERFACECONNECT indication lEIO$pipe l_ainterface_OC_PipeIn
<     FIELD l_module_OC_P2M lERI
<     FIELD l_module_OC_M2P lEIO
<     INTERFACE l_ainterface_OC_PipeIn request
<     INTERFACE/Ptr l_ainterface_OC_PipeIn indication
---
>     INTERFACECONNECT request lEcho$request l_ainterface_OC_EchoRequest
>     INTERFACECONNECT indication lEcho$indication l_ainterface_OC_EchoIndication
>     INTERFACE l_ainterface_OC_EchoRequest request
>     INTERFACE/Ptr l_ainterface_OC_EchoIndication indication
#endif
    FILE *OStrJ = nullptr;
    for (auto IR: irSeq) {
        for (auto interfaceName: IR->softwareName) {
            for (auto iitem: IR->interfaces) {
                if (iitem.fldName == interfaceName) {
                    ModuleIR *inter = lookupIR(iitem.type);
                    softwareNameList[inter->name.substr(strlen("l_ainterface_OC_"))] = SoftwareItem{iitem, IR, inter};
                }
            }
        }
    }
    if (softwareNameList.size() > 0) {
        int counter = 5;
        std::string enumList, sep;
        for (auto item: softwareNameList) {
            std::string name = "IfcNames_" + item.first + (item.second.field.isPtr ? "H2S" : "S2H");
            enumList += sep + "[ \"" + name + "\", \"" + autostr(counter++) + "\" ]";
            sep = ", ";
        }
        OStrJ = fopen((outName + ".json").c_str(), "w");
        fprintf(OStrJ, jsonPrefix, enumList.c_str());
        for (auto item: softwareNameList)
            jsonGenerate(OStrJ, item.first, item.second);
        fprintf(OStrJ, "\n    ]\n}\n");
        fclose(OStrJ);
        std::string commandLine(exename);
        int ind = commandLine.rfind("/");
        if (ind == -1)
            commandLine = "";
        else
            commandLine = commandLine.substr(0,ind+1);
        commandLine += "scripts/atomiccCppgen.py " + outName + ".json";
        int ret = system(commandLine.c_str());
        printf("[%s:%d] RETURN from '%s' %d\n", __FUNCTION__, __LINE__, commandLine.c_str(), ret);
        if (ret)
            return -1; // force error return to be propagated
        FILE *OStrFL = fopen((outName + ".filelist").c_str(), "w");
        std::string flist = "GENERATED_CPP = jni/GeneratedCppCallbacks.cpp \\\n   ";
        for (auto item: softwareNameList)
             flist += " jni/" + item.first + ".c";
        fprintf(OStrFL, "%s\n", flist.c_str());
        fclose(OStrFL);
    }
    return 0;
}
