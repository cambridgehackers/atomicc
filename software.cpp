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
        if (MI->type != "") {
            char buffer[1000];
            sprintf(buffer, ", \"rtype\": { \"name\": \"Bit\", \"params\": [ { "
                 "\"name\": \"%s\" } ] }", convertType(MI->type).c_str());
            retType = buffer;
        }
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
    std::string pipeName = "PipeIn(width=" + convertType("NOCDataH") + ")";
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
        ModuleIR *IRifc = allocIR("l_top___IFC", true);
        ModuleIR *IR = allocIR("l_top");
        IR->interfaceName = IRifc->name;
        irSeq.push_back(IR);
        std::list<std::string> pipeUser;
        OStrJ = fopen((outName + ".json").c_str(), "w");
        std::map<std::string, bool> dutPresent;
        for (auto item: softwareNameList) {
            assert(lookupInterface(item.second.IR->interfaceName));
            std::string name = "IfcNames_" + item.first + (item.second.field.isPtr ? "H2S" : "S2H");
            enumList += sep + "[ \"" + name + "\", \"" + autostr(counter++) + "\" ]";
            sep = ", ";
        }
        fprintf(OStrJ, jsonPrefix, enumList.c_str());
        for (auto item: softwareNameList) {
            jsonGenerate(OStrJ, item.first, item.second);
            std::string dutType = item.second.IR->name;
            std::string localName = "DUT__" + dutType;
            bool outcall = item.second.field.isPtr;
            std::string userTypeName = item.second.inter;
            std::string userInterface = item.second.field.fldName;
            std::string fieldName = (outcall ? "M2P" : "P2M") + ("__" + userInterface);
            ModuleIR *elementinterface = allocIR(fieldName + "___IFC", true);
            ModuleIR *element = allocIR((outcall ? "___M2P" : "___P2M") + userTypeName);
            element->interfaceName = elementinterface->name;
printf("[%s:%d] outcall %d usertname %s fieldname %s type %s\n", __FUNCTION__, __LINE__, outcall, userTypeName.c_str(), fieldName.c_str(), element->name.c_str());
            elementinterface->interfaces.push_back(FieldElement{"method", "", userTypeName, "CLK", !outcall, false, false, false, ""/*not param*/, false, false, false});
            elementinterface->interfaces.push_back(FieldElement{"pipe", "", pipeName, "CLK", outcall, false, false, false, ""/*not param*/, false, false, false});
            IR->fields.push_back(FieldElement{fieldName, "", element->name, "CLK", false, false, false, false, ""/*not param*/, false, false, false});
            IR->interfaceConnect.push_back(InterfaceConnectType{
                allocExpr(localName + DOLLAR + userInterface),
                allocExpr(fieldName + DOLLAR + "method"), userTypeName, true});
            if (!dutPresent[dutType]) {
                dutPresent[dutType] = true;
                IR->fields.push_back(FieldElement{localName, "", dutType, "CLK", false, false, false, false, ""/*not param*/, false, false, false});
                IRifc->interfaces.push_back(FieldElement{"request", "", pipeName, "CLK", false/*in*/, false, false, false, ""/*not param*/, false, false, false});
            }
            if (outcall)
                pipeUser.push_back(fieldName + DOLLAR + "pipe");
            else {
                if (auto userIR = lookupInterface(userTypeName)) {
                    for (auto MI: userIR->methods) {
                        std::string methodName = MI->name;
                        if (!isRdyName(methodName) && !isEnaName(methodName)) {
                            // see if the request deserialization can generate an indication
                            // actionValue methods get a callback for value
                            elementinterface->interfaces.push_back(FieldElement{"returnInd", "", pipeName, "CLK", true, false, false, false, ""/*not param*/, false, false, false});
                            pipeUser.push_back(fieldName + DOLLAR + "returnInd");
                            break;
                        }
                    }
                }
                IR->interfaceConnect.push_back(InterfaceConnectType{
                    allocExpr(fieldName + DOLLAR + "pipe"),
                    allocExpr("request"), pipeName, true});
            }
        }
        fprintf(OStrJ, "\n    ]\n}\n");
        fclose(OStrJ);
        if (int size = pipeUser.size()) {
            IRifc->interfaces.push_back(FieldElement{"indication", "", pipeName, "CLK", true/*out*/, false, false, false, ""/*not param*/, false, false, false});
            if (size == 1)
                IR->interfaceConnect.push_back(InterfaceConnectType{allocExpr("indication"),
                    allocExpr(pipeUser.front()), pipeName, true});
            else {
                IR->fields.push_back(FieldElement{"funnel", "", "FunnelBufferedBase(funnelWidth="
                   + autostr(pipeUser.size()) + ",dataWidth=" + convertType("NOCDataH") + ")", "CLK", false, false, false, false, ""/*not param*/, false, false, false});
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
