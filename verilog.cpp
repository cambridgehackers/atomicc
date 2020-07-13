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
std::list<ModData> modNew;
std::map<std::string, std::map<std::string, CondGroup>> condLines;
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

static void setAssign(std::string target, ACCExpr *value, std::string type)
{
    bool tDir = refList[target].out;
    if (!value)
        return;
    if (type == "Bit(1)")
        value = cleanupBool(value);
    int ind = target.find("[");
    if (ind > 0) {
        std::string base = target.substr(0, ind);
        std::string sub = target.substr(ind);
        ind = sub.rfind("]");
        if (ind > 0) {
            std::string after = sub.substr(ind+1);
            sub = sub.substr(0, ind+1);
            if (after != "" && after[0] == '.')
                base += MODULE_SEPARATOR + after.substr(1);
            if (refList[base].pin == PIN_MODULE) // normalize subscript location for module pin refs
                target = base + sub;
        }
    }
    if (trace_assign)
        printf("[%s:%d] start [%s/%d]count[%d] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tDir, refList[target].count, tree2str(value).c_str(), type.c_str());
    if (!refList[target].pin && generateSection == "") {
        printf("[%s:%d] missing target [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
if (0)
        if (target.find("[") == std::string::npos)
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
        if (target.find("[") == std::string::npos)
        if (tree2str(assignList[target].value).find("[") == std::string::npos)
        exit(-1);
    }
    if (generateSection != "")
        condAssignList[generateSection][target] = AssignItem{value, type, false};
    else {
        assignList[target] = AssignItem{value, type, false};
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

static void expandStruct(ModuleIR *IR, std::string fldName, std::string type, int out, bool inout, bool force, int pin, bool assign, std::string vecCount, bool isArgument)
{
    ACCExpr *itemList = allocExpr("{");
    std::list<FieldItem> fieldList;
    getFieldList(fieldList, fldName, "", type, out != 0, force);
    for (auto fitem : fieldList) {
        std::string lvecCount = vecCount;
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
printf("[%s:%d] set %s = %s out %d alias %d base %s , %s[%d : %s] fnew %s pin %d fnew %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), tempType.c_str(), out, fitem.alias, base.c_str(), fldName.c_str(), (int)offset, upper.c_str(), fnew.c_str(), refList[fitem.name].pin, tree2str(fexpr).c_str());
        assert (!refList[fitem.name].pin);
        refList[fitem.name] = RefItem{0, tempType, out != 0, false, fitem.alias ? PIN_WIRE : pin, false, false, lvecCount, isArgument};
        if (refList[fnew].pin) {
            printf("[%s:%d] %s pin exists %d PIN_ALIAS\n", __FUNCTION__, __LINE__, fnew.c_str(), refList[fnew].pin);
        }
        //assert (!refList[fnew].pin);
        refList[fnew] = RefItem{0, tempType, out != 0, false, PIN_ALIAS, false, false, lvecCount, isArgument};
        if (trace_declare)
            printf("[%s:%d]NEWREF %s %s type %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), fnew.c_str(), tempType.c_str());
        if (!fitem.alias && out)
            itemList->operands.push_front(allocExpr(fitem.name));
        else if (assign) {
//printf("[%s:%d]AAAAAAAA name %s fexpr %s type %s\n", __FUNCTION__, __LINE__, fitem.name.c_str(), tree2str(fexpr).c_str(), fitem.type.c_str());
            setAssign(fitem.name, fexpr, fitem.type);
//refList[fitem.name + " "].count++;
//refList[fitem.name].count++;
        }
    }
    if (force) {
        assert (!refList[fldName].pin);
        refList[fldName] = RefItem{0, type, true, false, PIN_WIRE, false, false, vecCount, isArgument};
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
    std::list<ParamElement> params; // for pinMethods (with module instantiation replacements)
    std::string init; /* for parameters */
    bool        action;
    std::string vecCount;
} PinInfo;
std::list<PinInfo> pinPorts, pinMethods, paramPorts;


static void collectInterfacePins(ModuleIR *IR, bool instance, std::string pinPrefix, std::string methodPrefix, bool isLocal, MapNameValue &parentMap, bool isPtr, std::string vecCount, bool localInterface, bool argIsSync)
{
//dumpModule("COLLECT", IR);
    assert(IR);
    bool isSync = startswith(IR->name, "PipeInSync") || argIsSync;
    MapNameValue mapValue = parentMap;
    extractParam(IR->name, mapValue);
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
        pinMethods.push_back(PinInfo{instantiateType(MI->type, mapValue), name, out, false, isLocal, params, ""/*not param*/, MI->action, vecCount});
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
            paramPorts.push_back(PinInfo{fld.isPtr ? "POINTER" : ftype, name, out, fld.isInout, isLocal, {}, init, false, vecCount});
        }
        else {
            //printf("[%s:%d] name %s type %s local %d\n", __FUNCTION__, __LINE__, name.c_str(), ftype.c_str(), isLocal);
            pinPorts.push_back(PinInfo{ftype, name, out, fld.isInout, isLocal, {}, ""/*not param*/, false, vecCount});
        }
    }
    for (FieldElement item : IR->interfaces) {
        //MapNameValue mapValue = parentMap;
        extractParam(item.type, mapValue);
        std::string interfaceName = item.fldName;
        ModuleIR *IIR = lookupInterface(item.type);
        if (!IIR) {
            printf("%s: in module '%s', interface lookup '%s' name '%s' failed\n", __FUNCTION__, IR->name.c_str(), item.type.c_str(), interfaceName.c_str());
            exit(-1);
        }
        if (interfaceName != "")
            interfaceName += MODULE_SEPARATOR;
//printf("[%s:%d]befpin\n", __FUNCTION__, __LINE__);
        collectInterfacePins(IIR, instance, pinPrefix + item.fldName,
            methodPrefix + interfaceName, isLocal || item.isLocalInterface, mapValue, isPtr || item.isPtr, instantiateType(item.vecCount, mapValue), localInterface, isSync);
    }
}

/*
 * Generate verilog module header for class definition or reference
 */
static void generateModuleSignature(ModuleIR *IR, std::string instanceType, std::string instance, std::list<ModData> &modParam, std::string params,
bool dontDeclare = false, std::string vecCount = "", int dimIndex = 0)
{
    MapNameValue mapValue;
    extractParam(instanceType, mapValue);
    std::string minst;
    if (instance != "")
        minst = instance.substr(0, instance.length()-1);
    auto checkWire = [&](std::string name, std::string type, int dir, bool inout, std::string isparam, bool isLocal, bool isArgument, std::string interfaceVecCount) -> void {
        std::string newtype = instantiateType(type, mapValue);
if (newtype != type) {
printf("[%s:%d] iinst %s ITYPE %s CHECKTYPE %s newtype %s\n", __FUNCTION__, __LINE__, instance.c_str(), instanceType.c_str(), type.c_str(), newtype.c_str());
exit(-1);
        type = newtype;
}
        std::string vc = vecCount;
        if (interfaceVecCount != "")
            vc = interfaceVecCount;   // HACKHACKHACK
        int refPin = instance != "" ? PIN_OBJECT: (isLocal ? PIN_LOCAL: PIN_MODULE);
        std::string instName = instance + name;
        if (vc != "") {
            if (dontDeclare && dimIndex != -1)
                instName = minst + "[" + autostr(dimIndex) + "]." + name;
            //else
                //instName = name;
        }
        if (!isLocal || instance == "" || dontDeclare) {
        if (refList[instName].pin) {
            printf("[%s:%d] %s pin exists %d new %d\n", __FUNCTION__, __LINE__, instName.c_str(), refList[instName].pin, refPin);
        }
        //assert (!refList[instName].pin);
        if (trace_ports)
            printf("[%s:%d] name %s type %s dir %d vecCount %s interfaceVecCount %s dontDeclare %d\n", __FUNCTION__, __LINE__, name.c_str(), type.c_str(), dir, vecCount.c_str(), interfaceVecCount.c_str(), dontDeclare);
        refList[instName] = RefItem{((dir != 0 || inout) && instance == "") || dontDeclare || vc != "", type, dir != 0, inout, refPin, false, dontDeclare, vc, isArgument};
            if(instance == "" && interfaceVecCount != "")
                refList[instName].done = true;  // prevent default blanket assignment generation
        }
        if (!isLocal && !dontDeclare)
        modParam.push_back(ModData{name, instName, type, false, false, dir, inout, isparam, vc});
        if (trace_ports)
            printf("[%s:%d] iName %s name %s type %s dir %d io %d ispar '%s' isLoc %d dDecl %d vec '%s' dim %d\n", __FUNCTION__, __LINE__, instName.c_str(), name.c_str(), type.c_str(), dir, inout, isparam.c_str(), isLocal, dontDeclare, vc.c_str(), dimIndex);
        if (isparam == "")
        expandStruct(IR, instName, type, dir, inout, false, PIN_WIRE, true, vc, isArgument);
    };
//printf("[%s:%d] name %s instance %s\n", __FUNCTION__, __LINE__, IR->name.c_str(), instance.c_str());
//dumpModule("PINS", IR);
    pinPorts.clear();
    pinMethods.clear();
    paramPorts.clear();
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    collectInterfacePins(implements, instance != "", "", "", false, mapValue, false, "", false, false);
    if (instance == "")
        collectInterfacePins(IR, instance != "", "", "", false, mapValue, false, "", true, false);
    for (FieldElement item : IR->parameters) {
        //extractParam(item.type, mapValue);
        std::string interfaceName = item.fldName;
        ModuleIR *IIR = lookupInterface(item.type);
        if (!IIR) {
            printf("%s: in module '%s', interface lookup '%s' name '%s' failed\n", __FUNCTION__, IR->name.c_str(), item.type.c_str(), interfaceName.c_str());
            exit(-1);
        }
        if (interfaceName != "")
            interfaceName += MODULE_SEPARATOR;
printf("[%s:%d]befpin '%s'\n", __FUNCTION__, __LINE__, interfaceName.c_str());
        collectInterfacePins(IIR, instance != "", item.fldName,
            interfaceName, item.isLocalInterface, mapValue, item.isPtr, item.vecCount, false, false);
    }
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
            checkWire(item.name, item.type, item.isOutput, item.isInout, item.init, item.isLocal, true/*isArgument*/, item.vecCount);
    for (auto item: pinMethods) {
        checkWire(item.name, item.type, item.isOutput ^ (item.type != ""), false, ""/*not param*/, item.isLocal, false/*isArgument*/, item.vecCount);
        if (item.action && !isEnaName(item.name))
            checkWire(item.name + "__ENA", "", item.isOutput ^ (0), false, ""/*not param*/, item.isLocal, false/*isArgument*/, item.vecCount);
        if (trace_ports)
            printf("[%s:%d] instance %s name '%s' type %s\n", __FUNCTION__, __LINE__, instance.c_str(), item.name.c_str(), item.type.c_str());
        for (auto pitem: item.params) {
            checkWire(baseMethodName(item.name) + MODULE_SEPARATOR + pitem.name, pitem.type, item.isOutput, false, ""/*not param*/, item.isLocal, instance==""/*isArgument*/, item.vecCount);
        }
    }
    for (auto item: pinPorts)
        checkWire(item.name, item.type, item.isOutput, item.isInout, ""/*not param*/, item.isLocal, instance==""/*isArgument*/, item.vecCount);
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
        refList["CLK"] = RefItem{1, "Bit(1)", false, false, PIN_LOCAL, false, dontDeclare, "", false};
    if (!handleCLK && !hasnRST && vecCount == "")
        refList["nRST"] = RefItem{1, "Bit(1)", false, false, PIN_LOCAL, false, dontDeclare, "", false};
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
        std::string base = item;
        int ind = base.find("[");
        if (ind > 0)
            base = base.substr(0, ind);
        if (!startswith(item, "__inst$Genvar")) {
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
    if (isIdChar(item[0]) && !expr->operands.size()) {
        if (item == guardName) {
            if (trace_assign)
            printf("[%s:%d] remove guard of called method from enable line %s\n", __FUNCTION__, __LINE__, item.c_str());
            return allocExpr("1");
        }
        if (!assignList[item].noRecursion || enableListProcessing)
        if (ACCExpr *assignValue = assignList[item].value)
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
             && (checkOperand(assignValue->value) || isRdyName(item) || isEnaName(item))) {
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
    for (auto tcond = ctop->second.begin(), tend = ctop->second.end(); tcond != tend; tcond++) {
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
    for (auto CI = condLines[generateSection][methodName].info.begin(), CE = condLines[generateSection][methodName].info.end(); CI != CE; CI++)
        if (matchExpr(cond, CI->first)) {
            CI->second.push_back(CondInfo{dest, value});
            return;
        }
    condLines[generateSection][methodName].guard = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), allocExpr(getRdyName(methodName))));
    condLines[generateSection][methodName].info[cond].push_back(CondInfo{dest, value});
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
    if (trace_assign || trace_connect || (!refList[tstr].out && !refList[sstr].out))
        printf("%s: IFCCC '%s'/%d '%s'/%d\n", __FUNCTION__, tstr.c_str(), refList[tstr].out, sstr.c_str(), refList[sstr].out);
    showRef("target", target->value);
    showRef("source", source->value);
    if (refList[sstr].out) {
        setAssign(sstr, target, type);
        refList[target->value].done = true;
    }
    else {
        setAssign(tstr, source, type);
        refList[source->value].done = true;
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
    for (auto MI : IIR->methods) {
        ACCExpr *target = dupExpr(targetTree), *source = dupExpr(sourceTree);
        if (trace_connect)
            printf("[%s:%d] ICtarget %s '%s' ICsource %s\n", __FUNCTION__, __LINE__, ICtarget.c_str(), ICtarget.substr(ICtarget.length()-1).c_str(), ICsource.c_str());
        if (target->value.substr(target->value.length()-1) == MODULE_SEPARATOR)
            target->value = target->value.substr(0, target->value.length()-1);
        if (source->value.substr(source->value.length()-1) == MODULE_SEPARATOR)
            source->value = source->value.substr(0, source->value.length()-1);
        target->value += MODULE_SEPARATOR + MI->name;
        source->value += MODULE_SEPARATOR + MI->name;
        connectTarget(target, source, MI->type, isForward);
        std::string tstr = baseMethodName(target->value) + MODULE_SEPARATOR;
        std::string sstr = baseMethodName(source->value) + MODULE_SEPARATOR;
        for (auto info: MI->params) {
            ACCExpr *target = dupExpr(targetTree), *source = dupExpr(sourceTree);
            target->value = tstr + info.name;
            source->value = sstr + info.name;
            connectTarget(target, source, info.type, isForward);
        }
    }
}

void appendMux(std::string name, ACCExpr *cond, ACCExpr *value, std::string defaultValue)
{
    ACCExpr *phi = muxValueList[generateSection][name].phi;
    if (!phi) {
        phi = allocExpr("__phi", allocExpr("("));
        muxValueList[generateSection][name].phi = phi;
        muxValueList[generateSection][name].defaultValue = defaultValue;
    }
    phi->operands.front()->operands.push_back(allocExpr(":", cond, value));
}

static void generateMethod(ModuleIR *IR, std::string methodName, MethodInfo *MI)
{
    if (MI->subscript)      // from instantiateFor
        methodName += "[" + tree2str(MI->subscript) + "]";
    generateSection = MI->generateSection;
    MI->guard = cleanupBool(MI->guard);
    if (!isRdyName(methodName)) {
        walkRead(MI, MI->guard, nullptr);
        if (MI->rule)
            setAssign(getEnaName(methodName), allocExpr(getRdyName(methodName)), "Bit(1)");
    }
    setAssign(methodName, MI->guard, MI->type); // collect the text of the return value into a single 'assign'
    for (auto info: MI->storeList) {
        walkRead(MI, info->cond, nullptr);
        walkRead(MI, info->value, info->cond);
        std::string dest = info->dest->value;
        if (isIdChar(dest[0]) && !info->dest->operands.size() && refList[dest].pin == PIN_WIRE) {
            ACCExpr *cond = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), info->cond));
            appendMux(dest, cond, info->value, "0");
        }
        else
            appendLine(methodName, info->cond, info->dest, info->value);
    }
    for (auto info: MI->letList) {
        ACCExpr *cond = cleanupBool(allocExpr("&&", allocExpr(getRdyName(methodName)), info->cond));
        ACCExpr *value = info->value;
        updateWidth(value, convertType(info->type));
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        std::string dest = tree2str(info->dest);
        std::list<FieldItem> fieldList;
        getFieldList(fieldList, dest, "", info->type, 1, true);
        if (fieldList.size() == 1) {
            std::string first = fieldList.front().name;
            appendMux(dest, cond, value, "0");
            if (first != dest)
                appendMux(first, cond, value, "0");
        }
        else {
        std::string splitItem = tree2str(value);
        if ((value->operands.size() || !isIdChar(value->value[0]))) {
            splitItem = dest + "$lettemp";
            appendMux(splitItem, cond, value, "0");
        }
        for (auto fitem : fieldList) {
            std::string offset = autostr(fitem.offset);
            std::string upper = convertType(fitem.type) + " - 1";
            if (offset != "0")
                upper += " + " + offset;
            appendMux(fitem.name, cond,
                allocExpr(splitItem, allocExpr("[", allocExpr(":", allocExpr(upper), allocExpr(offset)))), "0");
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
        ACCExpr *tempCond = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), allocExpr(getRdyName(methodName)), cond));
        std::string calledName = value->value, calledEna = getEnaName(calledName);
//printf("[%s:%d] CALLLLLL '%s' condition %s\n", __FUNCTION__, __LINE__, calledName.c_str(), tree2str(tempCond).c_str());
//dumpExpr("CALLCOND", tempCond);
        if (!value->operands.size() || value->operands.front()->value != PARAMETER_MARKER) {
            printf("[%s:%d] incorrectly formed call expression\n", __FUNCTION__, __LINE__);
            exit(-1);
        }
        if (calledName == "__finish") {
            appendLine(methodName, tempCond, nullptr, allocExpr("$finish;"));
            break;
        }
        if (!enableList[generateSection][calledEna].phi) {
            enableList[generateSection][calledEna].phi = allocExpr("|");
            enableList[generateSection][calledEna].defaultValue = "1'd0";
        }
        enableList[generateSection][calledEna].phi->operands.push_back(tempCond);
        MethodInfo *CI = lookupQualName(IR, calledName);
        if (!CI) {
            printf("[%s:%d] method %s not found\n", __FUNCTION__, __LINE__, calledName.c_str());
            exit(-1);
        }
        auto AI = CI->params.begin();
        std::string pname = baseMethodName(calledName) + MODULE_SEPARATOR;
        int argCount = CI->params.size();
        ACCExpr *param = value->operands.front();
        for (auto item: param->operands) {
            if(argCount-- > 0) {
                std::string size = tree2str(cleanupInteger(cleanupExpr(str2tree(convertType(AI->type)))));
                appendMux(pname + AI->name, tempCond, item, size + "'d0");
                AI++;
            }
        }
    }
    if (!implementPrintf)
        for (auto info: MI->printfList)
            appendLine(methodName, info->cond, nullptr, info->value);
}

/*
 * Generate *.sv and *.vh for a Verilog module
 */
void generateModuleDef(ModuleIR *IR, std::list<ModData> &modLineTop)
{
static std::list<ModData> modLine;
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
    generateModuleSignature(IR, "", "", modLineTop, "");

    iterField(IR, CBAct {
        int dimIndex = 0;
        std::string vecCount = item.vecCount;
        if (vecCount != "") {
            std::string fldName = item.fldName;
            ModuleIR *itemIR = lookupIR(item.type);
            if (itemIR && !item.isPtr) {
            if (itemIR->isStruct)
                expandStruct(IR, fldName, item.type, 1, false, true, item.isShared ? PIN_WIRE : PIN_REG, true, vecCount, false);
            else
                generateModuleSignature(itemIR, item.type, fldName + MODULE_SEPARATOR, modLine, IR->params[fldName], false, vecCount, -1);
            }
        }
        std::string pvec;
        do {
            std::string fldName = item.fldName;
            ModuleIR *itemIR = lookupIR(item.type);
            if (itemIR && !item.isPtr) {
            if (itemIR->isStruct) {
                if (vecCount == "")
                    expandStruct(IR, fldName, item.type, 1, false, true, item.isShared ? PIN_WIRE : PIN_REG, true, vecCount, false);
            }
            else
                generateModuleSignature(itemIR, item.type, fldName + MODULE_SEPARATOR, modLine, IR->params[fldName], vecCount != "", vecCount, dimIndex++);
            }
            else { // if (convertType(item.type) != 0)
                if (refList[fldName].pin) {
printf("[%s:%d] dupppp %s pin %d\n", __FUNCTION__, __LINE__, fldName.c_str(), refList[fldName].pin);
                }
                assert (!refList[fldName].pin);
                refList[fldName] = RefItem{0, item.type, false, false, item.isShared ? PIN_WIRE : PIN_REG, false, false, vecCount, false};
                if (trace_declare)
                    printf("[%s:%d]NEWFLD3 %s type %s\n", __FUNCTION__, __LINE__, fldName.c_str(), item.type.c_str());
            }
            pvec = autostr(atoi(vecCount.c_str()) - 1);
            if (vecCount == "" || pvec == "0" || !isdigit(vecCount[0])) pvec = "";
            if(vecCount != GENERIC_INT_TEMPLATE_FLAG_STRING) vecCount = pvec;
//break;
        } while(vecCount != GENERIC_INT_TEMPLATE_FLAG_STRING && pvec != "");
        return nullptr; });
    for (auto item: syncPins) {
        if (item.second.name != "") {
            //bool out = item.second.out;
            //bool isPtr = item.second.isPtr;
            bool instance = item.second.instance;
            std::string sname = item.second.name;
            refList[sname] = RefItem{4, "Bit(1)", false, false, PIN_LOCAL, false, false, "", false};
            modLine.push_back(ModData{item.first + "SyncFF", "SyncFF", "", true, false, false, false, "", ""});
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
            refList[item.first] = RefItem{0, item.second.type, true, false, PIN_WIRE, false, false, convertType(item.second.type, 2), false};
            expandStruct(IR, item.first, item.second.type, 1, false, false, PIN_WIRE, true, "", false);
            //refList[item.first].count++;
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
            if (item->value->value == "__finish")
                return;
            ACCExpr *tempCond = allocExpr(getRdyName(item->value->value));
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
    // combine mux'ed assignments into a single 'assign' statement
    for (auto top: muxValueList) {
        generateSection = top.first;
        for (auto item: top.second) {
        setAssign(item.first, cleanupExprBuiltin(item.second.phi, item.second.defaultValue), refList[item.first].type);
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
}
