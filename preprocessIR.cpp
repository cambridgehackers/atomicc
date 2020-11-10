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

#define TEMP_NAME "temp"
typedef struct {
    std::string name;
    std::string value;
} PARAM_MAP;
int implementPrintf;//=1;
std::map<std::string, int> genericModule;
static void replaceMethodExpr(MethodInfo *MI, ACCExpr *pattern, ACCExpr *replacement, bool replaceLetDest = false, bool prefixReplace = false);

static void walkSubst (ModuleIR *IR, ACCExpr *expr)
{
    if (!expr)
        return;
    if (expr->value == "__clog2" || expr->value == "__builtin_clog2")
        expr->value = "$clog2";
    for (auto item: expr->operands)
        walkSubst(IR, item);
}

static std::string updateCount(std::string count, std::list<PARAM_MAP> &paramMap) // also check instantiateType()
{
    for (auto item: paramMap)
         if (!checkIntegerString(count, "0") && !checkIntegerString(count, "1")) // prohibit silly replacement
         if (checkIntegerString(count, item.value)) {
             count = item.name;
             break;
         }
    return count;
}

static ACCExpr *walkToGeneric (ACCExpr *expr, std::list<PARAM_MAP> &paramMap)
{
    if (!expr)
        return expr;
    if (expr->value == "__clog2" || expr->value == "__builtin_clog2")
        expr->value = "$clog2";
    ACCExpr *ret = allocExpr(updateCount(expr->value, paramMap));
    for (auto item: expr->operands)
        ret->operands.push_back(walkToGeneric(item, paramMap));
    return ret;
}

static std::string updateType(std::string type, std::list<PARAM_MAP> &paramMap)
{
    if (startswith(type, "ARRAY_")) {
        std::string arr = type.substr(6);
        int ind = arr.find(ARRAY_ELEMENT_MARKER);
        if (ind > 0) {
            std::string post = updateType(arr.substr(ind+1), paramMap);
            return "ARRAY_" + tree2str(walkToGeneric(str2tree(arr.substr(0, ind)), paramMap), false) + ARRAY_ELEMENT_MARKER + post;
        }
    }
    if (startswith(type, "Bit(")) {
        std::string val = type.substr(4, type.length()-5);
        return "Bit(" + tree2str(walkToGeneric(str2tree(val), paramMap), false) + ")";
    }
    int ind = type.find("(");
    if (ind > 0)
        type = type.substr(0, ind) + tree2str(walkToGeneric(cleanupModuleParam(type.substr(ind)), paramMap), false);
    return type;
};

static void copyGenericMethod(ModuleIR *genericIR, MethodInfo *MI, std::list<PARAM_MAP> &paramMap)
{
    MethodInfo *newMI = allocMethod(MI->name);
    addMethod(genericIR, newMI);
    newMI->type = updateType(MI->type, paramMap);
    newMI->guard = walkToGeneric(MI->guard, paramMap);
    newMI->subscript = MI->subscript;
    newMI->generateSection = MI->generateSection;
    newMI->isRule = MI->isRule;
    newMI->action = MI->action;
    newMI->interfaceConnect = MI->interfaceConnect;
    for (auto item : MI->storeList)
        newMI->storeList.push_back(new StoreListElement{
            walkToGeneric(item->dest, paramMap),
            walkToGeneric(item->value, paramMap),
            walkToGeneric(item->cond, paramMap)});
    for (auto item : MI->letList)
        newMI->letList.push_back(new LetListElement{
            walkToGeneric(item->dest, paramMap),
            walkToGeneric(item->value, paramMap),
            walkToGeneric(item->cond, paramMap),
            updateType(item->type, paramMap)});
    for (auto item : MI->assertList)
        newMI->assertList.push_back(new AssertListElement{
            walkToGeneric(item->value, paramMap),
            walkToGeneric(item->cond, paramMap)});
    for (auto item : MI->callList)
        newMI->callList.push_back(new CallListElement{
            walkToGeneric(item->value, paramMap),
            walkToGeneric(item->cond, paramMap),
            item->isAction});
    for (auto item : MI->printfList)
        newMI->printfList.push_back(new CallListElement{
            walkToGeneric(item->value, paramMap),
            walkToGeneric(item->cond, paramMap),
            item->isAction});
    for (auto item : MI->alloca)
        newMI->alloca[item.first] = AllocaItem{updateType(item.second.type, paramMap), item.second.noReplace};
    for (auto item : MI->generateFor)
        newMI->generateFor.push_back(GenerateForItem{
            walkToGeneric(item.cond, paramMap), item.var,
            walkToGeneric(item.init, paramMap),
            walkToGeneric(item.limit, paramMap),
            walkToGeneric(item.incr, paramMap), item.body});
    for (auto item : MI->instantiateFor)
        newMI->instantiateFor.push_back(InstantiateForItem{
            walkToGeneric(item.cond, paramMap), item.var,
            walkToGeneric(item.init, paramMap),
            walkToGeneric(item.limit, paramMap),
            walkToGeneric(item.incr, paramMap),
            walkToGeneric(item.sub, paramMap), item.body});
    //newMI->meta = MI->meta;
    for (auto item : MI->params) {
        std::string type = updateType(item.type, paramMap);
        newMI->params.push_back(ParamElement{item.name, type, ""});
    }
}

static ModuleIR *buildGeneric(ModuleIR *IR, std::string irName, std::list<PARAM_MAP> &paramMap, bool isInterface)
{
static int counter;
    //if (!(startswith(irName, "PipeIn(") || startswith(irName, "PipeInLast(") || startswith(irName, "PipeInLength(")
     //|| startswith(irName, "PipeOut(")))
    if (isInterface) {
        std::string sub;
        int ind = irName.find("(");
        if (ind > 0) {
            sub = irName.substr(ind);
            irName = irName.substr(0, ind);
        }
        irName += SUFFIX_FOR_GENERIC + autostr(counter++) + sub;
    }
    IR->transformGeneric = true;
    ModuleIR *genericIR = allocIR(irName, isInterface);
    genericModule[irName] = 1;
    std::string iName = IR->interfaceName;
    if (!isInterface)
    if (auto iifc = lookupInterface(iName)) {
        auto IIR = buildGeneric(iifc, iifc->name, paramMap, true);
        genericIR->interfaceName = IIR->name;
    }
    genericIR->genvarCount = IR->genvarCount;
    genericIR->metaList = IR->metaList;
    genericIR->softwareName = IR->softwareName;
    genericIR->priority = IR->priority;
    genericIR->moduleParams = IR->moduleParams;
    genericIR->unionList = IR->unionList;
    genericIR->interfaceConnect = IR->interfaceConnect;
    genericIR->genvarCount = IR->genvarCount;
    genericIR->isExt = IR->isExt;
    genericIR->isInterface = isInterface;
    genericIR->isStruct = IR->isStruct;
    genericIR->isSerialize = IR->isSerialize;
    genericIR->isVerilog = IR->isVerilog;
    genericIR->isTrace = IR->isTrace;
    genericIR->isPrintf = IR->isPrintf;
    genericIR->overTable = IR->overTable;
    genericIR->sourceFilename = IR->sourceFilename;
    for (auto item : IR->fields)
        genericIR->fields.push_back(FieldElement{item.fldName,
            updateCount(item.vecCount, paramMap), updateType(item.type, paramMap),
            item.isPtr, item.isInput, item.isOutput, item.isInout,
            item.isParameter, item.isShared, item.isLocalInterface, item.isExternal});
    for (auto FI : IR->generateBody)
        copyGenericMethod(genericIR, FI.second, paramMap);
    for (auto MI : IR->methods)
        copyGenericMethod(genericIR, MI, paramMap);
    for (auto item : IR->interfaces) {
        std::string iname = updateType(item.type, paramMap);
        auto IIR = buildGeneric(lookupInterface(item.type), iname, paramMap, true);
        genericIR->interfaces.push_back(FieldElement{item.fldName,
             updateCount(item.vecCount, paramMap), IIR->name, item.isPtr, item.isInput,
             item.isOutput, item.isInout,
             item.isParameter, item.isShared, item.isLocalInterface, item.isExternal});
    }
    return genericIR;
}

void preprocessMethod(ModuleIR *IR, MethodInfo *MI, bool isGenerate)
{
    std::string methodName = MI->name;
    std::map<std::string, int> localConnect;
    for (auto &item : IR->interfaces)
        if (localConnect[item.fldName])
            item.isLocalInterface = true; // interface declaration that is used to connect to local objects (does not appear in module signature)
    // now replace __bitconcat, __bitsubstr, __phi
    MI->guard = cleanupExpr(MI->guard);
    for (auto info: MI->storeList) {
        walkSubst(IR, info->dest);
        walkSubst(IR, info->cond);
        walkSubst(IR, info->value);
        info->dest = cleanupExprBuiltin(info->dest);
        info->cond = cleanupBool(info->cond);
        info->value = cleanupExprBuiltin(info->value);
    }
    for (auto info: MI->printfList) {
        walkSubst(IR, info->cond);
        walkSubst(IR, info->value);
        info->cond = cleanupBool(info->cond);
        info->value = cleanupExprBuiltin(info->value);
    }
    for (auto info: MI->assertList) {
        walkSubst(IR, info->cond);
        walkSubst(IR, info->value);
        info->cond = cleanupBool(info->cond);
        info->value = cleanupExprBuiltin(info->value);
    }
    for (auto info: MI->callList) {
        walkSubst(IR, info->cond);
        walkSubst(IR, info->value);
        info->cond = cleanupBool(info->cond);
        info->value = cleanupExprBuiltin(info->value);
    }
    for (auto info: MI->letList) {
        walkSubst(IR, info->dest);
        walkSubst(IR, info->cond);
        walkSubst(IR, info->value);
        info->dest = cleanupExprBuiltin(info->dest);
        info->cond = cleanupBool(info->cond);
        info->value = cleanupExprBuiltin(info->value);
    }
    for (auto item: MI->generateFor) {
        MethodInfo *MIb = IR->generateBody[item.body];
        assert(MIb);
        for (auto pitem: MIb->params) {
            if (MI->alloca.find(pitem.name) != MI->alloca.end())
                MI->alloca[pitem.name].noReplace = true;
        }
        MIb->generateSection = makeSection(item.var, item.init, item.limit, item.incr);
    }
    // this processing is used for subscripted interface method definitions
    for (auto item: MI->instantiateFor) {
        MethodInfo *MIb = IR->generateBody[item.body];
        std::string methodName = baseMethodName(MI->name) + PERIOD;
        assert(MIb);
        MethodInfo *MIRdy = lookupMethod(IR, getRdyName(MI->name));
        assert(MIRdy);
        MIb->subscript = item.sub;     // make sure enable line is subscripted(body is in separate function!)
        MIb->generateSection = makeSection(item.var, item.init, item.limit, item.incr);
        MIRdy->subscript = item.sub;   // make sure ready line is subscripted
        MIRdy->generateSection = MIb->generateSection;
        for (auto pitem: MIb->params) {
            if (MI->alloca.find(pitem.name) != MI->alloca.end())
                MI->alloca[pitem.name].noReplace = true;
            // make sure that parameters are subscripted with instance parameter when processed
            if (startswith(pitem.name, methodName)) {
                std::string arraySize = convertType(pitem.type, 2);
                std::string elementSize = convertType(pitem.type, 1);
                ACCExpr *lsb = MIb->subscript;
                ACCExpr *msb = allocExpr("+", MIb->subscript, allocExpr("1"));
                if (elementSize != "1") {
                    lsb = allocExpr("*", allocExpr(elementSize), lsb);
                    msb = allocExpr("*", allocExpr(elementSize), msb);
                }
                replaceMethodExpr(MIb, allocExpr(pitem.name),
                    allocExpr(pitem.name, allocExpr(SUBSCRIPT_MARKER,
                        allocExpr(":", allocExpr("-", msb, allocExpr("1")), lsb))));
            }
        }
    }
}

void preprocessIR(std::list<ModuleIR *> &irSeq)
{
    int skipLine = 0;
    // Check/replace dummy intermediate classes that are only present for typechecking
    // e.g., Fifo1 -> Fifo1Base
    for (auto IR = irSeq.begin(), IRE = irSeq.end(); IR != IRE;) {
        bool replaced = false;
        FieldElement field;
        ModuleIR *fieldIR = nullptr;
        if ((*IR)->unionList.size()
         || (*IR)->moduleParams.size()
         || (*IR)->priority.size()
         || (*IR)->softwareName.size()
         || (*IR)->metaList.size()
         || (*IR)->isTrace != ""
         || (*IR)->fields.size() != 1)
            {skipLine = __LINE__; goto skipLab;};
        field = (*IR)->fields.front();
        fieldIR = lookupIR(field.type);
        if (!fieldIR)
            {skipLine = __LINE__; goto skipLab;};
        if (ModuleIR *fieldInterface = lookupInterface(fieldIR->interfaceName))
        if ((*IR)->interfaceConnect.size()) {
            if ((*IR)->interfaceConnect.size() != fieldInterface->interfaces.size())
                {skipLine = __LINE__; goto skipLab;};
            if ((*IR)->methods.size())
                {skipLine = __LINE__; goto skipLab;};
//dumpModule("MASTER", lookupInterface((*IR)->interfaceName));
//dumpModule("FIELD", lookupInterface(fieldIR->interfaceName));
            for (auto item: (*IR)->interfaceConnect) {
                std::string target = replacePeriod(tree2str(item.target));
                std::string source = replacePeriod(tree2str(item.source));
printf("[%s:%d]CONNNECT target %s source %s type %s forward %d\n", __FUNCTION__, __LINE__, target.c_str(), source.c_str(), item.type.c_str(), item.isForward);
                 if ((target != field.fldName + DOLLAR + source)
                  && (source != field.fldName + DOLLAR + target)) {
                     {skipLine = __LINE__; goto skipLab;};
                 }
            }
        }
        if (field.vecCount != "" || field.isPtr || field.isInput
          || field.isOutput || field.isInout || field.isParameter != "" || field.isLocalInterface)
            {skipLine = __LINE__; goto skipLab;};
        for (auto MI: (*IR)->methods) {
            if (isRdyName(MI->name) && !MI->callList.size())
               continue;
            if (MI->isRule || MI->storeList.size() || MI->callList.size() != 1
             || MI->letList.size() || MI->printfList.size())
                {skipLine = __LINE__; goto skipLab;};
            if (MI->alloca.size())
                {skipLine = __LINE__; goto skipLab;};
            CallListElement *call = MI->callList.front();
            std::string calltarget = replacePeriod(call->value->value);
            std::string fieldtarget = replacePeriod(field.fldName + DOLLAR + MI->name);
            if (call->cond || calltarget != fieldtarget)
                {skipLine = __LINE__; goto skipLab;};
        }
printf("[%s:%d]WASOK %s field %s type %s\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), field.fldName.c_str(), field.type.c_str());
        for (auto mIR : irSeq) {
             for (auto &item : mIR->fields)
                 if (item.type == (*IR)->name) {
                     item.type = field.type;
                     replaced = true;
                 }
        }
        // Since we replace all usages, no need to emit module definition
        // If we found no usages, generate verilog module
        if (replaced) {
            //printf("[%s:%d] erase %s/%p\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), (*IR));
            auto ptr = mapIndex.find((*IR)->name);
            if (ptr != mapIndex.end())
                mapIndex.erase(ptr);
            ptr = mapAllModule.find((*IR)->name);
            if (ptr != mapAllModule.end())
                mapAllModule.erase(ptr);
            IR = irSeq.erase(IR);
            continue;
        }
skipLab:;
        printf("[%s:%d] skipped %s check line %d\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), skipLine);
        IR++;
    }
    // even convert 'EMODULE' items
    for (auto mapItem : mapIndex) {
        ModuleIR *IR = mapItem.second;
        std::string modName = IR->name;
        int ind = modName.find("(");
        if (modName.find(SUFFIX_FOR_GENERIC) == std::string::npos && ind > 0) {
            std::list<PARAM_MAP> pmap;
            ACCExpr *param = cleanupModuleParam(modName.substr(ind));
            for (auto item: param->operands) // list of '=' assignments
                pmap.push_back({tree2str(item->operands.front(), false), tree2str(item->operands.back(), false)});
            ModuleIR *genericIR = buildGeneric(IR, modName, pmap, false);
            modName = genericIR->name;
            if (!IR->isExt && !IR->isInterface && !IR->isStruct)
                irSeq.push_back(genericIR);
            int ind = modName.find("(");
            if (ind > 0)
                modName = modName.substr(0, ind) + PERIOD "PARAM" + modName.substr(ind);
            else
                modName += PERIOD "PARAM";
            ModuleIR *paramIR = allocIR(modName, true);
            paramIR->isInterface = true;
            genericIR->parameters.push_back(FieldElement{"", "", paramIR->name, false, false, false, false, ""/*not param*/, false, false, false});
            for (auto item: pmap)
                paramIR->fields.push_back(FieldElement{item.name, "", "Bit(32)", false, false, false, false, item.value, false, false, false});
            //printf("[%s:%d]BEGOREGEN %s/%p -> %s/%p\n", __FUNCTION__, __LINE__, modName.c_str(), IR, genericIR->name.c_str(), genericIR);
            //dumpModule("GENERIC", genericIR);
        }
    }
    for (auto IR = irSeq.begin(), IRE = irSeq.end(); IR != IRE;) {
        if ((*IR)->transformGeneric) {
            printf("[%s:%d] delete generic %s/%p\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), (void *)(*IR));
            IR = irSeq.erase(IR);
        }
        else
            IR++;
    }
    implementPrintf = (lookupIR("Printf") != nullptr);
    for (auto IR : irSeq) {
        bool hasPrintf = false;
        if (implementPrintf)
        for (auto MI: IR->methods)
            hasPrintf |= MI->printfList.size();
        if (hasPrintf)
            IR->fields.push_back(FieldElement{"printfp", "", "Printf", false/*isPtr*/, false, false, false, ""/*not param*/, false, false, false});
        std::map<std::string, int> localConnect;
        for (auto IC : IR->interfaceConnect) {
            if (!IC.isForward) {
                localConnect[tree2str(IC.target)] = 1;
                localConnect[tree2str(IC.source)] = 1;
            }
        }
        for (auto &item : IR->interfaces)
            if (localConnect[item.fldName])
                item.isLocalInterface = true; // interface declaration that is used to connect to local objects (does not appear in module signature)
        // expand all subscript calculations before processing the module
        for (auto MI: IR->methods)
            preprocessMethod(IR, MI, false);
        for (auto item: IR->generateBody)
            preprocessMethod(IR, item.second, true);
    }
}

static ACCExpr *walkReplaceExpr (ACCExpr *expr, ACCExpr *pattern, ACCExpr *replacement, bool prefixReplace)
{
    if (!expr)
        return expr;
    if (prefixReplace) {
        if (startswith(expr->value, pattern->value))
            return allocExpr(replacement->value + PERIOD + expr->value.substr(pattern->value.length()));
    }
    else if (matchExpr(expr, pattern))
        return replacement;
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    for (auto item: expr->operands)
        if (ACCExpr *operand = walkReplaceExpr(item, pattern, replacement, prefixReplace))
            newExpr->operands.push_back(operand);
    return newExpr;
}
static void replaceMethodExpr(MethodInfo *MI, ACCExpr *pattern, ACCExpr *replacement, bool replaceLetDest, bool prefixReplace)
{
    MI->guard = walkReplaceExpr(MI->guard, pattern, replacement, prefixReplace);
    for (auto item: MI->storeList) {
        item->dest = walkReplaceExpr(item->dest, pattern, replacement, prefixReplace);
        item->value = walkReplaceExpr(item->value, pattern, replacement, prefixReplace);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement, prefixReplace);
    }
    for (auto item: MI->letList) {
        if (replaceLetDest)
            item->dest = walkReplaceExpr(item->dest, pattern, replacement, prefixReplace);
        item->value = walkReplaceExpr(item->value, pattern, replacement, prefixReplace);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement, prefixReplace);
    }
    for (auto item: MI->assertList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement, prefixReplace);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement, prefixReplace);
    }
    for (auto item: MI->callList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement, prefixReplace);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement, prefixReplace);
    }
    for (auto item: MI->printfList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement, prefixReplace);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement, prefixReplace);
    }
}

static void hoistVerilog(ModuleIR *top, ModuleIR *current, std::string prefix)
{
    if (top != current)
    for (auto item: current->fields)
         top->fields.push_back(FieldElement{prefix + item.fldName, item.vecCount,
             item.type, item.isPtr, item.isInput, item.isOutput, item.isInout,
             item.isParameter, item.isShared, item.isLocalInterface, item.isExternal});
    for (auto item: current->interfaces)
         hoistVerilog(top, lookupInterface(item.type), prefix+item.fldName);
}

void cleanupIR(std::list<ModuleIR *> &irSeq)
{
    // preprocess 'isVerilog' items first -> changes interface
    for (auto mapItem : mapAllModule) {
        ModuleIR *IR = mapItem.second;
        ModuleIR *IIR = lookupInterface(IR->interfaceName);
        if (IR->isVerilog)
            hoistVerilog(IIR, IIR, "");
    }
}
