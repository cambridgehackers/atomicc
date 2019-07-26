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

int trace_expand;//= 1;
std::map<std::string, ModuleIR *> mapIndex;
std::map<std::string, ModuleIR *> interfaceIndex;
static int trace_iter;//=1;

std::string baseMethodName(std::string pname)
{
    if (endswith(pname, "__ENA") || endswith(pname, "__RDY"))
        pname = pname.substr(0, pname.length()-5);
    return pname;
}
std::string getRdyName(std::string basename)
{
    return baseMethodName(basename) + "__RDY";
}

std::string getEnaName(std::string basename)
{
    return baseMethodName(basename) + "__ENA";
}

ModuleIR *lookupIR(std::string name)
{
    std::string ind = name;
    //int i = ind.find("(");
    //if (i > 0)
        //ind = ind.substr(0,i);
    if (ind == "" || mapIndex.find(ind) == mapIndex.end())
        return nullptr;
    if (mapIndex[ind]->isInterface)
        printf("[%s:%d] was interface %s\n", __FUNCTION__, __LINE__, ind.c_str());
    return mapIndex[ind];
}

ModuleIR *lookupInterface(std::string name)
{
    std::string ind = name;
    //int i = ind.find("(");
    //if (i > 0)
        //ind = ind.substr(0,i);
    if (ind == "" || interfaceIndex.find(ind) == interfaceIndex.end())
        return nullptr;
    if (!interfaceIndex[ind]->isInterface)
        printf("[%s:%d] not interface %s\n", __FUNCTION__, __LINE__, ind.c_str());
    return interfaceIndex[ind];
}

std::string convertType(std::string arg, int arrayProcess)
{
    const char *bp = arg.c_str();
    auto checkT = [&] (const char *val) -> bool {
        int len = strlen(val);
        bool ret = !strncmp(bp, val, len);
        if (ret)
            bp += len;
        return ret;
    };
    if (checkT("ARRAY_")) {
        std::string arr = bp;
        int ind = arr.find("_");
        if (ind > 0)
            arr = arr.substr(0, ind);
        while (isdigit(*bp) || *bp == '_')
            bp++;
        if (arrayProcess == 1) // element only
            return convertType(bp); // only return element size (caller handles array decl)
        if (arrayProcess == 2) // array size only
            return arr;
        return "(" + arr + " * " + convertType(bp) + ")";
    }
    if (arrayProcess == 2) // array size only
        return "";
    if (arg == "" || arg == "void")
        return "0";
    if (checkT("Bit(")) {
        std::string rets = bp;
        return rets.substr(0, rets.length()-1);
    }
    if (arg == "FLOAT")
        return "1";                 // should never occur in declarations
    if (checkT("@")) {
//printf("[%s:%d] PARAMETER %s\n", __FUNCTION__, __LINE__, bp);
        return bp;
    }
    if (auto IR = lookupIR(bp)) {
        std::string total;
        for (auto item: IR->fields) {
            std::string thisSize = convertType(item.type);
            if (item.vecCount != "")
                thisSize = "(" + thisSize + " * " + item.vecCount + ")";
            if (total == "")
                total = thisSize;
            else
                total = "(" + total + " + " + thisSize + ")";
        }
        return total;
    }
    printf("[%s:%d] convertType FAILED '%s'\n", __FUNCTION__, __LINE__, bp);
    exit(-1);
}

std::string sizeProcess(std::string type)
{
    std::string val = convertType(type, 1); // get element size only for arrays
    if (val == "" || val == "0" || val == "1")
        return "";
    return "[" + val + " - 1:0]";
}

ModuleIR *iterField(ModuleIR *IR, CBFun cbWorker)
{
    for (auto item: IR->fields) {
        if (trace_iter)
            printf("[%s:%d] item.fldname %s vec '%s'\n", __FUNCTION__, __LINE__, item.fldName.c_str(), item.vecCount.c_str());
        if (auto ret = (cbWorker)(item))
            return ret;
    }
    return nullptr;
}

ModuleIR *iterInterface(ModuleIR *IR, CBFun cbWorker)
{
    for (auto item: IR->interfaces) {
        if (trace_iter)
            printf("[%s:%d] item.fldname %s vec '%s'\n", __FUNCTION__, __LINE__, item.fldName.c_str(), item.vecCount.c_str());
        if (auto ret = (cbWorker)(item))
            return ret;
    }
    return nullptr;
}

MethodInfo *lookupMethod(ModuleIR *IR, std::string name)
{
    std::string nameENA = name + "__ENA";
    std::string nameShort = name;
    if (endswith(nameShort, "__ENA"))
        nameShort = baseMethodName(nameShort);
    for (auto MI : IR->methods) {
        if (MI->name == name || MI->name == nameENA || MI->name == nameShort)
            return MI;
    }
    return nullptr;
}

MethodInfo *lookupQualName(ModuleIR *searchIR, std::string searchStr)
{
    std::string fieldName;
    //printf("%s: START searchIR %s searchStr %s\n", __FUNCTION__, searchIR->name.c_str(), searchStr.c_str());
    while (1) {
        int ind = searchStr.find(MODULE_SEPARATOR);
        int ind2 = searchStr.find("[");
        int indnext = ind + 1;
        if (ind2 != -1 && (ind == -1 || ind2 < ind)) { // handle subscript
            ind = ind2;
            indnext = searchStr.find("]") + 1;
            if (searchStr[indnext] == '.')
                indnext++;
        }
        fieldName = searchStr.substr(0, ind);
        searchStr = searchStr.substr(indnext);
        //printf("[%s:%d] ind %d indnext %d fieldName %s searchStr %s\n", __FUNCTION__, __LINE__, ind, indnext, fieldName.c_str(), searchStr.c_str());
        ModuleIR *nextIR = iterField(searchIR, CBAct {
            std::string fldName = item.fldName;
            if (trace_iter)
                printf("[%s:%d] ind %d fldname %s item.fldname %s type %s\n", __FUNCTION__, __LINE__, ind, fldName.c_str(), item.fldName.c_str(), item.type.c_str());
            if (ind != -1 && fldName == fieldName)
                return lookupIR(item.type);
            return nullptr; });
        //printf("[%s:%d]nextIR %p\n", __FUNCTION__, __LINE__, nextIR);
        if (!nextIR)
            break;
        searchIR = nextIR;
    };
    std::string nullInterface;
    //printf("[%s:%d]interface %s\n", __FUNCTION__, __LINE__, fieldName.c_str());
    for (auto item: searchIR->interfaces)
        if (item.fldName == fieldName)
            return lookupMethod(lookupInterface(item.type), searchStr);
        else if (item.fldName == "")
            nullInterface = item.type;
    if (nullInterface != "")
        return lookupMethod(lookupInterface(nullInterface), searchStr);
    return nullptr;
}

std::string fixupQualPin(ModuleIR *searchIR, std::string searchStr)
{
    std::string fieldName, origName = searchStr;
    std::string outName;
    bool found = false;
    //printf("[%s:%d] start %s\n", __FUNCTION__, __LINE__, searchStr.c_str());
    while (1) {
        int ind = searchStr.find(MODULE_SEPARATOR);
        fieldName = searchStr.substr(0, ind);
        searchStr = searchStr.substr(ind+1);
        ModuleIR *nextIR = iterField(searchIR, CBAct {
                std::string fldName = item.fldName;
                if (trace_iter)
                    printf("[%s:%d] fldname %s item.fldname %s\n", __FUNCTION__, __LINE__, fldName.c_str(), item.fldName.c_str());
                if (fieldName != "" && fldName == fieldName)
                    return lookupIR(item.type);
            return nullptr; });
        if (!nextIR)
            break;
        outName += fieldName + MODULE_SEPARATOR;
        searchIR = nextIR;
    };
    //printf("[%s:%d] out %s field %s search %s\n", __FUNCTION__, __LINE__, outName.c_str(), fieldName.c_str(), searchStr.c_str());
    while (1) {
        if (found) {
            int ind = searchStr.find(MODULE_SEPARATOR);
            fieldName = searchStr.substr(0, ind);
            searchStr = searchStr.substr(ind+1);
        }
        ModuleIR *nextIR = iterInterface(searchIR, CBAct {
                std::string fldName = item.fldName;
                if (trace_iter)
                    printf("[%s:%d] fldname %s item.fldname %s\n", __FUNCTION__, __LINE__, fldName.c_str(), item.fldName.c_str());
                if (fieldName != "" && fldName == fieldName)
                    return lookupInterface(item.type);
            return nullptr; });
        if (!nextIR)
            break;
        outName += fieldName;
        searchIR = nextIR;
        found = true;
    };
    outName += fieldName;
    if (found)
    for (auto item: searchIR->fields)
        if (item.fldName == fieldName) {
            //printf("[%s:%d] found %s\n", __FUNCTION__, __LINE__, outName.c_str());
            return outName;
        }
    return origName;
}

void getFieldList(std::list<FieldItem> &fieldList, std::string name, std::string base, std::string type, bool out, bool force, uint64_t aoffset, bool alias, bool init)
{
    if (trace_expand)
        printf("[%s:%d] entry %s type %s force %d init %d\n", __FUNCTION__, __LINE__, name.c_str(), type.c_str(), force, init);
    __block uint64_t offset = aoffset;
    std::string sname = name + MODULE_SEPARATOR;
    if (init)
        fieldList.clear();
    if (ModuleIR *IR = lookupIR(type)) {
        for (auto item: IR->unionList) {
            uint64_t toff = offset;
            std::string tname;
            if (out) {
                toff = 0;
                tname = name;
            }
            getFieldList(fieldList, sname + item.name, tname, item.type, out, true, toff, true, false);
        }
        iterField(IR, CBAct {
                std::string fldName = item.fldName;
                if (trace_iter)
                    printf("[%s:%d] fldname %s item.fldname %s\n", __FUNCTION__, __LINE__, fldName.c_str(), item.fldName.c_str());
                if (item.isParameter == "") {
                    getFieldList(fieldList, sname + fldName, base, item.type, out, true, offset, alias, false);
                    offset += atoi(convertType(item.type).c_str());
                }
            return nullptr; });
    }
    else if (force) {
        if (trace_expand)
            printf("[%s:%d] getadd %s\n", __FUNCTION__, __LINE__, name.c_str());
        fieldList.push_back(FieldItem{name, base, type, alias, offset});
    }
    if (trace_expand && init)
        for (auto fitem: fieldList) {
printf("%s: name %s base %s type %s %s offset %d\n", __FUNCTION__, fitem.name.c_str(), fitem.base.c_str(), fitem.type.c_str(), fitem.alias ? "ALIAS" : "", (int)fitem.offset);
        }
}

ModuleIR *allocIR(std::string name, bool isInterface)
{
    std::string iname = name;
    ModuleIR *IR = new ModuleIR{name/*name*/,
        {}/*metaList*/, {}/*softwareName*/, {}/*methods*/,
        {}/*generateBody*/, {}/*priority*/, {}/*fields*/,
        {}/*params*/, {}/*unionList*/, {}/*interfaces*/,
        {}/*interfaceConnect*/, 0/*genvarCount*/, false/*isInterface*/,
        false/*isStruct*/, false/*isSerialize*/, false/*transformGeneric*/};
    if (isInterface)
        interfaceIndex[iname] = IR;
    else
        mapIndex[iname] = IR;
    return IR;
}

MethodInfo *allocMethod(std::string name)
{
    MethodInfo *MI = new MethodInfo{nullptr/*guard*/,
        name/*name*/, false/*rule*/, false/*action*/,
        {}/*storeList*/, {}/*letList*/, {}/*callList*/, {}/*printfList*/,
        ""/*type*/, {}/*params*/, {}/*generateFor*/, {}/*instantiateFor*/,
        {}/*alloca*/, {{}}/*meta*/};
    return MI;
}

bool addMethod(ModuleIR *IR, MethodInfo *MI)
{
    std::string methodName = MI->name; // baseMethodName??
    if (endswith(methodName, "__ENA"))
        methodName = methodName.substr(0, methodName.length()-5);
    bool ret = !startswith(methodName, "FOR$");
    if (ret)
        IR->methods.push_back(MI);
    else
        IR->generateBody[methodName] = MI;
    return ret;
}

void dumpMethod(std::string name, MethodInfo *MI)
{
    std::string methodName = MI->name;
    printf("%s    METHOD%s %s(", name.c_str(), MI->rule ? "/Rule" : "", methodName.c_str());
    std::string sep;
    for (auto param: MI->params) {
        printf("%s%s %s", sep.c_str(), param.type.c_str(), param.name.c_str());
        sep = ", ";
    }
    printf(")");
    std::string retType = MI->type, retVal = tree2str(MI->guard);
    if (retType != "" || retVal != "")
        printf(" %s = %s", retType.c_str(), retVal.c_str());
    printf(" {");
    if (MI->alloca.size() || MI->letList.size() || MI->storeList.size()
        || MI->callList.size() || MI->generateFor.size() || MI->instantiateFor.size()) {
    printf("\n");
    for (auto item: MI->alloca)
        printf("      ALLOCA %s %s\n", item.second.type.c_str(), item.first.c_str());
    for (auto item: MI->letList)
        printf("      LET %s %s: %s=%s\n", item->type.c_str(), tree2str(item->cond).c_str(), tree2str(item->dest).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->storeList)
        printf("      STORE %s: %s=%s\n", tree2str(item->cond).c_str(), tree2str(item->dest).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->callList)
        printf("      CALL%s %s: %s\n", item->isAction ? "/Action" : "", tree2str(item->cond).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->generateFor)
        printf("      GENERATE %s: %s, %s, %s, %s, %s\n", tree2str(item.cond).c_str(),
            item.var.c_str(), tree2str(item.init).c_str(),
            tree2str(item.limit).c_str(), tree2str(item.incr).c_str(), item.body.c_str());
    for (auto item: MI->instantiateFor)
        printf("      INSTANTIATE %s: %s, %s, %s, %s, %s, %s\n", tree2str(item.cond).c_str(),
            item.var.c_str(), tree2str(item.init).c_str(),
            tree2str(item.limit).c_str(), tree2str(item.incr).c_str(),
            tree2str(item.sub).c_str(), item.body.c_str());
    }
    printf("    }\n");
}

void dumpModule(std::string name, ModuleIR *IR)
{
    int interfaceNumber = 0;
    if (!IR)
        return;
    for (auto item: IR->interfaces)
        dumpModule(name + "_INTERFACE_" + autostr(interfaceNumber++), lookupInterface(item.type));
printf("[%s:%d] DDDDDDDDDDDDDDDDDDD %s\nMODULE %s{\n", __FUNCTION__, __LINE__, name.c_str(), IR->name.c_str());
    for (auto item: IR->fields) {
        std::string ret = "    FIELD";
        if (item.isPtr)
            ret += "/Ptr";
        ret += " ";
        if (item.vecCount != "")
            ret += "/Count " + item.vecCount;
        ret += item.type + " " + item.fldName;
        printf("%s\n", ret.c_str());
    }
    for (auto item: IR->interfaces) {
        std::string ret = "    INTERFACE";
        if (item.isPtr)
            ret += "/Ptr";
        if (item.isLocalInterface) // interface declaration that is used to connect to local objects (does not appear in module signature)
            ret += "/Local";
        ret += " ";
        if (item.vecCount != "")
            ret += "/Count " + item.vecCount;
        ret += item.type + " " + item.fldName;
        printf("%s\n", ret.c_str());
    }
    for (auto item: IR->interfaceConnect)
        printf("    INTERFACECONNECT %s %s %s\n", item.target.c_str(), item.source.c_str(), item.type.c_str());
    for (auto MI: IR->methods)
        dumpMethod("", MI);
printf("}\n");
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
std::string CBEMangle(const std::string &S)
{
    std::string Result;
    for (unsigned i = 0, e = S.size(); i != e; ++i)
        if (isalnum(S[i]) || S[i] == '_' || S[i] == '$')
            Result += S[i];
        else {
            Result += '_';
            Result += 'A'+(S[i]&15);
            Result += 'A'+((S[i]>>4)&15);
            Result += '_';
        }
    return Result;
}
