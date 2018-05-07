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
//#include <stdlib.h> // atol
#include <string.h>
#include <assert.h>
#include "AtomiccIR.h"
#include "common.h"

#if 0
static int trace_assign;//= 1;
static int trace_expand;//= 1;

static std::map<std::string, RefItem> refList;
static std::map<std::string, AssignItem> assignList;
static std::map<std::string, std::string> replaceTarget;
static std::map<std::string, std::map<uint64_t, BitfieldPart>> bitfieldList;

static std::string getRdyName(std::string basename);
static uint64_t convertType(std::string arg);

#include "AtomiccExpr.h"
#include "AtomiccReadIR.h"
#endif

std::string getRdyName(std::string basename)
{
    std::string rdyName = basename;
    if (endswith(rdyName, "__ENA"))
        rdyName = rdyName.substr(0, rdyName.length()-5);
    rdyName += "__RDY";
    return rdyName;
}

ModuleIR *lookupIR(std::string ind)
{
    if (ind == "")
        return nullptr;
    return mapIndex[ind];
}

uint64_t convertType(std::string arg)
{
    if (arg == "" || arg == "void")
        return 0;
    const char *bp = arg.c_str();
    auto checkT = [&] (const char *val) -> bool {
        int len = strlen(val);
        bool ret = !strncmp(bp, val, len);
        if (ret)
            bp += len;
        return ret;
    };
    if (checkT("INTEGER_"))
        return atoi(bp);
    if (checkT("ARRAY_"))
        return convertType(bp);
    if (auto IR = lookupIR(bp)) {
        uint64_t total = 0;
        for (auto item: IR->fields)
            total += convertType(item.type);
        return total;
    }
    printf("[%s:%d] convertType FAILED '%s'\n", __FUNCTION__, __LINE__, bp);
    exit(-1);
}

std::string sizeProcess(std::string type)
{
    uint64_t val = convertType(type);
    if (val > 1)
        return "[" + autostr(val - 1) + ":0]";
    return "";
}

ModuleIR *iterField(ModuleIR *IR, CBFun cbWorker)
{
    for (auto item: IR->fields) {
        int64_t vecCount = item.vecCount;
        int dimIndex = 0;
        do {
            std::string fldName = item.fldName;
            if (vecCount != -1)
                fldName += autostr(dimIndex++);
            if (auto ret = (cbWorker)(item, fldName))
                return ret;
        } while(--vecCount > 0);
    }
    return nullptr;
}

MethodInfo *lookupMethod(ModuleIR *IR, std::string name)
{
    if (IR->method.find(name) == IR->method.end()) {
        if (IR->method.find(name + "__ENA") != IR->method.end())
            name += "__ENA";
        else if (endswith(name, "__ENA") && IR->method.find(name.substr(0, name.length()-5)) != IR->method.end())
            name = name.substr(0, name.length()-5);
        else
            return nullptr;
    }
    return IR->method[name];
}

MethodInfo *lookupQualName(ModuleIR *searchIR, std::string searchStr)
{
    std::string fieldName;
    while (1) {
        int ind = searchStr.find(MODULE_SEPARATOR);
        fieldName = searchStr.substr(0, ind);
        searchStr = searchStr.substr(ind+1);
        ModuleIR *nextIR = iterField(searchIR, CBAct {
              if (ind != -1 && fldName == fieldName)
                  return lookupIR(item.type);
              return nullptr; });
        if (!nextIR)
            break;
        searchIR = nextIR;
    };
    for (auto item: searchIR->interfaces)
        if (item.fldName == fieldName)
            return lookupMethod(lookupIR(item.type), searchStr);
    return nullptr;
}

void getFieldList(std::list<FieldItem> &fieldList, std::string name, std::string base, std::string type, bool out, bool force, uint64_t offset, bool alias, bool init)
{
    if (init)
        fieldList.clear();
    if (ModuleIR *IR = lookupIR(type)) {
        if (IR->unionList.size() > 0) {
            for (auto item: IR->unionList) {
                uint64_t toff = offset;
                std::string tname;
                if (out) {
                    toff = 0;
                    tname = name;
                }
                getFieldList(fieldList, name + MODULE_SEPARATOR + item.name, tname, item.type, out, true, toff, true, false);
            }
            for (auto item: IR->fields) {
                fieldList.push_back(FieldItem{name, base, item.type, alias, offset}); // aggregate data
                offset += convertType(item.type);
            }
        }
        else
            for (auto item: IR->fields) {
                getFieldList(fieldList, name + MODULE_SEPARATOR + item.fldName, base, item.type, out, true, offset, alias, false);
                offset += convertType(item.type);
            }
    }
    else if (force)
        fieldList.push_back(FieldItem{name, base, type, alias, offset});
    if (trace_expand && init)
        for (auto fitem: fieldList) {
printf("%s: name %s base %s type %s %s offset %d\n", __FUNCTION__, fitem.name.c_str(), fitem.base.c_str(), fitem.type.c_str(), fitem.alias ? "ALIAS" : "", (int)fitem.offset);
        }
}

ModuleIR *allocIR(std::string name)
{
    ModuleIR *IR = new ModuleIR;
    IR->name = name;
    mapIndex[IR->name] = IR;
    return IR;
}

MethodInfo *allocMethod(std::string name)
{
    MethodInfo *MI = new MethodInfo{nullptr};
    MI->name = name;
    return MI;
}

void addMethod(ModuleIR *IR, MethodInfo *MI)
{
    std::string methodName = MI->name;
    if (endswith(methodName, "__ENA"))
        methodName = methodName.substr(0, methodName.length()-5);
    IR->method[methodName] = MI;
}

void dumpModule(std::string name, ModuleIR *IR)
{
printf("[%s:%d] DDDDDDDDDDDDDDDDDDD %s name %s\n", __FUNCTION__, __LINE__, name.c_str(), IR->name.c_str());
    for (auto FI: IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        printf("method %s(", methodName.c_str());
        std::string sep;
        for (auto param: MI->params) {
            printf("%s%s %s", sep.c_str(), param.type.c_str(), param.name.c_str());
            sep = ", ";
        }
        printf(") = %s\n", tree2str(MI->guard).c_str());
        for (auto item: MI->callList)
            printf("  call%s %s: %s\n", item->isAction ? "/Action" : "", tree2str(item->cond).c_str(), tree2str(item->value).c_str());
    }
}

std::string findType(std::string name)
{
//printf("[%s:%d] name %s\n", __FUNCTION__, __LINE__, name.c_str());
    if (refList.find(name) != refList.end())
        return refList[name].type;
printf("[%s:%d] reference to '%s', but could not locate RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR \n", __FUNCTION__, __LINE__, name.c_str());
    exit(-1);
    return "";
}
