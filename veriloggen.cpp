/*
   Copyright (C) 2018 John Ankcorn

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

static int trace_assign;//= 1;
static int trace_expand;//= 1;

typedef struct {
    std::string argName;
    std::string value;
    std::string type;
    bool        moduleStart;
    int         out;
} ModData;
typedef struct {
    ACCExpr    *cond;
    ACCExpr    *value;
} MuxValueEntry;
typedef struct {
    std::string name;
    std::string base;
    std::string type;
    bool        alias;
    uint64_t    offset;
} FieldItem;
typedef struct {
    ACCExpr    *value;
    std::string type;
    bool        noReplace;
} AssignItem;
enum {PIN_NONE, PIN_MODULE, PIN_OBJECT, PIN_REG, PIN_WIRE, PIN_ALIAS};
typedef struct {
    int         count;
    std::string type;
    bool        out;
    int         pin;
} RefItem;
typedef struct {
    uint64_t    upper;
    ACCExpr    *value;
    std::string type;
} BitfieldPart;

static std::map<std::string, RefItem> refList;
static std::map<std::string, AssignItem> assignList;
static std::map<std::string, std::string> replaceTarget;
static std::map<std::string, std::map<uint64_t, BitfieldPart>> bitfieldList;

typedef ModuleIR *(^CBFun)(FieldElement &item, std::string fldName);
#define CBAct ^ ModuleIR * (FieldElement &item, std::string fldName)
static std::string getRdyName(std::string basename);
static uint64_t convertType(std::string arg);

#include "AtomiccExpr.h"
#include "AtomiccReadIR.h"

static std::string getRdyName(std::string basename)
{
    std::string rdyName = basename;
    if (endswith(rdyName, "__ENA"))
        rdyName = rdyName.substr(0, rdyName.length()-5);
    rdyName += "__RDY";
    return rdyName;
}

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

static ModuleIR *lookupIR(std::string ind)
{
    if (ind == "")
        return nullptr;
    return mapIndex[ind];
}

static uint64_t convertType(std::string arg)
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

static std::string sizeProcess(std::string type)
{
    uint64_t val = convertType(type);
    if (val > 1)
        return "[" + autostr(val - 1) + ":0]";
    return "";
}

static ModuleIR *iterField(ModuleIR *IR, CBFun cbWorker)
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

static MethodInfo *lookupMethod(ModuleIR *IR, std::string name)
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

static MethodInfo *lookupQualName(ModuleIR *searchIR, std::string searchStr)
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

static void getFieldList(std::list<FieldItem> &fieldList, std::string name, std::string base, std::string type, bool out, bool force = true, uint64_t offset = 0, bool alias = false, bool init = true)
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
        if (!refList[item].pin)
            printf("[%s:%d] refList[%s] definition missing\n", __FUNCTION__, __LINE__, item.c_str());
        assert(refList[item].pin);
        refList[item].count++;
        int ind = item.find('[');
        if (ind != -1)
{
if (trace_assign)
printf("[%s:%d] RRRRREFFFF %s -> %s\n", __FUNCTION__, __LINE__, expr->value.c_str(), item.c_str());
            refList[item.substr(0,ind)].count++;
}
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

std::string findType(std::string name)
{
//printf("[%s:%d] name %s\n", __FUNCTION__, __LINE__, name.c_str());
    if (refList.find(name) != refList.end())
        return refList[name].type;
printf("[%s:%d] reference to '%s', but could not locate RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR \n", __FUNCTION__, __LINE__, name.c_str());
    exit(-1);
    return "";
}
/*
 * Generate verilog module header for class definition or reference
 */
static void generateModuleSignature(ModuleIR *IR, std::string instance, std::list<ModData> &modParam)
{
    auto checkWire = [&](std::string name, std::string type, int dir) -> void {
        refList[instance + name] = RefItem{dir != 0 && instance == "", type, dir != 0, instance == "" ? PIN_MODULE : PIN_OBJECT};
        modParam.push_back(ModData{name, instance + name, type, false, dir});
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

/*
 * Generate *.v and *.vh for a Verilog module
 */
static void generateModuleDef(ModuleIR *IR, FILE *OStr)
{
static std::list<ModData> modLine;
    std::map<std::string, ACCExpr *> enableList;
    refList.clear();
    // 'Mux' together parameter settings from all invocations of a method from this class
    std::map<std::string, std::list<MuxValueEntry>> muxValueList;

    assignList.clear();
    replaceTarget.clear();
    bitfieldList.clear();
    modLine.clear();
    generateModuleSignature(IR, "", modLine);
    // Generate module header
    std::string sep = "module ";
    for (auto mitem: modLine) {
        static const char *dirStr[] = {"input", "output"};
        fprintf(OStr, "%s", sep.c_str());
        if (mitem.moduleStart)
            fprintf(OStr, "%s (input CLK, input nRST", mitem.value.c_str());
        else {
            fprintf(OStr, "%s %s%s", dirStr[mitem.out], sizeProcess(mitem.type).c_str(), mitem.value.c_str());
            expandStruct(IR, mitem.value, mitem.type, mitem.out, false, PIN_WIRE);
        }
        sep = ",\n    ";
    }
    fprintf(OStr, ");\n");
    modLine.clear();
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        if (MI->rule)
            refList[methodName] = RefItem{1, MI->type, true, PIN_WIRE}; // both RDY and ENA must be generated
    }
    for (auto item: IR->softwareName)
        fprintf(OStr, "// software: %s\n", item.c_str());
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
    for (auto mitem: modLine)
        if (!mitem.moduleStart)
            expandStruct(IR, mitem.value, mitem.type, mitem.out, false, PIN_WIRE);

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
    // generate wires for internal methods RDY/ENA.  Collect state element assignments
    // from each method
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
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

    // last chance to optimize out single assigns to output ports
    std::map<std::string, std::string> mapPort;
    for (auto item: assignList)
        if (refList[item.first].out && refList[item.first].pin == PIN_MODULE
          && item.second.value && isIdChar(item.second.value->value[0])) {
            mapPort[item.second.value->value] = item.first;
        }
    // process assignList replacements, mark referenced items
    std::list<ModData> modNew;
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
    std::list<std::string> alwaysLines;
    bool hasAlways = false;
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
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
        for (auto info: MI->printfList) {
            ACCExpr *cond = cleanupExpr(info->cond);
            ACCExpr *value = cleanupExpr(info->value);
printf("[%s:%d] PRINTFFFFFF\n", __FUNCTION__, __LINE__);
dumpExpr("PRINTCOND", cond);
dumpExpr("PRINTF", value);
            value = value->operands.front();
            value->value = "(";
            ACCExpr *format = value->operands.front();
            if (format->value == ",")
                format = format->operands.front();
            if (endswith(format->value, "\\n\""))
                format->value = format->value.substr(0, format->value.length()-3) + "\"";
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

    // generate local state element declarations and wires
    for (auto item: refList) {
        if (item.second.pin == PIN_REG) {
        hasAlways = true;
        fprintf(OStr, "    reg %s;\n", (sizeProcess(item.second.type) + item.first).c_str());
        }
    }
    for (auto item: refList) {
        std::string temp = item.first;
        //int ind = temp.find('[');
        //if (ind != -1)
            //temp = temp.substr(0,ind);
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
    std::string endStr;
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

    // generate 'assign' items
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

static void walkSubscript (ModuleIR *IR, ACCExpr *expr)
{
    if (!expr)
        return;
    for (auto item: expr->operands)
        walkSubscript(IR, item);
    std::string fieldName = expr->value;
    if (!isIdChar(fieldName[0]) || !expr->operands.size() || expr->operands.front()->value != "[")
        return;
    ACCExpr *subscript = expr->operands.front()->operands.front();
    expr->operands.pop_front();
    std::string post;
    if (expr->operands.size() && isIdChar(expr->operands.front()->value[0])) {
        post = expr->operands.front()->value;
        expr->operands.pop_front();
    }
    int size = -1;
    for (auto item: IR->fields)
        if (item.fldName == fieldName) {
            size = item.vecCount;
            break;
        }
printf("[%s:%d] ARRAAA size %d '%s' post '%s'\n", __FUNCTION__, __LINE__, size, fieldName.c_str(), post.c_str());
    assert (!isdigit(subscript->value[0]));
    std::string lastElement = fieldName + autostr(size - 1) + post;
    expr->value = lastElement; // if only 1 element
    for (int i = 0; i < size - 1; i++) {
        std::string ind = autostr(i);
        expr->value = "?";
        expr->operands.push_back(allocExpr("==", subscript, allocExpr(ind)));
        expr->operands.push_back(allocExpr(fieldName + ind + post));
        if (i == size - 2)
            expr->operands.push_back(allocExpr(lastElement));
        else {
            ACCExpr *nitem = allocExpr("");
            expr->operands.push_back(nitem);
            expr = nitem;
        }
    }
}
static ACCExpr *findSubscript (ModuleIR *IR, ACCExpr *expr, int &size, std::string &fieldName, ACCExpr **subscript, std::string &post)
{
    if (isIdChar(expr->value[0]) && expr->operands.size() && expr->operands.front()->value == "[") {
        fieldName = expr->value;
        *subscript = expr->operands.front()->operands.front();
        expr->operands.pop_front();
        if (expr->operands.size() && isIdChar(expr->operands.front()->value[0])) {
            post = expr->operands.front()->value;
            expr->operands.pop_front();
        }
        for (auto item: IR->fields)
            if (item.fldName == expr->value) {
                size = item.vecCount;
                break;
            }
        return expr;
    }
    for (auto item: expr->operands)
        if (ACCExpr *ret = findSubscript(IR, item, size, fieldName, subscript, post))
            return ret;
    return nullptr;
}
static ACCExpr *cloneReplaceTree (ACCExpr *expr, ACCExpr *target)
{
    ACCExpr *newExpr = allocExpr(expr->value);
    for (auto item: expr->operands)
        newExpr->operands.push_back(expr == target ? item : cloneReplaceTree(item, target));
    return newExpr;
}

static ModuleIR *allocIR(std::string name)
{
    ModuleIR *IR = new ModuleIR;
    IR->name = name;
    mapIndex[IR->name] = IR;
    return IR;
}

static void processSerialize(ModuleIR *IR)
{
    std::string prefix = "__" + IR->name + "_";
    auto inter = IR->interfaces.front();
    ModuleIR *IIR = lookupIR(inter.type);
    IR->fields.clear();
    IR->fields.push_back(FieldElement{"len", -1, "INTEGER_16", 0, false});
    IR->fields.push_back(FieldElement{"tag", -1, "INTEGER_16", 0, false});
    ModuleIR *unionIR = allocIR(prefix + "UNION");
    IR->fields.push_back(FieldElement{"data", -1, unionIR->name, 0, false});
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
            variant->fields.push_back(FieldElement{param.name, -1, param.type, 0, false});
            dataLength += convertType(param.type);
        }
        if (dataLength > maxDataLength)
            maxDataLength = dataLength;
        counter++;
    }
    unionIR->fields.push_back(FieldElement{"data", -1, "INTEGER_" + autostr(maxDataLength), 0, false});
}

static void dumpModule(std::string name, ModuleIR *IR)
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
        MethodInfo *MInew = new MethodInfo{nullptr};
        MInew->name = methodName;
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
        MethodInfo *MInew = new MethodInfo{nullptr};
        MInew->name = host + MODULE_SEPARATOR + methodName;
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

static void processInterfaces(std::list<ModuleIR *> &irSeq)
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

#if 1
static void metaGenerate(ModuleIR *IR, FILE *OStr)
{
    std::map<std::string, int> exclusiveSeen;
    std::list<std::string>     metaList;
    // write out metadata comments at end of the file
    metaList.push_front("//METASTART; " + IR->name);
    for (auto item: IR->interfaces)
        if (item.isPtr)
        metaList.push_back("//METAEXTERNAL; " + item.fldName + "; " + lookupIR(item.type)->name + ";");
    for (auto item: IR->fields) {
        int64_t vecCount = item.vecCount;
        int dimIndex = 0;
        if (lookupIR(item.type))
        do {
            std::string fldName = item.fldName;
            if (vecCount != -1)
                fldName += autostr(dimIndex++);
            if (item.isPtr)
                metaList.push_back("//METAEXTERNAL; " + fldName + "; " + lookupIR(item.type)->name + ";");
            else if (!startswith(lookupIR(item.type)->name, "l_struct_OC_")
                 && !startswith(lookupIR(item.type)->name, "l_ainterface"))
                metaList.push_back("//METAINTERNAL; " + fldName + "; " + lookupIR(item.type)->name + ";");
        } while(--vecCount > 0);
    }
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        std::string gtemp = "; " + tree2str(MI->guard) + ";";
        if (endswith(methodName, "__RDY"))
            metaList.push_back("//METAGUARD; "
                + methodName.substr(0, methodName.length()-5) + gtemp);
        else if (endswith(methodName, "__READY"))
            metaList.push_back("//METAGUARDV; "
                + methodName.substr(0, methodName.length()-7) + gtemp);
        else {
            // For each method/rule of the current class,
            // gather up metadata generated by processFunction
            MetaRef *bm = MI->meta;
            std::string temp;
            for (auto titem: bm[MetaInvoke])
                for (auto item: titem.second)
                    temp += item + ":" + titem.first + ";";
            if (temp != "")
                metaList.push_back("//METAINVOKE; " + methodName + "; " + temp);
            std::map<std::string,std::string> metaBefore;
            std::map<std::string,std::string> metaConflict;
            for (auto innerFI : IR->method) {
                MethodInfo *innerMI = innerFI.second;
                std::string innermethodName = innerMI->name;
                MetaRef *innerbm = innerMI->meta;
                std::string tempConflict;
                if (innermethodName == methodName)
                    continue;
                // scan all other rule/methods of this class
                for (auto inneritem: innerMI->storeList) {
                    for (auto item: bm[MetaRead])
                        // if the current method reads a state element that
                        // is written by another method, add it to the 'before' list
                        if (item.first == inneritem->dest->value) {
printf("[%s:%d] innermethodName %s before conflict '%s' innerunc %s methodName %s\n", __FUNCTION__, __LINE__, innermethodName.c_str(), item.first.c_str(), innermethodName.c_str(), methodName.c_str());
                            metaBefore[innermethodName] = "; :";
                            break;
                        }
                    for (auto item: MI->storeList)
                        // if the current method writes a state element that
                        // is written by another method, add it to the 'conflict' list
                        if (tree2str(item->dest) == tree2str(inneritem->dest)) {
                            metaConflict[innermethodName] = "; ";
                            break;
                        }
                }
                for (auto inneritem: innerbm[MetaInvoke]) {
                    for (auto item: bm[MetaInvoke])
                        if (item.first == inneritem.first) {
//printf("[%s:%d] conflict methodName %s innermethodName %s item %s\n", __FUNCTION__, __LINE__, methodName.c_str(), innermethodName.c_str(), item.first.c_str());
                            metaConflict[innermethodName] = "; ";
                            break;
                        }
                }
            }
            std::string metaStr;
            for (auto item: metaConflict)
                 if (item.second != "" && !exclusiveSeen[item.first])
                     metaStr += item.second + item.first;
            exclusiveSeen[methodName] = 1;
            if (metaStr != "")
                metaList.push_back("//METAEXCLUSIVE; " + methodName + metaStr);
            metaStr = "";
            for (auto item: metaBefore)
                 if (item.second != "")
                     metaStr += item.second + item.first;
            if (metaStr != "")
                metaList.push_back("//METABEFORE; " + methodName + metaStr);
        }
    }
    std::string ruleNames;
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        if (MI->rule && endswith(methodName, "__ENA"))
            ruleNames += "; " + methodName.substr(0, methodName.length()-5);
    }
    if (ruleNames != "")
        metaList.push_back("//METARULES" + ruleNames);
    for (auto item: IR->interfaceConnect) {
        std::string tname = item.target;
        std::string sname = item.source;
printf("[%s:%d] METACONNECT %s %s\n", __FUNCTION__, __LINE__, tname.c_str(), sname.c_str());
        for (auto FI: lookupIR(item.type)->method) {
            MethodInfo *MI = FI.second;
            std::string methodName = MI->name;
            metaList.push_back("//METACONNECT; " + tname + MODULE_SEPARATOR + MI->name
                                              + "; " + sname + MODULE_SEPARATOR + MI->name);
        }
    }
    for (auto item : IR->priority)
        metaList.push_back("//METAPRIORITY; " + item.first + "; " + item.second);
    for (auto item : metaList)
        fprintf(OStr, "%s\n", item.c_str());
}
#else
static void metaGenerate(ModuleIR *IR, FILE *OStr)
{
    std::map<std::string, int> exclusiveSeen;
    std::list<std::string>     metaList;
    // write out metadata comments at end of the file
    metaList.push_front("//METASTART; " + IR->name);
    for (auto item: IR->interfaces)
        if (item.isPtr)
        metaList.push_back("//METAEXTERNAL; " + item.fldName + "; " + lookupIR(item.type)->name + ";");
    for (auto item: IR->fields) {
        int64_t vecCount = item.vecCount;
        int dimIndex = 0;
        if (lookupIR(item.type))
        do {
            std::string fldName = item.fldName;
            if (vecCount != -1)
                fldName += autostr(dimIndex++);
            if (item.isPtr)
                metaList.push_back("//METAEXTERNAL; " + fldName + "; " + lookupIR(item.type)->name + ";");
            else if (!startswith(lookupIR(item.type)->name, "l_struct_OC_")
                 && !startswith(lookupIR(item.type)->name, "l_ainterface"))
                metaList.push_back("//METAINTERNAL; " + fldName + "; " + lookupIR(item.type)->name + ";");
        } while(--vecCount > 0);
    }
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        std::string gtemp = "; " + tree2str(lookupMethod(IR, methodName)->guard) + ";";
        if (endswith(methodName, "__RDY"))
            metaList.push_back("//METAGUARD; "
                + methodName.substr(0, methodName.length()-5) + gtemp);
        else if (endswith(methodName, "__READY"))
            metaList.push_back("//METAGUARDV; "
                + methodName.substr(0, methodName.length()-7) + gtemp);
        else {
            // For each method/rule of the current class,
            // gather up metadata generated by processFunction
            MetaRef *bm = lookupMethod(IR, methodName)->meta;
            std::string temp;
            for (auto titem: bm[MetaInvoke])
                for (auto item: titem.second)
                    temp += item + ":" + titem.first + ";";
            if (temp != "")
                metaList.push_back("//METAINVOKE; " + methodName + "; " + temp);
            std::map<std::string,std::string> metaBefore;
            std::map<std::string,std::string> metaConflict;
            for (auto innerFI : IR->method) {
                MethodInfo *innerMI = innerFI.second;
                std::string innermethodName = innerMI->name;
                MetaRef *innerbm = lookupMethod(IR, innermethodName)->meta;
                std::string tempConflict;
                if (innermethodName == methodName)
                    continue;
                // scan all other rule/methods of this class
                for (auto inneritem: lookupMethod(IR, innermethodName)->storeList) {
                    for (auto item: bm[MetaRead])
                        // if the current method reads a state element that
                        // is written by another method, add it to the 'before' list
                        if (item.first == inneritem->dest->value) {
printf("[%s:%d] innermethodName %s before conflict '%s' innerunc %s methodName %s\n", __FUNCTION__, __LINE__, innermethodName.c_str(), item.first.c_str(), innermethodName.c_str(), methodName.c_str());
                            metaBefore[innermethodName] = "; :";
                            break;
                        }
                    for (auto item: lookupMethod(IR, methodName)->storeList)
                        // if the current method writes a state element that
                        // is written by another method, add it to the 'conflict' list
                        if (tree2str(item->dest) == tree2str(inneritem->dest)) {
                            metaConflict[innermethodName] = "; ";
                            break;
                        }
                }
                for (auto inneritem: innerbm[MetaInvoke]) {
                    for (auto item: bm[MetaInvoke])
                        if (item.first == inneritem.first) {
//printf("[%s:%d] conflict methodName %s innermethodName %s item %s\n", __FUNCTION__, __LINE__, methodName.c_str(), innermethodName.c_str(), item.first.c_str());
                            metaConflict[innermethodName] = "; ";
                            break;
                        }
                }
            }
            std::string metaStr;
            for (auto item: metaConflict)
                 if (item.second != "" && !exclusiveSeen[item.first])
                     metaStr += item.second + item.first;
            exclusiveSeen[methodName] = 1;
            if (metaStr != "")
                metaList.push_back("//METAEXCLUSIVE; " + methodName + metaStr);
            metaStr = "";
            for (auto item: metaBefore)
                 if (item.second != "")
                     metaStr += item.second + item.first;
            if (metaStr != "")
                metaList.push_back("//METABEFORE; " + methodName + metaStr);
        }
    }
    std::string ruleNames;
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        if (FI.second->rule && endswith(methodName, "__ENA"))
            ruleNames += "; " + methodName.substr(0, methodName.length()-5);
    }
    if (ruleNames != "")
        metaList.push_back("//METARULES" + ruleNames);
    for (auto item: IR->interfaceConnect) {
        std::string tname = item.target;
        std::string sname = item.source;
printf("[%s:%d] METACONNECT %s %s\n", __FUNCTION__, __LINE__, tname.c_str(), sname.c_str());
        for (auto FI: lookupIR(item.type)->method) {
            MethodInfo *MI = FI.second;
            std::string methodName = MI->name;
            metaList.push_back("//METACONNECT; " + tname + MODULE_SEPARATOR + methodName
                                              + "; " + sname + MODULE_SEPARATOR + methodName);
        }
    }
    for (auto item : IR->priority)
        metaList.push_back("//METAPRIORITY; " + item.first + "; " + item.second);
    for (auto item : metaList)
        fprintf(OStr, "%s\n", item.c_str());
}
#endif

static const char *jsonPrefix = 
    "{\n"
    "    \"bsvdefines\": [ ],\n"
    "    \"globaldecls\": [\n"
    "        { \"dtype\": \"TypeDef\",\n"
    "            \"tdtype\": { \"name\": \"Bit\", \"params\": [ { \"name\": \"32\" } ] },\n"
    "            \"tname\": \"SpecialTypeForSendingFd\"\n"
    "        },\n"
    "        { \"dtype\": \"TypeDef\",\n"
    "            \"tdtype\": {\n"
    "                \"elements\": [ %s ],\n"
    "                \"name\": \"IfcNames\",\n"
    "                \"type\": \"Enum\"\n"
    "            },\n"
    "            \"tname\": \"IfcNames\"\n"
    "        }\n"
    "    ],\n"
    "    \"interfaces\": [";
static ModuleIR *extractInterface(ModuleIR *IR, std::string interfaceName, bool &outbound)
{
    std::string type;
    for (auto iitem: IR->interfaces) {
        if (iitem.fldName == interfaceName) {
            type = iitem.type;
            outbound = iitem.isPtr;
            break;
        }
    }
    if (ModuleIR *exportedInterface = lookupIR(type))
    if (MethodInfo *PMI = lookupMethod(exportedInterface, "enq")) // must be a pipein
    if (ModuleIR *data = lookupIR(PMI->params.front().type))
        return lookupIR(data->interfaces.front().type);
    return nullptr;
}
static void jsonPrepare(std::list<ModuleIR *> &irSeq, std::map<std::string, bool> &softwareNameList, FILE *OStrJ)
{
    for (auto IR: irSeq) {
        for (auto interfaceName: IR->softwareName) {
            bool outbound = false;
            if (ModuleIR *inter = extractInterface(IR, interfaceName, outbound))
                softwareNameList[inter->name.substr(strlen("l_ainterface_OC_"))] = outbound;
        }
    }
}
static std::string jsonSep;
static void jsonGenerate(ModuleIR *IR, FILE *OStrJ)
{
    for (auto interfaceName: IR->softwareName) {
        fprintf(OStrJ, "%s\n        { \"cdecls\": [", jsonSep.c_str());
        bool outbound = false;
        if (ModuleIR *inter = extractInterface(IR, interfaceName, outbound)) {
            std::map<std::string, MethodInfo *> reorderList;
            for (auto FI : inter->method) {
                MethodInfo *MI = FI.second;
                std::string methodName = MI->name;
                if (!endswith(methodName, "__ENA"))
                    continue;
                reorderList[methodName.substr(0, methodName.length()-5)] = MI;
            }
            std::string msep;
            for (auto item: reorderList) {
                MethodInfo *MI = item.second;
                std::string methodName = item.first; // reorderList, not method!!
                std::string psep;
                fprintf(OStrJ, "%s\n                { \"dname\": \"%s\", \"dparams\": [",
                     msep.c_str(), methodName.c_str());
                for (auto pitem: MI->params) {
                     fprintf(OStrJ, "%s\n                        { \"pname\": \"%s\", "
                         "\"ptype\": { \"name\": \"Bit\", \"params\": [ { "
                         "\"name\": \"%d\" } ] } }",
                         psep.c_str(), pitem.name.c_str(), (int)convertType(pitem.type));
                     psep = ",";
                }
                fprintf(OStrJ, "\n                    ] }");
                msep = ",";
            }
            fprintf(OStrJ, "\n            ], \"direction\": \"%d\", \"cname\": \"%s\" }",
                outbound, inter->name.substr(strlen("l_ainterface_OC_")).c_str());
            jsonSep = ",";
        }
    }
}

int main(int argc, char **argv)
{
    bool noVerilator = false;
noVerilator = true;
printf("[%s:%d] VERILOGGGEN\n", __FUNCTION__, __LINE__);
    int argIndex = 1;
    if (argc == 3 && !strcmp(argv[argIndex], "-n")) {
        argIndex++;
        noVerilator = true;
    }
    if (argc - 1 != argIndex) {
        printf("[%s:%d] veriloggen <outputFileStem>\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    std::string OutputDir = argv[argIndex];
    std::string myName = OutputDir;
    int ind = myName.rfind('/');
    if (ind > 0)
        myName = myName.substr(ind+1);
    FILE *OStrIRread = fopen((OutputDir + ".generated.IR").c_str(), "r");
    if (!OStrIRread) {
        printf("veriloggen: unable to open '%s'\n", (OutputDir + ".generated.IR").c_str());
        exit(-1);
    }
    FILE *OStrV = fopen((OutputDir + ".generated.v").c_str(), "w");
    if (!OStrV) {
        printf("veriloggen: unable to open '%s'\n", (OutputDir + ".generated.v").c_str());
        exit(-1);
    }
    FILE *OStrVH = fopen((OutputDir + ".generated.vh").c_str(), "w");
    FILE *OStrJ = nullptr;
    fprintf(OStrV, "`include \"%s.generated.vh\"\n\n", myName.c_str());
    myName += "_GENERATED_";
    fprintf(OStrVH, "`ifndef __%s_VH__\n`define __%s_VH__\n\n", myName.c_str(), myName.c_str());
    std::list<ModuleIR *> irSeq;
    readModuleIR(irSeq, OStrIRread);
    processInterfaces(irSeq);
    std::map<std::string, bool> softwareNameList;
    jsonPrepare(irSeq, softwareNameList, OStrJ);
    if (softwareNameList.size() > 0) {
        int counter = 5;
        std::string enumList, sep;
        for (auto item: softwareNameList) {
            std::string name = "IfcNames_" + item.first + (item.second ? "H2S" : "S2H");
            enumList += sep + "[ \"" + name + "\", \"" + autostr(counter++) + "\" ]";
            sep = ", ";
        }
        OStrJ = fopen((OutputDir + ".generated.json").c_str(), "w");
        fprintf(OStrJ, jsonPrefix, enumList.c_str());
    }
    for (auto IR : irSeq) {
        // expand all subscript calculations before processing the module
        for (auto item: IR->method) {
            MethodInfo *MI = item.second;
            std::string methodName = MI->name;
            walkSubscript(IR, MI->guard);
            for (auto item: MI->storeList) {
                walkSubscript(IR, item->cond);
                walkSubscript(IR, item->value);
            }
            for (auto item: MI->letList) {
                walkSubscript(IR, item->cond);
                walkSubscript(IR, item->value);
            }
            for (auto item: MI->callList)
                walkSubscript(IR, item->cond);
            for (auto item: MI->printfList)
                walkSubscript(IR, item->cond);
            // subscript processing for calls/assigns requires that we defactor the entire statement,
            // not just add a condition expression into the tree
            auto expandTree = [&] (int sort, ACCExpr **condp, ACCExpr *expandArg, bool isAction = false, ACCExpr *value = nullptr, std::string type = "") -> void {
                int size = -1;
                ACCExpr *cond = *condp, *subscript = nullptr;
                std::string fieldName, post;
                if (ACCExpr *expr = findSubscript(IR, expandArg, size, fieldName, &subscript, post)) {
                    for (int ind = 0; ind < size; ind++) {
                        ACCExpr *newCond = allocExpr("==", subscript, allocExpr(autostr(ind)));
                        if (cond)
                            newCond = allocExpr("&", cond, newCond);
                        expr->value = fieldName + autostr(ind) + post;
                        if (ind == 0)
                            *condp = newCond;
                        else {
                            ACCExpr *newExpand = cloneReplaceTree(expandArg, expr);
                            if (sort == 1)
                                MI->storeList.push_back(new StoreListElement{newExpand, value, newCond});
                            else if (sort == 1)
                                MI->letList.push_back(new LetListElement{newExpand, value, newCond, type});
                            else if (sort == 3)
                                MI->callList.push_back(new CallListElement{newExpand, newCond, isAction});
                            else if (sort == 4)
                                MI->printfList.push_back(new CallListElement{newExpand, newCond, isAction});
                        }
                    }
                    expr->value = fieldName + "0" + post;
                }
            };
            for (auto item: MI->storeList)
                expandTree(1, &item->cond, item->dest, false, item->value);
            for (auto item: MI->letList)
                expandTree(2, &item->cond, item->dest, false, item->value, item->type);
            for (auto item: MI->callList)
                expandTree(3, &item->cond, item->value, item->isAction);
            for (auto item: MI->printfList)
                expandTree(4, &item->cond, item->value, item->isAction);
        }
        // Generate verilog
        generateModuleDef(IR, OStrV);
        // now generate the verilog header file '.vh'
        metaGenerate(IR, OStrVH);
        if (OStrJ)
            jsonGenerate(IR, OStrJ);
    }
    fprintf(OStrVH, "`endif\n");
    fclose(OStrIRread);
    fclose(OStrV);
    fclose(OStrVH);
    if (OStrJ) {
        fprintf(OStrJ, "\n    ]\n}\n");
        fclose(OStrJ);
        std::string commandLine(argv[0]);
        int ind = commandLine.rfind("/");
        if (ind == -1)
            commandLine = "";
        else
            commandLine = commandLine.substr(0,ind+1);
        commandLine += "scripts/atomiccCppgen.py " + OutputDir + ".generated.json";
        int ret = system(commandLine.c_str());
        printf("[%s:%d] RETURN from '%s' %d\n", __FUNCTION__, __LINE__, commandLine.c_str(), ret);
        if (ret)
            return -1; // force error return to be propagated
        FILE *OStrFL = fopen((OutputDir + ".generated.filelist").c_str(), "w");
        std::string flist = "GENERATED_CPP = jni/GeneratedCppCallbacks.cpp \\\n   ";
        for (auto item: softwareNameList)
             flist += " jni/" + item.first + ".c";
        fprintf(OStrFL, "%s\n", flist.c_str());
        fclose(OStrFL);
    }
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (!noVerilator) {
        std::string commandLine = "verilator --cc " + OutputDir + ".generated.v";
        int ret = system(commandLine.c_str());
        printf("[%s:%d] RETURN from '%s' %d\n", __FUNCTION__, __LINE__, commandLine.c_str(), ret);
        if (ret)
            return -1; // force error return to be propagated
    }
    return 0;
}
