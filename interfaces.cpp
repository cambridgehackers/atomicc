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
#define PRINTF_PORT 0x7fff

#define MAX_METHOD_SERIALIZE_LENGTH 128

static int printfNumber = 1;
int trace_software;//= 1;
int generateTrace;//=1;
static void processSerialize(ModuleIR *IR)
{
    MapNameValue mapValue;
    extractParam("SERIAL", IR->name, mapValue);
    std::string prefix = "__" + IR->name + "_";
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    IR->fields.clear();
    IR->fields.push_back(FieldElement{"len", "", "Bit(16)", false, false, false, false, ""/*not param*/, false, false, false});
    IR->fields.push_back(FieldElement{"tag", "", "Bit(16)", false, false, false, false, ""/*not param*/, false, false, false});
    ModuleIR *unionIR = allocIR(prefix + "UNION");
    IR->fields.push_back(FieldElement{"data", "", unionIR->name, false, false, false, false, ""/*not param*/, false, false, false});
    uint64_t maxDataLength = 0;
    for (auto MI: implements->methods) {
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
    }
    unionIR->fields.push_back(FieldElement{"data", "", "Bit(" + autostr(maxDataLength) + ")", false, false, false, false, ""/*not param*/, false, false, false});
}

static ACCExpr *makeIndication(std::string target, int counter, ACCExpr *sourceParam, int64_t dataLength)
{
    dataLength += 32;     // include length of tag
    if (dataLength > MAX_METHOD_SERIALIZE_LENGTH) {
        printf("%s: max method serialize length exceeded %s\n", __FUNCTION__, tree2str(sourceParam).c_str());
        exit(-1);
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
    sourceParam->operands.push_back(allocExpr("16'd" + autostr(vecLength * 32)));
    return allocExpr(target, allocExpr(PARAMETER_MARKER,
            allocExpr("{", sourceParam)));
}

static ACCExpr *allocBitsubstr(std::string name, std::string left, std::string right)
{
     return allocExpr("__bitsubstr", allocExpr(PARAMETER_MARKER, allocExpr(name), allocExpr(left), allocExpr(right)));
}

static void processM2P(ModuleIR *IR)
{
    MapNameValue mapValue;
    extractParam("M2P", IR->name, mapValue);
    ModuleIR *methodInterface = nullptr;
    std::string host = "method", target = "pipe.enq";
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    for (auto inter: implements->interfaces) {
        if (!inter.isPtr) {
            methodInterface = lookupInterface(inter.type);
            IR->name = "___M2P" + inter.type;
        }
    }
    int counter = 0;  // start method number at 0
    assert(methodInterface);
    for (auto MI: methodInterface->methods) {
        std::string methodName = host + PERIOD + MI->name;
        MethodInfo *MInew = allocMethod(methodName);
        addMethod(IR, MInew);
        MInew->type = MI->type;
        MInew->guard = MI->guard;
        if (isRdyName(methodName))
            continue;
        int64_t dataLength = 0;
        ACCExpr *sourceParam = allocExpr(",");
        std::string paramPrefix = baseMethodName(methodName) + DOLLAR;
        for (auto param: MI->params) {
            MInew->params.push_back(param);
            dataLength += atoi(convertType(instantiateType(param.type, mapValue)).c_str());
            sourceParam->operands.push_back(allocExpr(paramPrefix + param.name));
        }
        ACCExpr *expr = makeIndication(target, counter, sourceParam, dataLength);
        MInew->callList.push_back(new CallListElement{expr, nullptr, true, false});
        if (generateTrace) {
            ACCExpr *callExpr = allocExpr("printf", allocExpr(PARAMETER_MARKER,
                allocExpr("\"DISPLAYM2P %x\""), allocExpr("{", sourceParam)));
            MInew->printfList.push_back(new CallListElement{callExpr, nullptr, false, false});
        }
        counter++;
    }
    if (trace_software)
        dumpModule("M2P", IR);
}

static void processP2M(ModuleIR *IR)
{
    MapNameValue mapValue;
    extractParam("P2M", IR->name, mapValue);
    ModuleIR *methodInterface = nullptr, *pipeInterface = nullptr;
    std::string host = "pipe", target = "method";
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    for (auto inter: implements->interfaces) {
        if (inter.fldName == "method") {
            methodInterface = lookupInterface(inter.type);
            IR->name = "___P2M" + inter.type;
        }
        else if (inter.fldName == "pipe") {
            pipeInterface = lookupInterface(inter.type);
        }
    }
    assert(pipeInterface);
    for (auto MI: pipeInterface->methods) {
        MethodInfo *MInew = allocMethod(host + PERIOD + MI->name);
        addMethod(IR, MInew);
        for (auto param: MI->params)
            MInew->params.push_back(param);
        MInew->type = instantiateType(MI->type, mapValue);
        MInew->guard = MI->guard;
    }
    MethodInfo *MInew = IR->methods.front(); // "enq" method
    std::string sourceParam = baseMethodName(MInew->name) + DOLLAR + MInew->params.front().name;
    if (generateTrace) {
        ACCExpr *callExpr = allocExpr("printf", allocExpr(PARAMETER_MARKER,
            allocExpr("\"DISPLAYP2M %x\""), allocExpr(sourceParam)));
        MInew->printfList.push_back(new CallListElement{callExpr, nullptr, false, false});
    }
    int counter = 0;  // start method number at 0
    std::string paramLen = convertType("NOCDataH");
    for (auto MI: methodInterface->methods) {
        std::string methodName = MI->name;
        if (isRdyName(methodName))
            continue;
        uint64_t totalLength = 0;
        for (auto param: MI->params)
            totalLength += atoi(convertType(instantiateType(param.type, mapValue)).c_str());
        std::string offset = paramLen + " - 32"; // length
        uint64_t vecLength = (totalLength + 31) / 32;
        if ((totalLength = vecLength * 32 - totalLength))
            offset += "-" + autostr(totalLength);
        ACCExpr *paramList = allocExpr(",");
        for (auto param: MI->params) {
            std::string lower = "(" + offset + " - " + convertType(instantiateType(param.type, mapValue)) + ")";
            paramList->operands.push_back(allocBitsubstr(sourceParam, offset + "-1", lower));
            offset = lower;
        }
        ACCExpr *cond = allocExpr("==", allocBitsubstr(sourceParam, paramLen + " - 1", paramLen + "- 16"), // length
                 allocExpr("16'd" + autostr(counter)));
        ACCExpr *callExpr = allocExpr(target + PERIOD + methodName, allocExpr(PARAMETER_MARKER, paramList));
        if (isEnaName(methodName))
            MInew->callList.push_back(new CallListElement{callExpr, cond, true, false});
        else {
            if (MI->action) {
                // when calling 'actionValue', guarded call needed
                // need to call xxx__ENA function here!
                //MInew->callList.push_back(new CallListElement{call, cond, true, false});
            }
            // when calling 'value' or 'actionValue' method, enqueue return value
            int64_t dataLength = 16/*bitlength*/ + + atoi(convertType(instantiateType(MI->type, mapValue)).c_str());
            ACCExpr *sourceParam = allocExpr(",");
            sourceParam->operands.push_back(allocExpr(callExpr->value));
            sourceParam->operands.push_back(allocExpr("16'd" + autostr(10/* bitlength*/)));
            ACCExpr *expr = makeIndication("returnInd$enq__ENA", counter, sourceParam, dataLength);
            MInew->callList.push_back(new CallListElement{expr, cond, true, false});
        }
        if (totalLength > MAX_METHOD_SERIALIZE_LENGTH) {
            printf("%s: max method serialize length exceeded %s/%s\n", __FUNCTION__, IR->name.c_str(), methodName.c_str());
            exit(-1);
        }
        counter++;
    }
    if (trace_software)
        dumpModule("P2M", IR);
}

ACCExpr *printfArgs(ACCExpr *listp)
{
    std::string formatString = listp->operands.front()->value;
    listp->operands.pop_front();
    std::list<int> widthList;
    unsigned index = 0;
    int64_t dataLength = 16; // length of 'printfNumber'
    ACCExpr *sourceParam = allocExpr(",");
    for (auto item: listp->operands) {
        while (formatString[index] != '%' && index < formatString.length())
            index++;
        std::string val = item->value;
        if (index < formatString.length()-1) {
            if (formatString[index + 1] == 's' && val[0] == '"') {
                val = val.substr(1, val.length()-2);
                formatString = formatString.substr(0, index) + val + formatString.substr(index + 2);
                index += val.length();
                continue;
            }
            if (formatString[index + 1] == 'd' && isdigit(val[0])) {
                formatString = formatString.substr(0, index) + val + formatString.substr(index + 2);
                index += val.length();
                continue;
            }
        }
        int size = atoi(exprWidth(item).c_str());
        dataLength += size;
        widthList.push_back(size);
        sourceParam->operands.push_back(item);
    }
    sourceParam->operands.push_back(allocExpr("16'd" + autostr(printfNumber++)));
    printfFormat.push_back(PrintfInfo{formatString, widthList});
    return makeIndication("printfp$enq__ENA", PRINTF_PORT, sourceParam, dataLength);
}

void processInterfaces(std::list<ModuleIR *> &irSeq)
{
    for (auto mapp: mapIndex) {
        ModuleIR *IR = mapp.second;
        assert(IR);
        if (IR->isSerialize)
            processSerialize(IR);
        if (startswith(IR->name, "___P2M")) {
            irSeq.push_back(IR);
            processP2M(IR);
        }
        if (startswith(IR->name, "___M2P")) {
            irSeq.push_back(IR);
            std::list<FieldElement> temp;
            for (auto inter: IR->interfaces) {
                if (inter.fldName != "unused")
                    temp.push_back(inter);
            }
            IR->interfaces = temp;
            processM2P(IR);
        }
    }
}
