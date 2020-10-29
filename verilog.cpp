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

int trace_global;//= 1;
int trace_assign;//= 1;
int trace_declare;//= 1;
int trace_ports;//= 1;
int trace_connect;//= 1;
int trace_skipped;//= 1;
int trace_mux;//= 1;

typedef struct {
    std::string value;
    std::string type;
} InterfaceMapType;

std::list<PrintfInfo> printfFormat;
ModList modNew;
std::map<std::string, CondLineType> condLines;
std::map<std::string, SyncPinInfo> syncPins;    // SyncFF items needed for PipeInSync instances

std::map<std::string, AssignItem> assignList;
std::map<std::string, std::map<std::string, AssignItem>> condAssignList; // used for 'generate' items
std::map<std::string, std::map<std::string, MuxValueElement>> muxValueList; // used to calculate __phi for multiple assignments
static std::map<std::string, std::map<std::string, MuxValueElement>> enableList;
static std::string generateSection; // for 'generate' regions, this is the top level loop expression, otherwise ''
static std::map<std::string, std::string> mapParam;
static bool handleCLK;
static ModList::iterator moduleParameter;

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

static void decRef(std::string name)
{
    if (refList[name].count > 0 && refList[name].pin != PIN_MODULE) {
        refList[name].count--;
        if (trace_assign)
            printf("[%s:%d] dec count[%s]=%d\n", __FUNCTION__, __LINE__, name.c_str(), refList[name].count);
    }
}

static void setReference(std::string target, int count, std::string type, bool out = false, bool inout = false,
    int pin = PIN_WIRE, std::string vecCount = "", bool isArgument = false)
{
    if (trace_global)
        printf("%s: target %s pin %d out %d inout %d type %s veccount %s count %d isArgument %d\n", __FUNCTION__, target.c_str(), pin, out, inout, type.c_str(), vecCount.c_str(), count, isArgument);
    if (lookupIR(type) != nullptr || lookupInterface(type) != nullptr || vecCount != "") {
        count += 1;
printf("[%s:%d]STRUCT %s type %s vecCount %s\n", __FUNCTION__, __LINE__, target.c_str(), type.c_str(), vecCount.c_str());
    }
    if (refList[target].pin) {
        printf("[%s:%d] %s pin exists %d new %d\n", __FUNCTION__, __LINE__, target.c_str(), refList[target].pin, pin);
        exit(-1);
    }
    refList[target] = RefItem{count, type, out, inout, pin, false, vecCount, isArgument};
}

static void setAssign(std::string target, ACCExpr *value, std::string type)
{
    bool tDir = refList[target].out;
    int tPin = refList[target].pin;
    if (!value)
        return;
    std::string valStr = tree2str(value);
    bool sDir = refList[valStr].out;
    int sPin = refList[valStr].pin;
    if (trace_interface || trace_global)
        printf("%s: [%s/%d/%d]count[%d] = %s/%d/%d type '%s'\n", __FUNCTION__, target.c_str(), tDir, tPin, refList[target].count, valStr.c_str(), sDir, sPin, type.c_str());
    if (sPin == PIN_OBJECT && tPin != PIN_OBJECT) {
        value = allocExpr(target);
        target = valStr;
    }
    if (!refList[target].pin && generateSection == "") {
        std::string base = target;
        int ind = base.find_first_of(PERIOD "[");
        if (ind > 0)
            base = base.substr(0, ind);
        if (!refList[base].pin) {
            printf("[%s:%d] missing target [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), valStr.c_str(), type.c_str());
            exit(-1);
        }
    }
    updateWidth(value, convertType(type));
    if (assignList[target].type != "" && assignList[target].value->value != "{") { // aggregate alloca items always start with an expansion expr
        printf("[%s:%d] duplicate start [%s] = %s type '%s'\n", __FUNCTION__, __LINE__, target.c_str(), tree2str(value).c_str(), type.c_str());
        printf("[%s:%d] duplicate was      = %s type '%s'\n", __FUNCTION__, __LINE__, tree2str(assignList[target].value).c_str(), assignList[target].type.c_str());
        exit(-1);
    }
    bool noRecursion = (target == "CLK" || target == "nRST");
    if (generateSection != "")
        condAssignList[generateSection][target] = AssignItem{value, type, noRecursion, 0};
    else
        assignList[target] = AssignItem{value, type, noRecursion, 0};
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
    if (isIdChar(expr->value[0])) {
        fixupAccessible(expr->value);
        if (cond && (!expr->operands.size() || expr->operands.front()->value != PARAMETER_MARKER))
            addRead(MI->meta[MetaRead][expr->value], cond);
    }
}

static void addModulePort (ModList &modParam, std::string name, std::string type, int dir, bool inout, std::string isparam, bool isLocal, bool isArgument, std::string vecCount, MapNameValue &mapValue, std::string instance, bool isParam)
{
    std::string newtype = instantiateType(type, mapValue);
    vecCount = instantiateType(vecCount, mapValue);
    if (newtype != type) {
        printf("[%s:%d] iinst %s CHECKTYPE %s newtype %s\n", __FUNCTION__, __LINE__, instance.c_str(), type.c_str(), newtype.c_str());
        //exit(-1);
        type = newtype;
    }
    int refPin = (instance != "" || isLocal) ? PIN_OBJECT: PIN_MODULE;
    if (isParam)
        refPin = PIN_CONSTANT;
    std::string instName = instance + name;
    if (trace_assign || trace_ports || trace_interface)
        printf("[%s:%d] instance '%s' iName %s name %s type %s dir %d io %d ispar '%s' isLoc %d pin %d vecCount %s\n", __FUNCTION__, __LINE__, instance.c_str(), instName.c_str(), name.c_str(), type.c_str(), dir, inout, isparam.c_str(), isLocal, refPin, vecCount.c_str());
    fixupAccessible(instName);
    if (!isLocal || instance == "") {
        setReference(instName, (dir != 0 || inout) && instance == "", type, dir != 0, inout, refPin, vecCount, isArgument);
        if(instance == "" && vecCount != "")
            refList[instName].done = true;  // prevent default blanket assignment generation
    }
    if (!isLocal) {
        if (isparam != "")
            modParam.insert(moduleParameter, ModData{name, instName, type, false, false, dir, inout, isparam, vecCount});
        else {
            modParam.push_back(ModData{name, instName, type, false, false, dir, inout, isparam, vecCount});
            if (moduleParameter == modParam.end())
                moduleParameter--;
        }
    }
}

static void collectInterfacePins(ModuleIR *IR, ModList &modParam, std::string instance, std::string pinPrefix, std::string methodPrefix, bool isLocal, MapNameValue &parentMap, bool isPtr, std::string vecCount, bool localInterface, bool isVerilog)
{
    assert(IR);
    MapNameValue mapValue = parentMap;
    vecCount = instantiateType(vecCount, mapValue);
//for (auto item: mapValue)
//printf("[%s:%d] [%s] = %s\n", __FUNCTION__, __LINE__, item.first.c_str(), item.second.c_str());
//printf("[%s:%d] IR %s instance %s pinpref %s methpref %s isLocal %d ptr %d isVerilog %d veccount %s\n", __FUNCTION__, __LINE__, IR->name.c_str(), instance.c_str(), pinPrefix.c_str(), methodPrefix.c_str(), isLocal, isPtr, isVerilog, vecCount.c_str());
    if (!localInterface || methodPrefix != "")
    for (auto MI: IR->methods) {
        std::string name = methodPrefix + MI->name;
        bool out = (instance != "") ^ isPtr;
        addModulePort(modParam, name, MI->type, out ^ (MI->type != ""), false, ""/*not param*/, isLocal, false/*isArgument*/, vecCount, mapValue, instance, false);
        if (MI->action && !isEnaName(name))
            addModulePort(modParam, name + "__ENA", "", out ^ (0), false, ""/*not param*/, isLocal, false/*isArgument*/, vecCount, mapValue, instance, false);
        if (trace_ports)
            printf("[%s:%d] instance %s name '%s' type %s\n", __FUNCTION__, __LINE__, instance.c_str(), name.c_str(), MI->type.c_str());
        for (auto pitem: MI->params)
            addModulePort(modParam, baseMethodName(name) + DOLLAR + pitem.name, pitem.type, out, false, ""/*not param*/, isLocal, instance==""/*isArgument*/, vecCount, mapValue, instance, true);
    }
    if ((!localInterface || pinPrefix != "") && (pinPrefix == "" || !isVerilog))
    for (auto fld: IR->fields) {
        std::string name = pinPrefix + fld.fldName;
//printf("[%s:%d] name %s ftype %s local %d\n", __FUNCTION__, __LINE__, name.c_str(), fld.type.c_str(), isLocal);
        bool out = (instance != "") ^ fld.isOutput;
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
            if (instance == "")
                addModulePort(modParam, name, fld.isPtr ? "POINTER" : fld.type, out, fld.isInout, init, isLocal, true/*isArgument*/, vecCount, mapValue, instance, true);
        }
        else
            addModulePort(modParam, name, fld.type, out, fld.isInout, ""/*not param*/, isLocal, instance==""/*isArgument*/, vecCount, mapValue, instance, false);
    }
    for (FieldElement item : IR->interfaces) {
        MapNameValue imapValue;  // interface hoisting only deals with parameters for the interface itself (not containing module)
//printf("[%s:%d] INTERFACES instance %s type %s name %s\n", __FUNCTION__, __LINE__, instance.c_str(), item.type.c_str(), item.fldName.c_str());
        if (instance != "")
            extractParam("INTERFACES_" + item.fldName, item.type, imapValue);
        bool ptrFlag = isPtr || item.isPtr;
        bool out = (instance != "") ^ item.isOutput ^ ptrFlag;
        ModuleIR *IIR = lookupInterface(item.type);
        if (!IIR) {
            printf("%s: in module '%s', interface lookup '%s' name '%s' failed\n", __FUNCTION__, IR->name.c_str(), item.type.c_str(), item.fldName.c_str());
            exit(-1);
        }
        std::string type = item.type;
        std::string updatedVecCount = vecCount;
        if (item.vecCount != "")
            updatedVecCount = item.vecCount;
        //printf("[%s:%d] name %s type %s veccc %s updated %s\n", __FUNCTION__, __LINE__, (methodPrefix + item.fldName + DOLLAR).c_str(), type.c_str(), item.vecCount.c_str(), updatedVecCount.c_str());
        bool localFlag = isLocal || item.isLocalInterface;
        if (startswith(type, "PipeInSync")) {
            type = IIR->interfaces.front().type;   // rewrite to PipeIn type
            if (!localInterface) {
                std::string itemname = methodPrefix + item.fldName;
                std::string suffix = out ? "__RDY" : "__ENA";
                std::string oldName = itemname + PERIOD "enq" + suffix;
                std::string newName = LOCAL_VARIABLE_PREFIX + itemname + DOLLAR "enq" "S" + suffix;
                printf("[%s:%d] PipeInSync oldname %s newname %s out %d isPtr %d instance %d\n", __FUNCTION__, __LINE__, oldName.c_str(), newName.c_str(), out, isPtr, (instance != ""));
                setReference(newName, 4, "Bit(1)", false, false, PIN_OBJECT);
                refList[oldName].count++;
                refList[newName].count++;
                syncPins[oldName] = {newName, instance != ""};
            }
        }
        if (item.fldName == "" || isVerilog)
            collectInterfacePins(IIR, modParam, instance, pinPrefix + item.fldName, methodPrefix + item.fldName + DOLLAR, localFlag, imapValue, ptrFlag, updatedVecCount, localInterface, isVerilog);
        else
            addModulePort(modParam, methodPrefix + item.fldName, type, out, false, ""/*not param*/, localFlag, false/*isArgument*/, updatedVecCount, mapValue, instance, false);
    }
}

static void findCLK(ModuleIR *IR, std::string pinPrefix, bool isVerilog)
{
    if (pinPrefix == "" || !isVerilog)
    for (auto fld: IR->fields) {
        if (fld.isParameter == "") {
            handleCLK = false;
            return;
        }
    }
    for (FieldElement item : IR->interfaces) {
        ModuleIR *IIR = lookupInterface(item.type);
        if (item.fldName == "" || isVerilog)
            findCLK(IIR, pinPrefix + item.fldName, isVerilog);
    }
}

/*
 * Generate verilog module header for class definition or reference
 */
static void generateModuleSignature(std::string moduleName, std::string instance, ModList &modParam, std::string moduleParams, std::string vecCount)
{
    ModuleIR *IR = lookupIR(moduleName);
    assert(IR);
    ModuleIR *implements = lookupInterface(IR->interfaceName);
    assert(implements);
    std::string minst;
    if (instance != "")
        minst = instance.substr(0, instance.length()-1);
    std::string moduleInstantiation = moduleName;
    int ind = moduleInstantiation.find("(");
    if (ind > 0)
        moduleInstantiation = moduleInstantiation.substr(0, ind);
    if (instance != "")
        moduleInstantiation = genericModuleParam(moduleName, moduleParams, nullptr);
    handleCLK = true;
    findCLK(implements, "", IR->isVerilog);
    modParam.push_back(ModData{minst, moduleInstantiation, "", true/*moduleStart*/, !handleCLK, 0, false, ""/*not param*/, vecCount});
    moduleParameter = modParam.end();

    MapNameValue mapValue, mapValueMod;
    extractParam("SIGNIFC_" + moduleName + ":" + instance, IR->interfaceName, mapValue);
    if (instance != "")
        extractParam("SIGN_" + moduleName + ":" + instance, moduleName, mapValue);    // instance values overwrite duplicate interface values (if present)
    else // only substitute parameters from interface definition that were not present in original module definition
        extractParam("SIGN_" + moduleName, moduleName, mapValueMod);
    for (auto item: mapValueMod) {
        auto result = mapValue.find(item.first);
        if (result != mapValue.end())
            mapValue.erase(result);
    }
    collectInterfacePins(implements, modParam, instance, "", "", false, mapValue, false, vecCount, false, IR->isVerilog);
    if (instance == "") {
        mapValue.clear();
        collectInterfacePins(IR, modParam, instance, "", "", false, mapValue, false, vecCount, true, IR->isVerilog);
    }
    for (FieldElement item : IR->parameters) {
        std::string interfaceName = item.fldName;
        if (interfaceName != "")
            interfaceName += PERIOD;
        collectInterfacePins(lookupInterface(item.type), modParam, instance, item.fldName, interfaceName, item.isLocalInterface, mapValue, item.isPtr, item.vecCount, false, IR->isVerilog);
    }
}

static ACCExpr *walkRemoveParam (ACCExpr *expr)
{
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string op = expr->value;
    if (isIdChar(op[0])) {
        int pin = refList[op].pin;
        if (refList[op].isArgument) {
//if (trace_assign)
printf("[%s:%d] INFO: reject use of non-state op %s %d\n", __FUNCTION__, __LINE__, op.c_str(), pin);
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
    bool skipRecursion = false;
    if (!expr)
        return;
    std::string item = expr->value;
    if (item == PERIOD) {
        item = tree2str(expr);
        skipRecursion = true;
    }
//printf("[%s:%d]FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF %s count %d pin %d\n", __FUNCTION__, __LINE__, item.c_str(), refList[item].count, refList[item].pin);
    if (isIdChar(item[0])) {
        if (!startswith(item, "__inst$Genvar") && item != "$past") {
        std::string base = item;
        int ind = base.find(PERIOD "[");
        if (ind > 0)
            base = base.substr(0, ind);
        if (!refList[item].pin) {
            if (base != item) {
                if (trace_assign)
                    printf("[%s:%d] RRRRREFFFF %s -> %s\n", __FUNCTION__, __LINE__, base.c_str(), item.c_str());
                if(!refList[base].pin) {
                    printf("[%s:%d] refList[%s] definition missing\n", __FUNCTION__, __LINE__, item.c_str());
                    //exit(-1);
                }
            }
        }
        }
        refList[item].count++;
        ACCExpr *temp = assignList[item].value;
        if (trace_assign)
            printf("[%s:%d] inc count[%s]=%d temp '%s'\n", __FUNCTION__, __LINE__, item.c_str(), refList[item].count, tree2str(temp).c_str());
        if (temp && refList[item].count == 1)
            walkRef(temp);
    }
    if (!skipRecursion)
    for (auto item: expr->operands)
        walkRef(item);
}

static ACCExpr *replaceAssign(ACCExpr *expr, std::string guardName = "")
{
    if (!expr)
        return expr;
    if (trace_assign)
        printf("[%s:%d] start %s expr %s\n", __FUNCTION__, __LINE__, guardName.c_str(), tree2str(expr).c_str());
    std::string item = expr->value;
    if (item == PERIOD)
        item = tree2str(expr);
    if (guardName != "" && startswith(item, guardName)) {
        if (trace_interface)
        printf("[%s:%d] remove guard '%s' of called method from enable line %s\n", __FUNCTION__, __LINE__, guardName.c_str(), item.c_str());
        return allocExpr("1");
    }
    if (expr->value == PERIOD || (isIdChar(item[0]) && !expr->operands.size())) {
        if (trace_interface)
            printf("[%s:%d]item %s norec %d enableList %d value %s walkcount %d\n", __FUNCTION__, __LINE__, item.c_str(), assignList[item].noRecursion, guardName != "", tree2str(assignList[item].value).c_str(), walkCount(assignList[item].value));
        if (!assignList[item].noRecursion || guardName != "")
        if (ACCExpr *assignValue = assignList[item].value)
        if (walkCount(assignValue) < ASSIGN_SIZE_LIMIT || guardName != "") {
            decRef(item);
            walkRef(assignValue);
            std::string val = tree2str(assignValue);
            std::string valContents = tree2str(assignList[val].value);
            //if (!isdigit(valContents[0]))
                //assignList[val].noRecursion = true; // to prevent ping/pong
            if (trace_interface)
                printf("[%s:%d] replace %s norec %d with %s\n", __FUNCTION__, __LINE__, item.c_str(), assignList[item].noRecursion, tree2str(assignValue).c_str());
            return replaceAssign(assignValue, guardName);
        }
    }
    ACCExpr *newExpr = allocExpr(expr->value);
    for (auto item: expr->operands)
        if (expr->value == PERIOD) // cannot replace just part of a member specification
            newExpr->operands.push_back(item);
        else
            newExpr->operands.push_back(replaceAssign(item, guardName));
    newExpr = cleanupExpr(newExpr, true);
    if (trace_assign)
    printf("[%s:%d] end %s expr %s\n", __FUNCTION__, __LINE__, guardName.c_str(), tree2str(newExpr).c_str());
    return newExpr;
}

static ACCExpr *simpleReplace(ACCExpr *expr)
{
    if (!expr)
        return expr;
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    if (isIdChar(item[0]) && !expr->operands.size()) {
//printf("[%s:%d] item %s norec %d val %s\n", __FUNCTION__, __LINE__, item.c_str(), assignList[item].noRecursion, tree2str(assignList[item].value).c_str());
        ACCExpr *assignValue = assignList[item].value;
        if (!assignList[item].noRecursion)
        if (assignValue)
        if (refList[item].pin != PIN_MODULE
             && (checkOperand(assignValue->value) || isRdyName(item) || isEnaName(item)
                || (startswith(item, BLOCK_NAME) && assignList[item].size < COUNT_LIMIT))) {
            std::string val = tree2str(assignValue);
            std::string valContents = tree2str(assignList[val].value);
            //if (!isdigit(valContents[0]))
            //assignList[val].noRecursion = true; // to prevent ping/pong
            if (trace_interface)
            printf("[%s:%d] replace %s with %s\n", __FUNCTION__, __LINE__, item.c_str(), tree2str(assignValue).c_str());
            decRef(item);
            walkRef(assignValue);
            return simpleReplace(assignValue);
        }
    }
    for (auto oitem: expr->operands)
        if (expr->value == PERIOD)
            newExpr->operands.push_back(oitem);
        else
            newExpr->operands.push_back(simpleReplace(oitem));
    return newExpr;
}

static void setAssignRefCount(ModuleIR *IR)
{
    traceZero("SETASSIGNREF");
    // before doing the rest, clean up block guards
    for (auto &item : assignList) {
        if (item.second.type == "Bit(1)" && startswith(item.first, BLOCK_NAME)) {
            item.second.value = simpleReplace(item.second.value);
            item.second.size = walkCount(item.second.value);
        }
    }
    for (auto item: assignList) {
//printf("[%s:%d] ref[%s].norec %d value %s\n", __FUNCTION__, __LINE__, item.first.c_str(), assignList[item.first].noRecursion, tree2str(item.second.value).c_str());
        assignList[item.first].value = cleanupExpr(simpleReplace(item.second.value));
    }
    for (auto &ctop : condLines) // process all generate sections
    for (auto &alwaysGroup : ctop.second.always)
    for (auto &tcond : alwaysGroup.second.cond) {
        std::string methodName = tcond.first;
        walkRef(tcond.second.guard);
        tcond.second.guard = cleanupBool(replaceAssign(tcond.second.guard));
        if (trace_assign)
            printf("[%s:%d] %s: guard %s\n", __FUNCTION__, __LINE__, methodName.c_str(), tree2str(tcond.second.guard).c_str());
        auto info = tcond.second.info;
        tcond.second.info.clear();
        walkRef(tcond.second.guard);
        for (auto &item : info) {
            ACCExpr *cond = cleanupBool(replaceAssign(item.second.cond));
            tcond.second.info[tree2str(cond)] = CondGroupInfo{cond, item.second.info};
            walkRef(cond);
            for (auto citem: item.second.info) {
                if (trace_assign)
                    printf("[%s:%d] %s: %s dest %s value %s\n", __FUNCTION__, __LINE__, methodName.c_str(),
 item.first.c_str(), tree2str(citem.dest).c_str(), tree2str(citem.value).c_str());
                if (citem.dest) {
                    walkRef(citem.value);
                    walkRef(citem.dest);
                }
                else
                    walkRef(citem.value->operands.front());
            }
        }
    }
    for (auto &top: muxValueList)
        for (auto &item: top.second)
            walkRef(item.second.phi);

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
}

static void appendLine(std::string methodName, ACCExpr *cond, ACCExpr *dest, ACCExpr *value)
{
    dest = replaceAssign(dest);
    value = replaceAssign(value);
    auto &element = condLines[generateSection].always[ALWAYS_CLOCKED].cond[methodName];
    for (auto &CI : element.info)
        if (matchExpr(cond, CI.second.cond)) {
            CI.second.info.push_back(CondInfo{dest, value});
            return;
        }
    element.guard = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), allocExpr(getRdyName(methodName))));
    element.info[tree2str(cond)].cond = cond;
    element.info[tree2str(cond)].info.push_back(CondInfo{dest, value});
}

static bool getDirection(std::string &name)
{
    std::string orig = name;
    bool ret = refList[name].out;
    int ind = name.find_first_of(PERIOD "[");
    if (ind > 0) {
        std::string post = name.substr(ind);
        name = name.substr(0, ind);
        std::string type = refList[name].type;
        ret = refList[name].out;
        std::string sub;
        if (post[0] == '[') {
            int ind = post.find("]");
            sub = post.substr(0, ind);
            post = post.substr(ind);
        }
        if (post[0] == '.')
            post = post.substr(1);
        if (trace_assign || trace_connect || trace_interface)
            printf("[%s:%d] name %s post %s sub %s type %s count %d\n", __FUNCTION__, __LINE__, name.c_str(), post.c_str(), sub.c_str(), type.c_str(), refList[name].count);
        if (auto IR = lookupInterface(type)) {
            for (auto MI: IR->methods) {
                if (MI->name == post)
                    if (MI->type != "") {
                        //ret = !ret;
                    }
            }
        }
        //refList[orig].count++;
        //refList[orig].out = ret;
    }
    return ret;
}

static void connectTarget(ACCExpr *target, ACCExpr *source, std::string type, bool isForward)
{
    std::string tstr = tree2str(target), sstr = tree2str(source);
    std::string tif = tstr, sif = sstr;
    // TODO: need to update this to be 'has a return type' (i.e. handle value methods
    bool tdir = getDirection(tif) ^ endswith(tstr, "__RDY");
    bool sdir = getDirection(sif) ^ endswith(sstr, "__RDY");
    if (trace_assign || trace_connect || trace_interface || (!tdir && !sdir))
        printf("%s: IFCCC '%s'/%d/%d pin %d '%s'/%d/%d pin %d\n", __FUNCTION__, tstr.c_str(), tdir, refList[target->value].out, refList[tif].pin, sstr.c_str(), sdir, refList[source->value].out, refList[sif].pin);
    if (sdir) {
        if (assignList[sstr].type == "")
            setAssign(sstr, target, type);
        else
            printf("[%s:%d] skip assign to %s type %s\n", __FUNCTION__, __LINE__, sstr.c_str(), assignList[sstr].type.c_str());
        refList[tif].done = true;
    }
    else {
        if (assignList[tstr].type == "")
            setAssign(tstr, source, type);
        else
            printf("[%s:%d] skip assign to %s type %s\n", __FUNCTION__, __LINE__, tstr.c_str(), assignList[tstr].type.c_str());
        refList[sif].done = true;
    }
}

static void connectMethodList(std::string interfaceName, ACCExpr *targetTree, ACCExpr *sourceTree, bool isForward)
{
    ModuleIR *IIR = lookupInterface(interfaceName);
    assert(IIR);
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
        std::string tstr = replacePeriod(tree2str(target));
        std::string sstr = replacePeriod(tree2str(source));
        fixupAccessible(tstr);
        fixupAccessible(sstr);
        target = allocExpr(tstr);
        source = allocExpr(sstr);
//printf("[%s:%d] target %s source %s\n", __FUNCTION__, __LINE__, tstr.c_str(), sstr.c_str());
        connectTarget(target, source, MI->type, isForward);
        tstr = baseMethodName(tstr) + DOLLAR;
        sstr = baseMethodName(sstr) + DOLLAR;
        for (auto info: MI->params)
            connectTarget(allocExpr(tstr+info.name), allocExpr(sstr+info.name), info.type, isForward);
    }
}

static bool verilogInterface(ModuleIR *searchIR, std::string searchStr)
{
    int ind = searchStr.find("[");
    if (ind > 0) {
        std::string sub;
        extractSubscript(searchStr, ind, sub);
    }
    searchStr = replacePeriod(searchStr);
    ind = searchStr.find(DOLLAR);
    if (ind > 0) {
        std::string fieldname = searchStr.substr(0, ind);
        for (auto item: searchIR->fields)
            if (fieldname == item.fldName) {
                if (auto IR = lookupIR(item.type))
                    return IR->isVerilog;
                break;
            }
    }
    return false;
}

static void connectMethods(ModuleIR *IR, std::string ainterfaceName, ACCExpr *targetTree, ACCExpr *sourceTree, bool isForward)
{
    std::string interfaceName = ainterfaceName;
    if (startswith(interfaceName, "ARRAY_")) {
        interfaceName = interfaceName.substr(6);
        int ind = interfaceName.find(ARRAY_ELEMENT_MARKER);
        if (ind > 0)
            interfaceName = interfaceName.substr(ind+1);
    }
    ModuleIR *IIR = lookupInterface(interfaceName);
    if (!IIR) {
        printf("[%s:%d] interface not found '%s' target %s source %s\n", __FUNCTION__, __LINE__, interfaceName.c_str(), tree2str(targetTree).c_str(), tree2str(sourceTree).c_str());
    }
    assert(IIR);
    std::string ICtarget = tree2str(targetTree);
    std::string ICsource = tree2str(sourceTree);
    if (ICsource.find(DOLLAR) != std::string::npos && ICtarget.find(DOLLAR) == std::string::npos) {
        ACCExpr *temp = targetTree;
        targetTree = sourceTree;
        sourceTree = temp;
        ICtarget = tree2str(targetTree);
        ICsource = tree2str(sourceTree);
    }
    bool targetIsVerilog = verilogInterface(IR, ICtarget);
    bool sourceIsVerilog = verilogInterface(IR, ICsource);
    if (trace_connect)
        printf("%s: CONNECT target '%s' source '%s' forward %d interfaceName %s\n", __FUNCTION__, ICtarget.c_str(), ICsource.c_str(), isForward, interfaceName.c_str());
    for (auto fld : IIR->fields) {
        ACCExpr *target = dupExpr(targetTree), *source = dupExpr(sourceTree);
        if (target->value == "" || endswith(target->value, DOLLAR) || targetIsVerilog)
            target->value += fld.fldName;
        else
            target = allocExpr(PERIOD, target, allocExpr(fld.fldName));
        if (source->value == "" || endswith(source->value, DOLLAR) || sourceIsVerilog)
            source->value += fld.fldName;
        else
            source = allocExpr(PERIOD, source, allocExpr(fld.fldName));
        if (ModuleIR *IR = lookupIR(fld.type))
            connectMethods(IR, IR->interfaceName, target, source, isForward);
        else
            connectTarget(target, source, fld.type, isForward);
    }
    for (auto fld : IIR->interfaces) {
        ACCExpr *target = dupExpr(targetTree), *source = dupExpr(sourceTree);
        target->value += fld.fldName;
        source->value += fld.fldName;
        connectMethods(IR, fld.type, target, source, isForward);
    }
    if (IIR->methods.size())
        connectTarget(targetTree, sourceTree, interfaceName, isForward);
}

static void appendMux(std::string section, std::string name, ACCExpr *cond, ACCExpr *value, std::string defaultValue)
{
    ACCExpr *phi = muxValueList[section][name].phi;
    if (!phi) {
        phi = allocExpr("__phi", allocExpr("("));
        muxValueList[section][name].phi = phi;
        muxValueList[section][name].defaultValue = defaultValue;
    }
    if (trace_mux) {
        printf("[%s:%d] name %s \n", __FUNCTION__, __LINE__, name.c_str());
        dumpExpr("COND", cond);
        dumpExpr("VALUE", value);
    }
    if (!checkInteger(value, "0") || refList[name].type != "Bit(1)")
        phi->operands.front()->operands.push_back(allocExpr(":", cond ? cond : allocExpr("1"), value));
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
    ACCExpr *guard = MI->guard;
    if (MI->type == "Bit(1)") {
        guard = cleanupBool(guard);
    }
    setAssign(methodName, guard, MI->type); // collect the text of the return value into a single 'assign'
    for (auto IC : MI->interfaceConnect)
        connectMethods(IR, IC.type, IC.target, IC.source, IC.isForward);
}
static void generateMethod(ModuleIR *IR, std::string methodName, MethodInfo *MI)
{
    if (MI->subscript)      // from instantiateFor
        methodName += "[" + tree2str(MI->subscript) + "]";
    generateSection = MI->generateSection;
    for (auto info: MI->storeList) {
        std::string dest = info->dest->value;
        ACCExpr *cond = info->cond;
        ACCExpr *value = info->value;
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        updateWidth(value, convertType(refList[dest].type));
        if (isIdChar(dest[0]) && !info->dest->operands.size() && refList[dest].pin == PIN_WIRE) {
            cond = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), cond));
            appendMux(generateSection, dest, cond, value, "0");
        }
        else
            appendLine(methodName, cleanupBool(cond), info->dest, value);
    }
    for (auto info: MI->letList) {
        ACCExpr *cond = info->cond;
        ACCExpr *value = info->value;
        std::string dest = tree2str(info->dest);
        std::string root = dest;
        int ind = root.find(PERIOD);
        if (ind > 0)
            root = root.substr(0,ind);
        auto alloca = MI->alloca.find(root);
        if (alloca == MI->alloca.end()) {
//printf("[%s:%d] LEEEEEEEEEEEEETTTTTTTTTTTTTTTTTTTTTTTTTTTTT not alloca %s\n", __FUNCTION__, __LINE__, root.c_str());
            cond = allocExpr("&&", allocExpr(getEnaName(methodName)), info->cond);
        }
        cond = cleanupBool(cond);
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        updateWidth(value, convertType(info->type));
        appendMux(generateSection, dest, cond, value, "0");
    }
    for (auto info: MI->assertList) {
        ACCExpr *cond = info->cond;
        ACCExpr *value = info->value;
        walkRead(MI, cond, nullptr);
        walkRead(MI, value, cond);
        auto par = value->operands.front()->operands;
        ACCExpr *param = cleanupBool(par.front());
        par.clear();
        par.push_back(param);
        ACCExpr *guard = nullptr;
        if (MethodInfo *MIRdy = lookupMethod(IR, getRdyName(methodName))) {
            guard = cleanupBool(MIRdy->guard);
        }
        ACCExpr *tempCond = guard ? allocExpr("&&", guard, cond) : cond;
        tempCond = cleanupBool(tempCond);
        std::string calledName = value->value, calledEna = getEnaName(calledName);
        condLines[generateSection].assert.push_back(AssertVerilog{tempCond, value});
    }
    for (auto info: MI->callList) {
        ACCExpr *subscript = nullptr;
        int ind = info->value->value.find("[");
        if (ind > 0) {
            std::string sub = info->value->value.substr(ind+1);
            int rind = sub.find("]");
            if (rind > 0) {
                info->value->value = info->value->value.substr(0, ind) + sub.substr(rind+1);
                sub = sub.substr(0, rind);
                subscript = allocExpr(sub);
            }
        }
        std::string section = generateSection;
        ACCExpr *cond = info->cond;
        ACCExpr *value = info->value;
        walkRead(MI, value, cond);
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
        if (!info->isAction)
            continue;
        ACCExpr *tempCond = cleanupBool(allocExpr("&&", allocExpr(getEnaName(methodName)), //allocExpr(getRdyName(methodName)),
 cond));
        tempCond = cleanupBool(replaceAssign(simpleReplace(tempCond), getRdyName(calledEna))); // remove __RDY before adding subscript!
        walkRead(MI, tempCond, nullptr);
        if (subscript) {
            std::string sub, post;
            ACCExpr *var = allocExpr(GENVAR_NAME "1");
            section = makeSection(var->value, allocExpr("0"),
                allocExpr("<", var, allocExpr(vecCount)), allocExpr("+", var, allocExpr("1")));
            sub = "[" + var->value + "]";
            int ind = calledEna.find_last_of(DOLLAR PERIOD);
            if (ind > 0) {
                post = calledEna.substr(ind);
                calledEna = calledEna.substr(0, ind);
                ind = post.rfind(DOLLAR);
printf("[%s:%d] called %s ind %d\n", __FUNCTION__, __LINE__, calledEna.c_str(), ind);
                if (ind > 1)
                    post = post.substr(0, ind) + PERIOD + post.substr(ind+1);
            }
            calledEna += sub + post;
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
                std::string size; // = tree2str(cleanupInteger(cleanupExpr(str2tree(convertType(instantiateType(AI->type, mapValue)))))) + "'d";
                appendMux(section, pname + AI->name, tempCond, item, size + "0");
                AI++;
            }
        }
        MI->meta[MetaInvoke][calledEna].insert(tree2str(cond));
    }
    if (!implementPrintf)
        for (auto info: MI->printfList)
            appendLine(methodName, info->cond, nullptr, info->value);
}

static void generateMethodGroup(ModuleIR *IR, void (*generateMethod)(ModuleIR *IR, std::string methodName, MethodInfo *MI))
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
    std::string temp = target;
    int ind = temp.find('[');
    if (ind != -1)
        temp = temp.substr(0,ind);
    if (!refList[target].done && source)
    if (auto interface = lookupInterface(type)) {
        connectMethodList(type, allocExpr(target), source, false);
        refList[target].done = true; // mark that assigns have already been output
        refList[tree2str(source)].done = true; // mark that assigns have already been output
    }
}

static void generateField(ModuleIR *IR, ModList &modLine, FieldElement &item)
{
    ModuleIR *itemIR = lookupIR(item.type);
    if (!itemIR || item.isPtr || itemIR->isStruct)
        setReference(item.fldName, item.isShared, item.type, false, false, item.isShared ? PIN_WIRE : PIN_REG, item.vecCount);
    else {
//printf("[%s:%d] INSTANTIATEFIELD type %s fldName %s veccount %s\n", __FUNCTION__, __LINE__, item.type.c_str(), item.fldName.c_str(), item.vecCount.c_str());
        generateModuleSignature(item.type, item.fldName + DOLLAR, modLine, "(" + IR->moduleParams[item.fldName] + ")", item.vecCount);
    }
}

static ACCExpr *generateSubscriptReference(ModuleIR *IR, ModList &modLine, std::string name)
{
    fixupAccessible(name);
    std::string type, nameVec = refList[name].vecCount;
    std::string orig = name;
    ACCExpr *subExpr = nullptr;
    MapNameValue mapValue;
    MethodInfo *MI = lookupQualName(IR, name, nameVec, mapValue);
    if (MI)
        type = MI->type;
    if (isIdChar(name[0]) && nameVec != "" && type != "") {
        int ind = name.find("[");
        std::string sub;
        if (ind > 0) {
            extractSubscript(name, ind, sub);
            subExpr = str2tree(sub);
            subExpr = (subExpr && subExpr->operands.size() == 1) ? subExpr->operands.front() : nullptr;
//dumpExpr("SUBB", subExpr);
        }
//printf("[%s:%d] name %s sub %s namevec %s\n", __FUNCTION__, __LINE__, name.c_str(), sub.c_str(), nameVec.c_str());
        if (subExpr && isdigit(subExpr->value[0]))
            return nullptr; //allocExpr(orig);
    }
    else
        return nullptr; //allocExpr(orig);
    int ind;
    std::string newName = name;
    while ((ind = newName.find(PERIOD)) > 0)
        newName = newName.substr(0, ind) + "__" + newName.substr(ind+1);  // to defeat the walkAccessible() processing, use "__"
    std::string name_or = newName + "_or";       // convert unpacked array to vector
    std::string name_or1 = name_or + "1";
    if (!refList[name_or].pin) {
        // adding a space to the 'name_or' declaration forces the
        // code generator to emit bitvector dimensions explicitly,
        // to that the subscripted 'setAssign' below compiles correctly
        // (you can use 'foo[0]' if the declaration is 'wire [0:0] foo',
        // but not if the declaration is 'wire foo').  Hmm...
        setReference(name_or, 99, type, false, false, PIN_WIRE, nameVec);
        setReference(name_or1, 99, type);
        std::string objectName = name_or + "CC";
        IR->fields.push_back(FieldElement{objectName, "", "SelectIndex(width=" + convertType(type) + ",funnelWidth=" + nameVec + ")", false, false, false, false, "", false, false, false});
        generateField(IR, modLine, IR->fields.back());
        objectName += DOLLAR;
        setAssign(objectName + "out", allocExpr(name_or1), type);
        setAssign(objectName + "in", allocExpr(name_or), type);
        setAssign(objectName + "index", subExpr, "Bit(32)");
        ACCExpr *var = allocExpr(GENVAR_NAME "1");
        genvarMap[var->value] = 1;
        generateSection = makeSection(var->value, allocExpr("0"),
            allocExpr("<", var, allocExpr(nameVec)), allocExpr("+", var, allocExpr("1")));
        std::string newSub = "[" + var->value + "]";
        ind = name.find(PERIOD);
        if (ind > 0)
            name = name.substr(0, ind) + newSub + name.substr(ind);
        else
            name = name + newSub;
        setAssign(name_or + newSub, allocExpr(name), "Bit(1)");
        generateSection = "";
    }
    return allocExpr(name_or1);
}

static ACCExpr *walkSubscriptReference(ModuleIR *IR, ModList &modLine, ACCExpr *expr)
{
    std::string value = expr->value;
    bool recurse = true;
    if (value == PERIOD) {
        value = tree2str(expr);
        recurse = false;
    }
printf("[%s:%d] exprval %s value %s\n", __FUNCTION__, __LINE__, expr->value.c_str(), value.c_str());
    ACCExpr *ret = generateSubscriptReference(IR, modLine, value);
    if (!ret)
        return expr;
    if (recurse)
        for (auto item: expr->operands)
            ret->operands.push_back(walkSubscriptReference(IR, modLine, item));
    return ret;
}

static void prepareMethodGuards(ModuleIR *IR, ModList &modLine)
{
    for (auto MI : IR->methods) { // walkRemoveParam depends on the iterField above
        std::string methodName = MI->name;
        if (MI->rule)             // both RDY and ENA must be allocated for rules
            setReference(methodName, 0, MI->type, true);
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
            //ACCExpr *value = generateSubscriptReference(IR, modLine, getRdyName(name));
            name = getRdyName(name);
            fixupAccessible(name);
            ACCExpr *value = allocExpr(name);
            if (item->cond)
                value = allocExpr("|", walkRemoveParam(invertExpr(item->cond)), value);
            MIRdy->guard = cleanupBool(allocExpr("&&", MIRdy->guard, value));
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

        if (MI->type != "") {
            MI->guard = walkSubscriptReference(IR, modLine, MI->guard);
        }
    }
}

static void fixupModuleInstantiations(ModList &modLine)
{
    // last chance to optimize out single assigns to output ports
    std::map<std::string, std::string> mapPort;
    std::map<std::string, InterfaceMapType> interfaceMap;
    for (auto item: assignList) {
        std::string val = tree2str(item.second.value);
        if ((refList[item.first].out || refList[item.first].inout) && refList[item.first].pin == PIN_MODULE
          && item.second.value && isIdChar(item.second.value->value[0]) && !item.second.value->operands.size()
          && refList[item.second.value->value].pin != PIN_MODULE) {
            mapPort[val] = item.first;
        }
        if (lookupInterface(item.second.type)) {
            std::string value = tree2str(item.second.value);
            interfaceMap[item.first] = InterfaceMapType{value, item.second.type};
            interfaceMap[value] = InterfaceMapType{item.first, item.second.type};
            if (trace_interface)
                printf("[%s:%d] interfacemap %s %s\n", __FUNCTION__, __LINE__, item.first.c_str(), value.c_str());
        }
    }

    bool skipReplace = false;
    std::string modname;
    for (auto mitem: modLine) {
        std::string val = mitem.value;
        if (mitem.moduleStart) {
            skipReplace = mitem.vecCount != "" || val == "SyncFF";
            modname = mitem.value;
            mapParam.clear();
        }
        else if (!skipReplace) {
            ACCExpr *assignExpr = assignList[val].value;
            std::string assignValue = tree2str(assignExpr);
            std::string newValue = assignValue;
            if (newValue == "")
                newValue = mapParam[val];
            if (newValue == "")
                newValue = interfaceMap[val].value;
            if (trace_interface || trace_global)
                printf("%s: replaceParam %s: '%s' count %d done %d mapPort '%s' interfaceMap '%s' mapParam '%s' assign '%s'\n", __FUNCTION__, modname.c_str(), val.c_str(), refList[val].count, refList[val].done, mapPort[val].c_str(), interfaceMap[val].value.c_str(), mapParam[val].c_str(), newValue.c_str());
            int ind = val.find_first_of(PERIOD DOLLAR);
            if (newValue == "" && refList[val].count == 0 && ind > 1) {
                std::string prefix = val.substr(0, ind);
                std::string post = val.substr(ind);
                auto look = interfaceMap.find(prefix);
                if (look != interfaceMap.end()) {
                    if (trace_interface)
                        printf("[%s:%d] find prefix %s post %s lookvalue %s looktype %s\n", __FUNCTION__, __LINE__, prefix.c_str(), post.c_str(), look->second.value.c_str(), look->second.type.c_str());
                    refList[look->first].done = true;
                    refList[look->second.value].done = true;
                    auto IIR = lookupInterface(look->second.type);
                    for (auto MI : IIR->methods) {
                        std::string tstr = replacePeriod(prefix + DOLLAR + MI->name);
                        std::string sstr = replacePeriod(look->second.value + DOLLAR + MI->name);
                        mapParam[tstr] = sstr;
                        tstr = baseMethodName(tstr) + DOLLAR;
                        sstr = baseMethodName(sstr) + DOLLAR;
                        for (auto info: MI->params)
                            mapParam[tstr+info.name] = sstr+info.name;
                    }
                }
            }
            std::string oldVal = val;
            if (assignValue != "") {
                val = assignValue;
                decRef(mitem.value);
                walkRef(assignExpr);
            }
            else if (mapParam[val] != "")
                val = mapParam[val];
            else if (refList[val].count == 0 && interfaceMap[val].value != "") {
                val = interfaceMap[val].value;
                refList[val].count++;
                refList[val].done = true;
            }
            else if (mapPort[val] != "") {
                val = mapPort[val];
                decRef(mitem.value);
                walkRef(str2tree(val));
            }
            else if (refList[val].count) {
                val = tree2str(replaceAssign(allocExpr(val)));
            }
            else
                val = (refList[val].out && !refList[val].inout) ? "0" : "";
            if (oldVal != val)
                refList[val].done = true;  // 'assign' line not needed; value is assigned by object inst
            if (trace_interface)
                printf("[%s:%d] newval %s\n", __FUNCTION__, __LINE__, val.c_str());
        }
        modNew.push_back(ModData{mitem.argName, val, mitem.type, mitem.moduleStart, mitem.noDefaultClock, mitem.out, mitem.inout, ""/*not param*/, mitem.vecCount});
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
    // 'Mux' together parameter settings from all invocations of a method from this class
    muxValueList.clear();
    enableList.clear();
    modLine.clear();
    syncPins.clear();     // pins from PipeInSync

    printf("[%s:%d] STARTMODULE %s/%p\n", __FUNCTION__, __LINE__, IR->name.c_str(), (void *)IR);
    //dumpModule("START", IR);
    generateModuleSignature(IR->name, "", modLineTop, "", "");
    bool hasCLK = false, hasnRST = false;
    if (!handleCLK) {
        for (auto mitem: modLineTop)
            if (!mitem.moduleStart && mitem.isparam == "") {
                if (mitem.value == "CLK")
                    hasCLK = true;
                if (mitem.value == "nRST")
                    hasnRST = true;
            }
        if (!hasCLK)
            setReference("CLK", 2, "Bit(1)", false, false, PIN_WIRE);
        if (!hasnRST)
            setReference("nRST", 2, "Bit(1)", false, false, PIN_WIRE);
    }
    std::string traceDataGather, traceDataType;
    if (IR->isTrace) {
        ACCExpr *gather = allocExpr(","), *length = allocExpr("+");
        ACCExpr *gatherp = allocExpr(","), *lengthp = allocExpr("+");
        for (auto mitem: modLineTop) {
            std::string name = mitem.value;
            if (mitem.moduleStart || mitem.isparam != "" || name == "CLK" || name == "nRST")
                continue;
            std::string prefix = mitem.value + PERIOD;
            if (ModuleIR *IIR = lookupInterface(mitem.type)) {
                for (auto method : IIR->methods) {
                    gather->operands.push_back(allocExpr(prefix + method->name));
                    std::string mprefix = prefix + baseMethodName(method->name) + DOLLAR;
                    length->operands.push_back(allocExpr((method->type == "") ? "1" : convertType(method->type)));
                    for (auto &param: method->params) {
                        gatherp->operands.push_back(allocExpr(mprefix + param.name));
                        lengthp->operands.push_back(allocExpr(convertType(param.type)));
                    }
                }
            }
            else {
                gather->operands.push_back(allocExpr(name));
                length->operands.push_back(allocExpr(convertType(mitem.type)));
            }
        }
        std::string sensitivity = tree2str(length, false);
        for (auto item: lengthp->operands)
            length->operands.push_back(item);
        for (auto item: gatherp->operands)
            gather->operands.push_back(item);
        traceDataGather = "{32'd0," + tree2str(gather, false) + "}";
        std::string totalLength = "32+" + tree2str(length, false);
        length->value = ",";
        std::string interpretString = tree2str(length, false);
        traceDataType = "Trace(width=(" + totalLength + "), depth=" + autostr(IR->isTrace) + ", sensitivity=" + sensitivity + ")";
        IR->fields.push_back(FieldElement{"__traceMemory", "", traceDataType, false, false, false, false, "", false, false, false});
printf("gather %s\n", traceDataGather.c_str());
printf("interpret %s\n", interpretString.c_str());
printf("total %s\n", totalLength.c_str());
printf("traceDataType %s\n", traceDataType.c_str());
        std::string filename = IR->name;
        int ind = filename.find("(");
        if (ind > 0)
            filename = filename.substr(0, ind);
        FILE *traceDataFile = fopen (("generated/" + filename + ".trace").c_str(), "w");
        fprintf(traceDataFile, "WIDTH %s\nDEPTH %d\n", totalLength.c_str(), IR->isTrace);
        fprintf(traceDataFile, "COUNT %d\n------------\nTIME 32\n", (int)length->operands.size()+1);
        auto litem = length->operands.begin();
        auto gitem = gather->operands.begin();
        while (litem != length->operands.end()) {
            fprintf(traceDataFile, "%s %s\n", (*gitem)->value.c_str(), (*litem)->value.c_str());
            litem++;
            gitem++;
        }
        fclose(traceDataFile);
    }

    for (auto &item: IR->fields) {
        generateField(IR, modLine, item);
    }
    for (auto item : syncPins) {
        std::string oldName = item.first;
        std::string newName = item.second.name;
        modLine.push_back(ModData{replacePeriod(oldName) + "SyncFF", "SyncFF", "", true/*moduleStart*/, false, false, false, "", ""});
        modLine.push_back(ModData{"out", item.second.instance ? oldName : newName, "Bit(1)", false, false, true/*out*/, false, "", ""});
        modLine.push_back(ModData{"in", item.second.instance ? newName : oldName, "Bit(1)", false, false, false /*out*/, false, "", ""});
    }
    if (traceDataGather != "") {
       setAssign("__traceMemory$CLK", allocExpr("CLK"), "Bit(1)");
       setAssign("__traceMemory$nRST", allocExpr("nRST"), "Bit(1)");
       setAssign("__traceMemory$data", allocExpr(traceDataGather), traceDataType);
       setAssign("__traceMemory$enable", allocExpr("1"), "Bit(1)");
       refList["__traceMemory$out"].count++;  // force allocation so that we can hierarchically reference later
    }

    for (auto MI : IR->methods) { // walkRemoveParam depends on the iterField above
        for (auto item: MI->alloca) { // be sure to define local temps before walkRemoveParam
            setReference(item.first, 0, item.second.type, true, false, PIN_WIRE, convertType(item.second.type, 2));
        }
    }
    buildAccessible(IR);
    prepareMethodGuards(IR, modLine);

    // generate wires for internal methods RDY/ENA.  Collect state element assignments
    // from each method
    for (auto IC : IR->interfaceConnect)
        connectMethods(IR, IC.type, IC.target, IC.source, IC.isForward);
    traceZero("AFTCONNECT");
    generateMethodGroup(IR, generateMethodGuard);
    generateMethodGroup(IR, generateMethod);

    // combine mux'ed assignments into a single 'assign' statement
    for (auto &top: enableList) {
        generateSection = top.first;
        for (auto &item: top.second) {
            ACCExpr *expr = replaceAssign(simpleReplace(item.second.phi), getRdyName(item.first));
            setAssign(item.first, expr, "Bit(1)");
            if (startswith(item.first, BLOCK_NAME))
                assignList[item.first].size = walkCount(assignList[item.first].value);
        }
    }
    for (auto &top: muxValueList) {
        generateSection = top.first;
        for (auto &item: top.second) {
            item.second.phi = replaceAssign(simpleReplace(item.second.phi));
            if (refList[item.first].type == "Bit(1)")
                item.second.phi = cleanupBool(item.second.phi);
            else
                item.second.phi = cleanupExpr(item.second.phi);
            if (item.second.phi && (!item.second.phi->operands.size() || item.second.phi->operands.front()->operands.size() < 2 || refList[item.first].type == "Bit(1)")) {
                //if (refList[item.first].type != "Bit(1)")
                item.second.phi = cleanupExprBuiltin(item.second.phi, item.second.defaultValue);
                setAssign(item.first, item.second.phi, refList[item.first].type);
                if (startswith(item.first, BLOCK_NAME))
                    assignList[item.first].size = walkCount(assignList[item.first].value);
                item.second.phi = nullptr;
            }
            else {
                refList[item.first].count++;
                refList[item.first].done = true;
            }
        }
    }
    generateSection = "";

#if 0
    for (auto item: assignList) {
        interfaceAssign(item.first, item.second.value, item.second.type);
    }
#endif
    for (auto ctop: condAssignList) {
        for (auto item: ctop.second) {
            generateSection = ctop.first;
            interfaceAssign(item.first, item.second.value, item.second.type);
        }
    }
    generateSection = "";

    // if a struct is referenced, make sure all the members also have non-zero counts
    std::string prevName;
    int prevCount = 0;
    for (auto &item : refList) {
         if (prevName != "" && startswith(item.first, prevName))
             item.second.count += prevCount;
         else {
             prevName = item.first + PERIOD;
             prevCount = item.second.count;
         }
    }

    setAssignRefCount(IR);

    // recursively process all replacements internal to the list of 'setAssign' items
    for (auto &item : assignList)
        item.second.value = replaceAssign(item.second.value);
    for (auto &top: muxValueList)
        for (auto &item: top.second)
            item.second.phi = replaceAssign(item.second.phi);

    // process assignList replacements, mark referenced items
    fixupModuleInstantiations(modLine);
}
