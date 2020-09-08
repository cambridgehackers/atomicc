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

#define NOCDataH_SIZE    "(16+128)"

typedef struct {
    FieldElement field;
    ModuleIR *IR; // containing Module
    std::string inter; // interface
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
    std::string msep;
    ModuleIR *interface = lookupInterface(swInfo.inter);
    for (auto MI : interface->methods) {
        std::string methodName = MI->name;
        if (isRdyName(methodName))
            continue;
        methodName = baseMethodName(methodName);
        std::string psep, retType;
        fprintf(OStrJ, "%s\n                { \"dname\": \"%s\", \"dparams\": [",
             msep.c_str(), methodName.c_str());
        for (auto pitem: MI->params) {
            fprintf(OStrJ, "%s\n                        { \"pname\": \"%s\", "
                "\"ptype\": { \"name\": \"Bit\", \"params\": [ { "
                "\"name\": \"%s\" } ] } }",
                psep.c_str(), pitem.name.c_str(), convertType(pitem.type).c_str());
            psep = ",";
        }
#if 1
        if (MI->type != "") {
            char buffer[1000];
            sprintf(buffer, ", \"rtype\": { \"name\": \"Bit\", \"params\": [ { "
                 "\"name\": \"%s\" } ] }", convertType(MI->type).c_str());
            retType = buffer;
        }
#endif
        fprintf(OStrJ, "\n                    ]%s }", retType.c_str());
        msep = ",";
    }
    fprintf(OStrJ, "\n            ], \"direction\": \"%d\", \"cname\": \"%s\" }",
        swInfo.field.isPtr, aname.c_str());
    jsonSep = ",";
}

int generateSoftware(std::list<ModuleIR *> &irSeq, const char *exename, std::string outName)
{
    FILE *OStrJ = nullptr;
    std::string pipeName = "PipeIn(" NOCDataH_SIZE ")";
    for (auto IR: irSeq) {
        ModuleIR *implements = lookupInterface(IR->interfaceName);
        if (!implements) {
printf("[%s:%d] interface defintion error: %s\n", __FUNCTION__, __LINE__, IR->interfaceName.c_str());
dumpModule("SOFT", IR);
exit(-1);
        }
        for (auto interfaceName: implements->softwareName) {
            for (auto iitem: implements->interfaces) {
                if (iitem.fldName == interfaceName) {
                    softwareNameList[CBEMangle(iitem.type)] = SoftwareItem{iitem, IR, iitem.type};
                }
            }
        }
    }
    if (softwareNameList.size() > 0) {
        int counter = 5;
        std::string enumList, sep;
        ModuleIR *IR = allocIR("l_top");
        IR->interfaceName = "l_top___IFC";
        ModuleIR *IRifc = allocIR(IR->interfaceName, true);
        IR->isInterface = false;
        irSeq.push_back(IR);
        std::string dutType;
        for (auto item: softwareNameList) {
            ModuleIR *implements = lookupInterface(item.second.IR->interfaceName);
            assert(implements);
            dutType = item.second.IR->name;
            std::string name = "IfcNames_" + item.first + (item.second.field.isPtr ? "H2S" : "S2H");
            enumList += sep + "[ \"" + name + "\", \"" + autostr(counter++) + "\" ]";
            sep = ", ";
        }
        OStrJ = fopen((outName + ".json").c_str(), "w");
        fprintf(OStrJ, jsonPrefix, enumList.c_str());
        std::string localName = "DUT__" + dutType;
        std::string muxName = "mux";
        std::string muxTypeName = "MuxPipe";
        IR->fields.push_back(FieldElement{localName, "", dutType, false, false, false, false, ""/*not param*/, false, false, false});
        localName += DOLLAR;
        muxName += DOLLAR;
        std::string fieldName;
        IRifc->interfaces.push_back(FieldElement{"request", "", pipeName, false/*in*/, false, false, false, ""/*not param*/, false, false, false});
        std::list<std::string> pipeUser;
        for (auto item: softwareNameList) {
            jsonGenerate(OStrJ, item.first, item.second);
            bool outcall = item.second.field.isPtr;
            std::string userTypeName = item.second.inter;
            std::string userInterface = item.second.field.fldName;
            fieldName = (outcall ? "M2P" : "P2M") + ("__" + userInterface);
            std::string type = (outcall ? "___M2P" : "___P2M") + userTypeName;
            std::string interfaceName = fieldName + "___IFC";
            ModuleIR *ifcIR = allocIR(type);
            ifcIR->interfaceName = interfaceName;
            ModuleIR *ifcIRinterface = allocIR(interfaceName, true);
            ifcIRinterface->interfaces.push_back(FieldElement{"method", "", userTypeName, !outcall, false, false, false, ""/*not param*/, false, false, false});
            ifcIRinterface->interfaces.push_back(FieldElement{"pipe", "", pipeName, outcall, false, false, false, ""/*not param*/, false, false, false});
            IR->fields.push_back(FieldElement{fieldName, "", type, false, false, false, false, ""/*not param*/, false, false, false});
            IR->interfaceConnect.push_back(InterfaceConnectType{
                allocExpr(localName + userInterface),
                allocExpr(fieldName + DOLLAR + "method"), userTypeName, true});
            if (!outcall) // see if the request deserialization can generate an indication
            if (auto userIR = lookupInterface(userTypeName)) {
                for (auto MI: userIR->methods) {
                    std::string methodName = MI->name;
                    if (!isRdyName(methodName) && !isEnaName(methodName)) {
                        // actionValue methods get a callback for value
                        ifcIRinterface->interfaces.push_back(FieldElement{"returnInd", "", pipeName, true, false, false, false, ""/*not param*/, false, false, false});
                        pipeUser.push_back(fieldName + DOLLAR + "returnInd");
                        break;
                    }
                }
            }
printf("[%s:%d] outcall %d usertname %s userif %s fieldname %s type %s\n", __FUNCTION__, __LINE__, outcall, userTypeName.c_str(), userInterface.c_str(), fieldName.c_str(), type.c_str());
            if (outcall) {
                pipeUser.push_back(fieldName + DOLLAR + "pipe");
            }
            else
                IR->interfaceConnect.push_back(InterfaceConnectType{
                    allocExpr(fieldName + DOLLAR + "pipe"),
                    allocExpr("request"), pipeName, true});
            //dumpModule("SWIFC", ifcIR);
        }
        if (int size = pipeUser.size()) {
        IRifc->interfaces.push_back(FieldElement{"indication", "", pipeName, true/*out*/, false, false, false, ""/*not param*/, false, false, false});
        if (size == 1) {
            IR->interfaceConnect.push_back(InterfaceConnectType{allocExpr("indication"),
                allocExpr(pipeUser.front()), pipeName, true});
        }
        else {
        IR->fields.push_back(FieldElement{"funnel", "", "FunnelBufferedBase(funnelWidth="
           + autostr(pipeUser.size()) + ",dataWidth=" + convertType("NOCDataH") + ")", false, false, false, false, ""/*not param*/, false, false, false});
        int ind = 0;
        for (auto inter: pipeUser) {
            IR->interfaceConnect.push_back(InterfaceConnectType{allocExpr(inter),
                str2tree("funnel$in[" + autostr(ind) + "]"), pipeName, true});
            ind++;
        }
        IR->interfaceConnect.push_back(InterfaceConnectType{allocExpr("indication"),
            allocExpr("funnel$out"), pipeName, true});
        }
        }
#if 0
        if (!hasIndication) {
            IRifc->interfaces.push_back(FieldElement{"indication", "", pipeName,
                true/*outcall*/, false, false, false, ""/*not param*/, false, false, false});
            IR->interfaceConnect.push_back(InterfaceConnectType{allocExpr("indication"),
                allocExpr(fieldName + DOLLAR + "returnInd"), pipeName, true});
        }
#endif
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
