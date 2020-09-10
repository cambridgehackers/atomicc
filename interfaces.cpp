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
int generateTrace;//=1;
static void processSerialize(ModuleIR *IR)
{
    MapNameValue mapValue;
    extractParam("SERIAL", IR->name, mapValue);
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
            variant->fields.push_back(FieldElement{param.name, "", instantiateType(param.type, mapValue), false, false, false, false, ""/*not param*/, false, false, false});
            dataLength += atoi(convertType(instantiateType(param.type, mapValue)).c_str());
        }
        if (dataLength > maxDataLength)
            maxDataLength = dataLength;
        counter++;
    }
    unionIR->fields.push_back(FieldElement{"data", "", "Bit(" + autostr(maxDataLength) + ")", false, false, false, false, ""/*not param*/, false, false, false});
}

static void processM2P(ModuleIR *IR)
{
    MapNameValue mapValue;
    extractParam("M2P", IR->name, mapValue);
    ModuleIR *HIR = nullptr;
    std::string host, target, targetParam;
    std::string pipeArgSize = "-1";
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    //dumpModule("PROCESSM2P", IR);
    for (auto inter: implements->interfaces) {
        if (inter.isPtr) {
            auto IIR = lookupInterface(inter.type);
            assert(IIR);
            auto MI = IIR->methods.front();
            target = inter.fldName + PERIOD + MI->name;
            targetParam = baseMethodName(target) + DOLLAR + MI->params.front().name;
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
        std::string methodName = host + PERIOD + MI->name;
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
        std::string paramPrefix = baseMethodName(methodName) + DOLLAR;
        int64_t dataLength = 32; // include length of tag
#if 0
        std::string call;
        for (auto param: MI->params) {
            MInew->params.push_back(param);
            dataLength += atoi(convertType(instantiateType(param.type, mapValue)).c_str());
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
        if (generateTrace) {
            ACCExpr *callExpr = allocExpr("printf", allocExpr(PARAMETER_MARKER,
                allocExpr("\"DISPLAYM2P %x\""), allocExpr(sourceParam)));
            MInew->printfList.push_back(new CallListElement{callExpr, nullptr, false});
        }
#else
        ACCExpr *sourceParam = allocExpr(",");
        for (auto param: MI->params) {
            MInew->params.push_back(param);
            dataLength += atoi(convertType(instantiateType(param.type, mapValue)).c_str());
            sourceParam->operands.push_back(allocExpr(paramPrefix + param.name));
        }
        uint64_t vecLength = (dataLength + 31) / 32;
        dataLength = vecLength * 32 - dataLength;
        if (dataLength)
            sourceParam->operands.push_front(allocExpr(autostr(dataLength) + "'d0"));
        dataLength = 128 - vecLength * 32;
        if (dataLength > 0)
            sourceParam->operands.push_back(allocExpr(autostr(dataLength) + "'d0"));
        sourceParam->operands.push_front(allocExpr("16'd" + autostr(PORTALNUM)));
        sourceParam->operands.push_front(allocExpr("16'd" + autostr(counter)));
        sourceParam->operands.push_back(allocExpr("16'd" + autostr(vecLength)));
        MInew->callList.push_back(new CallListElement{
            allocExpr(target, allocExpr(PARAMETER_MARKER,
                allocExpr("{", sourceParam))), nullptr, true});
        if (generateTrace) {
            ACCExpr *callExpr = allocExpr("printf", allocExpr(PARAMETER_MARKER,
                allocExpr("\"DISPLAYM2P %x\""), allocExpr("{", sourceParam)));
            MInew->printfList.push_back(new CallListElement{callExpr, nullptr, false});
        }
#endif
        counter++;
    }
    if (trace_software)
    dumpModule("M2P", IR);
}

ACCExpr *allocBitsubstr(std::string name, std::string left, std::string right)
{
     ACCExpr *ret = allocExpr("__bitsubstr", allocExpr(PARAMETER_MARKER, allocExpr(name), allocExpr(left), allocExpr(right)));
     return ret;
}
static void processP2M(ModuleIR *IR)
{
    MapNameValue mapValue;
    extractParam("P2M", IR->name, mapValue);
    ModuleIR *IIR = nullptr, *HIR = nullptr;
    std::string host, target;
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    for (auto inter: implements->interfaces) {
        if (inter.fldName == "method") {
            IIR = lookupInterface(inter.type);
            target = inter.fldName;
            IR->name = "___P2M" + inter.type;
    if (trace_software)
dumpModule("P2M/IIR :" + target, IIR);
        }
        else if (inter.fldName == "pipe") {
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
        MethodInfo *MInew = allocMethod(host + PERIOD + methodName);
        addMethod(IR, MInew);
    if (trace_software)
printf("[%s:%d] create '%s'\n", __FUNCTION__, __LINE__, MInew->name.c_str());
        for (auto param: MI->params)
            MInew->params.push_back(param);
        MInew->type = instantiateType(MI->type, mapValue);
        MInew->guard = MI->guard;
    }
    MethodInfo *MInew = lookupMethod(IR, host + PERIOD + "enq");
    std::string sourceParam = baseMethodName(MInew->name) + DOLLAR + MInew->params.front().name;
    //extract from enq param std::string paramLen = convertType(instantiateType(MInew->params.front().type, mapValue));
    std::string paramLen = "(16 + 128)"; // TODO: hack replacement for __bitsize(NOCDataH)
    if (generateTrace) {
        ACCExpr *callExpr = allocExpr("printf", allocExpr(PARAMETER_MARKER,
            allocExpr("\"DISPLAYP2M %x\""), allocExpr(sourceParam)));
        MInew->printfList.push_back(new CallListElement{callExpr, nullptr, false});
    }
    if (trace_software)
printf("[%s:%d] lookup '%s' -> %p\n", __FUNCTION__, __LINE__, (host + PERIOD + "enq__ENA").c_str(), (void *)MInew);
assert(MInew);
    int counter = 0;  // start method number at 0
    for (auto MI: IIR->methods) {
        std::string offset = paramLen + " - 32"; // length
        std::string methodName = MI->name;
        std::string paramPrefix = baseMethodName(methodName) + DOLLAR;
        if (isRdyName(methodName))
            continue;
        uint64_t totalLength = 0;
        for (auto param: MI->params)
            totalLength += atoi(convertType(instantiateType(param.type, mapValue)).c_str());
        uint64_t vecLength = (totalLength + 31) / 32;
        totalLength = vecLength * 32 - totalLength;
        if (totalLength)
            offset += "-" + autostr(totalLength);
        ACCExpr *paramList = allocExpr(",");
        ACCExpr *callExpr = allocExpr(target + PERIOD + methodName, allocExpr(PARAMETER_MARKER, paramList));
        for (auto param: MI->params) {
            std::string lower = "(" + offset + " - " + convertType(instantiateType(param.type, mapValue)) + ")";
            paramList->operands.push_back(allocBitsubstr(sourceParam, offset + "-1", lower));
            offset = lower;
        }
        ACCExpr *cond = allocExpr("==", allocBitsubstr(sourceParam, paramLen + " - 1", paramLen + "- 16"), // length
                 allocExpr("16'd" + autostr(counter)));
        if (!isEnaName(methodName)) {
            if (MI->action) {
                // when calling 'actionValue', guarded call needed
                // need to call xxx__ENA function here!
                //MInew->callList.push_back(new CallListElement{call, cond, true});
            }
            // when calling 'value' or 'actionValue' method, enqueue return value
        int64_t dataLength = 32 + 16; // include length of tag and bitlength
        std::string target = "returnInd$enq__ENA";

            dataLength += atoi(convertType(instantiateType(MI->type, mapValue)).c_str());
#if 0
            std::string call = ", " + callExpr->value;
        call += ", 16'd" + autostr(10/* bitlength*/);
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
                allocExpr(sourceParam))), cond, true});
#else
        ACCExpr *sourceParam = allocExpr(",");
        sourceParam->operands.push_back(allocExpr(callExpr->value));
        sourceParam->operands.push_back(allocExpr("16'd" + autostr(10/* bitlength*/)));
        uint64_t vecLength = (dataLength + 31) / 32;
        dataLength = vecLength * 32 - dataLength;
        if (dataLength)
            sourceParam->operands.push_front(allocExpr(autostr(dataLength) + "'d0"));
        dataLength = 128 - vecLength * 32;
        if (dataLength > 0) {
            sourceParam->operands.push_back(allocExpr(autostr(dataLength) + "'d0"));
        }
        sourceParam->operands.push_front(allocExpr("16'd" + autostr(PORTALNUM)));
        sourceParam->operands.push_front(allocExpr("16'd" + autostr(counter)));
        sourceParam->operands.push_back(allocExpr("16'd" + autostr(vecLength)));
        MInew->callList.push_back(new CallListElement{
            allocExpr(target, allocExpr(PARAMETER_MARKER,
                allocExpr("{", sourceParam))), cond, true});
#endif
        }
        else
        MInew->callList.push_back(new CallListElement{callExpr, cond, true});
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
