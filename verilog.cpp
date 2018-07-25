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
int trace_connect;//= 1;
int trace_expand;//= 1;
int trace_skipped;//= 1;

std::map<std::string, RefItem> refList;
std::map<std::string, ModuleIR *> mapIndex;

std::map<std::string, AssignItem> assignList;
static std::map<std::string, std::string> replaceTarget;
static std::map<std::string, std::map<uint64_t, BitfieldPart>> bitfieldList;
static std::list<ModData> modNew;
typedef struct {
    ACCExpr      *guard;
    std::map<ACCExpr *, std::list<CondInfo>> info;
} CondGroup;
static std::map<std::string, CondGroup> condLines;

static void setAssign(std::string target, ACCExpr *value, std::string type, bool noReplace = false)
{
    std::string temp = replaceTarget[target];
    if (temp != "") {
        if (trace_assign)
printf("[%s:%d] ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ replace %s -> %s\n", __FUNCTION__, __LINE__, target.c_str(), temp.c_str());
        target = temp;
    }
    bool tDir = refList[target].out;
    if (value) {
if (trace_assign //|| !tDir
) printf("[%s:%d] start [%s/%d] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tDir, tree2str(value).c_str(), type.c_str());
    //assert(tDir || noReplace);
    if (!refList[target].pin) {
        printf("[%s:%d] missing target [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        exit(-1);
    }
    if (isIdChar(value->value[0]) && !noReplace) {
        bool sDir = refList[value->value].out;
        if (trace_assign)
        printf("[%s:%d] %s/%d = %s/%d\n", __FUNCTION__, __LINE__, target.c_str(), tDir, value->value.c_str(), sDir);
    }
    if (assignList[target].type != "") {
if (trace_assign) {
        printf("[%s:%d] duplicate start [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        printf("[%s:%d] duplicate was      = %s type '%s'\n", __FUNCTION__, __LINE__, tree2str(assignList[target].value).c_str(), assignList[target].type.c_str());
}
        //exit(-1);
    }
    assignList[target] = AssignItem{value, type, noReplace || refList[target].pin == PIN_MODULE, false};
    }
}

static void expandStruct(ModuleIR *IR, std::string fldName, std::string type, int out, bool inout, bool force, int pin)
{
    ACCExpr *itemList = allocExpr("{");
    std::list<FieldItem> fieldList;
    getFieldList(fieldList, fldName, "", type, out != 0, force);
    for (auto fitem : fieldList) {
        uint64_t offset = fitem.offset;
        uint64_t upper = offset + convertType(fitem.type) - 1;
        std::string base = fldName;
        if (fitem.base != "")
            base = fitem.base;
        std::string fnew = base + "[" + autostr(upper) + ":" + autostr(offset) + "]";
if (trace_expand)
printf("[%s:%d] set %s = %s out %d alias %d base %s , %s[%d : %d] fnew %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), fitem.type.c_str(), out, fitem.alias, base.c_str(), fldName.c_str(), (int)offset, (int)upper, fnew.c_str());
        assert (!refList[fitem.name].pin);
        refList[fitem.name] = RefItem{0, fitem.type, out != 0, false, fitem.alias ? PIN_WIRE : pin};
        refList[fnew] = RefItem{0, fitem.type, out != 0, false, PIN_ALIAS};
        if (!fitem.alias && out)
            itemList->operands.push_front(allocExpr(fitem.name));
        else if (out)
            replaceTarget[fitem.name] = fnew;
        else
            setAssign(fitem.name, allocExpr(fnew), fitem.type);
    }
    if (itemList->operands.size())
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
} PinInfo;
std::list<PinInfo> pinPorts, pinMethods, paramPorts;
static void collectInterfacePins(ModuleIR *IR, bool instance, std::string pinPrefix, std::string methodPrefix, bool isLocal)
{
    for (auto item : IR->interfaces) {
        ModuleIR *IIR = lookupIR(item.type);
        for (auto FI: IIR->method) {
            MethodInfo *MI = FI.second;
            std::string name = methodPrefix + item.fldName + MODULE_SEPARATOR + MI->name;
            bool out = instance ^ item.isPtr;
            pinMethods.push_back(PinInfo{MI->type, name, out, false, isLocal || item.isLocalInterface, MI});
        }
        for (auto fld: IIR->fields) {
            std::string name = pinPrefix + item.fldName + fld.fldName;
            bool out = instance ^ fld.isOutput;
            if (fld.isParameter)
                paramPorts.push_back(PinInfo{fld.isPtr ? "POINTER" : fld.type, name, out, fld.isInout, isLocal || item.isLocalInterface, nullptr});
            else
                pinPorts.push_back(PinInfo{fld.type, name, out, fld.isInout, isLocal || item.isLocalInterface, nullptr});
        }
        collectInterfacePins(IIR, instance, pinPrefix + item.fldName,
            methodPrefix + item.fldName + MODULE_SEPARATOR, isLocal || item.isLocalInterface);
    }
}

/*
 * Generate verilog module header for class definition or reference
 */
static void generateModuleSignature(ModuleIR *IR, std::string instance, std::list<ModData> &modParam, std::string params)
{
    auto checkWire = [&](std::string name, std::string type, int dir, bool inout, bool isparam, bool islocal) -> void {
        int refPin = instance != "" ? PIN_OBJECT: (islocal ? PIN_LOCAL: PIN_MODULE);
        if (!islocal || instance == "")
        refList[instance + name] = RefItem{(dir != 0 || inout) && instance == "", type, dir != 0 || refPin == PIN_LOCAL, inout, refPin};
        if (!islocal)
        modParam.push_back(ModData{name, instance + name, type, false, false, dir, inout, isparam});
        if (!isparam)
        expandStruct(IR, instance + name, type, dir || refPin == PIN_LOCAL, inout, false, PIN_WIRE);
    };
//printf("[%s:%d] name %s instance %s\n", __FUNCTION__, __LINE__, IR->name.c_str(), instance.c_str());
    std::string moduleInstantiation = IR->name;
    if (instance != "") {
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
    pinPorts.clear();
    pinMethods.clear();
    paramPorts.clear();
    collectInterfacePins(IR, instance != "", "", "", false);
    modParam.push_back(ModData{"", moduleInstantiation + ((instance != "") ? " " + instance.substr(0, instance.length()-1):""), "", true, pinPorts.size() > 0, 0, false, false});
    if (instance == "")
        for (auto item: paramPorts)
            checkWire(item.name, item.type, item.isOutput, item.isInout, true, item.isLocal);
    for (auto item: pinMethods) {
        checkWire(item.name, item.type, item.isOutput ^ (item.type != ""), false, false, item.isLocal);
        for (auto pitem: item.MI->params)
            checkWire(item.name.substr(0, item.name.length()-5) + MODULE_SEPARATOR + pitem.name, pitem.type, item.isOutput, false, false, item.isLocal);
    }
    for (auto item: pinPorts)
        checkWire(item.name, item.type, item.isOutput, item.isInout, false, item.isLocal);
}

static ACCExpr *walkRemoveParam (ACCExpr *expr)
{
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    if (isIdChar(item[0])) {
        int pin = refList[item].pin;
        if (pin == PIN_MODULE) {
//if (trace_assign)
printf("[%s:%d] reject use of non-state item %s %d\n", __FUNCTION__, __LINE__, item.c_str(), pin);
            return nullptr;
        }
        //assert(refList[item].pin);
    }
    for (auto item: expr->operands) {
        ACCExpr *operand = walkRemoveParam(item);
        if (operand)
            newExpr->operands.push_back(operand);
    }
    return newExpr;
}

static ACCExpr *walkRemoveCalledGuard (ACCExpr *expr, std::string guardName, bool useAssign)
{
    if (!expr)
        return expr;
    if (trace_assign)
    printf("[%s:%d] start %s expr %s\n", __FUNCTION__, __LINE__, guardName.c_str(), tree2str(expr).c_str());
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    if (isIdChar(item[0])) {
        if (item == guardName) {
            if (trace_assign)
            printf("[%s:%d] remove guard of called method from enable line %s\n", __FUNCTION__, __LINE__, item.c_str());
            return allocExpr("1");
        }
        if (ACCExpr *assignValue = assignList[item].value)
        if (useAssign && !assignList[item].noRecursion && !assignList[item].noReplace && refList[item].count < REF_COUNT_LIMIT) {
        decRef(item);
        if (replaceBlock[item]++ < 5) {
            if (trace_assign)
            printf("[%s:%d] replace %s norec %d with %s\n", __FUNCTION__, __LINE__, item.c_str(), assignList[item].noRecursion, tree2str(assignValue).c_str());
            return walkRemoveCalledGuard(assignValue, guardName, useAssign);
        }
        else if (replaceBlock[item] > 5 ) {
printf("[%s:%d] excessive replace of %s with %s top %s\n", __FUNCTION__, __LINE__, item.c_str(), tree2str(assignValue).c_str(), tree2str(expr).c_str());
if (replaceBlock[item] > 10)
exit(-1);
        }
        }
    }
    for (auto item: expr->operands) {
        ACCExpr *operand = walkRemoveCalledGuard(item, guardName, useAssign);
        if (operand)
            newExpr->operands.push_back(operand);
    }
    newExpr = cleanupExpr(newExpr);
    if (trace_assign)
    printf("[%s:%d] end %s expr %s\n", __FUNCTION__, __LINE__, guardName.c_str(), tree2str(newExpr).c_str());
    return newExpr;
}

static void processRecursiveAssign(void)
{
    // recursively process all replacements internal to the list of 'setAssign' items
    for (auto item: assignList)
        if (item.second.value) {
if (trace_assign)
printf("[%s:%d] checking [%s] = '%s'\n", __FUNCTION__, __LINE__, item.first.c_str(), tree2str(item.second.value).c_str());
            bool treeChanged = false;
            replaceBlock.clear();
            std::string newItem = tree2str(item.second.value, &treeChanged, true);
            if (treeChanged) {
if (trace_assign)
printf("[%s:%d] change [%s] = %s -> %s\n", __FUNCTION__, __LINE__, item.first.c_str(), tree2str(item.second.value).c_str(), newItem.c_str());
                assignList[item.first].value = cleanupExpr(str2tree(newItem, true));
            }
        }
}

static void optimizeBitAssigns(void)
{
    // gather bitfield assigns together
    for (auto item: assignList) {
        int ind = item.first.find('[');
        uint64_t lower, upper;
        if (ind != -1) {
            lower = atol(item.first.substr(ind+1).c_str());
            std::string temp = item.first.substr(0, ind);
            ind = item.first.find(':');
            if (ind != -1) {
                upper = atol(item.first.substr(ind+1).c_str());
                if (item.second.value)
                    bitfieldList[temp][lower] = BitfieldPart{upper, item.second.value, item.second.type};
            }
        }
    }
    // concatenate bitfield assigns
    for (auto item: bitfieldList) {
        std::string type = refList[item.first].type;
        uint64_t size = convertType(type), current = 0;
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
        size -= current;
        if (size > 0)
            newVal->operands.push_back(allocExpr(autostr(size) + "'d0"));
        setAssign(item.first, newVal, type);
    }
}

static void optimizeAssign(ModuleIR *IR)
{
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        refList[methodName].count++;
        for (auto info: MI->storeList) {
            walkRef(info->dest);
            walkRef(info->cond);
            walkRef(info->value);
        }
        for (auto info: MI->printfList) {
            walkRef(info->cond);
            walkRef(info->value);
        }
    }
    for (auto tcond: condLines) {
        std::string methodName = tcond.first;
        walkRef(tcond.second.guard);
        for (auto item: tcond.second.info) {
            walkRef(item.first);
            for (auto citem: item.second) {
                walkRef(citem.value);
                walkRef (citem.dest);
            }
        }
    }

    for (auto item: assignList)
        if (item.second.value && (refList[item.first].out || refList[item.first].inout) &&
            (refList[item.first].pin == PIN_OBJECT || refList[item.first].pin == PIN_LOCAL || refList[item.first].pin == PIN_MODULE))
            refList[item.first].count++;

    for (auto tcond = condLines.begin(), tend = condLines.end(); tcond != tend; tcond++) {
        std::string methodName = tcond->first;
        tcond->second.guard = cleanupExpr(allocExpr("&", allocExpr(methodName),
             allocExpr(getRdyName(methodName))));
        walkRef(tcond->second.guard);
    }

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
            printf("[%s:%d] ASSIGN %s = %s count %d[%d] pin %d\n", __FUNCTION__, __LINE__, aitem.first.c_str(), tree2str(aitem.second.value).c_str(), refList[aitem.first].count, refList[temp].count, refList[temp].pin);
    }
    for (auto item: refList)
        if (item.second.count) {
         std::string type = findType(item.first);
        }
}

static void generateAssign(FILE *OStr)
{
    bool hasAlways = condLines.size() > 0;

    // generate local state element declarations and wires
    for (auto item: refList) {
        if (item.second.pin == PIN_REG) {
        hasAlways = true;
        fprintf(OStr, "    reg %s;\n", (sizeProcess(item.second.type) + item.first).c_str());
        }
    }
    for (auto item: refList) {
        std::string temp = item.first;
        if (item.second.pin == PIN_WIRE || item.second.pin == PIN_OBJECT || item.second.pin == PIN_LOCAL) {
        if (item.second.count) {
            fprintf(OStr, "    wire %s;\n", (sizeProcess(item.second.type) + item.first).c_str());
if (trace_assign && item.second.out) {
printf("[%s:%d] JJJJ outputwire %s\n", __FUNCTION__, __LINE__, item.first.c_str());
//exit(-1);
}
        }
        else if (trace_assign)
            printf("[%s:%d] PINNOTALLOC %s\n", __FUNCTION__, __LINE__, item.first.c_str());
        }
    }
    for (auto item: assignList)
        if (item.second.value && refList[item.first].count && item.second.noReplace) {
            fprintf(OStr, "    assign %s = %s;\n", item.first.c_str(), tree2str(item.second.value).c_str());
            refList[item.first].count = 0; // mark that assigns have already been output
        }
    std::string endStr, sep;
    for (auto item: modNew) {
        if (item.moduleStart) {
            fprintf(OStr, "%s (%s", (endStr + "    " + item.value).c_str(),
                item.noDefaultClock ? "" : ".CLK(CLK), .nRST(nRST),");
            sep = "";
        }
        else {
            std::string val = tree2str(cleanupExpr(str2tree(item.value)));
            fprintf(OStr, "%s", (sep + "\n        ." + item.argName + "(" + val + ")").c_str());
            sep = ",";
        }
        endStr = ");\n";
    }
    fprintf(OStr, "%s", endStr.c_str());

    for (auto item: refList) {
        std::string temp = item.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if ((item.second.out || item.second.inout) && (item.second.pin == PIN_MODULE || item.second.pin == PIN_OBJECT || item.second.pin == PIN_LOCAL)) {
            if (item.second.inout && !assignList[item.first].value)
                for (auto alitem: assignList)
                    if (ACCExpr *val = alitem.second.value)
                    if (isIdChar(val->value[0]) && val->value == item.first)
                        goto next;
            if (refList[temp].count) {
                if (assignList[item.first].value)
                    fprintf(OStr, "    assign %s = %s;\n", item.first.c_str(), tree2str(assignList[item.first].value).c_str());
                else
                    fprintf(OStr, "    assign %s = 0; //MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE\n", item.first.c_str());
            }
            else if (trace_skipped)
                fprintf(OStr, "    skippedassign %s = %s; //temp = '%s', count = %d, pin = %d\n", item.first.c_str(), tree2str(assignList[item.first].value).c_str(), temp.c_str(), refList[temp].count, item.second.pin);
next:;
            refList[item.first].count = 0; // mark that assigns have already been output
        }
    }
    bool seen = false;
    for (auto item: assignList) {
        std::string temp = item.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if (item.second.value && refList[temp].count) {
            if (!seen)
                fprintf(OStr, "    // Extra assigments, not to output wires\n");
            seen = true;
            fprintf(OStr, "    assign %s = %s;\n", item.first.c_str(), tree2str(item.second.value).c_str());
        }
    }

    // generate clocked updates to state elements
    if (hasAlways) {
        fprintf(OStr, "\n    always @( posedge CLK) begin\n      if (!nRST) begin\n");
        for (auto item: refList)
            if (item.second.pin == PIN_REG)
                fprintf(OStr, "        %s <= 0;\n", item.first.c_str());
        fprintf(OStr, "      end // nRST\n");
        if (condLines.size() > 0) {
            fprintf(OStr, "      else begin\n");
            std::list<std::string> alwaysLines;
            for (auto tcond: condLines) {
                std::string methodName = tcond.first;
                alwaysLines.push_back("if (" + tree2str(tcond.second.guard, nullptr, true) + ") begin");
                for (auto item: tcond.second.info) {
                    std::string endStr;
                    std::string temp;
                    if (item.first) {
                        replaceBlock.clear();
                        temp = "    if (" + tree2str(item.first, nullptr, true) + ")";
                        if (item.second.size() > 1) {
                            temp += " begin";
                            endStr = "    end;";
                        }
                        alwaysLines.push_back(temp);
                    }
                    for (auto citem: item.second) {
                        replaceBlock.clear();
                        std::string val = tree2str(citem.value, nullptr, true) + ";";
                        if (citem.dest) {
                            replaceBlock.clear();
                            alwaysLines.push_back("    " + tree2str(citem.dest, nullptr, true) + " <= " + val);
                        }
                        else
                            alwaysLines.push_back("    $display" + val);
                    }
                    if (endStr != "")
                        alwaysLines.push_back(endStr);
                }
                alwaysLines.push_back("end; // End of " + methodName);
            }
            for (auto info: alwaysLines)
                fprintf(OStr, "        %s\n", info.c_str());
            fprintf(OStr, "      end\n");
        }
        fprintf(OStr, "    end // always @ (posedge CLK)\n");
    }
    fprintf(OStr, "endmodule \n\n");
}

int exprSize(ACCExpr *expr)
{
    return 16;
}

typedef struct {
    std::string format;
    std::list<int> width;
} PrintfInfo;
static std::list<PrintfInfo> printfFormat;
static int printfNumber = 1;
#define PRINTF_PORT 0x7fff
static ACCExpr *printfArgs(ACCExpr *listp)
{
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
        int size = exprSize(item);
        total_length += size;
        width.push_back(size);
        next->operands.push_back(item);
    }
    next->operands.push_back(allocExpr("16'd" + autostr(printfNumber++)));
    next->operands.push_back(allocExpr("16'd" + autostr(PRINTF_PORT)));
    next->operands.push_back(allocExpr("16'd" + autostr((total_length + sizeof(int) * 8 - 1)/(sizeof(int) * 8) + 2)));
    listp->operands.clear();
    listp->operands = next->operands;
    ACCExpr *ret = allocExpr("printfp$enq__ENA", allocExpr(PARAMETER_MARKER, next));
    printfFormat.push_back(PrintfInfo{format, width});
dumpExpr("PRINTFLL", ret);
    return ret;
}

static bool checkRecursion(ACCExpr *expr)
{
    std::string item = expr->value;
    if (isIdChar(item[0])) {
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
    if (isIdChar(item[0])) {
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

static std::string getExecute(std::string methodName)
{
    return methodName.substr(0, methodName.length() - 3) + "EXECUTE";
}

static void appendLine(std::string methodName, ACCExpr *cond, ACCExpr *dest, ACCExpr *value)
{
    for (auto CI = condLines[methodName].info.begin(), CE = condLines[methodName].info.end(); CI != CE; CI++)
        if (matchExpr(cond, CI->first)) {
            CI->second.push_back(CondInfo{dest, value});
            return;
        }
    condLines[methodName].guard = nullptr;
    condLines[methodName].info[cond].push_back(CondInfo{dest, value});
}

/*
 * Generate *.v and *.vh for a Verilog module
 */
static void generateModuleDef(ModuleIR *IR, FILE *OStr)
{
static std::list<ModData> modLine;
    bool hasPrintf = false;
    std::map<std::string, ACCExpr *> enableList;
    refList.clear();
    // 'Mux' together parameter settings from all invocations of a method from this class
    std::map<std::string, std::list<MuxValueEntry>> muxValueList;

    assignList.clear();
    replaceTarget.clear();
    bitfieldList.clear();
    modLine.clear();
    modNew.clear();
    condLines.clear();
    printf("[%s:%d] STARTMODULE %s\n", __FUNCTION__, __LINE__, IR->name.c_str());

    // Generate module header
    generateModuleSignature(IR, "", modLine, "");
    std::string sep;
    bool hasCLK = false, hasnRST = false;
    bool handleCLK = false;
    bool paramSeen = false;
    for (auto mitem: modLine) {
        static const char *dirStr[] = {"input wire", "output wire"};
        if (mitem.moduleStart) {
            fprintf(OStr, "module %s ", mitem.value.c_str());
            handleCLK = !mitem.noDefaultClock;
            if (handleCLK)
                sep = "(";
            else
                sep = "(\n    ";
        }
        else if (mitem.isparam) {
            if (!paramSeen) {
                fprintf(OStr, "#(\n    ");
                sep = "";
                paramSeen = true;
            }
            std::string typ = "UNKNOWN_PARAM_TYPE[" + mitem.type + "] ";
            std::string init = "\"FALSE\"";
            if (mitem.type == "FLOAT") {
                typ = "real ";
                init = "0.0";
            }
            else if (mitem.type == "POINTER")
                typ = "";
            else if (startswith(mitem.type, "INTEGER_")) {
                typ = "integer ";
                init = "0";
            }
            fprintf(OStr, "%sparameter %s%s = %s", sep.c_str(), typ.c_str(), mitem.value.c_str(), init.c_str());
            sep = ",\n    ";
        }
        else {
            if (paramSeen) {
                fprintf(OStr, ")(\n    ");
                sep = "";
                paramSeen = false;
            }
            fprintf(OStr, "%s", sep.c_str());
            sep = ",\n    ";
            if (handleCLK) {
                fprintf(OStr, "input wire CLK, input wire nRST,\n    ");
                hasCLK = true;
                hasnRST = true;
                handleCLK = false;
            }
            std::string dirs = dirStr[mitem.out];
            if (mitem.inout)
                dirs = "inout wire";
            fprintf(OStr, "%s %s%s", dirs.c_str(), sizeProcess(mitem.type).c_str(), mitem.value.c_str());
            if (mitem.value == "CLK")
                hasCLK = true;
            if (mitem.value == "nRST")
                hasnRST = true;
        }
    }
    if (handleCLK) {
        fprintf(OStr, "%sinput CLK, input nRST", sep.c_str());
        hasCLK = true;
        hasnRST = true;
        handleCLK = false;
    }
    fprintf(OStr, ");\n");
    if (!hasCLK) {
        refList["CLK"] = RefItem{0, "INTEGER_1", false, false, PIN_LOCAL};
        refList["CLK"].count++;
    }
    if (!hasnRST) {
        refList["nRST"] = RefItem{0, "INTEGER_1", false, false, PIN_LOCAL};
        refList["nRST"].count++;
    }
    modLine.clear();
    for (auto item: IR->interfaces)
        if (item.fldName == "printfp")
            hasPrintf = true;

    iterField(IR, CBAct {
            ModuleIR *itemIR = lookupIR(item.type);
            if (itemIR && !item.isPtr) {
            if (startswith(itemIR->name, "l_struct_OC_")) {
                refList[fldName] = RefItem{0, item.type, 1, false, PIN_WIRE};
                expandStruct(IR, fldName, item.type, 1, false, true, PIN_REG);
            }
            else
                generateModuleSignature(itemIR, fldName + MODULE_SEPARATOR, modLine, IR->params[fldName]);
            }
            else if (convertType(item.type) != 0)
                refList[fldName] = RefItem{0, item.type, false, false, PIN_REG};
          return nullptr;
          });
    for (auto FI : IR->method) { // walkRemoveParam depends on the iterField above
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        if (MI->rule)    // both RDY and ENA must be allocated for rules
            refList[methodName] = RefItem{1, MI->type, true, false, PIN_WIRE};
        for (auto info: MI->printfList) {
            ACCExpr *value = info->value->operands.front();
            value->value = "(";   // change from PARAMETER_MARKER
            info->value = value;
            if (hasPrintf)
                MI->callList.push_back(new CallListElement{printfArgs(info->value), info->cond, true});
            else {
                ACCExpr *listp = value->operands.front();
                if (endswith(listp->value, "\\n\""))
                    listp->value = listp->value.substr(0, listp->value.length()-3) + "\"";
            }
        }
        for (auto item: MI->alloca) { // be sure to define local temps before walkRemoveParam
            refList[item.first] = RefItem{0, item.second, true, false, PIN_WIRE};
            expandStruct(IR, item.first, item.second, 1, false, true, PIN_WIRE);
        }
        // lift guards from called method interfaces
        if (!endswith(methodName, "__RDY"))
        if (MethodInfo *MIRdy = lookupMethod(IR, getRdyName(methodName)))
        for (auto item: MI->callList) {
            ACCExpr *tempCond = allocExpr(getRdyName(item->value->value));
            if (item->cond)
                tempCond = allocExpr("|", invertExpr(walkRemoveParam(item->cond)), tempCond);
            MIRdy->guard = cleanupExpr(allocExpr("&", MIRdy->guard, tempCond));
        }
    }

    // generate wires for internal methods RDY/ENA.  Collect state element assignments
    // from each method
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        MI->guard = cleanupExpr(MI->guard);
        if (!endswith(methodName, "__RDY")) {
            walkRead(MI, MI->guard, nullptr);
            if (MI->rule)
                setAssign(methodName, allocExpr(getRdyName(methodName)), "INTEGER_1", true);
        }
        if (endswith(methodName, "__ENA")) {
            std::string mname = getExecute(methodName);
            refList[mname] = RefItem{0, "INTEGER_1", false, false, PIN_WIRE};
            setAssign(mname, allocExpr("&", allocExpr(methodName),
                 allocExpr(getRdyName(methodName))), "INTEGER_1", true);
        }
        setAssign(methodName, MI->guard, MI->type, MI->rule && !endswith(methodName, "__RDY"));  // collect the text of the return value into a single 'assign'
        for (auto info: MI->storeList) {
            walkRead(MI, info->cond, nullptr);
            walkRead(MI, info->value, info->cond);
        }
        for (auto info: MI->letList) {
            ACCExpr *cond = allocExpr("&", allocExpr(getRdyName(methodName)));
            if (info->cond)
                cond->operands.push_back(info->cond);
            cond = cleanupExpr(cond);
            ACCExpr *value = info->value;
            if (isdigit(value->value[0]))
                updateWidth(value, convertType(info->type));
            walkRead(MI, cond, nullptr);
            walkRead(MI, value, cond);
            std::list<FieldItem> fieldList;
            getFieldList(fieldList, "", "", info->type, false, true);
            for (auto fitem : fieldList) {
                std::string dest = info->dest->value + fitem.name;
                ACCExpr *newExpr = value;
                if (isIdChar(value->value[0]))
                    newExpr = allocExpr(value->value + fitem.name);
                muxValueList[dest].push_back(MuxValueEntry{cond, newExpr});
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
            ACCExpr *tempCond = allocExpr("&", allocExpr(methodName), allocExpr(getRdyName(methodName)));
            if (cond) {
                ACCExpr *temp = cond;
                if (temp->value != "&")
                    temp = allocExpr("&", temp);
                temp->operands.push_back(tempCond);
                tempCond = temp;
            }
            std::string calledName = value->value;
//printf("[%s:%d] CALLLLLL '%s' condition %s\n", __FUNCTION__, __LINE__, calledName.c_str(), tree2str(tempCond).c_str());
//dumpExpr("CALLCOND", tempCond);
            if (!value->operands.size() || value->operands.front()->value != PARAMETER_MARKER) {
                printf("[%s:%d] incorrectly formed call expression\n", __FUNCTION__, __LINE__);
                exit(-1);
            }
            if (!enableList[calledName])
                enableList[calledName] = allocExpr("||");
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
//printf("[%s:%d] param '%s'\n", __FUNCTION__, __LINE__, tree2str(param).c_str());
//dumpExpr("param", param);
            for (auto item: param->operands) {
                if(argCount-- > 0) {
//printf("[%s:%d] infmuxVL[%s] = cond '%s' tree '%s'\n", __FUNCTION__, __LINE__, (pname + AI->name).c_str(), tree2str(tempCond).c_str(), tree2str(item).c_str());
                    muxValueList[pname + AI->name].push_back(MuxValueEntry{tempCond, item});
                    //typeList[pname + AI->name] = AI->type;
                    AI++;
                }
            }
        }
    }
    // combine mux'ed assignments into a single 'assign' statement
    // Context: before local state declarations, to allow inlining
    for (auto item: muxValueList) {
        ACCExpr *prevCond = nullptr, *prevValue = nullptr;
        ACCExpr *temp = nullptr, *head = nullptr;
        std::string type = refList[item.first].type;
        for (auto element: item.second) {
            if (prevCond) {
                ACCExpr *newExpr = allocExpr("?", prevCond, prevValue);
                if (temp)
                    temp->operands.push_back(newExpr);
                temp = newExpr;
                if (!head)
                    head = temp;
            }
            prevCond = element.cond;
            prevValue = element.value;
        }
        if (type == "INTEGER_1" && prevCond) // since bools are also used for rdy/ena, be pedantic in calculating conditionalized value
            prevValue = allocExpr("?", prevCond, prevValue, allocExpr("0"));
        if (temp)
            temp->operands.push_back(prevValue);
        else
            head = prevValue;
        setAssign(item.first, head, type);
    }
    for (auto IC : IR->interfaceConnect) {
        ModuleIR *IIR = lookupIR(IC.type);
        if (!IIR)
            dumpModule("MISSINGCONNECT", IR);
        assert(IIR && "interfaceConnect interface type");
        if (trace_connect)
            printf("%s: CONNECT target %s source %s forward %d\n", __FUNCTION__, IC.target.c_str(), IC.source.c_str(), IC.isForward);
        for (auto fld : IIR->fields) {
            std::string tstr = IC.target + fld.fldName,
                        sstr = IC.source + fld.fldName;
            if (!IC.isForward) {
                refList[tstr].out = 1;   // for local connections, don't bias for 'output'
                refList[sstr].out = 0;
            }
            if (trace_connect || (!refList[tstr].out && !refList[sstr].out))
                printf("%s: IFCCCfield %s/%d %s/%d\n", __FUNCTION__, tstr.c_str(), refList[tstr].out, sstr.c_str(), refList[sstr].out);
            if (refList[sstr].out)
                setAssign(sstr, allocExpr(tstr), fld.type);
            else
                setAssign(tstr, allocExpr(sstr), fld.type);
        }
        for (auto FI : IIR->method) {
            MethodInfo *MI = FI.second;
            std::string tstr = IC.target + MODULE_SEPARATOR + MI->name,
                        sstr = IC.source + MODULE_SEPARATOR + MI->name;
            if (!IC.isForward) {
                refList[tstr].out = 1;   // for local connections, don't bias for 'output'
                refList[sstr].out = 0;
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
                if (trace_connect)
                    printf("%s: IFCCCparam %s/%d %s/%d\n", __FUNCTION__, tparm.c_str(), refList[tparm].out, sparm.c_str(), refList[sparm].out);
                if (refList[sparm].out)
                    setAssign(sparm, allocExpr(tparm), info.type);
                else
                    setAssign(tparm, allocExpr(sparm), info.type);
            }
        }
    }
    for (auto item: enableList)
        setAssign(item.first, item.second, "INTEGER_1");
    for (auto item: assignList)
        assignList[item.first].value = cleanupExpr(simpleReplace(item.second.value));
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        for (auto info: MI->storeList)
            appendLine(methodName, info->cond, info->dest, info->value);
        if (!hasPrintf)
            for (auto info: MI->printfList)
                appendLine(methodName, info->cond, nullptr, info->value);
    }

    optimizeAssign(IR);

    for (auto item: assignList) {
        int i = 0;
        if (!item.second.value)
            continue;
        while (true) {
            replaceBlock.clear();
            if (checkRecursion(item.second.value))
                break;
            std::string name;
            int length = -1;
            for (auto rep: replaceBlock) {
                 if (rep.second > length && !endswith(rep.first, "__RDY") && !endswith(rep.first, "__ENA")) {
                     name = rep.first;
                     length = rep.second;
                 }
            }
printf("[%s:%d] set [%s] noRecursion RRRRRRRRRRRRRRRRRRR\n", __FUNCTION__, __LINE__, name.c_str());
            assignList[name].noRecursion = true;
            if (i++ > 1000) {
printf("[%s:%d]SETNO error; exit\n", __FUNCTION__, __LINE__);
exit(-1);
            }
        }
    }
    // remove dependancy of the calling __ENA line on the calling __RDY
    for (auto item: enableList) {
        ACCExpr *val = assignList[item.first].value;
        replaceBlock.clear();
        assignList[item.first].value = cleanupExpr(walkRemoveCalledGuard(val, getRdyName(item.first), true));
    }
    optimizeBitAssigns();
    processRecursiveAssign();
    for (auto item: assignList)
        if (item.second.value && IR->method.find(item.first) != IR->method.end()) {
            ACCExpr *val = cleanupExpr(item.second.value);
            replaceBlock.clear();
            assignList[item.first].value = cleanupExpr(walkRemoveCalledGuard(val, getRdyName(item.first), false));
        }

    // last chance to optimize out single assigns to output ports
    std::map<std::string, std::string> mapPort;
    for (auto item: assignList)
        if (refList[item.first].out && refList[item.first].pin == PIN_MODULE
          && item.second.value && isIdChar(item.second.value->value[0])
          && refList[item.second.value->value].pin != PIN_MODULE) {
            mapPort[item.second.value->value] = item.first;
        }
    // process assignList replacements, mark referenced items
    for (auto mitem: modLine) {
        std::string val = mitem.value;
        if (!mitem.moduleStart) {
            if (refList[val].count == 0) {
                if (refList[val].out)
                    val = "0";
                else
                    val = "";
            }
            else {
            replaceBlock.clear();
            val = tree2str(allocExpr(mitem.value), nullptr, true);
            std::string temp = mapPort[val];
            if (temp != "") {
printf("[%s:%d] ZZZZ mappp %s -> %s\n", __FUNCTION__, __LINE__, val.c_str(), temp.c_str());
                refList[val].count = 0;
                refList[temp].count = 0;  // 'assign' line not needed; value is assigned by object inst
                val = temp;
            }
            }
        }
        modNew.push_back(ModData{mitem.argName, val, mitem.type, mitem.moduleStart, mitem.noDefaultClock, mitem.out, mitem.inout, false});
    }
    generateAssign(OStr);
}

void generateVerilog(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir)
{
    std::string baseDir = OutputDir;
    int ind = baseDir.rfind('/');
    if (ind > 0)
        baseDir = baseDir.substr(0, ind+1);
    for (auto IR : irSeq) {
        FILE *OStrV = fopen((baseDir + IR->name + ".v").c_str(), "w");
        if (!OStrV) {
            printf("veriloggen: unable to open '%s'\n", (baseDir + IR->name + ".v").c_str());
            exit(-1);
        }
        fprintf(OStrV, "`include \"%s.generated.vh\"\n\n", myName.c_str());
        fprintf(OStrV, "`default_nettype none\n");
        generateModuleDef(IR, OStrV); // Generate verilog
        fprintf(OStrV, "`default_nettype wire    // set back to default value\n");
        fclose(OStrV);
    }
    if (printfFormat.size()) {
    FILE *OStrP = fopen((OutputDir + ".printf").c_str(), "w");
    for (auto item: printfFormat) {
        fprintf(OStrP, "%s ", item.format.c_str());
        for (auto witem: item.width)
            fprintf(OStrP, "%d ", witem);
        fprintf(OStrP, "\n");
    }
    fclose(OStrP);
    }
}
