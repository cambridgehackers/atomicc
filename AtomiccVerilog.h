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
#include <stdlib.h> // atol
#include "AtomiccIR.h"

typedef struct {
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
enum {PIN_NONE, PIN_MODULE, PIN_OBJECT, PIN_DATA, PIN_REG, PIN_WIRE, PIN_ALIAS};
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

static int trace_assign;//= 1;
static int trace_expand;//= 1;

static std::map<std::string, RefItem> refList;
static std::map<std::string, AssignItem> assignList;
static std::map<std::string, std::string> replaceTarget;
static std::map<std::string, std::map<uint64_t, BitfieldPart>> bitfieldList;

typedef ModuleIR *(^CBFun)(FieldElement &item, std::string fldName);
#define CBAct ^ ModuleIR * (FieldElement &item, std::string fldName)

static void setAssign(std::string target, ACCExpr *value, std::string type, bool noReplace = false)
{
    std::string temp = replaceTarget[target];
    if (temp != "") {
printf("[%s:%d] ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ replace %s -> %s\n", __FUNCTION__, __LINE__, target.c_str(), temp.c_str());
        target = temp;
    }
    bool tDir = refList[target].out;
    if (value) {
if (trace_assign || !tDir) printf("[%s:%d] start [%s/%d] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tDir, tree2str(value).c_str(), type.c_str());
    //assert(tDir || noReplace);
    if (!refList[target].pin) {
        printf("[%s:%d] missing target [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        exit(-1);
    }
if (isIdChar(value->value[0]) && !noReplace) {
bool sDir = refList[value->value].out;
printf("[%s:%d] %s/%d = %s/%d\n", __FUNCTION__, __LINE__, target.c_str(), tDir, value->value.c_str(), sDir);
}
if (assignList[target].type != "") {
printf("[%s:%d] duplicate start [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
printf("[%s:%d] duplicate was      = %s type '%s'\n", __FUNCTION__, __LINE__, tree2str(assignList[target].value).c_str(), assignList[target].type.c_str());
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
            return lookupIR(item.type)->method[searchStr];
    return NULL;
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
printf("[%s:%d] FFFFF name %s base %s type %s %s offset %d\n", __FUNCTION__, __LINE__, fitem.name.c_str(), fitem.base.c_str(), fitem.type.c_str(), fitem.alias ? "ALIAS" : "", (int)fitem.offset);
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
        std::string fnew = base + "[" + autostr(offset) + ":" + autostr(upper) + "]";
if (trace_expand)
printf("[%s:%d] set %s = %s out %d alias %d base %s , %s[%d : %d] fnew %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), fitem.type.c_str(), out, fitem.alias, base.c_str(), fldName.c_str(), (int)offset, (int)upper, fnew.c_str());
        assert (!refList[fitem.name].pin);
        refList[fitem.name] = RefItem{0, fitem.type, out != 0, fitem.alias ? PIN_WIRE : pin};
        refList[fnew] = RefItem{0, fitem.type, out != 0, PIN_ALIAS};
        if (!fitem.alias && out)
            itemList->operands.push_back(allocExpr(fitem.name));
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
        refList[name] = RefItem{dir != 0 && instance == "", type, dir != 0, instance == "" ? PIN_MODULE : PIN_OBJECT};
        modParam.push_back(ModData{name, type, false, dir});
    };
//printf("[%s:%d] name %s instance %s\n", __FUNCTION__, __LINE__, IR->name.c_str(), instance.c_str());
    modParam.push_back(ModData{IR->name + ((instance != "") ? " " + instance.substr(0, instance.length()-1):""), "", true, 0});
    for (auto item : IR->interfaces)
        for (auto FI: lookupIR(item.type)->method) {
            MethodInfo *MI = FI.second;
            std::string name = instance + item.fldName + MODULE_SEPARATOR + FI.first;
            bool out = (instance != "") ^ item.isPtr;
            checkWire(name, MI->type, out ^ (MI->type != ""));
            for (auto pitem: MI->params)
                checkWire(name.substr(0, name.length()-5) + MODULE_SEPARATOR + pitem.name, pitem.type, out);
        }
}

/*
 * Generate *.v and *.vh for a Verilog module
 */
void generateModuleDef(ModuleIR *IR, FILE *OStr)
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
            fprintf(OStr, "%s (\n    input CLK,\n    input nRST", mitem.value.c_str());
        else {
            fprintf(OStr, "%s %s%s", dirStr[mitem.out], sizeProcess(mitem.type).c_str(), mitem.value.c_str());
            expandStruct(IR, mitem.value, mitem.type, mitem.out, false, PIN_WIRE);
        }
        sep = ",\n    ";
    }
    fprintf(OStr, ");\n");
    modLine.clear();
    for (auto FI : IR->method) {
        std::string methodName = FI.first;
        MethodInfo *MI = FI.second;
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
            std::string tstr = IC.target + MODULE_SEPARATOR + FI.first,
                        sstr = IC.source + MODULE_SEPARATOR + FI.first;
//printf("[%s:%d] IFCCC %s/%d %s/%d\n", __FUNCTION__, __LINE__, tstr.c_str(), refList[tstr].out, sstr.c_str(), refList[sstr].out);
            if (refList[sstr].out)
                setAssign(sstr, allocExpr(tstr), FI.second->type);
            else
                setAssign(tstr, allocExpr(sstr), FI.second->type);
            tstr = tstr.substr(0, tstr.length()-5) + MODULE_SEPARATOR;
            sstr = sstr.substr(0, sstr.length()-5) + MODULE_SEPARATOR;
            for (auto info: FI.second->params)
                setAssign(sstr + info.name, allocExpr(tstr + info.name), info.type);
        }
    // generate wires for internal methods RDY/ENA.  Collect state element assignments
    // from each method
    for (auto FI : IR->method) {
        std::string methodName = FI.first;
        MethodInfo *MI = FI.second;
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
            walkRead(MI, info.cond, nullptr);
            walkRead(MI, info.value, info.cond);
            std::list<FieldItem> fieldList;
            getFieldList(fieldList, "", "", info.type, false, true);
            for (auto fitem : fieldList) {
                std::string dest = info.dest->value + fitem.name;
                std::string src = info.value->value + fitem.name;
                muxValueList[dest].push_back(MuxValueEntry{info.cond, allocExpr(src)});
            }
        }
        for (auto info: MI->callList) {
            if (isIdChar(info->value->value[0]) && info->value->operands.size() && info->value->operands.front()->value == "{")
                MI->meta[MetaInvoke][info->value->value].insert(tree2str(info->cond));
            else {
                printf("[%s:%d] called method name not found %s\n", __FUNCTION__, __LINE__, tree2str(info->value).c_str());
dumpExpr("READCALL", info->value);
                    exit(-1);
            }
            walkRead(MI, info->cond, nullptr);
            walkRead(MI, info->value, info->cond);
            if (!info->isAction)
                continue;
            ACCExpr *tempCond = allocExpr(methodName);
            if (info->cond) {
                ACCExpr *temp = info->cond;
                if (temp->value != "&")
                    temp = allocExpr("&", temp);
                temp->operands.push_back(tempCond);
                tempCond = temp;
            }
            std::string calledName = info->value->value;
printf("[%s:%d] CALLLLLL '%s'\n", __FUNCTION__, __LINE__, calledName.c_str());
            if (!info->value->operands.size() || info->value->operands.front()->value != "{") {
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
            ACCExpr *param = info->value->operands.front()->operands.front();
printf("[%s:%d] param '%s'\n", __FUNCTION__, __LINE__, tree2str(param).c_str());
//dumpExpr("param", param);
            auto setParam = [&] (ACCExpr *item) -> void {
                if(argCount-- > 0) {
printf("[%s:%d] infmuxVL[%s] = cond '%s' tree '%s'\n", __FUNCTION__, __LINE__, (pname + AI->name).c_str(), tree2str(tempCond).c_str(), tree2str(item).c_str());
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
            assignList[item.first + "[" + autostr(bitem.first) + ":" + autostr(bitem.second.upper) + "]"].value = nullptr;
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

    // process assignList replacements, mark referenced items
    std::list<ModData> modNew;
    for (auto mitem: modLine) {
        std::string val = mitem.value;
        if (!mitem.moduleStart)
            val = walkTree(allocExpr(mitem.value), nullptr);
        modNew.push_back(ModData{val, mitem.type, mitem.moduleStart, mitem.out});
    }
    std::list<std::string> alwaysLines;
    bool hasAlways = false;
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        bool alwaysSeen = false;
        std::map<std::string, std::list<std::string>> condLines;
        for (auto info: FI.second->storeList) {
            walkRead(MI, info.cond, nullptr);
            walkRead(MI, info.value, info.cond);
    ACCExpr *destt = info.dest;
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
            hasAlways = true;
            if (!alwaysSeen)
                alwaysLines.push_back("if (" + FI.first + ") begin");
            alwaysSeen = true;
            std::string cond;
            if (info.cond)
                cond = "    if (" + walkTree(info.cond, nullptr) + ")";
            condLines[cond].push_back("    " + tree2str(destt) + " <= " + walkTree(info.value, nullptr) + ";");
        }
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
        if (alwaysSeen)
            alwaysLines.push_back("end; // End of " + FI.first);
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
if (item.second.out) {
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
            fprintf(OStr, "%s (\n        CLK,\n        nRST,", (endStr + "    " + item.value).c_str());
            sep = "";
        }
        else {
            fprintf(OStr, "%s", (sep + "\n        " + item.value).c_str());
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

std::string getRdyName(std::string basename)
{
    std::string rdyName = basename;
    if (endswith(rdyName, "__ENA"))
        rdyName = rdyName.substr(0, rdyName.length()-5);
    rdyName += "__RDY";
    return rdyName;
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

void generateModuleIR(std::list<ModuleIR *> &irSeq, FILE *OStrVH, FILE *OStrV)
{
    for (auto IR : irSeq) {
        for (auto item: IR->method) {
            std::string methodName = item.first;
            MethodInfo *MI = item.second;
            walkSubscript(IR, MI->guard);
            for (auto item: MI->storeList) {
                walkSubscript(IR, item.cond);
                walkSubscript(IR, item.dest);
                walkSubscript(IR, item.value);
            }
            for (auto item: MI->letList) {
                walkSubscript(IR, item.cond);
                walkSubscript(IR, item.dest);
                walkSubscript(IR, item.value);
            }
            for (auto item: MI->callList)
                walkSubscript(IR, item->cond);
            for (auto item: MI->callList) {
                int size = -1;
                ACCExpr *cond = item->cond, *subscript = nullptr;
                std::string fieldName, post;
                if (ACCExpr *expr = findSubscript(IR, item->value, size, fieldName, &subscript, post)) {
                    for (int ind = 0; ind < size; ind++) {
                        ACCExpr *newCond = allocExpr("==", subscript, allocExpr(autostr(ind)));
                        if (cond)
                            newCond = allocExpr("&", cond, newCond);
                        expr->value = fieldName + autostr(ind) + post;
                        if (ind == 0)
                            item->cond = newCond;
                        else
                            MI->callList.push_back(new CallListElement{
                                cloneReplaceTree(item->value, expr), newCond, item->isAction});
                    }
                    expr->value = fieldName + "0" + post;
                }
            }
            // lift guards from called method interfaces
            if (!endswith(methodName, "__RDY"))
            if (MethodInfo *MIRdy = IR->method[getRdyName(methodName)])
            for (auto item: MI->callList) {
                ACCExpr *tempCond = allocExpr(getRdyName(item->value->value));
                if (item->cond) {
                    ACCExpr *icon = invertExpr(item->cond);
                    if (icon->value != "|")
                        icon = allocExpr("|", icon);
                    icon->operands.push_back(tempCond);
                    tempCond = icon;
                }
                if (MIRdy->guard->value == "1")
                    MIRdy->guard = tempCond;
                else {
                    if (MIRdy->guard->value != "&")
                        MIRdy->guard = allocExpr("&", MIRdy->guard);
                    MIRdy->guard->operands.push_back(tempCond);
                }
            }
        }
        // Only generate verilog for modules derived from Module
        generateModuleDef(IR, OStrV);
        // now generate the verilog header file '.vh'
        metaGenerate(IR, OStrVH);
    }
}
