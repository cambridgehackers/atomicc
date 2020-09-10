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
    if (isIdChar(expr->value[0]))
         expr->value = fixupQualPin(IR, expr->value);  // fixup interface names
    for (auto item: expr->operands)
        walkSubst(IR, item);
}

static void walkSubscript (ModuleIR *IR, ACCExpr *expr, bool inGenerate)
{
    if (!expr)
        return;
    for (auto item: expr->operands)
        walkSubscript(IR, item, inGenerate);
    std::string fieldName = expr->value;
    if (!isIdChar(fieldName[0]) || !expr->operands.size() || expr->operands.front()->value != SUBSCRIPT_MARKER)
        return;
    ACCExpr *subscript = expr->operands.front()->operands.front();
    expr->operands.pop_front();
    std::string post;
    if (expr->operands.size() && isIdChar(expr->operands.front()->value[0])) {
        post = expr->operands.front()->value;
        expr->operands.pop_front();
        if (post[0] == '$')
            post = PERIOD + post.substr(1);
    }
    expr->value += "[" + tree2str(subscript) + "]" + post;
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
    int ind = type.find("(");
    if (ind > 0 && type[type.length()-1] == ')') {
        std::string base = type.substr(0, ind+1);
        type = type.substr(ind+1);
        type = type.substr(0, type.length()-1);
        // over
        type = base + tree2str(walkToGeneric(str2tree(type), paramMap), false) + ")";
    }
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
    newMI->rule = MI->rule;
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
    if (!(startswith(irName, "PipeIn(") || startswith(irName, "PipeInB(")
     || startswith(irName, "PipeOut(")))
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
    genericIR->params = IR->params;
    genericIR->unionList = IR->unionList;
    genericIR->interfaceConnect = IR->interfaceConnect;
    genericIR->genvarCount = IR->genvarCount;
    genericIR->isExt = IR->isExt;
    genericIR->isInterface = isInterface;
    genericIR->isStruct = IR->isStruct;
    genericIR->isSerialize = IR->isSerialize;
    genericIR->isVerilog = IR->isVerilog;
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
    for (auto item = IR->interfaces.begin(); item != IR->interfaces.end(); item++)
        if (localConnect[item->fldName])
            item->isLocalInterface = true; // interface declaration that is used to connect to local objects (does not appear in module signature)
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

static void typeCleanIR(ModuleIR *IR);
static std::map<std::string, std::string> typeCleanMap;
static std::string addTypeCleanMap(std::string oldType, std::string newType)
{
    auto lookup = typeCleanMap.find(oldType);
    if (lookup == typeCleanMap.end())
        typeCleanMap[oldType] = newType;
    return newType;
}

static std::string typeClean(std::string type)
{
    if (startswith(type + VARIANT, "PipeIn" VARIANT)) {
        auto IR = lookupInterface(type);
        auto argType = IR->methods.front()->params.front().type; // enq(Bit(x) v);
        return addTypeCleanMap(type, "PipeIn(width=" + convertType(argType) + ")");
    }
    if (startswith(type + VARIANT, "PipeOut" VARIANT)) {
        auto IR = lookupInterface(type);
        for (auto MI: IR->methods)
            if (MI->name == "first")
                return addTypeCleanMap(type, "PipeOut(width=" + convertType(MI->type) + ")"); // Bit(x) first();
    }
    if (startswith(type + VARIANTP, "PipeInB" VARIANTP)) {
        auto IR = lookupInterface(type);
        auto argType = IR->methods.front()->params.front().type; // enq(Bit(x) v);
        return addTypeCleanMap(type, "PipeInB(width=" + convertType(argType) + ")");
    }
    if (auto ftype = lookupIR(type))
        typeCleanIR(ftype);
    if (auto interface = lookupInterface(type))
        typeCleanIR(interface);
    return type;
}

static void typeCleanMethod(MethodInfo *MI)
{
    MI->type = typeClean(MI->type);
    for (auto itemi = MI->params.begin(), iteme = MI->params.end(); itemi != iteme; itemi++)
        itemi->type = typeClean(itemi->type);
    for (auto itemi = MI->alloca.begin(), iteme = MI->alloca.end(); itemi != iteme; itemi++)
        itemi->second.type = typeClean(itemi->second.type);
    for (auto item: MI->letList)
        item->type = typeClean(item->type);
    for (auto itemi = MI->interfaceConnect.begin(), iteme = MI->interfaceConnect.end(); itemi != iteme; itemi++)
        itemi->type = typeClean(itemi->type);
}

static void typeCleanIR(ModuleIR *IR)
{
    if (auto interface = lookupInterface(IR->interfaceName))
        typeCleanIR(interface);
    for (auto itemi = IR->interfaceConnect.begin(), iteme = IR->interfaceConnect.end(); itemi != iteme; itemi++)
        itemi->type = typeClean(itemi->type);
    for (auto itemi = IR->unionList.begin(), iteme = IR->unionList.end(); itemi != iteme; itemi++)
        itemi->type = typeClean(itemi->type);
    for (auto itemi = IR->fields.begin(), iteme = IR->fields.end(); itemi != iteme; itemi++)
        itemi->type = typeClean(itemi->type);
    for (auto itemi = IR->interfaces.begin(), iteme = IR->interfaces.end(); itemi != iteme; itemi++) {
        itemi->type = typeClean(itemi->type);
    }
    for (auto MI: IR->methods)
        typeCleanMethod(MI);
}

static ModuleIR *copyInterface(std::string oldName, std::string newName, MapNameValue &mapValue)
{
    ModuleIR *oldIR = lookupInterface(oldName);
    ModuleIR *IR = allocIR(newName, oldIR->isInterface);
    IR->metaList = oldIR->metaList;
    IR->softwareName = oldIR->softwareName;
    IR->generateBody = oldIR->generateBody;
    IR->priority = oldIR->priority;
    IR->fields = oldIR->fields;
    IR->params = oldIR->params;
    IR->unionList = oldIR->unionList;
    IR->interfaces = oldIR->interfaces;
    IR->interfaceConnect = oldIR->interfaceConnect;
    IR->genvarCount = oldIR->genvarCount;
    IR->isStruct = oldIR->isStruct;
    IR->isSerialize = oldIR->isSerialize;
    IR->isVerilog = oldIR->isVerilog;
    IR->sourceFilename = oldIR->sourceFilename;
    IR->transformGeneric = oldIR->transformGeneric;
    for (auto MI : oldIR->methods) {
        MethodInfo *nMI = allocMethod(MI->name);
        IR->methods.push_back(nMI);
        for (auto pitem: MI->params)
            nMI->params.push_back(ParamElement{pitem.name, instantiateType(pitem.type, mapValue), pitem.init});
        nMI->guard = MI->guard;
        nMI->subscript = MI->subscript;
        nMI->generateSection = MI->generateSection;
        nMI->rule = MI->rule;
        nMI->action = MI->action;
        nMI->storeList = MI->storeList;
        nMI->letList = MI->letList;
        nMI->assertList = MI->assertList;
        nMI->callList = MI->callList;
        nMI->printfList = MI->printfList;
        nMI->type = instantiateType(MI->type, mapValue);
        nMI->generateFor = MI->generateFor;
        nMI->instantiateFor = MI->instantiateFor;
        nMI->alloca = MI->alloca;
        //nMI->meta = MI->meta;
    }
//dumpModule("NEWMOD", IR);
    return IR;
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
         || (*IR)->params.size()
         || (*IR)->priority.size()
         || (*IR)->softwareName.size()
         || (*IR)->metaList.size()
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
            if (MI->rule || MI->storeList.size() || MI->callList.size() != 1
             || MI->alloca.size() || MI->letList.size() || MI->printfList.size())
                {skipLine = __LINE__; goto skipLab;};
            CallListElement *call = MI->callList.front();
            std::string calltarget = replacePeriod(call->value->value);
            std::string fieldtarget = replacePeriod(field.fldName + DOLLAR + MI->name);
            if (call->cond || calltarget != fieldtarget)
                {skipLine = __LINE__; goto skipLab;};
        }
printf("[%s:%d]WASOK %s field %s type %s\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), field.fldName.c_str(), field.type.c_str());
        for (auto mIR : irSeq) {
             for (auto item = mIR->fields.begin(), iteme = mIR->fields.end(); item != iteme; item++)
                 if (item->type == (*IR)->name) {
                     item->type = field.type;
                     replaced = true;
                 }
        }
        // Since we replace all usages, no need to emit module definition
        // If we found no usages, generate verilog module
        if (replaced) {
            //printf("[%s:%d] erase %s/%p\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), (*IR));
            mapIndex.erase(mapIndex.find((*IR)->name));
            mapAllModule.erase(mapAllModule.find((*IR)->name));
            IR = irSeq.erase(IR);
            continue;
        }
skipLab:;
        printf("[%s:%d] skipped %s check line %d\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), skipLine);
        IR++;
    }
    // even convert 'EMODULE' items
#if 1
    for (auto mapItem : mapIndex) {
        ModuleIR *IR = mapItem.second;
#else
    for (auto IR : irSeq)
#endif
        std::string modName = IR->name;
        if (modName.find(SUFFIX_FOR_GENERIC) != std::string::npos)
            continue;
        int ind = modName.find("(");
        if (ind > 0) {
            std::string irName = modName;//.substr(0, ind);
            std::string parg = modName.substr(ind + 1);
            if(parg.substr(parg.length()-1) != ")") {
                printf("[%s:%d] Error: argument without ')' modname '%s' irname '%s' parg '%s'\n", __FUNCTION__, __LINE__, modName.c_str(), irName.c_str(), parg.c_str());
                exit(-1);
            }
            parg = parg.substr(0, parg.length() - 1);
            std::string pname;
            std::list<PARAM_MAP> paramMap;
//printf("[%s:%d]START %s was %s\n", __FUNCTION__, __LINE__, irName.c_str(), IR->name.c_str());
            while (parg != "") {
                int indVal = parg.find("=");
                if (indVal <= 0)
                    break;
                pname = parg.substr(0, indVal);
                std::string pvalue = parg.substr(indVal+1);
                parg = "";
                int indNext = pvalue.find(",");
                if (indNext > 0) {
                    parg = pvalue.substr(indNext + 1);
                    pvalue = pvalue.substr(0, indNext);
                }
//printf("[%s:%d] name %s val %s\n", __FUNCTION__, __LINE__, pname.c_str(), pvalue.c_str());
                paramMap.push_back(PARAM_MAP{pname, pvalue});
            }
            ModuleIR *genericIR = buildGeneric(IR, irName, paramMap, false);
            irName = genericIR->name;
            if (!IR->isExt && !IR->isInterface && !IR->isStruct)
                irSeq.push_back(genericIR);
            int ind = irName.find("(");
            if (ind > 0)
                irName = irName.substr(0, ind) + PERIOD "PARAM" + irName.substr(ind);
            else
                irName += PERIOD "PARAM";
            ModuleIR *paramIR = allocIR(irName, true);
            paramIR->isInterface = true;
            genericIR->parameters.push_back(FieldElement{"", "", paramIR->name, false, false, false, false, ""/*not param*/, false, false, false});
            for (auto item: paramMap)
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
            walkSubscript(IR, IC.target, false);
            walkSubscript(IR, IC.source, false);
            //walkSubst(IR, info->target);
            //walkSubst(IR, info->source);
            //info->target = cleanupExprBuiltin(info->target);
            //info->source = cleanupExprBuiltin(info->source);
            if (!IC.isForward) {
                localConnect[tree2str(IC.target)] = 1;
                localConnect[tree2str(IC.source)] = 1;
            }
        }
        for (auto item = IR->interfaces.begin(); item != IR->interfaces.end(); item++)
            if (localConnect[item->fldName])
                item->isLocalInterface = true; // interface declaration that is used to connect to local objects (does not appear in module signature)
        // expand all subscript calculations before processing the module
        for (auto MI: IR->methods)
            preprocessMethod(IR, MI, false);
        for (auto item: IR->generateBody)
            preprocessMethod(IR, item.second, true);
    }
    for (auto mapItem : mapAllModule)
        typeCleanIR(mapItem.second);   // normalize all 'PipeIn_xxx' and 'PipeOut_xxx'
    for (auto item: typeCleanMap) {
        std::string name = item.second;
        if (startswith(name, "PipeIn("))
            name = "PipeIn(width=32)";
        auto updateCopyType = [&](std::string &type) -> void {
            if (type != "" && type != "Bit(1)")
                type = "Bit(width)";
        };
        MapNameValue mapValue;
        ModuleIR *IIR = copyInterface(item.first, name, mapValue);
        for (auto MI: IIR->methods) {
            updateCopyType(MI->type);
            for (auto itemi = MI->params.begin(), iteme = MI->params.end(); itemi != iteme; itemi++)
                updateCopyType(itemi->type);
        }
        //interfaceIndex[item.first] = IIR; // replace old with new
    }
#if 0
    for (auto IR : irSeq) {
        for (auto MI: IR->methods) {
            if (MI->type == "" && MI->params.size() == 0)
                continue;
            std::string name = MI->name;
            std::string interface = IR->interfaceName;
            auto IIR = lookupInterface(interface);
            int ind = name.find_first_of(DOLLAR PERIOD);
            std::string ifcname;
            if (ind > 0) {
                ifcname = name.substr(0, ind);
                name = name.substr(ind+1);
                for (auto item: IIR->interfaces) {
                    if (item.fldName == ifcname) {
                        interface = item.type;
                        IIR = lookupInterface(interface);
                        break;
                    }
                }
            }
            auto typeMI = lookupMethod(IIR, name);
            //printf("[%s:%d] interface %s ifcname %s name %s typeMI %p\n", __FUNCTION__, __LINE__, interface.c_str(), ifcname.c_str(), name.c_str(), typeMI);
            std::map<std::string, std::string> remapParam;
            if (typeMI)
            for (auto origp = MI->params.begin(), orige = MI->params.end(), typep = typeMI->params.begin(); origp != orige; origp++, typep++) {
                //if (origp->type != typep->type)
                if (startswith(interface, "PipeIn") && !startswith(origp->type, "Bit(")) {
                    remapParam[origp->name] = origp->type;
                    origp->type = "Bit(" + convertType(origp->type) + ")"; //typep->type;
                }
            }
            for(auto item: remapParam) {
                std::string param = baseMethodName(replacePeriod(MI->name)) + DOLLAR + item.first;
                ACCExpr *replacement = allocExpr(TEMP_NAME DOLLAR + param);
                MI->alloca[replacement->value] = AllocaItem{item.second, false};
                replaceMethodExpr(MI, allocExpr(param + DOLLAR), replacement, false, true);  // replace prefixes on all expr values
                MI->letList.push_back(new LetListElement{replacement, allocExpr(param), nullptr, item.second});
            }
        }
    }
#endif
}

/*
 * rewrite method call parameter and subscript markers, since '[' and '{'
 * are needed in verilog output
 */
static void rewriteExpr(MethodInfo *MI, ACCExpr *expr)
{
    if (!expr)
        return;
    if (expr->value == "[")
        expr->value = SUBSCRIPT_MARKER;
    else if (expr->value == "{")
        expr->value = PARAMETER_MARKER;
    else if (isIdChar(expr->value[0])) {
        auto ptr = MI->alloca.upper_bound(expr->value);
        if (ptr != MI->alloca.begin()) {
            --ptr;
            if (expr->value != ptr->first && startswith(expr->value, ptr->first)) {
                ptr->second.noReplace = true;
//printf("[%s:%d] %s: expr %s first %s\n", __FUNCTION__, __LINE__, MI->name.c_str(), expr->value.c_str(), ptr->first.c_str());
            }
        }
    }
    for (auto item: expr->operands)
        rewriteExpr(MI, item);
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

static void postParseCleanup(ModuleIR *IR, MethodInfo *MI)
{
    rewriteExpr(MI, MI->guard);
    for (auto item: MI->storeList) {
        rewriteExpr(MI, item->dest);
        rewriteExpr(MI, item->value);
        rewriteExpr(MI, item->cond);
    }
    for (auto item: MI->letList) {
        rewriteExpr(MI, item->dest);
        rewriteExpr(MI, item->value);
        rewriteExpr(MI, item->cond);
        updateWidth(item->value, convertType(item->type));
    }
    for (auto item: MI->assertList) {
        rewriteExpr(MI, item->value);
        rewriteExpr(MI, item->cond);
    }
    for (auto item: MI->callList) {
        rewriteExpr(MI, item->value);
        rewriteExpr(MI, item->cond);
    }
    for (auto item: MI->printfList) {
        rewriteExpr(MI, item->value);
        rewriteExpr(MI, item->cond);
    }
    for (auto item = MI->letList.begin(), iteme = MI->letList.end(); item != iteme;) {
        std::string dest = tree2str((*item)->dest);
        std::string guard = tree2str(MI->guard);
        if (!(*item)->cond && MI->alloca.find(dest) != MI->alloca.end() && !MI->alloca[dest].noReplace
          && dest == guard && !MI->storeList.size() && !MI->generateFor.size() && !MI->instantiateFor.size()) {
            replaceMethodExpr(MI, (*item)->dest, (*item)->value);
            MI->alloca.erase(dest);
            item = MI->letList.erase(item);
            continue;
        }
        item++;
    }
}

static bool hoistVerilog(ModuleIR *top, ModuleIR *current, std::string prefix)
{
    bool keep = false;
    if (top != current)
    for (auto item: current->fields)
         top->fields.push_back(FieldElement{prefix + item.fldName, item.vecCount,
             item.type, item.isPtr, item.isInput, item.isOutput, item.isInout,
             item.isParameter, item.isShared, item.isLocalInterface, item.isExternal});
    current->fields.clear();
    for (auto item: current->interfaces)
         keep |= hoistVerilog(top, lookupInterface(item.type), prefix+item.fldName);
    keep |= current->methods.size();
    if (!keep)
        current->interfaces.clear();
    return keep;
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
    for (auto IR: irSeq) {
        for (auto items = IR->interfaces.begin(), iteme = IR->interfaces.end(); items != iteme; items++) {
            MapNameValue mapValue;
            extractParam("CLEANUP_" + items->fldName, items->type, mapValue);
            if (mapValue.size() > 0) {
                std::string newName = CBEMangle(items->type);
                copyInterface(items->type, newName, mapValue);
                items->type = newName;
            }
        }
        for (auto MI: IR->methods)
            postParseCleanup(IR, MI);
        for (auto item: IR->generateBody)
            postParseCleanup(IR, item.second);
    }
}
