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

std::map<std::string, RefItem> refList;
std::map<std::string, ModuleIR *> mapIndex;

static std::map<std::string, AssignItem> assignList;
static std::map<std::string, std::string> replaceTarget;
static std::map<std::string, std::map<uint64_t, BitfieldPart>> bitfieldList;
static std::list<ModData> modNew;
static std::list<std::string> alwaysLines;

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
    assignList[target] = AssignItem{value, type, noReplace};
    }
}

static void expandStruct(ModuleIR *IR, std::string fldName, std::string type, int out, bool force, int pin)
{
    ACCExpr *itemList = allocExpr(",");
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
        refList[fitem.name] = RefItem{0, fitem.type, out != 0, fitem.alias ? PIN_WIRE : pin};
        refList[fnew] = RefItem{0, fitem.type, out != 0, PIN_ALIAS};
        if (!fitem.alias && out)
            itemList->operands.push_front(allocExpr(fitem.name));
        else if (out)
            replaceTarget[fitem.name] = fnew;
        else
            setAssign(fitem.name, allocExpr(fnew), fitem.type);
    }
    if (itemList->operands.size())
        setAssign(fldName, allocExpr("{", itemList), type);
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
    if (isIdChar(fieldName[0]) && cond && (!expr->operands.size() || expr->operands.front()->value != "{"))
        addRead(MI->meta[MetaRead][fieldName], cond);
}

static void walkRef (ACCExpr *expr)
{
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
            //refList[item.substr(0,ind)].count++;
item = base;
}
        assert(refList[item].pin);
        refList[item].count++;
    }
    for (auto item: expr->operands)
        walkRef(item);
}
static std::string walkTree (ACCExpr *expr, bool *changed)
{
    std::string ret = expr->value;
    if (isIdChar(ret[0])) {
if (trace_assign)
printf("[%s:%d] check '%s' exprtree %p\n", __FUNCTION__, __LINE__, ret.c_str(), assignList[ret].value);
        ACCExpr *temp = assignList[ret].value;
        if (temp && !assignList[ret].noReplace) {
            refList[ret].count = 0;
if (trace_assign)
printf("[%s:%d] changed %s -> %s\n", __FUNCTION__, __LINE__, ret.c_str(), tree2str(temp).c_str());
            ret = walkTree(temp, changed);
            if (changed)
                *changed = true;
            else
                walkRef(temp);
        }
        else if (!changed)
            refList[ret].count++;
    }
    else {
        ret = "";
        std::string sep, op = expr->value;
        if (isParenChar(op[0])) {
            ret += op + " ";
            op = "";
        }
        if (!expr->operands.size())
            ret += op;
        for (auto item: expr->operands) {
            bool operand = checkOperand(item->value) || item->value == "," || item->value == "?" || expr->operands.size() == 1;
            ret += sep;
            if (!operand)
                ret += "( ";
            ret += walkTree(item, changed);
            if (!operand)
                ret += " )";
            sep = " " + op + " ";
            if (op == "?")
                op = ":";
        }
    }
    ret += treePost(expr);
    return ret;
}

/*
 * Generate verilog module header for class definition or reference
 */
static void generateModuleSignature(ModuleIR *IR, std::string instance, std::list<ModData> &modParam)
{
    auto checkWire = [&](std::string name, std::string type, int dir) -> void {
        refList[instance + name] = RefItem{dir != 0 && instance == "", type, dir != 0, instance == "" ? PIN_MODULE : PIN_OBJECT};
        modParam.push_back(ModData{name, instance + name, type, false, dir});
        expandStruct(IR, instance + name, type, dir, false, PIN_WIRE);
    };
//printf("[%s:%d] name %s instance %s\n", __FUNCTION__, __LINE__, IR->name.c_str(), instance.c_str());
    modParam.push_back(ModData{"", IR->name + ((instance != "") ? " " + instance.substr(0, instance.length()-1):""), "", true, 0});
    for (auto item : IR->interfaces)
        for (auto FI: lookupIR(item.type)->method) {
            MethodInfo *MI = FI.second;
            std::string name = item.fldName + MODULE_SEPARATOR + MI->name;
            bool out = (instance != "") ^ item.isPtr;
            checkWire(name, MI->type, out ^ (MI->type != ""));
            for (auto pitem: MI->params)
                checkWire(name.substr(0, name.length()-5) + MODULE_SEPARATOR + pitem.name, pitem.type, out);
        }
}

static ACCExpr *walkRemoveParam (ACCExpr *expr)
{
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    if (isIdChar(item[0])) {
        int pin = refList[item].pin;
        if (pin != PIN_OBJECT && pin != PIN_REG) {
if (trace_assign)
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
        ACCExpr *newVal = allocExpr(",");
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
        setAssign(item.first, allocExpr("{", newVal), type);
    }

    // recursively process all replacements internal to the list of 'setAssign' items
    for (auto item: assignList)
        if (item.second.value) {
if (trace_assign)
printf("[%s:%d] checking [%s] = '%s'\n", __FUNCTION__, __LINE__, item.first.c_str(), tree2str(item.second.value).c_str());
            bool treeChanged = false;
            std::string newItem = walkTree(item.second.value, &treeChanged);
            if (treeChanged) {
if (trace_assign)
printf("[%s:%d] change [%s] = %s -> %s\n", __FUNCTION__, __LINE__, item.first.c_str(), tree2str(item.second.value).c_str(), newItem.c_str());
                assignList[item.first].value = str2tree(newItem, true);
            }
        }
}

static void optimizeAssign(void)
{
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
    bool hasAlways = false;
    // generate local state element declarations and wires
    for (auto item: refList) {
        if (item.second.pin == PIN_REG) {
        hasAlways = true;
        fprintf(OStr, "    reg %s;\n", (sizeProcess(item.second.type) + item.first).c_str());
        }
    }
    for (auto item: refList) {
        std::string temp = item.first;
        if (item.second.pin == PIN_WIRE || item.second.pin == PIN_OBJECT) {
        if (refList[temp].count) {
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
            refList[item.first].count = 0;
        }
    std::string endStr, sep;
    for (auto item: modNew) {
        if (item.moduleStart) {
            fprintf(OStr, "%s (.CLK(CLK), .nRST(nRST),", (endStr + "    " + item.value).c_str());
            sep = "";
        }
        else {
            fprintf(OStr, "%s", (sep + "\n        ." + item.argName + "(" + item.value + ")").c_str());
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
        if (item.second.out && (item.second.pin == PIN_MODULE || item.second.pin == PIN_OBJECT)) {
            if (!assignList[item.first].value)
                fprintf(OStr, "    // assign %s = MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE;\n", item.first.c_str());
            else if (refList[temp].count) // must have value != ""
                fprintf(OStr, "    assign %s = %s;\n", item.first.c_str(), tree2str(assignList[item.first].value).c_str());
            refList[item.first].count = 0;
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
        if (alwaysLines.size() > 0) {
            fprintf(OStr, "      else begin\n");
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
static ACCExpr *printfArgs(ACCExpr *value)
{
    //value->value = "printfp$enq__ENA";
    ACCExpr *listp = value->operands.front(); // get '{' node
    ACCExpr *next = listp->operands.front();
    if (next->value == ",")
        listp = next;
    ACCExpr *fitem = listp->operands.front();
    listp->operands.pop_front();
    std::string format = fitem->value;
    if (endswith(format, "\\n\""))
        format = format.substr(0, format.length()-3) + "\"";
    std::list<int> width;
    unsigned index = 0;
    next = allocExpr(",");
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
#define PRINTF_PORT 0x7fff
    next->operands.push_back(allocExpr("16'd" + autostr(PRINTF_PORT)));
    next->operands.push_back(allocExpr("16'd" + autostr((total_length + sizeof(int) * 8 - 1)/(sizeof(int) * 8) + 2)));
    listp->operands.clear();
    listp->operands = next->operands;
    value = allocExpr("printfp$enq__ENA", allocExpr("{", allocExpr("{", next)));
    printfFormat.push_back(PrintfInfo{format, width});
dumpExpr("PRINTFLL", value);
    return value;
}

static void processMethod(MethodInfo *MI, bool hasPrintf)
{
    std::string methodName = MI->name;
    std::map<std::string, std::list<std::string>> condLines;
    for (auto info: MI->storeList) {
        ACCExpr *cond = cleanupExpr(info->cond);
        ACCExpr *value = (info->value);
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        ACCExpr *destt = info->dest;
        destt = str2tree(walkTree(destt, nullptr), true);
        walkRef(destt);
        ACCExpr *expr = destt;
        if (expr->value == "?") {
            int i = 0;
            for (auto item: expr->operands)
                if (i++ == 1) {
                    expr = item;
                    break;
                }
        }
        if (expr->value != "{") { // }
            std::string destType = findType(expr->value);
            if (destType == "") {
            printf("[%s:%d] typenotfound %s\n", __FUNCTION__, __LINE__, tree2str(destt).c_str());
            exit(-1);
            }
        }
        std::string condStr;
        if (cond)
            condStr = "    if (" + walkTree(cond, nullptr) + ")";
        condLines[condStr].push_back("    " + tree2str(destt) + " <= " + walkTree(value, nullptr) + ";");
    }
    if (!hasPrintf)
    for (auto info: MI->printfList) {
printf("[%s:%d] PRINTFFFFFF\n", __FUNCTION__, __LINE__);
        ACCExpr *cond = cleanupExpr(info->cond);
        ACCExpr *value = cleanupExpr(info->value)->operands.front();
        value->value = "(";   // change from '{'
        ACCExpr *listp = value->operands.front();
        if (listp->value == ",")
            listp = listp->operands.front();
        if (endswith(listp->value, "\\n\""))
            listp->value = listp->value.substr(0, listp->value.length()-3) + "\"";
//dumpExpr("PRINTCOND", cond);
//dumpExpr("PRINTF", value);
        std::string condStr;
        if (cond)
            condStr = "    if (" + walkTree(cond, nullptr) + ")";
        condLines[condStr].push_back("    $display" + walkTree(value, nullptr) + ";");
    }
    if (condLines.size())
        alwaysLines.push_back("if (" + MI->name + ") begin");
    for (auto item: condLines) {
        std::string endStr;
        std::string temp = item.first;
        if (item.first != "") {
            if (item.second.size() > 1) {
                temp += " begin";
                endStr = "    end;";
            }
            alwaysLines.push_back(temp);
        }
        for (auto citem: item.second)
            alwaysLines.push_back(citem);
        if (endStr != "")
            alwaysLines.push_back(endStr);
    }
    if (condLines.size())
        alwaysLines.push_back("end; // End of " + MI->name);
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
    alwaysLines.clear();

    // Generate module header
    generateModuleSignature(IR, "", modLine);
    std::string sep = "module ";
    for (auto mitem: modLine) {
        static const char *dirStr[] = {"input", "output"};
        fprintf(OStr, "%s", sep.c_str());
        if (mitem.moduleStart)
            fprintf(OStr, "%s (input CLK, input nRST", mitem.value.c_str());
        else
            fprintf(OStr, "%s %s%s", dirStr[mitem.out], sizeProcess(mitem.type).c_str(), mitem.value.c_str());
        sep = ",\n    ";
    }
    fprintf(OStr, ");\n");
    modLine.clear();
    for (auto item: IR->interfaces)
        if (item.fldName == "printfp")
            hasPrintf = true;

    iterField(IR, CBAct {
            ModuleIR *itemIR = lookupIR(item.type);
            if (itemIR && !item.isPtr) {
            if (startswith(itemIR->name, "l_struct_OC_")) {
                refList[fldName] = RefItem{0, item.type, 1, PIN_WIRE};
                expandStruct(IR, fldName, item.type, 1, true, PIN_REG);
            }
            else
                generateModuleSignature(itemIR, fldName + MODULE_SEPARATOR, modLine);
            }
            else if (convertType(item.type) != 0)
                refList[fldName] = RefItem{0, item.type, false, PIN_REG};
          return nullptr;
          });
    for (auto FI : IR->method) { // walkRemoveParam depends on the iterField above
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        if (MI->rule)    // both RDY and ENA must be generated for rules
            refList[methodName] = RefItem{1, MI->type, true, PIN_WIRE};
        if (hasPrintf)
            for (auto info: MI->printfList) // must be before callList processing
                MI->callList.push_back(new CallListElement{printfArgs(info->value), info->cond, true});
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
        setAssign(methodName, MI->guard, MI->type, MI->rule);  // collect the text of the return value into a single 'assign'
        for (auto item: MI->alloca) {
            refList[item.first] = RefItem{0, item.second, true, PIN_WIRE};
            expandStruct(IR, item.first, item.second, 1, true, PIN_WIRE);
        }
        for (auto info: MI->letList) {
            ACCExpr *cond = cleanupExpr(info->cond);
            ACCExpr *value = cleanupExpr(info->value);
            if (isdigit(value->value[0]))
                updateWidth(value, convertType(info->type));
            walkRead(MI, cond, nullptr);
            walkRead(MI, value, cond);
            std::list<FieldItem> fieldList;
            getFieldList(fieldList, "", "", info->type, false, true);
            for (auto fitem : fieldList) {
                std::string dest = info->dest->value + fitem.name;
                std::string src = value->value + fitem.name;
                muxValueList[dest].push_back(MuxValueEntry{cond, allocExpr(src)});
            }
        }
        for (auto info: MI->callList) {
            ACCExpr *cond = cleanupExpr(info->cond);
            ACCExpr *value = cleanupExpr(info->value);
            if (isIdChar(value->value[0]) && value->operands.size() && value->operands.front()->value == "{")
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
            ACCExpr *tempCond = allocExpr(methodName);
            if (cond) {
                ACCExpr *temp = cond;
                if (temp->value != "&")
                    temp = allocExpr("&", temp);
                temp->operands.push_back(tempCond);
                tempCond = temp;
            }
            std::string calledName = value->value;
//printf("[%s:%d] CALLLLLL '%s'\n", __FUNCTION__, __LINE__, calledName.c_str());
            if (!value->operands.size() || value->operands.front()->value != "{") {
                printf("[%s:%d] incorrectly formed call expression\n", __FUNCTION__, __LINE__);
                exit(-1);
            }
            // 'Or' together ENA lines from all invocations of a method from this class
            if (info->isAction) {
                if (!enableList[calledName])
                    enableList[calledName] = allocExpr("||");
                enableList[calledName]->operands.push_back(tempCond);
            }
            MethodInfo *CI = lookupQualName(IR, calledName);
            if (!CI) {
                printf("[%s:%d] method %s not found\n", __FUNCTION__, __LINE__, calledName.c_str());
                exit(-1);
            }
            auto AI = CI->params.begin();
            std::string pname = calledName.substr(0, calledName.length()-5) + MODULE_SEPARATOR;
            int argCount = CI->params.size();
            ACCExpr *param = value->operands.front()->operands.front();
//printf("[%s:%d] param '%s'\n", __FUNCTION__, __LINE__, tree2str(param).c_str());
//dumpExpr("param", param);
            auto setParam = [&] (ACCExpr *item) -> void {
                if(argCount-- > 0) {
//printf("[%s:%d] infmuxVL[%s] = cond '%s' tree '%s'\n", __FUNCTION__, __LINE__, (pname + AI->name).c_str(), tree2str(tempCond).c_str(), tree2str(item).c_str());
                    muxValueList[pname + AI->name].push_back(MuxValueEntry{tempCond, item});
                    //typeList[pname + AI->name] = AI->type;
                    AI++;
                }
            };
            if (param) {
                if (param->value == ",")
                    for (auto item: param->operands)
                        setParam(item);
                else
                    setParam(param);
            }
        }
    }
    for (auto item: enableList)
        setAssign(item.first, item.second, "INTEGER_1");
    // combine mux'ed assignments into a single 'assign' statement
    // Context: before local state declarations, to allow inlining
    for (auto item: muxValueList) {
        ACCExpr *prevCond = nullptr, *prevValue = nullptr;
        ACCExpr *temp = nullptr, *head = nullptr;
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
        if (temp)
            temp->operands.push_back(prevValue);
        else
            head = prevValue;
        setAssign(item.first, head, refList[item.first].type);
    }
    for (auto IC : IR->interfaceConnect)
        for (auto FI : lookupIR(IC.type)->method) {
            MethodInfo *MI = FI.second;
            std::string tstr = IC.target + MODULE_SEPARATOR + MI->name,
                        sstr = IC.source + MODULE_SEPARATOR + MI->name;
//printf("[%s:%d] IFCCC %s/%d %s/%d\n", __FUNCTION__, __LINE__, tstr.c_str(), refList[tstr].out, sstr.c_str(), refList[sstr].out);
            if (refList[sstr].out)
                setAssign(sstr, allocExpr(tstr), MI->type);
            else
                setAssign(tstr, allocExpr(sstr), MI->type);
            tstr = tstr.substr(0, tstr.length()-5) + MODULE_SEPARATOR;
            sstr = sstr.substr(0, sstr.length()-5) + MODULE_SEPARATOR;
            for (auto info: MI->params) {
                std::string sparm = sstr + info.name, tparm = tstr + info.name;
                if (refList[sparm].out)
                    setAssign(sparm, allocExpr(tparm), info.type);
                else
                    setAssign(tparm, allocExpr(sparm), info.type);
            }
        }
    optimizeBitAssigns();

    // last chance to optimize out single assigns to output ports
    std::map<std::string, std::string> mapPort;
    for (auto item: assignList)
        if (refList[item.first].out && refList[item.first].pin == PIN_MODULE
          && item.second.value && isIdChar(item.second.value->value[0])) {
            mapPort[item.second.value->value] = item.first;
        }
    // process assignList replacements, mark referenced items
    for (auto mitem: modLine) {
        std::string val = mitem.value;
        if (!mitem.moduleStart) {
            val = walkTree(allocExpr(mitem.value), nullptr);
            std::string temp = mapPort[val];
            if (temp != "") {
printf("[%s:%d] ZZZZ mappp %s -> %s\n", __FUNCTION__, __LINE__, val.c_str(), temp.c_str());
                refList[val].count = 0;
                refList[temp].count = 0;
                val = temp;
            }
        }
        modNew.push_back(ModData{mitem.argName, val, mitem.type, mitem.moduleStart, mitem.out});
    }
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        processMethod(MI, hasPrintf);
    }
    optimizeAssign();
    generateAssign(OStr);
}

void generateVerilog(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir)
{
    FILE *OStrV = fopen((OutputDir + ".v").c_str(), "w");
    if (!OStrV) {
        printf("veriloggen: unable to open '%s'\n", (OutputDir + ".v").c_str());
        exit(-1);
    }
    fprintf(OStrV, "`include \"%s.generated.vh\"\n\n", myName.c_str());
    for (auto IR : irSeq)
        generateModuleDef(IR, OStrV); // Generate verilog
    fclose(OStrV);
    FILE *OStrP = fopen((OutputDir + ".printf").c_str(), "w");
    for (auto item: printfFormat) {
        fprintf(OStrP, "ITEM %s: ", item.format.c_str());
        for (auto witem: item.width)
            fprintf(OStrP, "%d, ", witem);
        fprintf(OStrP, "\n");
    }
    fclose(OStrP);
}
