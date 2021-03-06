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

static int trace_accessible;//=1;
int trace_interface;//=1;
int trace_expand;//=1;
int trace_parameters;//=1;
int trace_IR;//=1;
static int traceLookup;//=1;
std::map<std::string, ModuleIR *> mapIndex, interfaceIndex, mapAllModule;
static std::map<std::string, ModuleIR *> mapStripped, interfaceStripped;

std::string baseMethodName(std::string pname)
{
    int ind = pname.find("__ENA[");
    if (ind == -1)
        ind = pname.find("__RDY[");
    if (ind == -1)
        ind = pname.find("__ACK[");
    if (ind > 0)
        pname = pname.substr(0, ind) + pname.substr(ind + 5);
    if (endswith(pname, "__ENA") || endswith(pname, "__RDY") || endswith(pname, "__ACK"))
        pname = pname.substr(0, pname.length()-5);
    return pname;
}
std::string getRdyName(std::string basename, bool isAsync)
{
    std::string base = baseMethodName(basename), sub;
    if (endswith(base, "]")) {
        int ind = base.find("[");
        sub = base.substr(ind);
        base = base.substr(0, ind);
    }
    return base + (isAsync ? "__ACK" : "__RDY") + sub;
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
    std::string raname = getRdyName(name, true);
    return name == rname || name == raname;
}

bool isEnaName(std::string name)
{
    std::string rname = getEnaName(name);
    return name == rname;
}

void extractParam(std::string debugName, std::string replaceList, MapNameValue &mapValue)
{
    int count = 0;
    std::string origList = replaceList;
    int ind = replaceList.find("(");
    if (ind > 0) {
        ACCExpr *expr = cleanupModuleParam(replaceList.substr(ind));
        for (auto item: expr->operands) // list of '=' assignments
            mapValue[tree2str(item->operands.front(), false)] = tree2str(item->operands.back(), false);
    }
    if (trace_parameters)
        for (auto item: mapValue)
            printf("[%s:%d] %s[%s]: %d: %s = %s\n", __FUNCTION__, __LINE__, debugName.c_str(), origList.c_str(), count++, item.first.c_str(), item.second.c_str());
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
    }
    if (!IR && mapIndex.find(ind) != mapIndex.end())
        IR = mapIndex[ind];
    if (!IR && mapStripped.find(ind) != mapStripped.end())
        IR = mapStripped[ind];
    if (trace_IR)
        printf("[%s:%d]lookup %p name %s alt %s\n", __FUNCTION__, __LINE__, (void *)IR, name.c_str(), ind.c_str());
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
    }
    if (!IR && interfaceIndex.find(ind) != interfaceIndex.end())
        IR = interfaceIndex[ind];
    if (!IR && interfaceStripped.find(ind) != interfaceStripped.end())
        IR = interfaceStripped[ind];
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
        std::string newval;
        auto val = mapValue.find(rets);
        if (val != mapValue.end())
            newval = val->second;
        if (rets != "0" && rets != "1" && newval != "") { // don't translate trivial values
            if (trace_parameters)
                printf("%s: INSTANTIATE %s -> %s\n", __FUNCTION__, arg.c_str(), newval.c_str());
            rets = newval;
        }
        if (hasAtSign)
            rets = "@" + rets;
        return rets;
    };
    if (checkT("ARRAY_")) {
        std::string arr = bp;
        int ind = arr.find(ARRAY_ELEMENT_MARKER);
        if (ind > 0) {
            arr = arr.substr(0, ind);
            bp += ind+1;
        }
        arr = mapReturn(arr);
        std::string newtype = "ARRAY_" + arr + ARRAY_ELEMENT_MARKER + instantiateType(bp, mapValue);
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
    auto IR = lookupIR(bp);
    if (!IR)
        IR = lookupInterface(bp);
    if (IR)
        return mapReturn(genericModuleParam(bp, "", &mapValue));
    return mapReturn(bp);
}

typedef struct {
    std::string name;
    ACCExpr    *value;
} ParamMapType;
std::string genericModuleParam(std::string name, std::string param, MapNameValue *mapValue)
{
    std::string orig = name;
    std::list<ParamMapType> pmap;
    name = cleanupModuleType(name);
    std::string ret = name;
    int ind = name.find("(");
    if (ind > 0)
        ret = name.substr(0, ind);
    else
        name = param;
    ind = name.find("(");
    if (ind >= 0) {
        ACCExpr *param = cleanupModuleParam(name.substr(ind));
        for (auto item: param->operands) // list of '=' assignments
            pmap.push_back({tree2str(item->operands.front(), false), item->operands.back()});
        std::string sep;
        if (mapValue) {
            ret += "(";
            for (auto item: pmap) {
                ret += sep + item.name + "=";
                std::string oldVal = tree2str(item.value);
                std::string newval;
                auto val = mapValue->find(oldVal);
                if (val != mapValue->end())
                    newval = val->second;
                if (oldVal != "0" && oldVal != "1" && newval != "") { // don't translate trivial values
                    oldVal = newval;
                }
                for (auto c: oldVal)
                    if (!std::isalnum(c) && c != '(' && c != ')') {
                        oldVal = "(" + oldVal + ")";
                        break;
                    }
                ret += oldVal;
                sep = ",";
            }
            ret += ")";
        }
        else if (pmap.size()) {
            ret += "#(";
            for (auto item: pmap) {
                ret += sep + PERIOD + item.name + "(";
                ret += tree2str(item.value, false);
                ret += ")";
                sep = ",";
            }
            ret += ")";
        }
    }
    return ret;
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
        int ind = arr.find(ARRAY_ELEMENT_MARKER);
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
            if (total == "") {
                for (auto c: thisSize)
                    if (!std::isalnum(c)) {
                        thisSize = "(" + thisSize + ")";
                        break;
                    }
                total = thisSize;
            }
            else
                total = "(" + total + " + " + thisSize + ")";
        }
        return total;
    }
    if (lookupInterface(bp))
        return "0";
    if (*bp == '(')
        return bp;
    printf("[%s:%d] convertType FAILED '%s'\n", __FUNCTION__, __LINE__, bp);
    exit(-1);
}

std::string cleanupModuleType(std::string type)
{
    std::string ret = type, post;
    int ind = ret.find_first_of("#" "(" PERIOD);
    if (ind > 0) {
        post = ret.substr(ind);
        ret = ret.substr(0, ind);
    }
    ind = ret.find(SUFFIX_FOR_GENERIC);
    if (ind > 0)
        ret = ret.substr(0, ind);
    return ret + post;
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
    if (lookupIR(type) || lookupInterface(type))
        return type;
    return "logic " + sizeProcess(type);
}

MethodInfo *lookupMethod(ModuleIR *IR, std::string name)
{
    std::string nameENA = name + "__ENA";
    std::string nameShort = name;
    if (isEnaName(nameShort))
        nameShort = baseMethodName(nameShort);
    if (IR)
    for (auto MI : IR->methods) {
        if (MI->name == name || MI->name == nameENA || MI->name == nameShort)
            return MI;
    }
    return nullptr;
}

typedef struct {
    std::string type;
    std::string vecCount;
    std::string fieldType; // top level field element type (used for extracting mapValue)
} AccessibleInfo;

static std::map<std::string, AccessibleInfo> accessibleInterfaces; // map of 'interface local name' -> 'type'
void addAccessible(std::string interfaceType, std::string name, std::string vecCount, std::string fieldType, bool top)
{
    ModuleIR *IR = lookupInterface(interfaceType);
    if (!IR)
        return;
    std::string prefix = name;
    if (prefix != "")
        prefix += DOLLAR;
    for (auto item: IR->interfaces) {
        bool forceInterface = false;
        if (auto IIR = lookupInterface(item.type)) {
            if (IIR->methods.size() || IIR->interfaces.size())
                forceInterface = true;
            for (auto fitem: IIR->fields)
                if (!fitem.isInout)
                    forceInterface = true;
        }
        if (item.fldName != "" && forceInterface)
        addAccessible(item.type, prefix + item.fldName, vecCount, fieldType, false);
    }
    if (IR->methods.size() || IR->fields.size() || startswith(interfaceType, "PipeInSync")) {
        if (name[name.length()-1] == DOLLAR[0] || name[name.length()-1] == PERIOD[0])
            name = name.substr(0, name.length()-1);
        if (!top)
        accessibleInterfaces[name] = AccessibleInfo{interfaceType, vecCount, fieldType};
    }
}

std::string findAccessible(std::string aname)
{
    std::string name = replacePeriod(aname);
    std::string ret;
    for (auto item: accessibleInterfaces) {
        unsigned len = item.first.length();
        if (name.length() > len && startswith(name, item.first) && len > ret.length()
         && (name[len] == DOLLAR[0] || name[len] == '['))
            ret = item.first;
    }
    return ret;
}

void fixupAccessible(std::string &name)
{
    std::string orig = name;
    name = replacePeriod(name);
    if (int len = findAccessible(name).length()) {
        std::string interface = name.substr(0, len), sub1, sub2;
        name = name.substr(len);
        if (trace_interface)
            printf("[%s:%d] orig %s len = %d interface %s name %s pin %d count %d\n", __FUNCTION__, __LINE__, orig.c_str(), len, interface.c_str(), name.c_str(), refList[interface].pin, refList[interface].count);
        if (name[0] == '[') {
            int ind = name.find("]");
            sub1 = name.substr(0, ind+1);
            name = name.substr(ind+1);
        }
        if (name != "")
            name = name.substr(1);
        int ind = name.find("[");
        if (ind >= 0) {
            std::string first = name.substr(0, ind);
            name = name.substr(ind);
            ind = name.find("]");
            sub2 = name.substr(0, ind+1);
            name = first + name.substr(ind+1);
        }
        if (trace_interface)
            printf("[%s:%d] interface '%s' sub1 '%s' sub2 '%s' name '%s' pin %d count %d\n", __FUNCTION__, __LINE__, interface.c_str(), sub1.c_str(), sub2.c_str(), name.c_str(), refList[interface].pin, refList[interface].count);
        interface += sub1 + sub2;
        refList[interface].count++;
        if (interface != "" && name != "")
            name = PERIOD + name;
        name = interface + name;
    }
}

void normalizeIdentifier(ACCExpr *expr)
{
    if (expr->value == PERIOD) {
        std::string ret, sep;
        for (auto item: expr->operands) {
            if (!isIdChar(item->value[0]) || item->operands.size())
                return;
            ret += sep + item->value;
            sep = PERIOD;
        }
        expr->value = ret;
        expr->operands.clear();
    }
    fixupAccessible(expr->value);
}

void buildAccessible(ModuleIR *IR)
{
    accessibleInterfaces.clear();
    if (!IR->isVerilog)
        addAccessible(IR->interfaceName, "", "", "", true);
    for (auto item: IR->fields) {
//printf("[%s:%d] STRUCCUCUC %s type %s\n", __FUNCTION__, __LINE__, item.fldName.c_str(), item.type.c_str());
        if (auto IIR = lookupIR(item.type)) {
           if (!IR->isVerilog)
           addAccessible(IIR->interfaceName, item.fldName, item.vecCount, item.type, true);
//printf("[%s:%d] STRUCCUCUC %s str %d type %s\n", __FUNCTION__, __LINE__, item.fldName.c_str(), IIR->isStruct, item.type.c_str());
           if (IIR->isStruct)
               accessibleInterfaces[item.fldName] = AccessibleInfo{"", item.vecCount, item.type};
        }
    }
    if (!IR->isVerilog)
        for (auto item: IR->interfaces) {
            addAccessible(item.type, item.fldName, item.vecCount, item.type, false);
        }
    for (auto MI: IR->methods) {
        for (auto item: MI->alloca) {
            if (auto IIR = lookupIR(item.second.type))
            if (IIR->isStruct)
               accessibleInterfaces[item.first] = AccessibleInfo{item.second.type, "", ""};
        }
    }
    for (auto item: refList) {
        if (lookupIR(item.second.type) || lookupInterface(item.second.type))
            accessibleInterfaces[item.first] = AccessibleInfo{item.second.type, item.second.vecCount, ""};
    }
    if (trace_accessible || trace_interface) {
        printf("[%s:%d]LIST OF ACCESSIBLE INTERFACES\n", __FUNCTION__, __LINE__);
        for (auto item: accessibleInterfaces)
            printf("[%s:%d] %s %s\n", __FUNCTION__, __LINE__, item.first.c_str(), item.second.type.c_str());
    }
}

MethodInfo *lookupQualName(ModuleIR *searchIR, std::string searchStr, std::string &vecCount, MapNameValue &mapValue)
{
    if (traceLookup) {
        printf("[%s:%d] start searchIR %p search %s\n", __FUNCTION__, __LINE__, (void *)searchIR, searchStr.c_str());
        //dumpModule("SEARCHIR", searchIR);
    }
    int ind = searchStr.find("[");
    if (ind > 0) {
        std::string sub;
        extractSubscript(searchStr, ind, sub);
    }
    searchStr = replacePeriod(searchStr);
    std::string prefix = findAccessible(searchStr);
    if (prefix != "") {
        auto info = accessibleInterfaces[prefix];
        if (startswith(info.type, "PipeInSync"))
            info.type = "PipeIn"; // for now...
        searchIR = lookupInterface(info.type);
        vecCount = info.vecCount;
        extractParam("lookupQualName", info.fieldType, mapValue);
        searchStr = searchStr.substr(prefix.length() + 1);
        if (traceLookup)
            printf("[%s:%d] prefix %s info.type %s\n", __FUNCTION__, __LINE__, prefix.c_str(), info.type.c_str());
    }
    else {
        ind = searchStr.find(DOLLAR);
        if (ind > 0) {
            std::string fieldname = searchStr.substr(0, ind);
            if (traceLookup)
                printf("[%s:%d] fieldname %s\n", __FUNCTION__, __LINE__, fieldname.c_str());
            for (auto item: searchIR->fields)
                if (fieldname == item.fldName) {
                    auto IR = lookupIR(item.type);
                    if (IR) {
                        searchIR = lookupInterface(IR->interfaceName);
                        searchStr = searchStr.substr(ind+1);
                    }
                    if (traceLookup)
                        printf("[%s:%d] found field %s IR %p type %s\n", __FUNCTION__, __LINE__, fieldname.c_str(), (void *)searchIR, item.type.c_str());
                    goto finalLookup;
                }
            if (auto IIR = lookupInterface(searchIR->interfaceName))
            for (auto item: IIR->interfaces)
                if (fieldname == item.fldName) {
                    searchIR = lookupInterface(item.type);
                    searchStr = searchStr.substr(ind+1);
                    if (traceLookup)
                        printf("[%s:%d] found interface %s IR %p type %s\n", __FUNCTION__, __LINE__, fieldname.c_str(), (void *)searchIR, item.type.c_str());
                    goto finalLookup;
                }
        }
    }
finalLookup:;
    if (traceLookup) {
        printf("[%s:%d] searchIR %p search %s\n", __FUNCTION__, __LINE__, (void *)searchIR, searchStr.c_str());
        //dumpModule("SEARCHIR", searchIR);
    }
    return lookupMethod(searchIR, searchStr);
    return nullptr;
}

ModuleIR *allocIR(std::string name, bool isInterface)
{
    std::string iname = name;
    ModuleIR *IR = new ModuleIR{name/*name*/,
        {}/*metaList*/, {}/*softwareName*/, {}/*methods*/,
        {}/*generateBody*/, {}/*priority*/, {}/*fields*/,
        {}/*params*/, {}/*unionList*/, {}/*interfaces*/, {}/*parameters*/,
        {}/*overTable*/,
        {}/*interfaceConnect*/, 0/*genvarCount*/, ""/*isTrace*/, false/*isExt*/, isInterface,
        false/*isStruct*/, false/*isSerialize*/, false/*isVerilog*/, false/*isPrintf*/, false/*isTopModule*/, false/*transformGeneric*/,
        "" /*interfaceVecCount*/, "" /*interfaceName*/, ""/*sourceFilename*/};

    if (isInterface)
        interfaceIndex[iname] = IR;
    else
        mapIndex[iname] = IR;
    mapAllModule[iname] = IR;
    int i = iname.find("(");
    if (i > 0) {        // also insert without parameter instantiation
        iname = iname.substr(0,i);
        if (isInterface)
            interfaceStripped[iname] = IR;
        else
            mapStripped[iname] = IR;
    }
    if (trace_IR)
        printf("[%s:%d] allocIR %s alt %s\n", __FUNCTION__, __LINE__, name.c_str(), iname.c_str());
    return IR;
}

MethodInfo *allocMethod(std::string name)
{
    MethodInfo *MI = new MethodInfo{nullptr/*guard*/,
        nullptr/*subscript*/, ""/*generateSection*/,
        name/*name*/, false/*isRule*/, false/*action*/, false/*async*/,
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
    if (!MI)
        return;
    std::string methodName = MI->name;
    std::string attr;
    if (MI->isRule)
        attr += "/Rule";
    if (MI->action)
        attr += "/Action";
    if (MI->async)
        attr += "/Async";
    printf("%s    METHOD%s %s(", name.c_str(), attr.c_str(), methodName.c_str());
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
static int traceFields;//=1;
    int interfaceNumber = 0;
    if (!IR)
        return;
    if (traceFields)
        for (auto item: IR->fields)
             dumpModule(name + "_field", lookupIR(item.type));
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
    for (auto item: IR->parameters)
        printf("    PARAMETERS %s %s\n", item.type.c_str(), item.fldName.c_str());
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
        if (isalnum(S[i]) || S[i] == '$')
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
        else
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
    if (!expr || clen == "" //|| !isdigit(clen[0]) 
       || clen == "0" || clen.find(" ") != std::string::npos)
        return;
    if (isIdChar(expr->value[0]) && !expr->operands.size()) {
        if (refList[expr->value].pin == PIN_CONSTANT) {
            expr->value = "((" + clen + ")'(" + expr->value + "))";
            return;
        }
    }
    int len = atoi(clen.c_str());
    std::string cilen = exprWidth(expr);
    int ilen = atoi(cilen.c_str());
    if (ilen < 0 || len < 0) {
        printf("[%s:%d] len %d ilen %d tree %s\n", __FUNCTION__, __LINE__, len, ilen, tree2str(expr).c_str());
        exit(-1);
    }
    if ((!isdigit(clen[0]) || !isdigit(expr->value[0])) && plainInteger(expr->value))
        expr->value = "(" + clen + ") ' ('d" + expr->value + ")";
    else if (len > 0 && plainInteger(expr->value))
        expr->value = clen + "'d" + expr->value;
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
    else if (expr->value == "!" || expr->value == "^") {
        for (auto item: expr->operands)
            updateWidth(item, clen);
    }
    else if (expr->value == "||" || expr->value == "&&") {
        for (auto item: expr->operands)
            updateWidth(item, "1");
    }
    else if (relationalOp(expr->value)) {
        std::string width;
        bool performUpdate = true;
        for (auto item: expr->operands) {
            std::string val = tree2str(item);
            int pin = refList[val].pin;
            std::string size = exprWidth(item);
            if (pin != PIN_CONSTANT) {
                if (width == "")
                    width = size;
                if (width != size)
                    performUpdate = false;
            }
        }
        if (performUpdate && width != "")
        for (auto item: expr->operands) {
            if (width != exprWidth(item))
                updateWidth(item, width);
        }
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
    else if (expr->value == "__bitsize") {
        ACCExpr *list = expr->operands.front();
        assert(list->value == "(" && expr->operands.size() == 1);
        list = list->operands.front();
        expr->value = convertType(list->value);
        expr->operands.clear();
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
        if (bitem->value == PERIOD) {
            bitem->value = tree2str(bitem);
            bitem->operands.clear();
        }
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
        //std::string valSize;
        //bool firstSize = true;
        //for (auto item: list->operands) {
             //std::string size = exprWidth(getRHS(item));
             //if (firstSize)
                 //valSize = size;
             //else if (valSize != size)
                 //valSize = "";
             //firstSize = false;
        //}
        ACCExpr *firstInList = getRHS(list, 0), *secondInList = getRHS(list);
        ACCExpr *newe = nullptr;
        if (size == 2 && matchExpr(getRHS(firstInList, 0), invertExpr(getRHS(secondInList, 0))))
            newe = allocExpr("?", getRHS(firstInList, 0), getRHS(firstInList), getRHS(secondInList));
        else if (size == 2 && getRHS(firstInList, 0)->value == "__default" && exprWidth(getRHS(secondInList)) == "1")
            newe = cleanupBool(allocExpr("&&", getRHS(secondInList, 0), getRHS(secondInList)));
        else if (size == 1 && exprWidth(getRHS(firstInList)) == "1")
            newe = cleanupBool(allocExpr("&&", getRHS(firstInList, 0), getRHS(firstInList)));
        else if (size == 0)
            newe = allocExpr(phiDefault);
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

ACCExpr *cleanupExprBuiltin(ACCExpr *expr, std::string phiDefault, bool preserveParen)
{
    if (!expr)
        return expr;
    walkReplaceBuiltin(expr, phiDefault);
    return cleanupExpr(expr, preserveParen);
}

std::string makeSection(std::string var, ACCExpr *init, ACCExpr *limit, ACCExpr *incr)
{
    char tempBuf[1000];
    snprintf(tempBuf, sizeof(tempBuf), "for(genvar %s = %s; %s; %s = %s) begin", // genvar not allowed for loops inside procedural blocks
        var.c_str(), tree2str(init).c_str(), tree2str(limit).c_str(), var.c_str(), tree2str(incr).c_str());
    return tempBuf;
}

std::string getBoundedString(CCharPointer *bufpp, char terminator)
{
    const char *bufp = *bufpp;
    const char *startp = bufp;
    int level = 0, levelb = 0, levelc = 0;
    bool inQuote = false, beforeParen = true;
    while (*bufp && ((terminator ? (*bufp != terminator) : beforeParen) || level != 0 || levelb != 0 || levelc != 0)) {
        if (inQuote) {
            if (*bufp == '"')
                inQuote = false;
            if (*bufp == '\\')
                bufp++;
        }
        else {
        if (*bufp == '"')
            inQuote = true;
        else if (*bufp == '(') {
            level++;
            beforeParen = false;
        }
        else if (*bufp == ')')
            level--;
        else if (*bufp == '{') {
            levelb++;
            beforeParen = false;
        }
        else if (*bufp == '}')
            levelb--;
        else if (*bufp == '[') {
            levelc++;
            beforeParen = false;
        }
        else if (*bufp == ']')
            levelc--;
        }
        bufp++;
    }
    *bufpp = bufp;
    return std::string(startp, bufp - startp);
}

void extractSubscript(std::string &source, int index, std::string &sub)
{
    const char *bufp = source.c_str() + index;
    sub = getBoundedString(&bufp);
    source = source.substr(0, index) + source.substr(index + sub.length());
}
