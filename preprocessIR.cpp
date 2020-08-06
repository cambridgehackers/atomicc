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

typedef struct {
    std::string name;
    std::string value;
} PARAM_MAP;
int implementPrintf;//=1;
std::map<std::string, int> genericModule;
static void replaceMethodExpr(MethodInfo *MI, ACCExpr *pattern, ACCExpr *replacement, bool replaceLetDest = false);

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
            post = "." + post.substr(1);
    }
    expr->value += "[" + tree2str(subscript) + "]" + post;
}
#if 0
static ACCExpr *findSubscript (ModuleIR *IR, ACCExpr *expr, std::string &size, std::string &fieldName, ACCExpr **subscript, std::string &post)
{
    if (isIdChar(expr->value[0]) && expr->operands.size() && expr->operands.front()->value == SUBSCRIPT_MARKER) {
        fieldName = expr->value;
        ACCExpr *sub = expr->operands.front()->operands.front();
        expr->operands.pop_front();
        if (expr->operands.size() && isIdChar(expr->operands.front()->value[0])) {
            post = expr->operands.front()->value;
            expr->operands.pop_front();
            if (post[0] == '$')
                post = "." + post.substr(1);
        }
        if (isConstExpr(sub)) {
            expr->value += "[" + tree2str(sub) + "]" + post;
            return nullptr;
        }
        *subscript = sub;
        for (auto item: IR->fields)
            if (item.fldName == expr->value) {
                size = item.vecCount;
                return expr;
            }
        for (auto item: IR->interfaces)
            if (item.fldName == expr->value) {
                size = item.vecCount;
                return expr;
            }
        return expr;
    }
    for (auto item: expr->operands)
        if (ACCExpr *ret = findSubscript(IR, item, size, fieldName, subscript, post))
            return ret;
    return nullptr;
}
#endif
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
    for (auto item : MI->params)
        newMI->params.push_back(ParamElement{item.name, updateType(item.type, paramMap), ""});
}

static std::string stripName(std::string iName)
{
    int iind = iName.find("(");
    if (iind > 0)
        iName = iName.substr(0, iind);
    return iName;
}

static ModuleIR *buildGeneric(ModuleIR *IR, std::string irName, std::list<PARAM_MAP> &paramMap, bool isInterface = false)
{
    std::string iName = stripName(IR->interfaceName);
    if (!isInterface)
        buildGeneric(lookupInterface(IR->interfaceName), iName, paramMap, true);
    IR->transformGeneric = true;
    ModuleIR *genericIR = allocIR(irName, isInterface);
    genericIR->interfaceName = iName;
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
        std::string iname = stripName(item.type);
        buildGeneric(lookupInterface(item.type), iname, paramMap, true);
        genericIR->interfaces.push_back(FieldElement{item.fldName,
             updateCount(item.vecCount, paramMap), iname, item.isPtr, item.isInput,
             item.isOutput, item.isInout,
             item.isParameter, item.isShared, item.isLocalInterface, item.isExternal});
    }
    return genericIR;
}

void preprocessMethod(ModuleIR *IR, MethodInfo *MI, bool isGenerate)
{
    std::string methodName = MI->name;
    std::map<std::string, int> localConnect;
#if 0
    static int bodyIndex = 99;
    walkSubscript(IR, MI->guard, isGenerate);
    for (auto item: MI->storeList) {
        walkSubscript(IR, item->dest, isGenerate);
        walkSubscript(IR, item->cond, isGenerate);
        walkSubscript(IR, item->value, isGenerate);
    }
    for (auto item: MI->letList) {
        walkSubscript(IR, item->dest, isGenerate);
        walkSubscript(IR, item->cond, isGenerate);
        walkSubscript(IR, item->value, isGenerate);
    }
    for (auto item: MI->assertList)
        walkSubscript(IR, item->cond, isGenerate);
    for (auto item: MI->callList)
        walkSubscript(IR, item->cond, isGenerate);
    for (auto item: MI->printfList)
        walkSubscript(IR, item->cond, isGenerate);
    for (auto IC : MI->interfaceConnect) {
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
#endif
    for (auto item = IR->interfaces.begin(); item != IR->interfaces.end(); item++)
        if (localConnect[item->fldName])
            item->isLocalInterface = true; // interface declaration that is used to connect to local objects (does not appear in module signature)
    // subscript processing requires that we defactor the entire statement,
    // not just add a condition expression into the tree
//bool moved = false;
#if 0
    auto expandTree = [&] (int sort, ACCExpr **condp, ACCExpr *expandArg, bool isAction = false, ACCExpr *value = nullptr, std::string type = "") -> bool {
        std::string size;
        ACCExpr *cond = *condp, *subscript = nullptr;
        std::string fieldName, post;
        if (ACCExpr *expr = findSubscript(IR, expandArg, size, fieldName, &subscript, post)) {
printf("[%s:%d] sort %d FORINDE %d expandard %s expr %s subscr %s size %s\n", __FUNCTION__, __LINE__, sort, bodyIndex, tree2str(expandArg).c_str(),
tree2str(expr).c_str(), tree2str(subscript).c_str(), size.c_str());
            ACCExpr *var = allocExpr(GENVAR_NAME "1");
            cond = cleanupBool(allocExpr("&", allocExpr("==", var, subscript), cond));
            expr->value = fieldName + "[" + var->value + "]" + post;
            std::string body = "FOR$" + autostr(bodyIndex++) + "Body__ENA";
            MethodInfo *BMI = allocMethod(body);
            BMI->params.push_back(ParamElement{var->value, "Bit(32)", ""});
            addMethod(IR, BMI);
            if (sort == 1)
                BMI->storeList.push_back(new StoreListElement{expandArg, value, cond});
            else if (sort == 2)
                BMI->letList.push_back(new LetListElement{expandArg, value, cond, type});
            else if (sort == 3)
                BMI->callList.push_back(new CallListElement{expandArg, cond, isAction});
            else if (sort == 4)
                BMI->printfList.push_back(new CallListElement{expandArg, cond, isAction});
            MI->generateFor.push_back(GenerateForItem{cond, var->value,
                 allocExpr("0"), allocExpr("<", var, allocExpr(size)),
                 allocExpr("+", var, allocExpr("1")), baseMethodName(body)});
            IR->genvarCount = 1;
//dumpMethod("NEWFOR", BMI);
//moved = true;
            return true;
        }
        return false;
    };
//dumpMethod("BEFORE", MI);
    for (auto item = MI->storeList.begin(), iteme = MI->storeList.end(); item != iteme; ) {
        if (expandTree(1, &(*item)->cond, (*item)->dest, false, (*item)->value))
            item = MI->storeList.erase(item);
        else
            item++;
    }
    for (auto item = MI->letList.begin(), iteme = MI->letList.end(); item != iteme; ) {
        std::string iname = (*item)->type;
        if (startswith(iname, "ARRAY_")) {
            iname = iname.substr(6);
            int ind = iname.find("_");
            if (ind > 0)
                iname = iname.substr(ind+1);
        }
        if (lookupInterface(iname)) {
            bool        isForward = false; //checkItem("/Forward");
            std::string target = tree2str((*item)->dest);
            std::string source = tree2str((*item)->value);
            for (auto iitem: IR->interfaces) {
                if (target == iitem.fldName || source == iitem.fldName)
                    isForward = true;
            }
            IR->interfaceConnect.push_back(InterfaceConnectType{(*item)->dest, (*item)->value, (*item)->type, isForward});
            item = MI->letList.erase(item);
        }
        else if (expandTree(2, &(*item)->cond, (*item)->dest, false, (*item)->value, (*item)->type))
            item = MI->letList.erase(item);
        else
            item++;
    }
    for (auto item = MI->callList.begin(), iteme = MI->callList.end(); item != iteme; ) {
        if (expandTree(3, &(*item)->cond, (*item)->value, (*item)->isAction))
            item = MI->callList.erase(item);
        else
            item++;
    }
    for (auto item = MI->printfList.begin(), iteme = MI->printfList.end(); item != iteme; ) {
        if (expandTree(4, &(*item)->cond, (*item)->value, (*item)->isAction))
            item = MI->printfList.erase(item);
        else
            item++;
    }
//if (moved) dumpMethod("PREVMETH", MI);
#endif

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
        std::string methodName = baseMethodName(MI->name) + MODULE_SEPARATOR;
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
    // Check/replace dummy intermediate classes that are only present for typechecking
    // e.g., Fifo1 -> Fifo1Base
    for (auto IR = irSeq.begin(), IRE = irSeq.end(); IR != IRE;) {
        if ((*IR)->unionList.size()
         || (*IR)->params.size()
         || (*IR)->priority.size()
         || (*IR)->softwareName.size()
         || (*IR)->metaList.size()
         || (*IR)->fields.size() != 1)
            {} // skip
        else {
        FieldElement field = (*IR)->fields.front();
        ModuleIR *fieldIR = lookupIR(field.type);
        if (fieldIR)
        if (ModuleIR *fieldInterface = lookupInterface(fieldIR->interfaceName))
        if ((*IR)->interfaceConnect.size()) {
            if ((*IR)->interfaceConnect.size() != fieldInterface->interfaces.size())
                goto skipLab;
            if ((*IR)->methods.size())
                goto skipLab;
//dumpModule("MASTER", lookupInterface((*IR)->interfaceName));
//dumpModule("FIELD", lookupInterface(fieldIR->interfaceName));
            for (auto item: (*IR)->interfaceConnect) {
                std::string target = tree2str(item.target);
                std::string source = tree2str(item.source);
printf("[%s:%d]CONNNECT target %s source %s type %s forward %d\n", __FUNCTION__, __LINE__, target.c_str(), source.c_str(), item.type.c_str(), item.isForward);
                 if ((target != field.fldName + MODULE_SEPARATOR + source)
                  && (source != field.fldName + MODULE_SEPARATOR + target)) {
                     goto skipLab;
                 }
            }
        }
        if (!fieldIR || field.vecCount != "" || field.isPtr || field.isInput
          || field.isOutput || field.isInout || field.isParameter != "" || field.isLocalInterface)
            goto skipLab;
        for (auto MI: (*IR)->methods) {
            if (isRdyName(MI->name) && !MI->callList.size())
               continue;
            if (MI->rule || MI->storeList.size() || MI->callList.size() != 1
             || MI->alloca.size() || MI->letList.size() || MI->printfList.size())
                goto skipLab;
            CallListElement *call = MI->callList.front();
            if (call->cond || call->value->value != (field.fldName + MODULE_SEPARATOR + MI->name))
                goto skipLab;
        }
printf("[%s:%d]WASOK %s field %s type %s\n", __FUNCTION__, __LINE__, (*IR)->name.c_str(), field.fldName.c_str(), field.type.c_str());
        for (auto mIR : irSeq) {
             for (auto item = mIR->fields.begin(), iteme = mIR->fields.end(); item != iteme; item++)
                 if (item->type == (*IR)->name)
                     item->type = field.type;
        }
// Why?
        IR = irSeq.erase(IR);
        continue;
        }
skipLab:;
        IR++;
    }
    // even convert 'EMODULE' items
    for (auto mapItem : mapIndex//irSeq
) {
        ModuleIR *IR = mapItem.second;
        std::string modName = IR->name;
        int ind = modName.find("(");
        if (ind > 0) {
            std::string irName = modName.substr(0, ind);
            std::string parg = modName.substr(ind + 1);
            if(parg.substr(parg.length()-1) != ")") {
                printf("[%s:%d] Error: argument without ')' modname '%s' irname '%s' parg '%s'\n", __FUNCTION__, __LINE__, modName.c_str(), irName.c_str(), parg.c_str());
                exit(-1);
            }
            parg = parg.substr(0, parg.length() - 1);
            std::string pname;
            std::list<PARAM_MAP> paramMap;
//printf("[%s:%d]START %s\n", __FUNCTION__, __LINE__, irName.c_str());
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
            ModuleIR *genericIR = buildGeneric(IR, irName, paramMap);
            if (!IR->isExt && !IR->isInterface && !IR->isStruct)
                irSeq.push_back(genericIR);
            genericModule[irName] = 1;
            ModuleIR *paramIR = allocIR(irName+MODULE_SEPARATOR+"PARAM", true);
            paramIR->isInterface = true;
            genericIR->parameters.push_back(FieldElement{"", "", paramIR->name, false, false, false, false, ""/*not param*/, false, false, false});
            for (auto item: paramMap)
                paramIR->fields.push_back(FieldElement{item.name, "", "Bit(32)", false, false, false, false, item.value, false, false, false});
            //dumpModule("GENERIC", genericIR);
        }
    }
    for (auto IR = irSeq.begin(), IRE = irSeq.end(); IR != IRE;) {
        if ((*IR)->transformGeneric) {
            //printf("[%s:%d] delete generic %s\n", __FUNCTION__, __LINE__, (*IR)->name.c_str());
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

static ACCExpr *walkReplaceExpr (ACCExpr *expr, ACCExpr *pattern, ACCExpr *replacement)
{
    if (!expr)
        return expr;
    if (matchExpr(expr, pattern))
        return replacement;
    ACCExpr *newExpr = allocExpr(expr->value);
    std::string item = expr->value;
    for (auto item: expr->operands)
        if (ACCExpr *operand = walkReplaceExpr(item, pattern, replacement))
            newExpr->operands.push_back(operand);
    return newExpr;
}
static void replaceMethodExpr(MethodInfo *MI, ACCExpr *pattern, ACCExpr *replacement, bool replaceLetDest)
{
    MI->guard = walkReplaceExpr(MI->guard, pattern, replacement);
    for (auto item: MI->storeList) {
        item->dest = walkReplaceExpr(item->dest, pattern, replacement);
        item->value = walkReplaceExpr(item->value, pattern, replacement);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement);
    }
    for (auto item: MI->letList) {
        if (replaceLetDest)
            item->dest = walkReplaceExpr(item->dest, pattern, replacement);
        item->value = walkReplaceExpr(item->value, pattern, replacement);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement);
    }
    for (auto item: MI->assertList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement);
    }
    for (auto item: MI->callList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement);
    }
    for (auto item: MI->printfList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement);
    }
}
void copyInterface(std::string oldName, std::string newName, MapNameValue &mapValue)
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
    for (auto item: MI->alloca) {
        if (!item.second.noReplace)
            continue;
        ACCExpr *itemList = allocExpr("{"); // }
        std::list<FieldItem> fieldList;
        getFieldList(fieldList, item.first, "", item.second.type, true, true);
        for (auto fitem : fieldList)
            if (!fitem.alias)
                itemList->operands.push_front(allocExpr(fitem.name));
        if (itemList->operands.size() > 1)
            replaceMethodExpr(MI, allocExpr(item.first), itemList);
    }
}

void cleanupIR(std::list<ModuleIR *> &irSeq)
{
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
