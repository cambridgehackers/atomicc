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
        ModuleIR *IR = allocIR("l_top");
        irSeq.push_back(IR);
        std::string dutType;
        bool hasPrintf = false;
        for (auto item: softwareNameList) {
            dutType = item.second.IR->name;
            for (auto iitem: item.second.IR->interfaces)
                if (iitem.fldName == "printfp")
                    hasPrintf = true;
            std::string name = "IfcNames_" + item.first + (item.second.field.isPtr ? "H2S" : "S2H");
            enumList += sep + "[ \"" + name + "\", \"" + autostr(counter++) + "\" ]";
            sep = ", ";
        }
        OStrJ = fopen((outName + ".json").c_str(), "w");
        fprintf(OStrJ, jsonPrefix, enumList.c_str());
        std::string localName = "DUT__" + dutType;
        std::string muxName = "mux";
        std::string muxTypeName = "MuxPipe";
        std::string pipeName = "l_ainterface_OC_PipeIn";
        IR->fields.push_back(FieldElement{localName, -1, dutType, false, false, false, false, false, false});
        if (hasPrintf) {
#if 0
            ModuleIR *muxDef = lookupIR(muxTypeName);
printf("[%s:%d] HASHSHSHSHSPRINTF %p\n", __FUNCTION__, __LINE__, muxDef);
            if (muxDef)
                irSeq.push_back(muxDef); // HACK FOR NOW!!!!!!!!!!!!!!!!!!!!!!
            else {
                muxDef = allocIR(muxTypeName);
                irSeq.push_back(muxDef);
                muxDef->interfaces.push_back(FieldElement{"in", -1, pipeName, false, false, false, false, false, false});
                muxDef->interfaces.push_back(FieldElement{"forward", -1, pipeName, false, false, false, false, false, false});
                muxDef->interfaces.push_back(FieldElement{"out", -1, pipeName, true, false, false, false, false, false});
                auto makeEnq = [&](std::string methodName) -> void {
                    MethodInfo *MI = allocMethod(methodName);
                    addMethod(muxDef, MI);
                    MI->params.push_back(ParamElement{"v", "l_struct_OC_NOCData"});
                    std::string call;
                    MI->callList.push_back(new CallListElement{
                        allocExpr("out$enq__ENA", allocExpr(PARAMETER_MARKER, allocExpr(
                            methodName.substr(0, methodName.length()-5)
                               + MODULE_SEPARATOR + "v"))),
                        nullptr, true});
                    MethodInfo *MIRdy = allocMethod(getRdyName(methodName));
                    addMethod(muxDef, MIRdy);
                    MIRdy->type = "INTEGER_1";
                    MIRdy->guard = allocExpr("1");
                };
                makeEnq("forward$enq__ENA");
                makeEnq("in$enq__ENA");
dumpModule("MUX", muxDef);
            }
#endif
            IR->fields.push_back(FieldElement{muxName, -1, muxTypeName, false, false, false, false, false, false});
        }
        localName += MODULE_SEPARATOR;
        muxName += MODULE_SEPARATOR;
        for (auto item: softwareNameList) {
            jsonGenerate(OStrJ, item.first, item.second);
            bool outcall = item.second.field.isPtr;
            std::string userTypeName = item.second.inter->name;
            std::string userInterface = item.second.field.fldName;
            std::string pName = pipeName + (outcall ? "H" : "");
            std::string fieldName = (outcall ? "M2P" : "P2M") + ("__" + userInterface);
            ModuleIR *ifcIR = allocIR(fieldName);
            ifcIR->interfaces.push_back(FieldElement{"method", -1, userTypeName, !outcall, false, false, false, false, false});
            ifcIR->interfaces.push_back(FieldElement{"pipe", -1, pName, outcall, false, false, false, false, false});
            IR->fields.push_back(FieldElement{fieldName, -1, ifcIR->name, false, false, false, false, false, false});
            IR->interfaces.push_back(FieldElement{userInterface, -1, pName, outcall, false, false, false, false, false});
            IR->interfaceConnect.push_back(InterfaceConnectType{
                localName + userInterface,
                fieldName + MODULE_SEPARATOR + "method", userTypeName, true});
            if (outcall && hasPrintf) {
                IR->interfaceConnect.push_back(InterfaceConnectType{
                    muxName + "in", fieldName + MODULE_SEPARATOR + "pipe", pName, true});
                IR->interfaceConnect.push_back(InterfaceConnectType{
                    muxName + "forward", localName + "printfp", pName, true});
                IR->interfaceConnect.push_back(InterfaceConnectType{userInterface,
                    muxName + "out", pName, true});
            }
            else
                IR->interfaceConnect.push_back(InterfaceConnectType{userInterface,
                    fieldName + MODULE_SEPARATOR + "pipe", pName, true});
        }
        fprintf(OStrJ, "\n    ]\n}\n");
        fclose(OStrJ);
        //dumpModule("TOP", IR);
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
    }
    return 0;
}
