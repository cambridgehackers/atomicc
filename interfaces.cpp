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
    auto inter = IR->interfaces.front();
    if (trace_software)
printf("[%s:%d] serialize %s\n", __FUNCTION__, __LINE__, inter.type.c_str());
    ModuleIR *IIR = lookupInterface(inter.type);
    IR->fields.clear();
    IR->fields.push_back(FieldElement{"len", "", "Bit(16)", false, false, false, false, ""/*not param*/, false, false});
    IR->fields.push_back(FieldElement{"tag", "", "Bit(16)", false, false, false, false, ""/*not param*/, false, false});
    ModuleIR *unionIR = allocIR(prefix + "UNION");
    unionIR->isInterface = false;
    IR->fields.push_back(FieldElement{"data", "", unionIR->name, false, false, false, false, ""/*not param*/, false, false});
    int counter = 0;  // start method number at 0
    uint64_t maxDataLength = 0;
    for (auto MI: IIR->methods) {
        std::string methodName = MI->name;
        if (endswith(methodName, "__RDY"))
            continue;
        if (!endswith(methodName, "__ENA")) {
printf("[%s:%d] cannot serialize method %s\n", __FUNCTION__, __LINE__, methodName.c_str());
exit(-1);
            continue;
        }
        methodName = methodName.substr(0, methodName.length()-5);
        ModuleIR *variant = allocIR(prefix + "VARIANT_" + methodName);
        variant->isInterface = false;
        unionIR->unionList.push_back(UnionItem{methodName, variant->name});
        uint64_t dataLength = 0;
        for (auto param: MI->params) {
            variant->fields.push_back(FieldElement{param.name, "", param.type, false, false, false, false, ""/*not param*/, false, false});
            dataLength += convertType(param.type);
        }
        if (dataLength > maxDataLength)
            maxDataLength = dataLength;
        counter++;
    }
    unionIR->fields.push_back(FieldElement{"data", "", "Bit(" + autostr(maxDataLength) + ")", false, false, false, false, ""/*not param*/, false, false});
}

static void processM2P(ModuleIR *IR)
{
    ModuleIR *HIR = nullptr;
    std::string host, target;
    uint64_t pipeArgSize = -1;
    for (auto inter: IR->interfaces) {
        if (inter.isPtr) {
            target = inter.fldName;
            if (trace_software)
                dumpModule("M2P/IIR :" + target, lookupInterface(inter.type));
        }
        else {
            HIR = lookupInterface(inter.type);
            host = inter.fldName;
            std::string iname = inter.type.substr(16);
            IR->name = iname + "___M2P";
            std::string type = "l_serialize_OC_" + iname + "__Data";
            ModuleIR *II = lookupIR(type);
            if (!II) {
                II = allocIR(type);
                II->isInterface = false;
                II->interfaces.push_back(FieldElement{"ifc", "", inter.type, false, false, false, false, ""/*not param*/, false, false});
            }
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
        if (endswith(methodName, "__RDY"))
            continue;
        if (!endswith(methodName, "__ENA")) {
printf("[%s:%d] cannot serialize method %s\n", __FUNCTION__, __LINE__, methodName.c_str());
exit(-1);
            continue;
        }
        std::string paramPrefix = methodName.substr(0, methodName.length()-5) + MODULE_SEPARATOR;
        std::string call;
        uint64_t dataLength = 32; // include length of tag
        for (auto param: MI->params) {
            MInew->params.push_back(param);
            dataLength += convertType(param.type);
            call = paramPrefix + param.name + ", " + call;
        }
        if (pipeArgSize > dataLength)
            call = autostr(pipeArgSize - dataLength) + "'d0, " + call;
        MInew->callList.push_back(new CallListElement{
             allocExpr(target + "$enq__ENA", allocExpr(PARAMETER_MARKER,
                 allocExpr("{ " + call + "16'd" + autostr(counter) + ", 16'd" + autostr(PORTALNUM) + "}"),
                 allocExpr("16'd" + autostr(dataLength/32)))), nullptr, true});
        counter++;
    }
    if (trace_software)
    dumpModule("M2P", IR);
}

static void processP2M(ModuleIR *IR)
{
    ModuleIR *IIR = nullptr, *HIR = nullptr;
    std::string host, target;
    for (auto inter: IR->interfaces) {
        if (inter.isPtr) {
            IIR = lookupInterface(inter.type);
            target = inter.fldName;
            std::string iname = inter.type.substr(16);
            IR->name = iname + "___P2M";
            std::string type = "l_serialize_OC_P2M_MD_" + iname + "_OD__KD__KD_Data";
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
    if (trace_software)
printf("[%s:%d] lookup '%s' -> %p\n", __FUNCTION__, __LINE__, (host + MODULE_SEPARATOR + "enq__ENA").c_str(), (void *)MInew);
assert(MInew);
    int counter = 0;  // start method number at 0
    for (auto MI: IIR->methods) {
        uint64_t offset = 32;
        std::string methodName = MI->name;
        std::string paramPrefix = methodName.substr(0, methodName.length()-5) + MODULE_SEPARATOR;
        if (endswith(methodName, "__RDY"))
            continue;
        ACCExpr *paramList = allocExpr(",");
        ACCExpr *call = allocExpr(target + MODULE_SEPARATOR + methodName, allocExpr(PARAMETER_MARKER, paramList));
        for (auto param: MI->params) {
            uint64_t upper = offset + convertType(param.type);
            paramList->operands.push_back(allocExpr(host + "$enq$v[" + autostr(upper-1) + ":" + autostr(offset) + "]"));
            offset = upper;
        }
        if (!endswith(methodName, "__ENA")) {
printf("[%s:%d] cannot serialize method %s\n", __FUNCTION__, __LINE__, methodName.c_str());
exit(-1);
            continue;
        }
        MInew->callList.push_back(new CallListElement{call, allocExpr("==", allocExpr(host + "$enq$v[31:16]"),
                 allocExpr("16'd" + autostr(counter))), true});
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
        if (startswith(IR->name, "l_serialize_"))
            processSerialize(IR);
        if (startswith(IR->name, "P2M")) {
    if (trace_software)
dumpModule("P2M", IR);
            irSeq.push_back(IR);
            processP2M(IR);
        }
        if (startswith(IR->name, "M2P")) {
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
dumpModule("M2P", IR);
            processM2P(IR);
        }
    }
}
