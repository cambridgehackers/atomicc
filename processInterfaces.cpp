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

static void processSerialize(ModuleIR *IR)
{
    std::string prefix = "__" + IR->name + "_";
    auto inter = IR->interfaces.front();
    ModuleIR *IIR = lookupIR(inter.type);
    IR->fields.clear();
    IR->fields.push_back(FieldElement{"len", -1, "INTEGER_16", false});
    IR->fields.push_back(FieldElement{"tag", -1, "INTEGER_16", false});
    ModuleIR *unionIR = allocIR(prefix + "UNION");
    IR->fields.push_back(FieldElement{"data", -1, unionIR->name, false});
    int counter = 0;  // start method number at 0
    uint64_t maxDataLength = 0;
    for (auto FI: IIR->method) {
        MethodInfo *MI = FI.second;
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
        unionIR->unionList.push_back(UnionItem{methodName, variant->name});
        uint64_t dataLength = 0;
        for (auto param: MI->params) {
            variant->fields.push_back(FieldElement{param.name, -1, param.type, false});
            dataLength += convertType(param.type);
        }
        if (dataLength > maxDataLength)
            maxDataLength = dataLength;
        counter++;
    }
    unionIR->fields.push_back(FieldElement{"data", -1, "INTEGER_" + autostr(maxDataLength), false});
}

static void processM2P(ModuleIR *IR)
{
    ModuleIR *IIR = nullptr, *HIR = nullptr;
    std::string host, target;
    uint64_t pipeArgSize = -1;
    for (auto inter: IR->interfaces) {
        if (inter.isPtr) {
            IIR = lookupIR(inter.type);
            target = inter.fldName;
            for (auto FI: IIR->method) {
                MethodInfo *MI = FI.second;
                std::string methodName = MI->name;
printf("[%s:%d] methodname %s\n", __FUNCTION__, __LINE__, MI->name.c_str());
                if (!endswith(methodName, "__RDY")) {
                    std::string type = MI->params.front().type;
printf("[%s:%d] type %s\n", __FUNCTION__, __LINE__, type.c_str());
                    processSerialize(lookupIR(type));
                    pipeArgSize = convertType(type);
                }
            }
                
        }
        else {
            HIR = lookupIR(inter.type);
            host = inter.fldName;
            IR->name = "l_module" + inter.type.substr(12) + "___M2P";
        }
    }
    int counter = 0;  // start method number at 0
    for (auto FI: HIR->method) {
        MethodInfo *MI = FI.second;
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
             allocExpr(target + "$enq__ENA", allocExpr("{", allocExpr("{ " + call
                 + "16'd" + autostr(counter) + ", 16'd" + autostr(dataLength/32) + "}"))), nullptr, true});
        counter++;
    }
    dumpModule("M2P", IR);
}

static void processP2M(ModuleIR *IR)
{
    ModuleIR *IIR = nullptr, *HIR = nullptr;
    std::string host, target;
    for (auto inter: IR->interfaces) {
        if (inter.isPtr) {
            IIR = lookupIR(inter.type);
            target = inter.fldName;
            IR->name = "l_module" + inter.type.substr(12) + "___P2M";
        }
        else {
            HIR = lookupIR(inter.type);
            host = inter.fldName;
        }
    }
    for (auto FI: HIR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
printf("[%s:%d] P2Mhifmethod %s\n", __FUNCTION__, __LINE__, methodName.c_str());
        MethodInfo *MInew = allocMethod(host + MODULE_SEPARATOR + methodName);
        addMethod(IR, MInew);
printf("[%s:%d] create '%s'\n", __FUNCTION__, __LINE__, MInew->name.c_str());
        for (auto param: MI->params)
            MInew->params.push_back(param);
        MInew->type = MI->type;
        MInew->guard = MI->guard;
    }
    MethodInfo *MInew = lookupMethod(IR, host + MODULE_SEPARATOR + "enq");
printf("[%s:%d] lookup '%s' -> %p\n", __FUNCTION__, __LINE__, (host + MODULE_SEPARATOR + "enq__ENA").c_str(), MInew);
assert(MInew);
    int counter = 0;  // start method number at 0
    for (auto FI: IIR->method) {
        uint64_t offset = 32;
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        std::string paramPrefix = methodName.substr(0, methodName.length()-5) + MODULE_SEPARATOR;
        if (endswith(methodName, "__RDY"))
            continue;
        ACCExpr *paramList = allocExpr(",");
        ACCExpr *call = allocExpr(target + MODULE_SEPARATOR + methodName, allocExpr("{", paramList));
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
    dumpModule("P2M", IR);
}

void processInterfaces(std::list<ModuleIR *> &irSeq)
{
    for (auto mapp: mapIndex) {
        ModuleIR *IR = mapp.second;
        if (startswith(IR->name, "l_serialize_"))
            processSerialize(IR);
        if (startswith(IR->name, "l_module_OC_P2M")) {
            irSeq.push_back(IR);
            processP2M(IR);
        }
        if (startswith(IR->name, "l_module_OC_M2P")) {
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
