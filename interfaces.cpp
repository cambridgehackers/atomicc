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

//////////////////HACKHACK /////////////////
#define IfcNames_EchoIndicationH2S 5
#define PORTALNUM IfcNames_EchoIndicationH2S
//////////////////HACKHACK /////////////////
int trace_software;//= 1;
static void processSerialize(ModuleIR *IR)
{
    std::string prefix = "__" + IR->name + "_";
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    auto inter = implements->interfaces.front();
    if (trace_software)
printf("[%s:%d] serialize %s\n", __FUNCTION__, __LINE__, inter.type.c_str());
    ModuleIR *IIR = lookupInterface(inter.type);
    IR->fields.clear();
    IR->fields.push_back(FieldElement{"len", "", "Bit(16)", false, false, false, false, ""/*not param*/, false, false, false});
    IR->fields.push_back(FieldElement{"tag", "", "Bit(16)", false, false, false, false, ""/*not param*/, false, false, false});
    ModuleIR *unionIR = allocIR(prefix + "UNION");
    IR->fields.push_back(FieldElement{"data", "", unionIR->name, false, false, false, false, ""/*not param*/, false, false, false});
    int counter = 0;  // start method number at 0
    uint64_t maxDataLength = 0;
    for (auto MI: IIR->methods) {
        std::string methodName = MI->name;
        if (isRdyName(methodName))
            continue;
        methodName = baseMethodName(methodName);
        ModuleIR *variant = allocIR(prefix + "VARIANT_" + methodName);
        unionIR->unionList.push_back(UnionItem{methodName, variant->name});
        uint64_t dataLength = 0;
        for (auto param: MI->params) {
            variant->fields.push_back(FieldElement{param.name, "", param.type, false, false, false, false, ""/*not param*/, false, false, false});
            dataLength += atoi(convertType(param.type).c_str());
        }
        if (dataLength > maxDataLength)
            maxDataLength = dataLength;
        counter++;
    }
    unionIR->fields.push_back(FieldElement{"data", "", "Bit(" + autostr(maxDataLength) + ")", false, false, false, false, ""/*not param*/, false, false, false});
}

static void processM2P(ModuleIR *IR)
{
    ModuleIR *HIR = nullptr;
    std::string host, target, targetParam;
    std::string pipeArgSize = "-1";
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    //dumpModule("PROCESSM2P", IR);
    for (auto inter: implements->interfaces) {
        if (inter.isPtr) {
            auto IIR = lookupInterface(inter.type);
            auto MI = IIR->methods.front();
            target = inter.fldName + MODULE_SEPARATOR + MI->name;
            targetParam = baseMethodName(target) + MODULE_SEPARATOR + MI->params.front().name;
            if (trace_software)
                dumpModule("M2P/IIR :" + inter.fldName, IIR);
        }
        else {
            HIR = lookupInterface(inter.type);
            host = inter.fldName;
            std::string iname = inter.type;
            IR->name = "___M2P" + iname;
            std::string type = iname + "__Data";
            std::string interfaceName = type + "___IFC";
            ModuleIR *II = allocIR(type);
            II->interfaceName = interfaceName;
            ModuleIR *IIifc = allocIR(interfaceName, true);
            IIifc->interfaces.push_back(FieldElement{"ifc", "", inter.type, false, false, false, false, ""/*not param*/, false, false, false});
    if (trace_software)
printf("[%s:%d] IIIIIIIIIII iname %s IRNAME %s type %s\n", __FUNCTION__, __LINE__, iname.c_str(), IR->name.c_str(), type.c_str());
            processSerialize(II);
            pipeArgSize = convertType(type);
    if (trace_software)
dumpModule("M2P/HIR :" + host, HIR);
        }
    }
    int counter = 0;  // start method number at 0
assert(HIR);
    for (auto MI: HIR->methods) {
        std::string methodName = host + MODULE_SEPARATOR + MI->name;
        MethodInfo *MInew = allocMethod(methodName);
        addMethod(IR, MInew);
        MInew->type = MI->type;
        MInew->guard = MI->guard;
        if (isRdyName(methodName))
            continue;
        //if (!isEnaName(methodName)) {
//printf("[%s:%d] cannot serialize method %s\n", __FUNCTION__, __LINE__, methodName.c_str());
//exit(-1);
            //continue;
        //}
        std::string paramPrefix = baseMethodName(methodName) + MODULE_SEPARATOR;
        std::string call;
        int64_t dataLength = 32; // include length of tag
        for (auto param: MI->params) {
            MInew->params.push_back(param);
            dataLength += atoi(convertType(param.type).c_str());
            call += ", " + paramPrefix + param.name;
        }
        uint64_t vecLength = (dataLength + 31) / 32;
        dataLength = vecLength * 32 - dataLength;
        if (dataLength)
            call = ", " + autostr(dataLength) + "'d0" + call;
        dataLength = 128 - vecLength * 32;
        if (dataLength > 0)
            call += ", " + autostr(dataLength) + "'d0";
        std::string sourceParam = "{ 16'd" + autostr(counter) + ", 16'd"
            + autostr(PORTALNUM)+ call + ", 16'd" + autostr(vecLength) + "}";
        MInew->callList.push_back(new CallListElement{
             allocExpr(target, allocExpr(PARAMETER_MARKER,
                 allocExpr(sourceParam))), nullptr, true});
        if (1) {
            ACCExpr *callExpr = allocExpr("printf", allocExpr(PARAMETER_MARKER,
                allocExpr("\"DISPLAYM2P %x\""), allocExpr(sourceParam)));
            MInew->printfList.push_back(new CallListElement{callExpr, nullptr, false});
        }
        counter++;
    }
    if (trace_software)
    dumpModule("M2P", IR);
}

static void processP2M(ModuleIR *IR)
{
    ModuleIR *IIR = nullptr, *HIR = nullptr;
    std::string host, target;
    bool addedReturnInd = false;
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    for (auto inter: implements->interfaces) {
        if (inter.isPtr) {
            IIR = lookupInterface(inter.type);
            target = inter.fldName;
            std::string iname = inter.type;
            IR->name = "___P2M" + iname;
            std::string type = "P2M_MD_" + iname + "_OD__KD__KD_Data";
    if (trace_software)
dumpModule("P2M/IIR :" + target, IIR);
        }
        else {
            HIR = lookupInterface(inter.type);
    if (trace_software)
printf("[%s:%d] HIR type %s IR %p\n", __FUNCTION__, __LINE__, inter.type.c_str(), (void *)HIR);
            host = inter.fldName;
    if (trace_software)
dumpModule("P2M/HIR :" + host, HIR);
        }
    }
assert(HIR);
    for (auto MI: HIR->methods) {
        std::string methodName = MI->name;
    if (trace_software)
printf("[%s:%d] P2Mhifmethod %s\n", __FUNCTION__, __LINE__, methodName.c_str());
        MethodInfo *MInew = allocMethod(host + MODULE_SEPARATOR + methodName);
        addMethod(IR, MInew);
    if (trace_software)
printf("[%s:%d] create '%s'\n", __FUNCTION__, __LINE__, MInew->name.c_str());
        for (auto param: MI->params)
            MInew->params.push_back(param);
        MInew->type = MI->type;
        MInew->guard = MI->guard;
    }
    MethodInfo *MInew = lookupMethod(IR, host + MODULE_SEPARATOR + "enq");
    std::string sourceParam = baseMethodName(MInew->name) + MODULE_SEPARATOR + MInew->params.front().name;
    std::string paramLen = convertType(MInew->params.front().type);
    if (1) {
        ACCExpr *callExpr = allocExpr("printf", allocExpr(PARAMETER_MARKER,
            allocExpr("\"DISPLAYP2M %x\""), allocExpr(sourceParam)));
        MInew->printfList.push_back(new CallListElement{callExpr, nullptr, false});
    }
    if (trace_software)
printf("[%s:%d] lookup '%s' -> %p\n", __FUNCTION__, __LINE__, (host + MODULE_SEPARATOR + "enq__ENA").c_str(), (void *)MInew);
assert(MInew);
    int counter = 0;  // start method number at 0
    for (auto MI: IIR->methods) {
        std::string offset = paramLen + " - 32"; // length
        std::string methodName = MI->name;
        std::string paramPrefix = baseMethodName(methodName) + MODULE_SEPARATOR;
        if (isRdyName(methodName))
            continue;
        uint64_t totalLength = 0;
        for (auto param: MI->params)
            totalLength += atoi(convertType(param.type).c_str());
        uint64_t vecLength = (totalLength + 31) / 32;
        totalLength = vecLength * 32 - totalLength;
        if (totalLength)
            offset += "-" + autostr(totalLength);
        ACCExpr *paramList = allocExpr(",");
        ACCExpr *call = allocExpr(target + MODULE_SEPARATOR + methodName, allocExpr(PARAMETER_MARKER, paramList));
        for (auto param: MI->params) {
            std::string lower = "(" + offset + " - " + convertType(param.type) + ")";
            paramList->operands.push_back(allocExpr(sourceParam + "[" + offset + " -1 :" + lower + "]"));
            offset = lower;
        }
        if (!isEnaName(methodName)) {
printf("[%s:%d] cannot serialize method %s\n", __FUNCTION__, __LINE__, methodName.c_str());
//exit(-1);
            //continue;
        }
        ACCExpr *cond = allocExpr("==", allocExpr(sourceParam + "[" + paramLen + " - 1: (" + paramLen + "- 16)]"), // length
                 allocExpr("16'd" + autostr(counter)));
        if (!isEnaName(methodName)) {
            if (!addedReturnInd)
                IR->interfaces.push_back(FieldElement{"returnInd", "", "PipeIn", true, false, false, false, ""/*not param*/, false, false, false});
            addedReturnInd = true;
            if (MI->action) {
                // when calling 'actionValue', guarded call needed
                // need to call xxx__ENA function here!
                //MInew->callList.push_back(new CallListElement{call, cond, true});
            }
            // when calling 'value' or 'actionValue' method, enqueue return value
            uint64_t dataLength = 48 + 16/*length*/; // include length of tag
            uint64_t thisLen = atoi(convertType(MI->type).c_str());
            dataLength += thisLen;
            call = allocExpr("returnInd$enq__ENA", allocExpr(PARAMETER_MARKER,
                allocExpr("{ " + call->value
                               + ", 16'd" + autostr(thisLen) // bit len used
                               + ", 16'd" + autostr(counter) // which method had return
                               + ", 16'd" + autostr(PORTALNUM)
                               + ", 16'd" + autostr(dataLength/32) + "/* length */"
                               + "}")));
                
        }
        MInew->callList.push_back(new CallListElement{call, cond, true});
        counter++;
    }
    if (trace_software)
    dumpModule("P2M", IR);
}

void processInterfaces(std::list<ModuleIR *> &irSeq)
{
    for (auto mapp: mapIndex) {
        ModuleIR *IR = mapp.second;
        if (!IR) {
printf("[%s:%d] module pointer missing %s\n", __FUNCTION__, __LINE__, mapp.first.c_str());
        }
        if (IR->isSerialize)
            processSerialize(IR);
        if (startswith(IR->name, "___P2M")) {
    if (trace_software)
dumpModule("P2Morig", IR);
            irSeq.push_back(IR);
            processP2M(IR);
        }
        if (startswith(IR->name, "___M2P")) {
            irSeq.push_back(IR);
            std::list<FieldElement> temp;
    if (trace_software)
dumpModule("M2Porig", IR);
            for (auto inter: IR->interfaces) {
                if (inter.fldName != "unused")
                    temp.push_back(inter);
            }
            IR->interfaces = temp;
    if (trace_software)
dumpModule("M2Paft", IR);
            processM2P(IR);
        }
    }
}
