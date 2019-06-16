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

int trace_assign;//= 1;
int trace_declare;//= 1;
int trace_connect;//= 1;
int trace_skipped;//= 1;

std::list<PrintfInfo> printfFormat;
std::list<ModData> modNew;
std::map<std::string, CondGroup> condLines;
typedef std::map<std::string, ACCExpr *> NamedExprList;

std::map<std::string, AssignItem> assignList;

static void setAssign(std::string target, ACCExpr *value, std::string type)
{
    bool tDir = refList[target].out;
    if (!value)
        return;
    if (type == "Bit(1)")
        value = cleanupBool(value);
    if (trace_assign)
        printf("[%s:%d] start [%s/%d] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tDir, tree2str(value).c_str(), type.c_str());
    if (!refList[target].pin) {
        printf("[%s:%d] missing target [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        exit(-1);
    }
    updateWidth(value, convertType(type));
    if (isIdChar(value->value[0])) {
        bool sDir = refList[value->value].out;
        if (trace_assign)
        printf("[%s:%d] %s/%d = %s/%d\n", __FUNCTION__, __LINE__, target.c_str(), tDir, value->value.c_str(), sDir);
    }
    if (assignList[target].type != "" && assignList[target].value->value != "{") { // aggregate alloca items always start with an expansion expr
//if (trace_assign) {
        printf("[%s:%d] duplicate start [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        printf("[%s:%d] duplicate was      = %s type '%s'\n", __FUNCTION__, __LINE__, tree2str(assignList[target].value).c_str(), assignList[target].type.c_str());
//}
        exit(-1);
    }
    assignList[target] = AssignItem{value, type, false};
}

static void expandStruct(ModuleIR *IR, std::string fldName, std::string type, int out, bool inout, bool force, int pin, bool assign = true, std::string vecCount = "")
{
    ACCExpr *itemList = allocExpr("{");
    std::list<FieldItem> fieldList;
    getFieldList(fieldList, fldName, "", type, out != 0, force);
    for (auto fitem : fieldList) {
        uint64_t offset = fitem.offset;
        uint64_t uppern = offset - 1;
        std::string upper = autostr(uppern);
        if (fitem.type[0] == '@')
            upper = fitem.type.substr(1) + "+ (" + upper + ")";
        else if (upper != " ")
            upper = "(" + upper + " + " + convertType(fitem.type) + ")";
        else
            upper = convertType(fitem.type);
        std::string base = fldName;
        if (fitem.base != "")
            base = fitem.base;
        std::string fnew = base + "[" + upper + ":" + autostr(offset) + "]";
        ACCExpr *fexpr = allocExpr(base, allocExpr("[", allocExpr(":", allocExpr(upper), allocExpr(autostr(offset)))));
if (trace_expand || refList[fitem.name].pin)
printf("[%s:%d] set %s = %s out %d alias %d base %s , %s[%d : %s] fnew %s pin %d fnew %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), fitem.type.c_str(), out, fitem.alias, base.c_str(), fldName.c_str(), (int)offset, upper.c_str(), fnew.c_str(), refList[fitem.name].pin, tree2str(fexpr).c_str());
        assert (!refList[fitem.name].pin);
        refList[fitem.name] = RefItem{0, fitem.type, out != 0, false, fitem.alias ? PIN_WIRE : pin, false, false, vecCount};
        refList[fnew] = RefItem{0, fitem.type, out != 0, false, PIN_ALIAS, false, false, vecCount};
        if (trace_declare)
            printf("[%s:%d]NEWREF %s %s type %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), fnew.c_str(), fitem.type.c_str());
        if (!fitem.alias && out)
            itemList->operands.push_front(allocExpr(fitem.name));
        else if (assign)
            setAssign(fitem.name, fexpr, fitem.type);
    }
    if (force) {
        refList[fldName] = RefItem{0, type, true, false, PIN_WIRE, false, false, vecCount};
        if (trace_declare)
            printf("[%s:%d]NEWREF2 %s type %s\n", __FUNCTION__, __LINE__, fldName.c_str(), type.c_str());
    }
    if (itemList->operands.size() > 0 && assign)
        setAssign(fldName, itemList, type);
}

static void addRead(MetaSet &list, ACCExpr *cond)
{
    if(isIdChar(cond->value[0]))
        list.insert(cond->value);
    for (auto item: cond->operands)
        addRead(list, item);
}

static void walkRead (MethodInfo *MI, ACCExpr *expr, ACCExpr *cond)
{
    if (!expr)
        return;
    for (auto item: expr->operands)
        walkRead(MI, item, cond);
    std::string fieldName = expr->value;
    if (isIdChar(fieldName[0]) && cond && (!expr->operands.size() || expr->operands.front()->value != PARAMETER_MARKER))
        addRead(MI->meta[MetaRead][fieldName], cond);
}

typedef struct {
    std::string type, name;
    bool        isOutput, isInout, isLocal;
    MethodInfo *MI;
    std::string init; /* for parameters */
} PinInfo;
std::list<PinInfo> pinPorts, pinMethods, paramPorts;
static void collectInterfacePins(ModuleIR *IR, bool instance, std::string pinPrefix, std::string methodPrefix, bool isLocal)
{
    for (auto item : IR->interfaces) {
        std::string interfaceName = item.fldName;
        ModuleIR *IIR = lookupInterface(item.type);
        if (!IIR) {
            printf("%s: in module '%s', interface lookup '%s' name '%s' failed\n", __FUNCTION__, IR->name.c_str(), item.type.c_str(), interfaceName.c_str());
            exit(-1);
        }
        if (interfaceName != "")
            interfaceName += MODULE_SEPARATOR;
        for (auto MI: IIR->methods) {
            std::string name = methodPrefix + interfaceName + MI->name;
            bool out = instance ^ item.isPtr;
            pinMethods.push_back(PinInfo{MI->type, name, out, false, isLocal || item.isLocalInterface, MI, ""/*not param*/});
        }
        for (auto fld: IIR->fields) {
            std::string name = pinPrefix + item.fldName + fld.fldName;
            bool out = instance ^ fld.isOutput;
            if (fld.isParameter != "") {
                std::string init = fld.isParameter;
                if (init == " ") {
                    init = "\"FALSE\"";
                    if (fld.type == "FLOAT")
                        init = "0.0";
                    else if (fld.isPtr)
                        init = "0";
                    else if (startswith(fld.type, "Bit("))
                        init = "0";
                }
                paramPorts.push_back(PinInfo{fld.isPtr ? "POINTER" : fld.type, name, out, fld.isInout, isLocal || item.isLocalInterface, nullptr, init});
            }
            else
                pinPorts.push_back(PinInfo{fld.type, name, out, fld.isInout, isLocal || item.isLocalInterface, nullptr, ""/*not param*/});
        }
        collectInterfacePins(IIR, instance, pinPrefix + item.fldName,
            methodPrefix + interfaceName, isLocal || item.isLocalInterface);
    }
}

/*
 * Generate verilog module header for class definition or reference
 */
static void generateModuleSignature(ModuleIR *IR, std::string instance, std::list<ModData> &modParam, std::string params,
bool dontDeclare = false, std::string vecCount = "", int dimIndex = 0)
{
    std::string minst;
    if (instance != "")
        minst = instance.substr(0, instance.length()-1);
    auto checkWire = [&](std::string name, std::string type, int dir, bool inout, std::string isparam, bool isLocal) -> void {
        int refPin = instance != "" ? PIN_OBJECT: (isLocal ? PIN_LOCAL: PIN_MODULE);
        std::string instName = instance + name;
        if (vecCount != "") {
            if (dontDeclare && dimIndex != -1)
                instName = minst + "[" + autostr(dimIndex) + "]." + name;
            else
                instName = name;
        }
        if (!isLocal || instance == "" || dontDeclare)
        refList[instName] = RefItem{((dir != 0 || inout) && instance == "") || dontDeclare, type, dir != 0, inout, refPin, false, dontDeclare, ""};
        if (!isLocal && !dontDeclare)
        modParam.push_back(ModData{name, instName, type, false, false, dir, inout, isparam, vecCount});
        if (trace_connect)
            printf("[%s:%d] iName %s name %s type %s dir %d io %d ispar '%s' isLoc %d dDecl %d vec '%s' dim %d\n", __FUNCTION__, __LINE__, instName.c_str(), name.c_str(), type.c_str(), dir, inout, isparam.c_str(), isLocal, dontDeclare, vecCount.c_str(), dimIndex);
        if (isparam == "")
        expandStruct(IR, instName, type, dir, inout, false, PIN_WIRE);
    };
//printf("[%s:%d] name %s instance %s\n", __FUNCTION__, __LINE__, IR->name.c_str(), instance.c_str());
    pinPorts.clear();
    pinMethods.clear();
    paramPorts.clear();
    collectInterfacePins(IR, instance != "", "", "", false);
    std::string moduleInstantiation = IR->name;
    if (instance != "") {
        std::string genericMName = genericName(IR->name);
        if (genericMName != "")
            moduleInstantiation = genericMName + "#(" + genericModuleParam(IR->name) + ")";
//printf("[%s:%d] instance %s params %s\n", __FUNCTION__, __LINE__, instance.substr(0,instance.length()-1).c_str(), params.c_str());
        if (params != "") {
            std::string actual, sep;
            const char *p = params.c_str();
            p++;
            char ch = ';';
            while (ch == ';') {
                const char *start = p;
                while (*p++ != ':')
                    ;
                std::string name(start, p-1);
                start = p;
                while ((ch = *p++) && ch != ';' && ch != '>')
                    ;
                actual += sep + "." + name + "(" + std::string(start, p-1) + ")";
                sep = ",";
            }
            moduleInstantiation += "#(" + actual + ")";
        }
    }
    if (!dontDeclare)
        modParam.push_back(ModData{minst, moduleInstantiation, "", true, pinPorts.size() > 0, 0, false, ""/*not param*/, vecCount});
    if (instance == "")
        for (auto item: paramPorts)
            checkWire(item.name, item.type, item.isOutput, item.isInout, item.init, item.isLocal);
    for (auto item: pinMethods) {
        checkWire(item.name, item.type, item.isOutput ^ (item.type != ""), false, ""/*not param*/, item.isLocal);
        if (trace_connect)
            printf("[%s:%d] instance %s name '%s' type %s\n", __FUNCTION__, __LINE__, instance.c_str(), item.name.c_str(), item.type.c_str());
        for (auto pitem: item.MI->params)
            checkWire(item.name.substr(0, item.name.length()-5) + MODULE_SEPARATOR + pitem.name, pitem.type, item.isOutput, false, ""/*not param*/, item.isLocal);
    }
    for (auto item: pinPorts)
        checkWire(item.name, item.type, item.isOutput, item.isInout, ""/*not param*/, item.isLocal);
    if (instance != "")
        return;
    bool hasCLK = false, hasnRST = false;
    bool handleCLK = !pinPorts.size();
    for (auto mitem: modParam)
        if (!mitem.moduleStart && mitem.isparam == "") {
            if (handleCLK) {
                hasCLK = true;
                hasnRST = true;
                handleCLK = false;
            }
            if (mitem.value == "CLK")
                hasCLK = true;
            if (mitem.value == "nRST")
                hasnRST = true;
        }
    if (!handleCLK && !hasCLK && vecCount == "")
        refList["CLK"] = RefItem{1, "Bit(1)", false, false, PIN_LOCAL, false, dontDeclare, ""};
    if (!handleCLK && !hasnRST && vecCount == "")
        refList["nRST"] = RefItem{1, "Bit(1)", false, false, PIN_LOCAL, false, dontDeclare, ""};
}

static ACCExpr *walkRemoveParam (ACCExpr *expr)
{
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    if (isIdChar(item[0])) {
        int pin = refList[item].pin;
        if (pin != PIN_OBJECT && pin != PIN_REG) {
//if (trace_assign)
printf("[%s:%d] reject use of non-state item %s %d\n", __FUNCTION__, __LINE__, item.c_str(), pin);
            return nullptr;
        }
        //assert(refList[item].pin);
    }
    for (auto item: expr->operands)
        if (ACCExpr *operand = walkRemoveParam(item))
            newExpr->operands.push_back(operand);
    return newExpr;
}

static void walkRef (ACCExpr *expr)
{
    if (!expr)
        return;
    std::string item = expr->value;
    if (isIdChar(item[0])) {
        std::string base = item;
        int ind = base.find("[");
        if (ind > 0)
            base = base.substr(0, ind);
        if (!refList[item].pin)
            printf("[%s:%d] refList[%s] definition missing\n", __FUNCTION__, __LINE__, item.c_str());
        if (base != item)
{
if (trace_assign)
printf("[%s:%d] RRRRREFFFF %s -> %s\n", __FUNCTION__, __LINE__, expr->value.c_str(), item.c_str());
item = base;
}
        if(!refList[item].pin) {
            printf("[%s:%d] pin not found '%s'\n", __FUNCTION__, __LINE__, item.c_str());
            //exit(-1);
        }
        refList[item].count++;
        ACCExpr *temp = assignList[item].value;
        if (temp && refList[item].count == 1)
            walkRef(temp);
    }
    for (auto item: expr->operands)
        walkRef(item);
}

static void decRef(std::string name)
{
//return;
    if (refList[name].count > 0 && refList[name].pin != PIN_MODULE)
        refList[name].count--;
}

static ACCExpr *replaceAssign (ACCExpr *expr, std::string guardName = "")
{
    if (!expr)
        return expr;
    if (trace_assign)
    printf("[%s:%d] start %s expr %s\n", __FUNCTION__, __LINE__, guardName.c_str(), tree2str(expr).c_str());
    std::string item = expr->value;
    if (isIdChar(item[0]) && !expr->operands.size()) {
        if (item == guardName) {
            if (trace_assign)
            printf("[%s:%d] remove guard of called method from enable line %s\n", __FUNCTION__, __LINE__, item.c_str());
            return allocExpr("1");
        }
        if (ACCExpr *assignValue = assignList[item].value)
        if (!assignList[item].noRecursion && (assignValue->value == "{" || walkCount(assignValue) < ASSIGN_SIZE_LIMIT)) {
        decRef(item);
        walkRef(assignValue);
        if (trace_assign)
            printf("[%s:%d] replace %s norec %d with %s\n", __FUNCTION__, __LINE__, item.c_str(), assignList[item].noRecursion, tree2str(assignValue).c_str());
        return replaceAssign(assignValue, guardName);
        }
    }
    ACCExpr *newExpr = allocExpr(expr->value);
    for (auto item: expr->operands)
        newExpr->operands.push_back(replaceAssign(item, guardName));
    newExpr = cleanupExpr(newExpr, true);
    if (trace_assign)
    printf("[%s:%d] end %s expr %s\n", __FUNCTION__, __LINE__, guardName.c_str(), tree2str(newExpr).c_str());
    return newExpr;
}

static void optimizeBitAssigns(void)
{
    std::map<std::string, std::map<uint64_t, BitfieldPart>> bitfieldList;
    // gather bitfield assigns together
    for (auto item: assignList) {
        int ind = item.first.find('[');
        uint64_t lower;
        if (ind != -1) {
            lower = atol(item.first.substr(ind+1).c_str());
            std::string temp = item.first.substr(0, ind);
            ind = item.first.find(':');
            if (ind != -1 && item.second.value)
                bitfieldList[temp][lower] = BitfieldPart{
                    atol(item.first.substr(ind+1).c_str()), item.second.value, item.second.type};
        }
    }
    // concatenate bitfield assigns
    for (auto item: bitfieldList) {
        std::string type = refList[item.first].type;
        std::string size = convertType(type);
        uint64_t current = 0;
printf("[%s:%d] BBBSTART %s type %s\n", __FUNCTION__, __LINE__, item.first.c_str(), type.c_str());
        ACCExpr *newVal = allocExpr("{");
        for (auto bitem: item.second) {
            uint64_t diff = bitem.first - current;
            if (diff > 0)
                newVal->operands.push_back(allocExpr(autostr(diff) + "'d0"));
            newVal->operands.push_back(bitem.second.value);
            current = bitem.second.upper + 1;
printf("[%s:%d] BBB lower %d upper %d val %s\n", __FUNCTION__, __LINE__, (int)bitem.first, (int)bitem.second.upper, tree2str(bitem.second.value).c_str());
            assignList[item.first + "[" + autostr(bitem.second.upper) + ":" + autostr(bitem.first) + "]"].value = nullptr;
        }
        size = "(" + size + " - " + autostr(current) + ")";
        if (isdigit(size[0]))
            newVal->operands.push_back(allocExpr(size + "'d0"));
        setAssign(item.first, newVal, type);
    }
}

static void setAssignRefCount(ModuleIR *IR)
{
    for (auto tcond = condLines.begin(), tend = condLines.end(); tcond != tend; tcond++) {
        std::string methodName = tcond->first;
        walkRef(tcond->second.guard);
        tcond->second.guard = cleanupBool(replaceAssign(tcond->second.guard));
        if (trace_assign)
            printf("[%s:%d] %s: guard %s\n", __FUNCTION__, __LINE__, methodName.c_str(), tree2str(tcond->second.guard).c_str());
        for (auto item: tcond->second.info) {
            walkRef(item.first);
            for (auto citem: item.second) {
                if (trace_assign)
                    printf("[%s:%d] %s: %s dest %s value %s\n", __FUNCTION__, __LINE__, methodName.c_str(),
 tree2str(item.first).c_str(), tree2str(citem.dest).c_str(), tree2str(citem.value).c_str());
                if (citem.dest) {
                    walkRef(citem.value);
                    walkRef(citem.dest);
                }
                else
                    walkRef(citem.value->operands.front());
            }
        }
    }

    for (auto item: assignList)
        if (item.second.value && refList[item.first].pin == PIN_OBJECT)
            refList[item.first].count++;

    // Now extend 'was referenced' from assignList items actually referenced
    for (auto aitem: assignList) {
        std::string temp = aitem.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if (aitem.second.value && (refList[aitem.first].count || refList[temp].count))
            walkRef(aitem.second.value);
    }
    if (trace_assign)
    for (auto aitem: assignList) {
        std::string temp = aitem.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if (aitem.second.value)
            printf("[%s:%d] ASSIGN %s = %s size %d count %d[%d] pin %d type %s\n", __FUNCTION__, __LINE__, aitem.first.c_str(), tree2str(aitem.second.value).c_str(), walkCount(aitem.second.value), refList[aitem.first].count, refList[temp].count, refList[temp].pin, refList[temp].type.c_str());
    }
    for (auto item: refList)
        if (item.second.count) {
         std::string type = findType(item.first);
        }
}

static void walkCSE (ACCExpr *expr)
{
    if (!expr)
        return;
    if (0)
    if (walkCount(expr) > 3)
    for (auto item: assignList) {
        if (item.second.value != expr && matchExpr(expr, item.second.value)) {
printf("[%s:%d]MMMMMMMAAAAAAAAAAAATTTTTTTCCCCCCCCHHHHHHHH %s\n", __FUNCTION__, __LINE__, item.first.c_str());
            expr->value = item.first;
            expr->operands.clear();
            refList[item.first].count++;
        }
    }
    for (auto item: expr->operands)
        walkCSE(item);
}

static void collectCSE(void)
{
//printf("[%s:%d] START\n", __FUNCTION__, __LINE__);
    for (auto item = assignList.begin(), aend = assignList.end(); item != aend; item++) {
         walkCSE(item->second.value);
    }
//printf("[%s:%d] END\n", __FUNCTION__, __LINE__);
}

static int printfNumber = 1;
#define PRINTF_PORT 0x7fff
//////////////////HACKHACK /////////////////
#define IfcNames_EchoIndicationH2S 5
#define PORTALNUM IfcNames_EchoIndicationH2S
//////////////////HACKHACK /////////////////
static ACCExpr *printfArgs(ACCExpr *listp)
{
    int pipeArgSize = 128;
    ACCExpr *fitem = listp->operands.front();
    listp->operands.pop_front();
    std::string format = fitem->value;
    std::list<int> width;
    unsigned index = 0;
    ACCExpr *next = allocExpr("{");
    int total_length = 0;
    for (auto item: listp->operands) {
        while (format[index] != '%' && index < format.length())
            index++;
        std::string val = item->value;
        if (index < format.length()-1) {
            if (format[index + 1] == 's' && val[0] == '"') {
                val = val.substr(1, val.length()-2);
                format = format.substr(0, index) + val + format.substr(index + 2);
                index += val.length();
                continue;
            }
            if (format[index + 1] == 'd' && isdigit(val[0])) {
                format = format.substr(0, index) + val + format.substr(index + 2);
                index += val.length();
                continue;
            }
        }
        int size = atoi(exprWidth(item).c_str());
        total_length += size;
        width.push_back(size);
        next->operands.push_back(item);
    }
    next->operands.push_back(allocExpr("16'd" + autostr(printfNumber++)));
    next->operands.push_back(allocExpr("16'd" + autostr(PRINTF_PORT)));
    next->operands.push_back(allocExpr("16'd" + autostr(PORTALNUM)));
    total_length += 3 * 16;
    listp->operands.clear();
    listp->operands = next->operands;
    if (pipeArgSize > total_length)
        next->operands.push_front(allocExpr(autostr(pipeArgSize - total_length) + "'d0"));
    printfFormat.push_back(PrintfInfo{format, width});
    ACCExpr *ret = allocExpr("printfp$enq__ENA", allocExpr(PARAMETER_MARKER, next,
        allocExpr("16'd" + autostr((total_length + 31)/32))));
    return ret;
}

static bool checkRecursion(ACCExpr *expr)
{
    std::string item = expr->value;
    if (isIdChar(item[0]) && !expr->operands.size()) {
        if (replaceBlock[item]) // flag multiple expansions of an item
            return false;
        ACCExpr *res = assignList[item].value;
        if (res && !assignList[item].noRecursion) {
            replaceBlock[item] = tree2str(res).length();
            if (!checkRecursion(res))
                return false;
        }
    }
    for (auto item: expr->operands)
        if (!checkRecursion(item))
            return false;
    return true;
}

static ACCExpr *simpleReplace (ACCExpr *expr)
{
    if (!expr)
        return expr;
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    if (isIdChar(item[0]) && !expr->operands.size()) {
        if (ACCExpr *assignValue = assignList[item].value)
        if (refList[item].pin != PIN_MODULE && !assignList[item].noRecursion
             && (checkOperand(assignValue->value) || endswith(item, "__RDY") || endswith(item, "__ENA"))) {
            if (trace_assign)
            printf("[%s:%d] replace %s with %s\n", __FUNCTION__, __LINE__, item.c_str(), tree2str(assignValue).c_str());
            decRef(item);
            return simpleReplace(assignValue);
        }
    }
    for (auto item: expr->operands)
        newExpr->operands.push_back(simpleReplace(item));
    return newExpr;
}

static void appendLine(std::string methodName, ACCExpr *cond, ACCExpr *dest, ACCExpr *value)
{
    dest = replaceAssign(dest);
    value = replaceAssign(value);
    for (auto CI = condLines[methodName].info.begin(), CE = condLines[methodName].info.end(); CI != CE; CI++)
        if (matchExpr(cond, CI->first)) {
            CI->second.push_back(CondInfo{dest, value});
            return;
        }
    condLines[methodName].guard = cleanupBool(allocExpr("&", allocExpr(methodName), allocExpr(getRdyName(methodName))));
    condLines[methodName].info[cond].push_back(CondInfo{dest, value});
}

static void connectInterfaces(ModuleIR *IR)
{
    for (auto IC : IR->interfaceConnect) {
        ModuleIR *IIR = lookupInterface(IC.type);
        if (!IIR)
            dumpModule("MISSINGCONNECT", IR);
        assert(IIR && "interfaceConnect interface type");
        bool targetLocal = false, sourceLocal = false;
        for (auto item: IR->interfaces) {
            if (item.fldName == IC.target)
                targetLocal = true;
            if (item.fldName == IC.source)
                sourceLocal = true;
        }
        if (trace_connect)
            printf("%s: CONNECT target %s/%d source %s/%d forward %d\n", __FUNCTION__, IC.target.c_str(), targetLocal, IC.source.c_str(), sourceLocal, IC.isForward);
        for (auto fld : IIR->fields) {
            std::string tstr = IC.target + fld.fldName,
                        sstr = IC.source + fld.fldName;
            if (trace_connect || (!refList[tstr].out && !refList[sstr].out))
                printf("%s: IFCCCfield %s/%d %s/%d\n", __FUNCTION__, tstr.c_str(), refList[tstr].out, sstr.c_str(), refList[sstr].out);
            if (!IC.isForward) {
                refList[tstr].out = 1;   // for local connections, don't bias for 'output'
                refList[sstr].out = 0;
            }
            if (refList[sstr].out)
                setAssign(sstr, allocExpr(tstr), fld.type);
            else
                setAssign(tstr, allocExpr(sstr), fld.type);
        }
        for (auto MI : IIR->methods) {
            if (trace_connect)
                printf("[%s:%d] ICtarget %s '%s' ICsource %s\n", __FUNCTION__, __LINE__, IC.target.c_str(), IC.target.substr(IC.target.length()-1).c_str(), IC.source.c_str());
            if (IC.target.substr(IC.target.length()-1) == MODULE_SEPARATOR)
                IC.target = IC.target.substr(0, IC.target.length()-1);
            if (IC.source.substr(IC.source.length()-1) == MODULE_SEPARATOR)
                IC.source = IC.source.substr(0, IC.source.length()-1);
            std::string tstr = IC.target + MODULE_SEPARATOR + MI->name,
                        sstr = IC.source + MODULE_SEPARATOR + MI->name;
            if (!IC.isForward) {
                refList[tstr].out ^= targetLocal;
                refList[sstr].out ^= sourceLocal;
            }
            if (trace_connect || (!refList[tstr].out && !refList[sstr].out))
                printf("%s: IFCCCmeth %s/%d %s/%d\n", __FUNCTION__, tstr.c_str(), refList[tstr].out, sstr.c_str(), refList[sstr].out);
            if (refList[sstr].out)
                setAssign(sstr, allocExpr(tstr), MI->type);
            else
                setAssign(tstr, allocExpr(sstr), MI->type);
            tstr = tstr.substr(0, tstr.length()-5) + MODULE_SEPARATOR;
            sstr = sstr.substr(0, sstr.length()-5) + MODULE_SEPARATOR;
            for (auto info: MI->params) {
                std::string sparm = sstr + info.name, tparm = tstr + info.name;
                if (!IC.isForward) {
                    refList[tparm].out ^= targetLocal;
                    refList[sparm].out ^= sourceLocal;
                }
                if (trace_connect)
                    printf("%s: IFCCCparam %s/%d %s/%d\n", __FUNCTION__, tparm.c_str(), refList[tparm].out, sparm.c_str(), refList[sparm].out);
                if (refList[sparm].out)
                    setAssign(sparm, allocExpr(tparm), info.type);
                else
                    setAssign(tparm, allocExpr(sparm), info.type);
            }
        }
    }
}

void appendMux(std::string name, ACCExpr *cond, ACCExpr *value, NamedExprList &muxValueList)
{
    ACCExpr *args = muxValueList[name];
    if (!args) {
        args = allocExpr("(");
        muxValueList[name] = args;
    }
    args->operands.push_back(allocExpr(":", cond, value));
}

static void generateMethod(ModuleIR *IR, MethodInfo *MI, NamedExprList &enableList, NamedExprList &muxValueList)
{
    std::string methodName = MI->name;
    MI->guard = cleanupBool(MI->guard);
    if (!endswith(methodName, "__RDY")) {
        walkRead(MI, MI->guard, nullptr);
        if (MI->rule)
            setAssign(methodName, allocExpr(getRdyName(methodName)), "Bit(1)");
    }
    setAssign(methodName, MI->guard, MI->type); // collect the text of the return value into a single 'assign'
    for (auto info: MI->storeList) {
        walkRead(MI, info->cond, nullptr);
        walkRead(MI, info->value, info->cond);
    }
    for (auto info: MI->letList) {
        ACCExpr *cond = cleanupBool(allocExpr("&", allocExpr(getRdyName(methodName)), info->cond));
        ACCExpr *value = info->value;
        updateWidth(value, convertType(info->type));
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        std::string dest = tree2str(info->dest);
        std::list<FieldItem> fieldList;
        getFieldList(fieldList, dest, "", info->type, 1, true);
        if (fieldList.size() == 1) {
            std::string first = fieldList.front().name;
            appendMux(dest, cond, value, muxValueList);
            if (first != dest)
                appendMux(first, cond, value, muxValueList);
        }
        else {
        std::string splitItem = tree2str(value);
        if ((value->operands.size() || !isIdChar(value->value[0]))) {
            splitItem = dest + "$lettemp";
            appendMux(splitItem, cond, value, muxValueList);
        }
        for (auto fitem : fieldList) {
            std::string offset = autostr(fitem.offset);
            std::string upper;
            if (fitem.type[0] == '@')
                upper = fitem.type.substr(1);
            else
                upper = convertType(fitem.type);
            upper += " - 1";
            if (offset != "0")
                upper += " + " + offset;
            appendMux(fitem.name, cond,
                allocExpr(splitItem, allocExpr("[", allocExpr(":", allocExpr(upper), allocExpr(offset)))), muxValueList);
        }
        }
    }
    for (auto info: MI->callList) {
        ACCExpr *cond = info->cond;
        ACCExpr *value = info->value;
        if (isIdChar(value->value[0]) && value->operands.size() && value->operands.front()->value == PARAMETER_MARKER)
            MI->meta[MetaInvoke][value->value].insert(tree2str(cond));
        else {
            printf("[%s:%d] called method name not found %s\n", __FUNCTION__, __LINE__, tree2str(value).c_str());
dumpExpr("READCALL", value);
                exit(-1);
        }
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        if (!info->isAction)
            continue;
        ACCExpr *tempCond = cleanupBool(allocExpr("&", allocExpr(methodName), allocExpr(getRdyName(methodName)), cond));
        std::string calledName = value->value;
//printf("[%s:%d] CALLLLLL '%s' condition %s\n", __FUNCTION__, __LINE__, calledName.c_str(), tree2str(tempCond).c_str());
//dumpExpr("CALLCOND", tempCond);
        if (!value->operands.size() || value->operands.front()->value != PARAMETER_MARKER) {
            printf("[%s:%d] incorrectly formed call expression\n", __FUNCTION__, __LINE__);
            exit(-1);
        }
        if (!enableList[calledName])
            enableList[calledName] = allocExpr("|");
        enableList[calledName]->operands.push_back(tempCond);
        MethodInfo *CI = lookupQualName(IR, calledName);
        if (!CI) {
            printf("[%s:%d] method %s not found\n", __FUNCTION__, __LINE__, calledName.c_str());
            exit(-1);
        }
        auto AI = CI->params.begin();
        std::string pname = calledName.substr(0, calledName.length()-5) + MODULE_SEPARATOR;
        int argCount = CI->params.size();
        ACCExpr *param = value->operands.front();
        for (auto item: param->operands) {
            if(argCount-- > 0) {
                appendMux(pname + AI->name, tempCond, item, muxValueList);
                AI++;
            }
        }
    }
}

/*
 * Generate *.v and *.vh for a Verilog module
 */
void generateModuleDef(ModuleIR *IR, std::list<ModData> &modLineTop)
{
static std::list<ModData> modLine;
    refList.clear();
    assignList.clear();
    modNew.clear();
    condLines.clear();
    generateModuleSignature(IR, "", modLineTop, "");
    bool hasPrintf = false;
    NamedExprList enableList;
    // 'Mux' together parameter settings from all invocations of a method from this class
    NamedExprList muxValueList;

    modLine.clear();
    printf("[%s:%d] STARTMODULE %s\n", __FUNCTION__, __LINE__, IR->name.c_str());

    for (auto item: IR->interfaces)
        if (item.fldName == "printfp")
            hasPrintf = true;

    iterField(IR, CBAct {
        int dimIndex = 0;
        std::string vecCount = item.vecCount;
        if (vecCount != "") {
            std::string fldName = item.fldName;
            ModuleIR *itemIR = lookupIR(item.type);
            if (itemIR && !item.isPtr) {
            if (itemIR->isStruct)
                expandStruct(IR, fldName, item.type, 1, false, true, item.isShared ? PIN_WIRE : PIN_REG, true, vecCount);
            else
                generateModuleSignature(itemIR, fldName + MODULE_SEPARATOR, modLine, IR->params[fldName], false, vecCount, -1);
            }
        }
        std::string pvec;
        do {
            std::string fldName = item.fldName;
            ModuleIR *itemIR = lookupIR(item.type);
            if (itemIR && !item.isPtr) {
            if (itemIR->isStruct) {
                if (vecCount == "")
                    expandStruct(IR, fldName, item.type, 1, false, true, item.isShared ? PIN_WIRE : PIN_REG);
            }
            else
                generateModuleSignature(itemIR, fldName + MODULE_SEPARATOR, modLine, IR->params[fldName], vecCount != "", vecCount, dimIndex++);
            }
            else { // if (convertType(item.type) != 0)
                refList[fldName] = RefItem{0, item.type, false, false, item.isShared ? PIN_WIRE : PIN_REG, false, false, ""};
                if (trace_declare)
                    printf("[%s:%d]NEWFLD3 %s type %s\n", __FUNCTION__, __LINE__, fldName.c_str(), item.type.c_str());
            }
            pvec = autostr(atoi(vecCount.c_str()) - 1);
            if (vecCount == "" || pvec == "0" || !isdigit(vecCount[0])) pvec = "";
            if(vecCount != GENERIC_INT_TEMPLATE_FLAG_STRING) vecCount = pvec;
        } while(vecCount != GENERIC_INT_TEMPLATE_FLAG_STRING && pvec != "");
        return nullptr; });
    for (auto MI : IR->methods) { // walkRemoveParam depends on the iterField above
        std::string methodName = MI->name;
        if (MI->rule)    // both RDY and ENA must be allocated for rules
            refList[methodName] = RefItem{0, MI->type, true, false, PIN_WIRE, false, false, ""};
        for (auto info: MI->printfList) {
            ACCExpr *value = info->value->operands.front();
            value->value = "(";   // change from PARAMETER_MARKER
            if (hasPrintf)
                MI->callList.push_back(new CallListElement{printfArgs(value), info->cond, true});
            else {
                ACCExpr *listp = value->operands.front();
                if (endswith(listp->value, "\\n\""))
                    listp->value = listp->value.substr(0, listp->value.length()-3) + "\"";
            }
        }
        for (auto item: MI->alloca) // be sure to define local temps before walkRemoveParam
            expandStruct(IR, item.first, item.second.type, 1, false, true, PIN_WIRE);
        // lift guards from called method interfaces
        if (!endswith(methodName, "__RDY"))
        if (MethodInfo *MIRdy = lookupMethod(IR, getRdyName(methodName)))
        for (auto item: MI->callList) {
            ACCExpr *tempCond = allocExpr(getRdyName(item->value->value));
            if (item->cond)
                tempCond = allocExpr("|", invertExpr(walkRemoveParam(item->cond)), tempCond);
            MIRdy->guard = cleanupBool(allocExpr("&", MIRdy->guard, tempCond));
        }
    }

    // generate wires for internal methods RDY/ENA.  Collect state element assignments
    // from each method
    for (auto MI : IR->methods)
        generateMethod(IR, MI, enableList, muxValueList);
    // combine mux'ed assignments into a single 'assign' statement
    for (auto item: muxValueList) {
        setAssign(item.first,
           cleanupExprBuiltin(allocExpr("__phi", item.second)),
           refList[item.first].type);
    }
    connectInterfaces(IR);
    for (auto item: enableList) // remove dependancy of the __ENA line on the __RDY
        setAssign(item.first, replaceAssign(simpleReplace(item.second), getRdyName(item.first)), "Bit(1)");
    for (auto item: assignList)
        if (item.second.type == "Bit(1)")
            assignList[item.first].value = cleanupBool(simpleReplace(item.second.value));
        else
            assignList[item.first].value = cleanupExpr(simpleReplace(item.second.value));
    for (auto MI : IR->methods) {
        std::string methodName = MI->name;
        for (auto info: MI->storeList) {
            std::string item = info->dest->value;
            if (isIdChar(item[0]) && !info->dest->operands.size() && refList[item].pin == PIN_WIRE)
                setAssign(item, info->value, refList[item].type);
            else
                appendLine(methodName, info->cond, info->dest, info->value);
        }
        if (!hasPrintf)
            for (auto info: MI->printfList)
                appendLine(methodName, info->cond, nullptr, info->value);
    }

    for (auto item: assignList) {
        int i = 0;
        if (!item.second.value)
            continue;
        while (true) {
            replaceBlock.clear();
            if (checkRecursion(item.second.value))
                break;
            std::string name;
            int length = 0;
            for (auto rep: replaceBlock) {
                 if (rep.second > length && !endswith(rep.first, "__RDY") && !endswith(rep.first, "__ENA")) {
                     name = rep.first;
                     length = rep.second;
                 }
            }
            if (name == "")
                break;
printf("[%s:%d] set [%s] noRecursion RRRRRRRRRRRRRRRRRRR\n", __FUNCTION__, __LINE__, name.c_str());
            assignList[name].noRecursion = true;
            if (i++ > 1000) {
                printf("[%s:%d]checkRecursion loop; exit\n", __FUNCTION__, __LINE__);
                for (auto rep: replaceBlock)
                    printf("[%s:%d] name %s length %d\n", __FUNCTION__, __LINE__, rep.first.c_str(), rep.second);
                exit(-1);
            }
        }
    }

    setAssignRefCount(IR);
    collectCSE();
    optimizeBitAssigns();
    // recursively process all replacements internal to the list of 'setAssign' items
    for (auto item = assignList.begin(), iend = assignList.end(); item != iend; item++)
        item->second.value = replaceAssign(item->second.value);

    // last chance to optimize out single assigns to output ports
    std::map<std::string, std::string> mapPort;
    for (auto item: assignList)
        if ((refList[item.first].out || refList[item.first].inout) && refList[item.first].pin == PIN_MODULE
          && item.second.value && isIdChar(item.second.value->value[0]) && !item.second.value->operands.size()
          && refList[item.second.value->value].pin != PIN_MODULE) {
            mapPort[item.second.value->value] = item.first;
        }
    // process assignList replacements, mark referenced items
    bool skipReplace = false;
    for (auto mitem: modLine) {
        std::string val = mitem.value;
        if (mitem.moduleStart) {
            skipReplace = mitem.vecCount != "";
        }
        else if (!skipReplace) {
if (trace_assign)
printf("[%s:%d] replaceParam '%s' count %d done %d\n", __FUNCTION__, __LINE__, mitem.value.c_str(), refList[val].count, refList[val].done);
            if (refList[mitem.value].count == 0) {
                refList[mitem.value].done = true;  // 'assign' line not needed; value is assigned by object inst
                if (refList[mitem.value].out && !refList[mitem.value].inout)
                    val = "0";
                else
                    val = "";
            }
            else if (refList[mitem.value].count <= 1) {
            val = mapPort[mitem.value];
            if (val != "") {
if (trace_assign)
printf("[%s:%d] ZZZZ mappp %s -> %s\n", __FUNCTION__, __LINE__, mitem.value.c_str(), val.c_str());
                decRef(mitem.value);
                refList[val].done = true;  // 'assign' line not needed; value is assigned by object inst
            }
            else
                val = tree2str(replaceAssign(allocExpr(mitem.value)));
            }
        }
        modNew.push_back(ModData{mitem.argName, val, mitem.type, mitem.moduleStart, mitem.noDefaultClock, mitem.out, mitem.inout, ""/*not param*/, mitem.vecCount});
    }
}
