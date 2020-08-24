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
int trace_ports;//= 1;
int trace_connect;//= 1;
int trace_skipped;//= 1;

std::list<PrintfInfo> printfFormat;
typedef std::list<ModData> ModList;
ModList modNew;
std::map<std::string, CondLineType> condLines;
std::map<std::string, SyncPinsInfo> syncPins;    // SyncFF items needed for PipeInSync instances

std::map<std::string, AssignItem> assignList;
std::map<std::string, std::map<std::string, AssignItem>> condAssignList; // used for 'generate' items
typedef struct {
    ACCExpr *phi;
    std::string defaultValue;
} MuxValueElement;
std::map<std::string, std::map<std::string, MuxValueElement>> enableList, muxValueList; // used for 'generate' items
static std::string generateSection; // for 'generate' regions, this is the top level loop expression, otherwise ''

static void traceZero(const char *label)
{
    if (trace_assign)
    for (auto aitem: assignList) {
        std::string temp = aitem.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if (aitem.second.value && refList[aitem.first].count == 0)
            printf("[%s:%d] %s: ASSIGN %s = %s size %d count %d[%d] pin %d type %s\n", __FUNCTION__, __LINE__, label, aitem.first.c_str(), tree2str(aitem.second.value).c_str(), walkCount(aitem.second.value), refList[aitem.first].count, refList[temp].count, refList[temp].pin, refList[temp].type.c_str());
    }
}

std::string findRewrite(std::string item)
{
    std::string prefix;
    if (!refList[item].pin) {
        for (auto ritem: refList) {
            if ((startswith(item, ritem.first + DOLLAR) || startswith(item, ritem.first + PERIOD))
             && ritem.first.length() > prefix.length())
                prefix = ritem.first;
        }
        if (prefix != "") { // interface reference
            refList[prefix].count++;
            prefix += PERIOD + item.substr(prefix.length() + 1);
        }
    }
    return prefix;
}

static void walkRewrite (ACCExpr *expr)
{
    if (!expr)
        return;
    if (isIdChar(expr->value[0])) {
        std::string prefix = findRewrite(expr->value);
        if (prefix != "") { // interface reference
            expr->value = prefix;
        }
    }
    for (auto item: expr->operands)
        walkRewrite(item);
}

static void setAssign(std::string target, ACCExpr *value, std::string type)
{
    if (!refList[target].pin) {
        std::string rewrite = findRewrite(target);
        if (rewrite != "")
            target = rewrite;
    }
    bool tDir = refList[target].out;
    if (!value)
        return;
    if (type == "Bit(1)") {
        value = cleanupBool(value);
    }
    int ind = target.find("[");
    std::string base, sub;
    if (ind > 0) {
        base = target.substr(0, ind);
        sub = target.substr(ind);
        ind = sub.rfind("]");
        if (ind > 0) {
            std::string after = sub.substr(ind+1);
            sub = sub.substr(0, ind+1);
            if (after != "" && after[0] == '.')
                base += PERIOD + after.substr(1);
            if (refList[base].pin == PIN_MODULE) // normalize subscript location for module pin refs
                target = base + sub;
        }
    }
    walkRewrite(value);
    if (trace_assign)
        printf("[%s:%d] start [%s/%d]count[%d] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tDir, refList[target].count, tree2str(value).c_str(), type.c_str());
    if (!refList[target].pin && generateSection == "") {
        std::string base = target;
        ind = base.find_first_of(PERIOD "[");
        if (ind > 0)
            base = base.substr(0, ind);
        if (!refList[base].pin) {
        printf("[%s:%d] missing target [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
if (0)
        if (target.find("[") == std::string::npos)
        exit(-1);
        }
    }
    updateWidth(value, convertType(type));
    if (isIdChar(value->value[0])) {
        bool sDir = refList[value->value].out;
        if (trace_assign)
        printf("[%s:%d] %s/%d = %s/%d\n", __FUNCTION__, __LINE__, target.c_str(), tDir, value->value.c_str(), sDir);
    }
    std::string valStr = tree2str(value);
    if (assignList[target].type != "" && assignList[target].value->value != "{") { // aggregate alloca items always start with an expansion expr
        if (isIdChar(value->value[0]) && assignList[valStr].type == "") {
        printf("[%s:%d] warnduplicate start [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        printf("[%s:%d] warnduplicate was      = %s type '%s'\n", __FUNCTION__, __LINE__, tree2str(assignList[target].value).c_str(), assignList[target].type.c_str());
            value = allocExpr(target);
            target = valStr;
        }
        else {
//if (trace_assign) {
        printf("[%s:%d] duplicate start [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        printf("[%s:%d] duplicate was      = %s type '%s'\n", __FUNCTION__, __LINE__, tree2str(assignList[target].value).c_str(), assignList[target].type.c_str());
//}
        if (target.find("[") == std::string::npos)
        if (tree2str(assignList[target].value).find("[") == std::string::npos)
        exit(-1);
        }
    }
    if (generateSection != "")
        condAssignList[generateSection][target] = AssignItem{value, type, false, 0};
    else {
        assignList[target] = AssignItem{value, type, false, 0};
        if ((isEnaName(target) || isRdyName(target))
         && !checkInteger(value, "1")
         && !isEnaName(value->value)
         && !isRdyName(value->value)) {   // preserve these for ease of reading verilog
            assignList[target].noRecursion = true;
            // we need to replace these in __ENA expressions so that we can remove __RDY expression elements for the __ENA method
        }
        int ind = target.find('[');
        if (ind != -1) {
            std::string name = target.substr(0,ind);
            refList[name].count++;
            if (trace_assign)
                printf("[%s:%d] inc count[%s]=%d\n", __FUNCTION__, __LINE__, name.c_str(), refList[name].count);
            assignList[target].noRecursion = true;
        }
    }
}
#if 0
static void expandStruct(ModuleIR *IR, std::string fldName, std::string type)
{
    ACCExpr *itemList = allocExpr("{");
    std::list<FieldItem> fieldList;
    getFieldList(fieldList, fldName, "", type, false, 0, false);
    for (auto fitem : fieldList) {
        std::string lvecCount;
        std::string tempType = fitem.type;
        if (startswith(fitem.type, "ARRAY_")) {
            tempType = fitem.type.substr(6);
            int ind = tempType.find("_");
            if (ind > 0) {
                if (lvecCount != "") {
                    printf("[%s:%d] dup veccount %s new %s\n", __FUNCTION__, __LINE__, lvecCount.c_str(), tempType.c_str());
                    exit(-1);
                }
                lvecCount = tempType.substr(0, ind);
                tempType = tempType.substr(ind+1);
            }
        }
        uint64_t offset = fitem.offset;
        int64_t uppern = offset - 1;
        std::string upper = autostr(uppern);
        if (uppern < 0)
            upper = "-" + autostr(-uppern);
        if (tempType[0] == '@')
            upper = tempType.substr(1) + "+ (" + upper + ")";
        else if (upper != " ")
            upper = "(" + upper + " + " + convertType(tempType) + ")";
        else
            upper = convertType(tempType);
        std::string base = fldName;
        if (fitem.base != "")
            base = fitem.base;
        std::string fnew = base + "[" + upper + ":" + autostr(offset) + "]";
        ACCExpr *fexpr = allocExpr(base, allocExpr("[", allocExpr(":", allocExpr(upper), allocExpr(autostr(offset)))));
if (trace_expand || refList[fitem.name].pin)
printf("[%s:%d] set %s = %s alias %d base %s , %s[%d : %s] fnew %s pin %d fnew %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), tempType.c_str(), fitem.alias, base.c_str(), fldName.c_str(), (int)offset, upper.c_str(), fnew.c_str(), refList[fitem.name].pin, tree2str(fexpr).c_str());
        assert (!refList[fitem.name].pin || (refList[fitem.name].pin == PIN_WIRE));
        refList[fitem.name] = RefItem{0, tempType, true, false, PIN_WIRE, false, false, lvecCount, false};
        if (refList[fnew].pin) {
            printf("[%s:%d] %s pin exists %d PIN_ALIAS\n", __FUNCTION__, __LINE__, fnew.c_str(), refList[fnew].pin);
        }
        //assert (!refList[fnew].pin);
        refList[fnew] = RefItem{0, tempType, true, false, PIN_ALIAS, false, false, lvecCount, false};
        if (trace_declare)
            printf("[%s:%d]NEWREF %s %s type %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), fnew.c_str(), tempType.c_str());
        if (!fitem.alias)
            itemList->operands.push_front(allocExpr(fitem.name));
        else {
//printf("[%s:%d]AAAAAAAA name %s fexpr %s type %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), tree2str(fexpr).c_str(), fitem.type.c_str());
            setAssign(fitem.name, fexpr, fitem.type);
//refList[fitem.name + " "].count++;
//refList[fitem.name].count++;
        }
    }
#if 0
    if (force) {
        assert (!refList[fldName].pin);
        refList[fldName] = RefItem{0, type, true, false, PIN_WIRE, false, false, "", false};
        if (trace_declare)
            printf("[%s:%d]NEWREF2 %s type %s\n", __FUNCTION__, __LINE__, fldName.c_str(), type.c_str());
    }
#endif
    if (itemList->operands.size() > 0)
        setAssign(fldName, itemList, type);
}
#endif

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

static bool walkSearch (ACCExpr *expr, std::string search)
{
    if (!expr)
        return false;
    if (expr->value == search)
        return true;
    for (auto item: expr->operands)
        if (walkSearch(item, search))
            return true;
    return false;
}

enum {PINI_NONE, PINI_PORT, PINI_METHOD, PINI_PARAM, PINI_INTERFACE};
typedef struct {
    int         variant;
    std::string type, name;
    bool        isOutput, isInout, isLocal;
    std::list<ParamElement> params; // for pinMethods (with module instantiation replacements)
    std::string init; /* for parameters */
    bool        action;
    std::string vecCount;
} PinInfo;
std::list<PinInfo> pinPorts;


static bool handleCLK;
static void collectInterfacePins(ModuleIR *IR, bool instance, std::string pinPrefix, std::string methodPrefix, bool isLocal, MapNameValue &parentMap, bool isPtr, std::string vecCount, bool localInterface, bool argIsSync)
{
//dumpModule("COLLECT", IR);
    assert(IR);
    bool isSync = startswith(IR->name, "PipeInSync") || argIsSync;
    MapNameValue mapValue = parentMap;
    extractParam("COLLECT", IR->name, mapValue);
    vecCount = instantiateType(vecCount, mapValue);
//for (auto item: mapValue)
//printf("[%s:%d] [%s] = %s\n", __FUNCTION__, __LINE__, item.first.c_str(), item.second.c_str());
    if (!localInterface || methodPrefix != "")
    for (auto MI: IR->methods) {
        std::string name = methodPrefix + MI->name;
        bool out = instance ^ isPtr;
        std::list<ParamElement> params;
        if (isSync && endswith(name, "__RDY") == out && !isLocal)
{
printf("[%s:%d] SSSS name %s out %d isPtr %d instance %d\n", __FUNCTION__, __LINE__, name.c_str(), out, isPtr, instance);
            syncPins[name] = SyncPinsInfo{baseMethodName(name) + "S" + name.substr(name.length()-5), out, isPtr, instance};
}
        for (auto pitem: MI->params)
            params.push_back(ParamElement{pitem.name, instantiateType(pitem.type, mapValue), pitem.init});
        pinPorts.push_back(PinInfo{PINI_METHOD, instantiateType(MI->type, mapValue), name, out, false, isLocal, params, ""/*not param*/, MI->action, vecCount});
    }
    if (!localInterface || pinPrefix != "")
    for (auto fld: IR->fields) {
        std::string name = pinPrefix + fld.fldName;
        std::string ftype = instantiateType(fld.type, mapValue);
//printf("[%s:%d] name %s ftype %s fld.type %s\n", __FUNCTION__, __LINE__, name.c_str(), ftype.c_str(), fld.type.c_str());
        bool out = instance ^ fld.isOutput;
        if (fld.isParameter != "") {
            std::string init = fld.isParameter;
            if (init == " ") {
                init = "\"FALSE\"";
                if (ftype == "FLOAT")
                    init = "0.0";
                else if (fld.isPtr)
                    init = "0";
                else if (startswith(ftype, "Bit("))
                    init = "0";
            }
            pinPorts.push_back(PinInfo{PINI_PARAM, fld.isPtr ? "POINTER" : ftype, name, out, fld.isInout, isLocal, {}, init, false, vecCount});
        }
        else {
            //printf("[%s:%d] name %s type %s local %d\n", __FUNCTION__, __LINE__, name.c_str(), ftype.c_str(), isLocal);
            pinPorts.push_back(PinInfo{PINI_PORT, ftype, name, out, fld.isInout, isLocal, {}, ""/*not param*/, false, vecCount});
            handleCLK = false;
        }
    }
    for (FieldElement item : IR->interfaces) {
        //MapNameValue mapValue = parentMap;
        extractParam("FIELD_" + item.fldName, item.type, mapValue);
        std::string interfaceName = item.fldName;
        ModuleIR *IIR = lookupInterface(item.type);
        if (!IIR) {
            printf("%s: in module '%s', interface lookup '%s' name '%s' failed\n", __FUNCTION__, IR->name.c_str(), item.type.c_str(), interfaceName.c_str());
            exit(-1);
        }
//printf("[%s:%d]befpin\n", __FUNCTION__, __LINE__);
        std::list<ParamElement> params;
        std::string updatedVecCount = instantiateType(item.vecCount, mapValue);
        bool localFlag = isLocal || item.isLocalInterface;
        bool ptrFlag = isPtr || item.isPtr;
        if (item.fldName == "")
            collectInterfacePins(IIR, instance, pinPrefix + item.fldName, methodPrefix + interfaceName, localFlag, mapValue, ptrFlag, updatedVecCount, localInterface, isSync);
        else
        pinPorts.push_back(PinInfo{PINI_INTERFACE, item.type, methodPrefix + interfaceName, ptrFlag, false, localFlag, params, ""/*not param*/, false, updatedVecCount});
    }
}

/*
 * Generate verilog module header for class definition or reference
 */
static void generateModuleSignature(ModuleIR *IR, std::string instanceType, std::string instance, ModList &modParam, std::string params, std::string vecCount)
{
    MapNameValue mapValue;
    extractParam("SIGN_" + IR->name, instanceType, mapValue);
    std::string minst;
    if (instance != "")
        minst = instance.substr(0, instance.length()-1);
    auto checkWire = [&](std::string name, std::string type, int dir, bool inout, std::string isparam, bool isLocal, bool isArgument, std::string interfaceVecCount) -> void {
        std::string newtype = instantiateType(type, mapValue);
if (newtype != type) {
printf("[%s:%d] iinst %s ITYPE %s CHECKTYPE %s newtype %s\n", __FUNCTION__, __LINE__, instance.c_str(), instanceType.c_str(), type.c_str(), newtype.c_str());
//exit(-1);
        type = newtype;
}
        std::string vc = vecCount;
        if (interfaceVecCount != "")
            vc = interfaceVecCount;   // HACKHACKHACK
        int refPin = instance != "" ? PIN_OBJECT: (isLocal ? PIN_LOCAL: PIN_MODULE);
        std::string instName = instance + name;
        if (!isLocal || instance == "") {
        if (refList[instName].pin) {
            printf("[%s:%d] %s pin exists %d new %d\n", __FUNCTION__, __LINE__, instName.c_str(), refList[instName].pin, refPin);
        }
        assert (!refList[instName].pin);
        if (trace_ports)
            printf("[%s:%d] name %s type %s dir %d vecCount %s interfaceVecCount %s\n", __FUNCTION__, __LINE__, name.c_str(), type.c_str(), dir, vecCount.c_str(), interfaceVecCount.c_str());
        refList[instName] = RefItem{((dir != 0 || inout) && instance == "") || vc != "", type, dir != 0, inout, refPin, false, false, vc, isArgument};
            if(instance == "" && interfaceVecCount != "")
                refList[instName].done = true;  // prevent default blanket assignment generation
        }
        if (!isLocal)
        modParam.push_back(ModData{name, instName, type, false, false, dir, inout, isparam, vc});
        if (trace_ports)
            printf("[%s:%d] iName %s name %s type %s dir %d io %d ispar '%s' isLoc %d vec '%s'\n", __FUNCTION__, __LINE__, instName.c_str(), name.c_str(), type.c_str(), dir, inout, isparam.c_str(), isLocal, vc.c_str());
        //if (isparam == "")
        //xpandStruct(IR, instName, type, dir, false, PIN_WIRE, vc, isArgument);
    };
//printf("[%s:%d] name %s instance %s\n", __FUNCTION__, __LINE__, IR->name.c_str(), instance.c_str());
//dumpModule("PINS", IR);
    pinPorts.clear();
    handleCLK = true;
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    collectInterfacePins(implements, instance != "", "", "", false, mapValue, false, "", false, false);
    if (instance == "")
        collectInterfacePins(IR, instance != "", "", "", false, mapValue, false, "", true, false);
    for (FieldElement item : IR->parameters) {
        //extractParam("PARAM_" + item.name, item.type, mapValue);
        std::string interfaceName = item.fldName;
        ModuleIR *IIR = lookupInterface(item.type);
        if (!IIR) {
            printf("%s: in module '%s', interface lookup '%s' name '%s' failed\n", __FUNCTION__, IR->name.c_str(), item.type.c_str(), interfaceName.c_str());
            exit(-1);
        }
        if (interfaceName != "")
            interfaceName += PERIOD;
printf("[%s:%d]befpin '%s'\n", __FUNCTION__, __LINE__, interfaceName.c_str());
        collectInterfacePins(IIR, instance != "", item.fldName, interfaceName, item.isLocalInterface, mapValue, item.isPtr, item.vecCount, false, false);
    }
    std::string moduleInstantiation = IR->name;
    int ind = moduleInstantiation.find("(");
    if (ind > 0)
        moduleInstantiation = moduleInstantiation.substr(0, ind);
    if (instance != "") {
        moduleInstantiation = genericModuleParam(instanceType);
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
                actual += sep + PERIOD + name + "(" + std::string(start, p-1) + ")";
                sep = ",";
            }
            moduleInstantiation += "#(" + actual + ")";
        }
    }
    modParam.push_back(ModData{minst, moduleInstantiation, "", true/*moduleStart*/, !handleCLK, 0, false, ""/*not param*/, vecCount});
    for (auto item: pinPorts)
    switch (item.variant) {
    case PINI_PARAM:
        if (instance == "")
            checkWire(item.name, item.type, item.isOutput, item.isInout, item.init, item.isLocal, true/*isArgument*/, item.vecCount);
        break;
    }
    for (auto item: pinPorts)
    switch (item.variant) {
    case PINI_INTERFACE:
        checkWire(item.name, item.type, item.isOutput, item.isInout, ""/*not param*/, item.isLocal, false/*isArgument*/, item.vecCount);
        break;
    case PINI_METHOD:
        checkWire(item.name, item.type, item.isOutput ^ (item.type != ""), false, ""/*not param*/, item.isLocal, false/*isArgument*/, item.vecCount);
        if (item.action && !isEnaName(item.name))
            checkWire(item.name + "__ENA", "", item.isOutput ^ (0), false, ""/*not param*/, item.isLocal, false/*isArgument*/, item.vecCount);
        if (trace_ports)
            printf("[%s:%d] instance %s name '%s' type %s\n", __FUNCTION__, __LINE__, instance.c_str(), item.name.c_str(), item.type.c_str());
        for (auto pitem: item.params) {
            checkWire(baseMethodName(item.name) + PERIOD + pitem.name, pitem.type, item.isOutput, false, ""/*not param*/, item.isLocal, instance==""/*isArgument*/, item.vecCount);
        }
        break;
    case PINI_PORT:
        checkWire(item.name, item.type, item.isOutput, item.isInout, ""/*not param*/, item.isLocal, instance==""/*isArgument*/, item.vecCount);
        break;
    }
    if (instance != "")
        return;
    bool hasCLK = false, hasnRST = false;
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
        refList["CLK"] = RefItem{1, "Bit(1)", false, false, PIN_LOCAL, false, false, "", false};
    if (!handleCLK && !hasnRST && vecCount == "")
        refList["nRST"] = RefItem{1, "Bit(1)", false, false, PIN_LOCAL, false, false, "", false};
}

static ACCExpr *walkRemoveParam (ACCExpr *expr)
{
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string op = expr->value;
    if (isIdChar(op[0])) {
        int pin = refList[op].pin;
        if (refList[op].isArgument) {
//if (trace_assign)
printf("[%s:%d] reject use of non-state op %s %d\n", __FUNCTION__, __LINE__, op.c_str(), pin);
            return nullptr;
        }
        //assert(refList[op].pin);
    }
    for (auto oitem: expr->operands) {
        ACCExpr *operand = walkRemoveParam(oitem);
        if (!operand) {
            //continue;
            if (relationalOp(op) || op == "!")
                return nullptr;
            if (booleanBinop(op) || arithOp(op))
                continue;
printf("[%s:%d] removedope %s relational %d operator %s orig %s\n", __FUNCTION__, __LINE__, tree2str(oitem).c_str(), relationalOp(op), expr->value.c_str(), tree2str(expr).c_str());
            continue;
        }
        newExpr->operands.push_back(operand);
    }
    return newExpr;
}

static void walkRef (ACCExpr *expr)
{
    if (!expr)
        return;
    std::string item = expr->value;
    if (isIdChar(item[0])) {
        if (!startswith(item, "__inst$Genvar") && item != "$past") {
        std::string base = item;
        int ind = base.find(PERIOD "[");
        if (ind > 0)
            base = base.substr(0, ind);
        if (!refList[item].pin) {
            if (base != item) {
                if (trace_assign)
                    printf("[%s:%d] RRRRREFFFF %s -> %s\n", __FUNCTION__, __LINE__, expr->value.c_str(), item.c_str());
                if(!refList[base].pin) {
                    printf("[%s:%d] refList[%s] definition missing\n", __FUNCTION__, __LINE__, item.c_str());
                    //exit(-1);
                }
            }
        }
        }
        refList[item].count++;
        if (trace_assign)
            printf("[%s:%d] inc count[%s]=%d\n", __FUNCTION__, __LINE__, item.c_str(), refList[item].count);
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
    if (refList[name].count > 0 && refList[name].pin != PIN_MODULE) {
        refList[name].count--;
        if (trace_assign)
            printf("[%s:%d] dec count[%s]=%d\n", __FUNCTION__, __LINE__, name.c_str(), refList[name].count);
    }
}

static ACCExpr *replaceAssign (ACCExpr *expr, std::string guardName = "", bool enableListProcessing = false)
{
    if (!expr)
        return expr;
    if (trace_assign)
    printf("[%s:%d] start %s expr %s\n", __FUNCTION__, __LINE__, guardName.c_str(), tree2str(expr).c_str());
    std::string item = expr->value;
    if (guardName != "" && (startswith(item, guardName) || (item == PERIOD && startswith(expr->operands.front()->value, guardName)))) {
        if (trace_assign)
        printf("[%s:%d] remove guard of called method from enable line %s\n", __FUNCTION__, __LINE__, item.c_str());
        return allocExpr("1");
    }
    if (isIdChar(item[0]) && !expr->operands.size()) {
        if (!assignList[item].noRecursion || enableListProcessing)
        if (ACCExpr *assignValue = assignList[item].value)
        if (assignValue->value[0] != '@')    // hack to prevent propagation of __reduce operators
        if (assignValue->value == "{" || walkCount(assignValue) < ASSIGN_SIZE_LIMIT) {
        decRef(item);
        walkRef(assignValue);
        if (trace_assign)
            printf("[%s:%d] replace %s norec %d with %s\n", __FUNCTION__, __LINE__, item.c_str(), assignList[item].noRecursion, tree2str(assignValue).c_str());
        return replaceAssign(assignValue, guardName, enableListProcessing);
        }
    }
    ACCExpr *newExpr = allocExpr(expr->value);
    for (auto item: expr->operands)
        newExpr->operands.push_back(replaceAssign(item, guardName, enableListProcessing));
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

static bool checkRecursion(ACCExpr *expr)
{
    std::string item = expr->value;
    if (isIdChar(item[0]) && !expr->operands.size()) {
        if (replaceBlock[item]) // flag multiple expansions of an item
            return false;
        if (!assignList[item].noRecursion)
        if (ACCExpr *res = assignList[item].value) {
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
        if (!assignList[item].noRecursion)
        if (ACCExpr *assignValue = assignList[item].value)
        if (refList[item].pin != PIN_MODULE
             && (checkOperand(assignValue->value) || isRdyName(item) || isEnaName(item)
                || (startswith(item, BLOCK_NAME) && assignList[item].size < COUNT_LIMIT))) {
            if (trace_assign)
            printf("[%s:%d] replace %s with %s\n", __FUNCTION__, __LINE__, item.c_str(), tree2str(assignValue).c_str());
            decRef(item);
            return simpleReplace(assignValue);
        }
    }
    for (auto oitem: expr->operands)
        newExpr->operands.push_back(simpleReplace(oitem));
    return newExpr;
}

static void setAssignRefCount(ModuleIR *IR)
{
    traceZero("SETASSIGNREF");
    // before doing the rest, clean up block guards
    for (auto item = assignList.begin(), itemEnd = assignList.end(); item != itemEnd; item++) {
        if (item->second.type == "Bit(1)" && startswith(item->first, BLOCK_NAME)) {
            item->second.value = cleanupBool(simpleReplace(item->second.value));
            item->second.size = walkCount(item->second.value);
        }
    }
    for (auto item: assignList) {
        //assignList[item.first].noRecursion = true;
//printf("[%s:%d] ref[%s].norec %d value %s\n", __FUNCTION__, __LINE__, item.first.c_str(), assignList[item.first].noRecursion, tree2str(item.second.value).c_str());
        if (item.second.type == "Bit(1)")
            assignList[item.first].value = cleanupBool(simpleReplace(item.second.value));
        else
            assignList[item.first].value = cleanupExpr(simpleReplace(item.second.value));
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
                 if (rep.second > length && !isRdyName(rep.first) && !isEnaName(rep.first)) {
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
    for (auto ctop = condLines.begin(), ctopEnd = condLines.end(); ctop != ctopEnd; ctop++) { // process all generate sections
    for (auto tcond = ctop->second.always.begin(), tend = ctop->second.always.end(); tcond != tend; tcond++) {
        std::string methodName = tcond->first;
        walkRef(tcond->second.guard);
        tcond->second.guard = cleanupBool(replaceAssign(tcond->second.guard));
        if (trace_assign)
            printf("[%s:%d] %s: guard %s\n", __FUNCTION__, __LINE__, methodName.c_str(), tree2str(tcond->second.guard).c_str());
        auto info = tcond->second.info;
        tcond->second.info.clear();
        for (auto item = info.begin(), itemEnd = info.end(); item != itemEnd; item++) {
            tcond->second.info[cleanupBool(replaceAssign(item->first))] = info[item->first];
            walkRef(item->first);
            for (auto citem: item->second) {
                if (trace_assign)
                    printf("[%s:%d] %s: %s dest %s value %s\n", __FUNCTION__, __LINE__, methodName.c_str(),
 tree2str(item->first).c_str(), tree2str(citem.dest).c_str(), tree2str(citem.value).c_str());
                if (citem.dest) {
                    walkRef(citem.value);
                    walkRef(citem.dest);
                }
                else
                    walkRef(citem.value->operands.front());
            }
        }
    }
    }

    for (auto item: assignList)
        if (item.second.value && refList[item.first].pin == PIN_OBJECT) {
            refList[item.first].count++;
            if (trace_assign)
                printf("[%s:%d] inc count[%s]=%d\n", __FUNCTION__, __LINE__, item.first.c_str(), refList[item.first].count);
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
            if (trace_assign)
                printf("[%s:%d] inc count[%s]=%d\n", __FUNCTION__, __LINE__, item.first.c_str(), refList[item.first].count);
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

static void appendLine(std::string methodName, ACCExpr *cond, ACCExpr *dest, ACCExpr *value)
{
    dest = replaceAssign(dest);
    value = replaceAssign(value);
    for (auto CI = condLines[generateSection].always[methodName].info.begin(), CE = condLines[generateSection].always[methodName].info.end(); CI != CE; CI++)
        if (matchExpr(cond, CI->first)) {
            CI->second.push_back(CondInfo{dest, value});
            return;
        }
    condLines[generateSection].always[methodName].guard = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), allocExpr(getRdyName(methodName))));
    condLines[generateSection].always[methodName].info[cond].push_back(CondInfo{dest, value});
}

void showRef(const char *label, std::string name)
{
    if (trace_connect)
    printf("%s: %s count %d pin %d type %s out %d inout %d done %d isGenerated %d veccount %s isArgument %d\n",
        label, name.c_str(), refList[name].count, refList[name].pin,
        refList[name].type.c_str(), refList[name].out,
        refList[name].inout, refList[name].done, refList[name].isGenerated,
        refList[name].vecCount.c_str(), refList[name].isArgument);
}

static void connectTarget(ACCExpr *target, ACCExpr *source, std::string type, bool isForward)
{
    std::string tstr = tree2str(target), sstr = tree2str(source);
    std::string tif = tstr, sif = sstr;
    int ind = tif.find_first_of(PERIOD "[");
    if (ind > 0)
        tif = tif.substr(0, ind);
    ind = sif.find_first_of(PERIOD "[");
    if (ind > 0)
        sif = sif.substr(0, ind);
    bool tdir = refList[tif].out ^ (tif.find(DOLLAR) != std::string::npos) ^ endswith(tstr, "__RDY");;
    bool sdir = refList[sif].out ^ (sif.find(DOLLAR) != std::string::npos) ^ endswith(sstr, "__RDY");;
    if (trace_assign || trace_connect || (!tdir && !sdir))
        printf("%s: IFCCC '%s'/%d/%d '%s'/%d/%d\n", __FUNCTION__, tstr.c_str(), tdir, refList[target->value].out, sstr.c_str(), sdir, refList[source->value].out);
    showRef("target", target->value);
    showRef("source", source->value);
    if (sdir) {
        setAssign(sstr, target, type);
        refList[tif].done = true;
    }
    else {
        setAssign(tstr, source, type);
        refList[sif].done = true;
    }
}

void connectMethodList(ModuleIR *IIR, ACCExpr *targetTree, ACCExpr *sourceTree, bool isForward)
{
    std::string ICtarget = tree2str(targetTree);
    std::string ICsource = tree2str(sourceTree);
    for (auto MI : IIR->methods) {
        ACCExpr *target = dupExpr(targetTree), *source = dupExpr(sourceTree);
        if (trace_connect)
            printf("[%s:%d] ICtarget '%s' ICsource '%s'\n", __FUNCTION__, __LINE__, ICtarget.c_str(), ICsource.c_str());
        if (target->value.length() && target->value.substr(target->value.length()-1) == PERIOD)
            target->value = target->value.substr(0, target->value.length()-1);
        if (source->value.length() && source->value.substr(source->value.length()-1) == PERIOD)
            source->value = source->value.substr(0, source->value.length()-1);
        target = allocExpr(PERIOD, target, allocExpr(MI->name));
        source = allocExpr(PERIOD, source, allocExpr(MI->name));
        connectTarget(target, source, MI->type, isForward);
        std::string tstr = baseMethodName(tree2str(target)) + DOLLAR;
        std::string sstr = baseMethodName(tree2str(source)) + DOLLAR;
        for (auto info: MI->params)
            connectTarget(allocExpr(tstr+info.name), allocExpr(sstr+info.name), info.type, isForward);
    }
}

static void connectMethods(ModuleIR *IIR, ACCExpr *targetTree, ACCExpr *sourceTree, bool isForward)
{
    std::string ICtarget = tree2str(targetTree);
    std::string ICsource = tree2str(sourceTree);
    if (trace_connect)
        printf("%s: CONNECT target '%s' source '%s' forward %d\n", __FUNCTION__, ICtarget.c_str(), ICsource.c_str(), isForward);
    for (auto fld : IIR->fields) {
        ACCExpr *target = dupExpr(targetTree), *source = dupExpr(sourceTree);
        target->value += fld.fldName;
        source->value += fld.fldName;
        if (ModuleIR *IR = lookupIR(fld.type)) {
            connectMethods(lookupInterface(IR->interfaceName), target, source, isForward);
            continue;
        }
        connectTarget(target, source, fld.type, isForward);
    }
    for (auto fld : IIR->interfaces) {
        ACCExpr *target = dupExpr(targetTree), *source = dupExpr(sourceTree);
        target->value += fld.fldName;
        source->value += fld.fldName;
        connectMethods(lookupInterface(fld.type), target, source, isForward);
    }
    if (IIR->methods.size())
        connectTarget(targetTree, sourceTree, IIR->name, isForward);
}

void appendMux(std::string section, std::string name, ACCExpr *cond, ACCExpr *value, std::string defaultValue)
{
    ACCExpr *phi = muxValueList[section][name].phi;
    if (!phi) {
        phi = allocExpr("__phi", allocExpr("("));
        muxValueList[section][name].phi = phi;
        muxValueList[section][name].defaultValue = defaultValue;
    }
    phi->operands.front()->operands.push_back(allocExpr(":", cond, value));
}

static void generateMethodGuard(ModuleIR *IR, std::string methodName, MethodInfo *MI)
{
    if (MI->subscript)      // from instantiateFor
        methodName += "[" + tree2str(MI->subscript) + "]";
    generateSection = MI->generateSection;
    MI->guard = cleanupExpr(MI->guard);
    if (!isRdyName(methodName)) {
        walkRead(MI, MI->guard, nullptr);
        if (MI->rule)
            setAssign(getEnaName(methodName), allocExpr(getRdyName(methodName)), "Bit(1)");
    }
    setAssign(methodName, MI->guard, MI->type); // collect the text of the return value into a single 'assign'
    for (auto IC : MI->interfaceConnect) {
        std::string iname = IC.type;
        if (startswith(iname, "ARRAY_")) {
            iname = iname.substr(6);
            int ind = iname.find("_");
            if (ind > 0)
                iname = iname.substr(ind+1);
        }
        ModuleIR *IIR = lookupInterface(iname);
        if (!IIR)
            dumpMethod("MISSINGCONNECT", MI);
        assert(IIR && "interfaceConnect interface type");
        connectMethods(IIR, IC.target, IC.source, IC.isForward);
    }
}
static void generateMethod(ModuleIR *IR, std::string methodName, MethodInfo *MI)
{
    if (MI->subscript)      // from instantiateFor
        methodName += "[" + tree2str(MI->subscript) + "]";
    generateSection = MI->generateSection;
    for (auto info: MI->storeList) {
        walkRead(MI, info->cond, nullptr);
        walkRead(MI, info->value, info->cond);
        walkRewrite(info->cond);
        walkRewrite(info->value);
        std::string dest = info->dest->value;
        if (isIdChar(dest[0]) && !info->dest->operands.size() && refList[dest].pin == PIN_WIRE) {
            ACCExpr *cond = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), info->cond));
            appendMux(generateSection, dest, cond, info->value, "0");
        }
        else
            appendLine(methodName, info->cond, info->dest, info->value);
    }
    for (auto info: MI->letList) {
        walkRewrite(info->cond);
        walkRewrite(info->value);
        ACCExpr *cond = cleanupBool(allocExpr("&&", allocExpr(getRdyName(methodName)), info->cond));
        ACCExpr *value = info->value;
        updateWidth(value, convertType(info->type));
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        std::string dest = tree2str(info->dest);
        std::list<FieldItem> fieldList;
        getFieldList(fieldList, dest, "", info->type, true, 0, false);
        if (fieldList.size() == 1) {
            std::string first = fieldList.front().name;
            appendMux(generateSection, dest, cond, value, "0");
            if (first != dest)
                appendMux(generateSection, first, cond, value, "0");
        }
        else {
        std::string splitItem = tree2str(value);
        if ((value->operands.size() || !isIdChar(value->value[0]))) {
            splitItem = dest + "$lettemp";
            appendMux(generateSection, splitItem, cond, value, "0");
        }
        for (auto fitem : fieldList) {
            std::string offset = autostr(fitem.offset);
            std::string upper = convertType(fitem.type) + " - 1";
            if (offset != "0")
                upper += " + " + offset;
            appendMux(generateSection, fitem.name, cond,
                allocExpr(splitItem, allocExpr("[", allocExpr(":", allocExpr(upper), allocExpr(offset)))), "0");
        }
        }
    }
    for (auto info: MI->assertList) {
        walkRewrite(info->cond);
        walkRewrite(info->value);
        ACCExpr *cond = info->cond;
        ACCExpr *value = info->value;
        auto par = value->operands.front()->operands;
        ACCExpr *param = cleanupBool(par.front());
        par.clear();
        par.push_back(param);
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        ACCExpr *guard = nullptr;
        if (MethodInfo *MIRdy = lookupMethod(IR, getRdyName(methodName))) {
            guard = cleanupBool(MIRdy->guard);
        }
        ACCExpr *tempCond = guard ? allocExpr("&&", guard, cond) : cond;
        tempCond = cleanupBool(tempCond);
        std::string calledName = value->value, calledEna = getEnaName(calledName);
        std::string sensitivity = "*";
        if (walkSearch(tempCond, "$past"))
            sensitivity = " posedge CLK";
        condLines[generateSection].assert.push_back("always @(" + sensitivity + ")");
        std::string indent;
        std::string condStr = tree2str(tempCond);
        if (condStr != "" && condStr != "1") {
            condLines[generateSection].assert.push_back("    if (" + condStr + ")");
            indent = "    ";
        }
        condLines[generateSection].assert.push_back("    " + indent + tree2str(value) + ";");
    }
    for (auto info: MI->callList) {
        ACCExpr *subscript = nullptr;
        if (info->value->operands.size() > 1) {
            subscript = info->value->operands.front()->operands.front();
            info->value->operands.pop_front();
        }
        walkRewrite(info->cond);
        walkRewrite(info->value);
        std::string section = generateSection;
        ACCExpr *cond = info->cond;
        ACCExpr *value = info->value;
        ACCExpr *param = nullptr;
        if (!(isIdChar(value->value[0]) && value->operands.size()
         && (param = value->operands.back()) && param->value == PARAMETER_MARKER)) {
            printf("[%s:%d] incorrectly formed call expression\n", __FUNCTION__, __LINE__);
            dumpExpr("READCALL", value);
            exit(-1);
        }
        ACCExpr *callTarget = dupExpr(value);
        callTarget->operands.pop_back();
        std::string calledName = tree2str(callTarget), calledEna = getEnaName(calledName);
        std::string vecCount;
        MapNameValue mapValue;
        MethodInfo *CI = lookupQualName(IR, calledName, vecCount, mapValue);
        if (!CI) {
            printf("[%s:%d] method %s not found\n", __FUNCTION__, __LINE__, calledName.c_str());
            dumpExpr("CALL", value);
            exit(-1);
        }
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        if (!info->isAction)
            continue;
        ACCExpr *tempCond = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), allocExpr(getRdyName(methodName)), cond));
        if (subscript) {
            std::string sub;
            tempCond = simpleReplace(tempCond);
            tempCond = replaceAssign(tempCond, getRdyName(calledEna), true); // remove __RDY before adding subscript!
            ACCExpr *var = allocExpr(GENVAR_NAME "1");
            section = makeSection(var->value, allocExpr("0"),
                allocExpr("<", var, allocExpr(vecCount)), allocExpr("+", var, allocExpr("1")));
            sub = "[" + var->value + "]";
            int ind = calledEna.find_last_of(DOLLAR PERIOD);
            if (ind > 0) {
                std::string post = calledEna.substr(ind);
                calledEna = calledEna.substr(0, ind);
                ind = post.rfind(DOLLAR);
                if (ind > 1)
                    post = post.substr(0, ind) + PERIOD + post.substr(ind+1);
                //if (post[0] == DOLLAR[0])
                    //post = PERIOD + post.substr(1);
                calledEna += sub + post;
            }
            else
                calledEna += sub;
            tempCond = allocExpr("&&", tempCond, allocExpr("==", subscript, var));
        }
//printf("[%s:%d] CALLLLLL '%s' condition %s\n", __FUNCTION__, __LINE__, calledName.c_str(), tree2str(tempCond).c_str());
//dumpExpr("CALLCOND", tempCond);
        if (calledName == "__finish") {
            appendLine(methodName, tempCond, nullptr, allocExpr("$finish;"));
            break;
        }
        if (!enableList[section][calledEna].phi) {
            enableList[section][calledEna].phi = allocExpr("|");
            enableList[section][calledEna].defaultValue = "1'd0";
        }
        enableList[section][calledEna].phi->operands.push_back(tempCond);
        auto AI = CI->params.begin();
        std::string pname = baseMethodName(calledEna) + DOLLAR;
        int argCount = CI->params.size();
        for (auto item: param->operands) {
            if(argCount-- > 0) {
                std::string size = tree2str(cleanupInteger(cleanupExpr(str2tree(convertType(instantiateType(AI->type, mapValue))))));
                appendMux(section, pname + AI->name, tempCond, item, size + "'d0");
                AI++;
            }
        }
        MI->meta[MetaInvoke][calledEna].insert(tree2str(cond));
    }
    if (!implementPrintf)
        for (auto info: MI->printfList)
            appendLine(methodName, info->cond, nullptr, info->value);
}

void generateMethodGroup(ModuleIR *IR, void (*generateMethod)(ModuleIR *IR, std::string methodName, MethodInfo *MI))
{
    for (auto MI : IR->methods)
        if (MI->generateSection == "")
        generateMethod(IR, MI->name, MI);
    for (auto MI : IR->methods)
        if (MI->generateSection != "")
        generateMethod(IR, MI->name, MI);
    for (auto MI : IR->methods) {
    for (auto item: MI->generateFor) {
        genvarMap[item.var] = 1;
        MethodInfo *MIb = IR->generateBody[item.body];
        assert(MIb && "body item ");
        generateMethod(IR, MI->name, MIb);
    }
    for (auto item: MI->instantiateFor) {
        genvarMap[item.var] = 1;
        MethodInfo *MIb = IR->generateBody[item.body];
        assert(MIb && "body item ");
        generateMethod(IR, MI->name, MIb);
    }
    }
}

static void interfaceAssign(std::string target, ACCExpr *source, std::string type)
{
    if (auto interface = lookupInterface(type)) {
        connectMethodList(interface, allocExpr(target), source, false);
        refList[target].done = true; // mark that assigns have already been output
        refList[tree2str(source)].done = true; // mark that assigns have already been output
    }
}

/*
 * Generate *.sv and *.vh for a Verilog module
 */
void generateModuleDef(ModuleIR *IR, ModList &modLineTop)
{
static ModList modLine;
    generateSection = "";
    condAssignList.clear();
    refList.clear();
    assignList.clear();
    modNew.clear();
    condLines.clear();
    genvarMap.clear();
    enableList.clear();
    // 'Mux' together parameter settings from all invocations of a method from this class
    muxValueList.clear();
    modLine.clear();
    syncPins.clear();     // pins from PipeInSync

    printf("[%s:%d] STARTMODULE %s\n", __FUNCTION__, __LINE__, IR->name.c_str());
    generateModuleSignature(IR, "", "", modLineTop, "", "");

    iterField(IR, CBAct {
        ModuleIR *itemIR = lookupIR(item.type);
        if (!itemIR || item.isPtr || itemIR->isStruct) {
            if (refList[item.fldName].pin) {
                printf("[%s:%d] dupppp %s pin %d\n", __FUNCTION__, __LINE__, item.fldName.c_str(), refList[item.fldName].pin);
            }
            assert (!refList[item.fldName].pin);
            refList[item.fldName] = RefItem{0, item.type, false, false, item.isShared ? PIN_WIRE : PIN_REG, false, false, item.vecCount, false};
        }
        else
            generateModuleSignature(itemIR, item.type, item.fldName + DOLLAR, modLine, IR->params[item.fldName], item.vecCount);
        return nullptr; });
    for (auto item: syncPins) {
        if (item.second.name != "") {
            //bool out = item.second.out;
            //bool isPtr = item.second.isPtr;
            bool instance = item.second.instance;
            std::string sname = item.second.name;
            refList[sname] = RefItem{4, "Bit(1)", false, false, PIN_LOCAL, false, false, "", false};
            modLine.push_back(ModData{item.first + "SyncFF", "SyncFF", "", true/*moduleStart*/, false, false, false, "", ""});
            modLine.push_back(ModData{"out", instance ? item.first : sname, "Bit(1)", false, false, true/*out*/, false, "", ""});
            modLine.push_back(ModData{"in", instance ? sname : item.first, "Bit(1)", false, false, false /*out*/, false, "", ""});
            refList[item.first].count++;
            refList[sname].count++;
        }
    }
    for (auto MI : IR->methods) { // walkRemoveParam depends on the iterField above
        std::string methodName = MI->name;
        if (MI->rule) {    // both RDY and ENA must be allocated for rules
            assert (!refList[methodName].pin);
            refList[methodName] = RefItem{0, MI->type, true, false, PIN_WIRE, false, false, "", false};
        }
        for (auto info: MI->printfList) {
            ACCExpr *value = info->value->operands.front();
            value->value = "(";   // change from PARAMETER_MARKER
            if (implementPrintf)
                MI->callList.push_back(new CallListElement{printfArgs(value), info->cond, true});
            else {
                ACCExpr *listp = value->operands.front();
                assert(listp);
                if (endswith(listp->value, "\\n\""))
                    listp->value = listp->value.substr(0, listp->value.length()-3) + "\"";
            }
        }
        for (auto item: MI->alloca) { // be sure to define local temps before walkRemoveParam
            if (refList[item.first].pin) {
                printf("[%s:%d] error in alloca name '%s' pin %d type '%s'\n", __FUNCTION__, __LINE__, item.first.c_str(), refList[item.first].pin, item.second.type.c_str());
            }
            assert (!refList[item.first].pin);
            bool isStruct = lookupIR(item.second.type) != nullptr || lookupInterface(item.second.type) != nullptr;
            refList[item.first] = RefItem{isStruct ? 2 : 0, item.second.type, true, false, PIN_WIRE, false, false, convertType(item.second.type, 2), false};
            //expandStruct(IR, item.first, item.second.type);
        }
        for (auto item: MI->generateFor) {
            MethodInfo *MIb = IR->generateBody[item.body];
            assert(MIb);
            for (auto pitem: MIb->params) {
                if (refList[pitem.name].pin) {
                    refList[pitem.name].count++;
                    if (trace_assign)
                        printf("[%s:%d] inc count[%s]=%d\n", __FUNCTION__, __LINE__, pitem.name.c_str(), refList[pitem.name].count);
                }
            }
        }
        for (auto item: MI->instantiateFor) {
            MethodInfo *MIb = IR->generateBody[item.body];
            assert(MIb);
            for (auto pitem: MIb->params) {
                if (refList[pitem.name].pin) {
                    refList[pitem.name].count++;
                    if (trace_assign)
                        printf("[%s:%d] inc count[%s]=%d\n", __FUNCTION__, __LINE__, pitem.name.c_str(), refList[pitem.name].count);
                }
            }
        }
        // lift guards from called method interfaces
        if (!isRdyName(methodName))
        if (MethodInfo *MIRdy = lookupMethod(IR, getRdyName(methodName))) {
        auto appendGuard = [&] (CallListElement *item) -> void {
            std::string name = item->value->value;
            if (name == "__finish" || name == "$past")
                return;
            name = getRdyName(name);
            std::string nameVec = refList[name].vecCount;
            if (isIdChar(name[0]) && nameVec != "") {
                std::string name_or = name + "_or";       // convert unpacked array to vector
                std::string name_or1 = name_or + "1";
                if (!refList[name_or].pin) {
                    // adding a space to the 'name_or' declaration forces the
                    // code generator to emit bitvector dimensions explicitly,
                    // to that the subscripted 'setAssign' below compiles correctly
                    // (you can use 'foo[0]' if the declaration is 'wire [0:0] foo',
                    // but not if the declaration is 'wire foo').  Hmm...
                    refList[name_or] = RefItem{99, "Bit( " + nameVec + ")", false, false, PIN_WIRE, false, false, "", false};
                    refList[name_or1] = RefItem{99, "Bit(1)", false, false, PIN_WIRE, false, false, "", false};
                    setAssign(name_or1, allocExpr("@|", allocExpr(name_or)), "Bit(1)");
                    assignList[name_or1].noRecursion = true;
                    ACCExpr *var = allocExpr(GENVAR_NAME "1");
                    generateSection = makeSection(var->value, allocExpr("0"),
                        allocExpr("<", var, allocExpr(nameVec)), allocExpr("+", var, allocExpr("1")));
                    std::string sub = "[" + var->value + "]";
                    setAssign(name_or + sub, allocExpr(name + sub), "Bit(1)");
                    generateSection = "";
                }
                name = name_or1;
            }
            ACCExpr *tempCond = allocExpr(name);
            if (item->cond)
                tempCond = allocExpr("|", walkRemoveParam(invertExpr(item->cond)), tempCond);
            MIRdy->guard = cleanupBool(allocExpr("&&", MIRdy->guard, tempCond));
        };
        for (auto item: MI->callList)
            appendGuard(item);
        for (auto item: MI->generateFor) {
            MethodInfo *MIb = IR->generateBody[item.body];
            assert(MIb);
            for (auto item: MIb->callList)
                appendGuard(item);
        }
        // instantiateFor needs to subscript the RDY
        for (auto item: MI->instantiateFor) {
            MethodInfo *MIb = IR->generateBody[item.body];
            assert(MIb);
            for (auto item: MIb->callList)
                appendGuard(item);
        }
        }
    }

    // generate wires for internal methods RDY/ENA.  Collect state element assignments
    // from each method
    for (auto IC : IR->interfaceConnect) {
        std::string iname = IC.type;
        if (startswith(iname, "ARRAY_")) {
            iname = iname.substr(6);
            int ind = iname.find("_");
            if (ind > 0)
                iname = iname.substr(ind+1);
        }
        ModuleIR *IIR = lookupInterface(iname);
        if (!IIR)
            dumpModule("MISSINGCONNECT", IR);
        assert(IIR && "interfaceConnect interface type");
        connectMethods(IIR, IC.target, IC.source, IC.isForward);
    }
    traceZero("AFTCONNECT");
    generateMethodGroup(IR, generateMethodGuard);
    generateMethodGroup(IR, generateMethod);
    // combine mux'ed assignments into a single 'assign' statement
    for (auto top: muxValueList) {
        generateSection = top.first;
        for (auto item: top.second) {
        setAssign(item.first, cleanupExprBuiltin(item.second.phi, item.second.defaultValue), refList[item.first].type);
        if (startswith(item.first, BLOCK_NAME))
            assignList[item.first].size = walkCount(assignList[item.first].value);
        else
            assignList[item.first].noRecursion = true;
        }
    }
    for (auto top: enableList) { // remove dependancy of the __ENA line on the __RDY
        generateSection = top.first;
        for (auto item: top.second) {
        setAssign(item.first, replaceAssign(simpleReplace(item.second.phi), getRdyName(item.first), true), "Bit(1)");
        }
    }
    generateSection = "";

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
            skipReplace = mitem.vecCount != "" || mitem.value == "SyncFF";
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
#if 0
    for (auto item: assignList) {
        std::string temp = item.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if (item.second.value && (refList[temp].count || !refList[temp].pin) && !refList[item.first].done) {
    if (auto interface = lookupInterface(item.second.type)) {
printf("[%s:%d] RRRRRRRRRRRRRRRR count %d done %d %s = %s type %s\n", __FUNCTION__, __LINE__, refList[item.first].count, refList[item.first].done, item.first.c_str(), tree2str(item.second.value).c_str(), item.second.type.c_str());
}
            interfaceAssign(item.first, item.second.value, item.second.type);
        }
    }
#endif
    for (auto ctop: condAssignList) {
        for (auto item: ctop.second) {
            generateSection = ctop.first;
            interfaceAssign(item.first, item.second.value, item.second.type);
        }
    }
    generateSection = "";
}
