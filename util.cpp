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

int trace_expand;//=1;
std::map<std::string, ModuleIR *> mapIndex, mapStripped, interfaceIndex, interfaceStripped;
static int trace_iter;//=1;
static int traceLookup;//=1;

std::string baseMethodName(std::string pname)
{
    int ind = pname.find("__ENA[");
    if (ind == -1)
        ind = pname.find("__RDY[");
    if (ind > 0)
        pname = pname.substr(0, ind) + pname.substr(ind + 5);
    if (endswith(pname, "__ENA") || endswith(pname, "__RDY"))
        pname = pname.substr(0, pname.length()-5);
    return pname;
}
std::string getRdyName(std::string basename)
{
    std::string base = baseMethodName(basename), sub;
    if (endswith(base, "]")) {
        int ind = base.find("[");
        sub = base.substr(ind);
        base = base.substr(0, ind);
    }
    return base + "__RDY" + sub;
}

std::string getEnaName(std::string basename)
{
    std::string base = baseMethodName(basename), sub;
    if (endswith(base, "]")) {
        int ind = base.find("[");
        sub = base.substr(ind);
        base = base.substr(0, ind);
    }
    return base + "__ENA" + sub;
}

bool isRdyName(std::string name)
{
    std::string rname = getRdyName(name);
    return name == rname;
}

bool isEnaName(std::string name)
{
    std::string rname = getEnaName(name);
    return name == rname;
}

void extractParam(std::string replaceList, MapNameValue &mapValue)
{
    int ind = replaceList.find("(");
    if (ind > 0) {
        replaceList = replaceList.substr(ind+1);
        replaceList = replaceList.substr(0, replaceList.length()-1); // remove ')'
        while ((ind = replaceList.find("=")) > 0) {
            std::string name = replaceList.substr(0, ind);
            replaceList = replaceList.substr(ind+1);
            ind = replaceList.find(",");
            std::string value = replaceList;
            replaceList = "";
            if (ind > 0) {
                replaceList = value.substr(ind+1);
                value = value.substr(0, ind);
            }
            mapValue[name] = value;
        }
    }
}

ModuleIR *lookupIR(std::string name)
{
    std::string ind = name;
    ModuleIR *IR = nullptr;
    if (ind == "")
        return nullptr;
    if (mapIndex.find(ind) != mapIndex.end())
        IR = mapIndex[ind];
    else {
        int i = ind.find("(");
        if (i > 0)   // now try without parameter instantiation
            ind = ind.substr(0,i);
        if (mapStripped.find(ind) != mapStripped.end())
            IR = mapStripped[ind];
    }
    if (IR && IR->isInterface)
        printf("[%s:%d] was interface %s\n", __FUNCTION__, __LINE__, ind.c_str());
    return IR;
}

ModuleIR *lookupInterface(std::string name)
{
    std::string ind = name;
    ModuleIR *IR = nullptr;
    if (ind == "")
        return nullptr;
    if (interfaceIndex.find(ind) != interfaceIndex.end())
        IR = interfaceIndex[ind];
    else {
        int i = ind.find("(");
        if (i > 0)   // now try without parameter instantiation
            ind = ind.substr(0,i);
        if (interfaceStripped.find(ind) != interfaceStripped.end())
            IR = interfaceStripped[ind];
    }
    if (IR && !IR->isInterface)
        printf("[%s:%d] not interface %s\n", __FUNCTION__, __LINE__, ind.c_str());
    return IR;
}

std::string instantiateType(std::string arg, MapNameValue &mapValue) // also check updateCount()
{
    const char *bp = arg.c_str();
    auto checkT = [&] (const char *val) -> bool {
        int len = strlen(val);
        bool ret = !strncmp(bp, val, len);
        if (ret)
            bp += len;
        return ret;
    };
    auto mapReturn = [&] (std::string rets) -> std::string {
        bool hasAtSign = rets[0] == '@';
        if (hasAtSign)
            rets = rets.substr(1);
        std::string newval = mapValue[rets];
        if (rets != "0" && rets != "1" && newval != "") { // don't translate trivial values
printf("[%s:%d] INSTTTANTIATEEEEEEEEEEEEEEEEEEEEEEEEEEE %s -> %s\n", __FUNCTION__, __LINE__, arg.c_str(), newval.c_str());
            rets = newval;
        }
        if (hasAtSign)
            rets = "@" + rets;
        return rets;
    };
    if (checkT("ARRAY_")) {
        std::string arr = bp;
        int ind = arr.find("_");
        if (ind > 0)
            arr = arr.substr(0, ind);
        while (isdigit(*bp) || *bp == '_')
            bp++;
        std::string newtype = "ARRAY_" + arr + "_" + instantiateType(bp, mapValue);
printf("[%s:%d] oldtype %s newtype %s\n", __FUNCTION__, __LINE__, arg.c_str(), newtype.c_str());
        return newtype;
    }
    if (arg == "" || arg == "void")
        return arg;
    if (checkT("Bit(")) {
        std::string rets = bp;
        return "Bit(" + mapReturn(rets.substr(0, rets.length()-1)) + ")";
    }
    if (arg == "FLOAT")
        return arg;
    if (checkT("@")) {
//printf("[%s:%d] PARAMETER %s\n", __FUNCTION__, __LINE__, bp);
        return mapReturn(arg);
    }
    if (auto IR = lookupIR(bp)) {
        return mapReturn(bp);
    }
    return mapReturn(bp);
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
        if (ind > 0) {
            arr = arr.substr(0, ind);
            bp += ind + 1;
        }
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
    if (lookupInterface(bp))
        return "0";
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

std::string typeDeclaration(std::string type)
{
    return "logic " + sizeProcess(type);
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
    if (isEnaName(nameShort))
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
ModuleIR *implements = lookupInterface(searchIR->interfaceName);
if (!implements) {
    printf("[%s:%d] module %s missing interfaceName %s\n", __FUNCTION__, __LINE__, searchIR->name.c_str(), searchIR->interfaceName.c_str());
    exit(-1);
}
    if (traceLookup)
        printf("%s: START searchIR %p %s ifc %s searchStr %s implements %p\n", __FUNCTION__, (void *)searchIR, searchIR->name.c_str(), searchIR->interfaceName.c_str(), searchStr.c_str(), (void *)implements);
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
        if (traceLookup)
            printf("[%s:%d] IR %s: ind %d indnext %d fieldName %s searchStr %s\n", __FUNCTION__, __LINE__, searchIR->name.c_str(), ind, indnext, fieldName.c_str(), searchStr.c_str());
        ModuleIR *nextIR = nullptr;
        while(1) {
        if (traceLookup) {
            printf("[%s:%d] searchir %p implements %p\n", __FUNCTION__, __LINE__, (void *)searchIR, (void *)implements);
            dumpModule("IMPL", searchIR);
        }
        nextIR = iterField(searchIR, CBAct {
            std::string fldName = item.fldName;
            if (traceLookup)
                printf("[%s:%d] ind %d fldname %s fieldName %s type %s\n", __FUNCTION__, __LINE__, ind, fldName.c_str(), fieldName.c_str(), item.type.c_str());
            if (fldName == fieldName) { //ind != -1 && 
                if (traceLookup)
                     printf("[%s:%d]found \n", __FUNCTION__, __LINE__);
                return lookupIR(item.type);
            }
            return nullptr; });
        if (traceLookup)
            printf("[%s:%d] nextIR %p\n", __FUNCTION__, __LINE__, (void *)nextIR);
        if (!nextIR)
        nextIR = iterInterface(searchIR, CBAct {
            std::string fldName = item.fldName;
            if (traceLookup)
                printf("[%s:%d] ind %d fldname %s fieldName %s type %s searchStr %s\n", __FUNCTION__, __LINE__, ind, fldName.c_str(), fieldName.c_str(), item.type.c_str(), searchStr.c_str());
            if (fldName == fieldName) {     //ind != -1 && 
                if (traceLookup)
                     printf("[%s:%d]found \n", __FUNCTION__, __LINE__);
                return lookupInterface(item.type);
            }
            return nullptr; });
        if (!nextIR) {
        nextIR = iterInterface(searchIR, CBAct {
            std::string fldName = item.fldName;
            if (traceLookup)
                printf("[%s:%d] ind %d fldname %s fieldName %s type %s searchStr %s\n", __FUNCTION__, __LINE__, ind, fldName.c_str(), fieldName.c_str(), item.type.c_str(), searchStr.c_str());
            if (fldName == "") {     //ind != -1 && 
                if (traceLookup)
                     printf("[%s:%d]found \n", __FUNCTION__, __LINE__);
                return lookupInterface(item.type);
            }
            return nullptr; });
            if (traceLookup)
                printf("[%s:%d] NULL nextIR %p searchstr %s\n", __FUNCTION__, __LINE__, (void *)nextIR, searchStr.c_str());
            if (nextIR) {
                searchIR = nextIR;
                continue;
            }
        }
        if (traceLookup)
            printf("[%s:%d] nextIR %p\n", __FUNCTION__, __LINE__, (void *)nextIR);
        if (nextIR && nextIR->interfaceName != "") { // lookup points to an interface
            if (traceLookup)
                printf("[%s:%d] lookingup %s\n", __FUNCTION__, __LINE__, nextIR->interfaceName.c_str());
            nextIR = lookupInterface(nextIR->interfaceName);
        }
        if (traceLookup)
            printf("[%s:%d]nextIR %p name %s implements %p\n", __FUNCTION__, __LINE__, (void *)nextIR, nextIR ? nextIR->name.c_str() : "", (void *)implements);
        ModuleIR *temp = implements;
        implements = nullptr;
        if (traceLookup)
            printf("[%s:%d] searchstr '%s' indnext %d nextIR %p temp %p\n", __FUNCTION__, __LINE__, searchStr.c_str(), indnext, (void *)nextIR, (void *)temp);
        if (!nextIR && temp) {
            searchIR = temp;
            continue;
        }
        break;
        };
        searchStr = searchStr.substr(indnext);
        if (!nextIR)
            break;
        searchIR = nextIR;
    };
    if (traceLookup) {
        printf("[%s:%d] searchIR %p search %s\n", __FUNCTION__, __LINE__, (void *)searchIR, searchStr.c_str());
        dumpModule("SEARCHIR", searchIR);
    }
    if (searchIR)
        return lookupMethod(searchIR, searchStr);
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
        {}/*params*/, {}/*unionList*/, {}/*interfaces*/, {}/*parameters*/,
        {}/*interfaceConnect*/, 0/*genvarCount*/, isInterface,
        false/*isStruct*/, false/*isSerialize*/, false/*transformGeneric*/,
        "" /*interfaceVecCount*/, "" /*interfaceName*/};

    if (isInterface)
        interfaceIndex[iname] = IR;
    else
        mapIndex[iname] = IR;
    int i = iname.find("(");
    if (i > 0) {        // also insert without parameter instantiation
        iname = iname.substr(0,i);
        if (isInterface)
            interfaceStripped[iname] = IR;
        else
            mapStripped[iname] = IR;
    }
    return IR;
}

MethodInfo *allocMethod(std::string name)
{
    MethodInfo *MI = new MethodInfo{nullptr/*guard*/,
        nullptr/*subscript*/, ""/*generateSection*/,
        name/*name*/, false/*rule*/, false/*action*/,
        {}/*storeList*/, {}/*letList*/, {}/*assertList*/, {}/*callList*/, {}/*printfList*/,
        ""/*type*/, {}/*params*/, {}/*generateFor*/, {}/*instantiateFor*/,
        {}/*alloca*/, {}/*interfaceConnect*/, {{}}/*meta*/};
    return MI;
}

bool addMethod(ModuleIR *IR, MethodInfo *MI)
{
    std::string methodName = MI->name; // baseMethodName??
    if (isEnaName(methodName))
        methodName = baseMethodName(methodName);
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
    if (MI->alloca.size() || MI->letList.size() || MI->storeList.size() || MI->assertList.size()
        || MI->callList.size() || MI->generateFor.size() || MI->instantiateFor.size() || MI->interfaceConnect.size()) {
    printf("\n");
    for (auto item: MI->alloca)
        printf("      ALLOCA %s %s\n", item.second.type.c_str(), item.first.c_str());
    for (auto item: MI->letList)
        printf("      LET %s %s: %s=%s\n", item->type.c_str(), tree2str(item->cond).c_str(), tree2str(item->dest).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->storeList)
        printf("      STORE %s: %s=%s\n", tree2str(item->cond).c_str(), tree2str(item->dest).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->assertList)
        printf("      ASSERT %s: %s\n", tree2str(item->cond).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->callList)
        printf("      CALL%s %s: %s\n", item->isAction ? "/Action" : "", tree2str(item->cond).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->printfList)
        printf("      PRINTF%s %s: %s\n", item->isAction ? "/Action" : "", tree2str(item->cond).c_str(), tree2str(item->value).c_str());
    for (auto item: MI->generateFor)
        printf("      GENERATE %s: %s, %s, %s, %s, %s\n", tree2str(item.cond).c_str(),
            item.var.c_str(), tree2str(item.init).c_str(),
            tree2str(item.limit).c_str(), tree2str(item.incr).c_str(), item.body.c_str());
    for (auto item: MI->instantiateFor)
        printf("      INSTANTIATE %s: %s, %s, %s, %s, %s, %s\n", tree2str(item.cond).c_str(),
            item.var.c_str(), tree2str(item.init).c_str(),
            tree2str(item.limit).c_str(), tree2str(item.incr).c_str(),
            tree2str(item.sub).c_str(), item.body.c_str());
    for (auto IC: MI->interfaceConnect)
         printf("      INTERFACECONNECT%s %s %s %s\n", (IC.isForward ? "/Forward" : ""), tree2str(IC.target).c_str(), tree2str(IC.source).c_str(), IC.type.c_str());
    }
    printf("    }\n");
}

void dumpModule(std::string name, ModuleIR *IR)
{
    int interfaceNumber = 0;
    if (!IR)
        return;
    if (ModuleIR *implements = lookupInterface(IR->interfaceName))
        dumpModule(name + "__interface", implements);
    for (auto item: IR->interfaces)
        dumpModule(name + "_INTERFACE_" + autostr(interfaceNumber++), lookupInterface(item.type));
    for (auto item: IR->parameters)
        dumpModule(name + "_PARAMETERS_" + autostr(interfaceNumber++), lookupInterface(item.type));
    printf("%s: %s\nMODULE %s %s {\n", __FUNCTION__, name.c_str(), IR->name.c_str(), IR->interfaceName.c_str());
    for (auto item: IR->fields) {
        std::string ret = "    FIELD";
        if (item.isPtr)
            ret += "/Ptr";
        if (item.vecCount != "")
            ret += "/Count " + item.vecCount;
        ret += " ";
        ret += item.type + " '" + item.fldName + "'";
        printf("%s\n", ret.c_str());
    }
    for (auto item: IR->interfaces) {
        std::string ret = "    INTERFACE";
        if (item.isPtr)
            ret += "/Ptr";
        if (item.isLocalInterface) // interface declaration that is used to connect to local objects (does not appear in module signature)
            ret += "/Local";
        if (item.vecCount != "")
            ret += "/Count " + item.vecCount;
        ret += " ";
        ret += item.type + " '" + item.fldName + "'";
        printf("%s\n", ret.c_str());
    }
    for (auto item: IR->interfaceConnect)
        printf("    INTERFACECONNECT %s %s %s\n", tree2str(item.target).c_str(), tree2str(item.source).c_str(), item.type.c_str());
    for (auto MI: IR->methods)
        dumpMethod("", MI);
    for (auto item: IR->softwareName)
        printf("    SOFTWARE %s\n", item.c_str());
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

#include <unistd.h>
#include <fcntl.h>
#ifdef __APPLE__ // hack for debugging
#include <libproc.h>
#endif
char *getExecutionFilename(char *buf, int buflen)
{
    int rc, fd;
#ifdef __APPLE__ // hack for debugging
    static char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    rc = proc_pidpath (getpid(), pathbuf, sizeof(pathbuf));
    return pathbuf;
#endif
    char *filename = 0;
    buf[0] = 0;
    fd = open("/proc/self/maps", O_RDONLY);
    while ((rc = read(fd, buf, buflen-1)) > 0) {
	buf[rc] = 0;
	rc = 0;
	while(buf[rc]) {
	    char *endptr;
	    unsigned long addr = strtoul(&buf[rc], &endptr, 16);
	    if (endptr && *endptr == '-') {
		char *endptr2;
		unsigned long addr2 = strtoul(endptr+1, &endptr2, 16);
		if (addr <= (unsigned long)&getExecutionFilename && (unsigned long)&getExecutionFilename <= addr2) {
		    filename = strstr(endptr2, "  ");
		    while (*filename == ' ')
			filename++;
		    endptr2 = strstr(filename, "\n");
		    if (endptr2)
			*endptr2 = 0;
		    fprintf(stderr, "buffer %s\n", filename);
		    goto endloop;
		}
	    }
	    while(buf[rc] && buf[rc] != '\n')
		rc++;
	    if (buf[rc])
		rc++;
	}
    }
endloop:
    if (!filename) {
	fprintf(stderr, "[%s:%d] could not find execution filename\n", __FUNCTION__, __LINE__);
	return 0;
    }
    return filename;
}

std::string exprWidth(ACCExpr *expr, bool forceNumeric)
{
    if (!expr)
        return "0";
    std::string op = expr->value;
    if (relationalOp(op))
        return "1";
    if (isdigit(op[0])) {
        int ind = op.find("'");
        if (ind > 0) {
            return op.substr(0, ind);
        }
        else if (forceNumeric)
            return "1";
    }
    if (isIdChar(op[0])) {
        ACCExpr *lhs = getRHS(expr, 0);
        if (lhs && lhs->value == "[" && lhs->operands.size() > 0) {
            ACCExpr *first = getRHS(lhs, 0);
            if (first->value == ":") {
                ACCExpr *second = getRHS(first);
                first = getRHS(first, 0);
                if (!second)
                    return "1";
                if (isdigit(first->value[0]) && isdigit(second->value[0]))
                    return "(" + first->value + " - " + second->value + " + 1)";
            }
            else if (isdigit(first->value[0]))
                return "1";
        }
        return convertType(refList[op].type);
    }
    if (op == "?") {
        std::string len = exprWidth(getRHS(expr, 1), forceNumeric);
        if (len != "" && len != "0")
            return len;
        len = exprWidth(getRHS(expr, 2), forceNumeric);
        if (len != "" && len != "0")
            return len;
    }
    if (bitOp(op)) {
        for (auto item: expr->operands)
            if (exprWidth(item, forceNumeric) != "1")
                goto nextand;
        return "1";
nextand:;
    }
    if (op == "!") {
        return exprWidth(expr->operands.front(), forceNumeric);
    }
    return "0";
}

void updateWidth(ACCExpr *expr, std::string clen)
{
    if (!expr || clen == "" || !isdigit(clen[0]) || clen.find(" ") != std::string::npos)
        return;
    int len = atoi(clen.c_str());
    std::string cilen = exprWidth(expr);
    int ilen = atoi(cilen.c_str());
    if (ilen < 0 || len < 0) {
        printf("[%s:%d] len %d ilen %d tree %s\n", __FUNCTION__, __LINE__, len, ilen, tree2str(expr).c_str());
        exit(-1);
    }
    if (len > 0 && plainInteger(expr->value))
        expr->value = autostr(len) + "'d" + expr->value;
    else if (isIdChar(expr->value[0])) {
        if (trace_expr)
            printf("[%s:%d] ID %s ilen %d len %d\n", __FUNCTION__, __LINE__, tree2str(expr).c_str(), ilen, len);
        if (ilen > len && len > 0 && !expr->operands.size()) {
printf("[%s:%d] expr %s clen %s conv %s\n", __FUNCTION__, __LINE__, tree2str(expr).c_str(), clen.c_str(), convertType(refList[expr->value].type).c_str());
            ACCExpr *subexpr = allocExpr(":", allocExpr(autostr(len-1)));
            if (len > 1)
                subexpr->operands.push_back(allocExpr("0"));
            expr->operands.push_back(allocExpr("[", subexpr));
        }
    }
    else if (expr->value == ":") // for __phi
        updateWidth(getRHS(expr), clen);
    else if (expr->value == "?") {
        updateWidth(getRHS(expr), clen);
        updateWidth(getRHS(expr, 2), clen);
    }
    else if (arithOp(expr->value) || expr->value == "(" || expr->value == "@-") {
        if (expr->value == "@-"
            && isdigit(expr->operands.front()->value[0]) && len == 1) {
            /* hack to update width on "~foo", which translates to "foo ^ -1" in the IR */
            expr->value = expr->operands.front()->value;
            expr->operands.clear();
            updateWidth(expr, clen);
        }
        else
        for (auto item: expr->operands)
            updateWidth(item, clen);
    }
}

void walkReplaceBuiltin(ACCExpr *expr, std::string phiDefault)
{
    while(1) {
    if (expr->value == "__reduce") {
        ACCExpr *list = expr->operands.front();
        std::string op = getRHS(list, 0)->value;
        expr->value = "@" + op.substr(1, op.length() - 2);
        expr->operands.clear();
        expr->operands.push_back(getRHS(list, 1));
    }
    else if (expr->value == "__bitconcat") {
        ACCExpr *list = expr->operands.front();
        if (list->value == PARAMETER_MARKER)
            list->value = "{";
        expr->value = list->value;
        expr->operands = list->operands;
    }
    else if (expr->value == "__bitsubstr") {
        ACCExpr *list = expr->operands.front();
        ACCExpr *bitem = list->operands.front();
        if (!isIdChar(bitem->value[0])) {  // can only do bit select on net or reg (not expressions)
            printf("[%s:%d] can only do __bitsubstr on elementary items '%s'\n", __FUNCTION__, __LINE__, bitem->value.c_str());
            dumpExpr("BITSUB", expr);
            exit(-1);
        }
        bitem->operands.push_back(allocExpr("[", allocExpr(":", getRHS(list), getRHS(list, 2))));
        expr->value = bitem->value;
        expr->operands = bitem->operands;
    }
    else if (expr->value == "__phi") {
        ACCExpr *list = expr->operands.front(); // get "(" list of [":", cond, value] items
        int size = list->operands.size();
        ACCExpr *firstInList = getRHS(list, 0), *secondInList = getRHS(list);
        ACCExpr *newe = nullptr;
        if (size == 2 && matchExpr(getRHS(firstInList, 0), invertExpr(getRHS(secondInList, 0))))
            newe = allocExpr("?", getRHS(firstInList, 0), getRHS(firstInList), getRHS(secondInList));
        else if (size == 2 && getRHS(firstInList, 0)->value == "__default" && exprWidth(getRHS(secondInList)) == "1")
            newe = allocExpr("&&", getRHS(secondInList, 0), getRHS(secondInList));
        else if (size == 1)
            newe = getRHS(firstInList);
        else {
            //dumpExpr("PHI", list);
            newe = allocExpr("|");
            for (auto item: list->operands) {
                if (trace_expr)
                    dumpExpr("PHIELEMENTBEF", item);
                if (checkInteger(getRHS(item), "0"))
                    continue;    // default value is already '0'
                item->value = "?"; // Change from ':' -> '?'
                item->operands.push_back(allocExpr(phiDefault));
                updateWidth(item, exprWidth(getRHS(item)));
                newe->operands.push_back(item);
                if (trace_expr)
                    dumpExpr("PHIELEMENT", item);
            }
        }
        expr->value = newe->value;
        expr->operands = newe->operands;
    }
    else
        break;
    }
    for (auto item: expr->operands)
        walkReplaceBuiltin(item, "0");
}

ACCExpr *cleanupExprBuiltin(ACCExpr *expr, std::string phiDefault)
{
    if (!expr)
        return expr;
    walkReplaceBuiltin(expr, phiDefault);
    return cleanupExpr(expr);
}
