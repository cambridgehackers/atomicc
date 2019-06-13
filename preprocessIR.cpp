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
std::map<std::string, int> genericModule;

std::string genericName(std::string name)
{
    int ind = name.find(PARAM_MARKER);
    if (ind > 0) {
        name = name.substr(0, ind);
        //if (genericModule[name])   // actual body could be declared externally
            return name;
    }
    return "";
}
std::string genericModuleParam(std::string name)
{
    int ind = name.rfind(MODULE_SEPARATOR);
    if (ind > 0)
        return name.substr(ind+1);
    return "";
}
static void walkSubst (ModuleIR *IR, ACCExpr *expr)
{
    if (!expr)
        return;
    if (isIdChar(expr->value[0]))
         expr->value = fixupQualPin(IR, expr->value);  // fixup interface names
    for (auto item: expr->operands)
        walkSubst(IR, item);
}

static void walkSubscript (ModuleIR *IR, ACCExpr *expr)
{
    if (!expr)
        return;
    for (auto item: expr->operands)
        walkSubscript(IR, item);
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
    if (isdigit(subscript->value[0]) || startswith(subscript->value, GENVAR_NAME)) {
        expr->value += "[" + subscript->value + "]" + post;
        return;
    }
}
static ACCExpr *findSubscript (ModuleIR *IR, ACCExpr *expr, int &size, std::string &fieldName, ACCExpr **subscript, std::string &post)
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
        if (isdigit(sub->value[0]) || startswith(sub->value, GENVAR_NAME)) {
            expr->value += "[" + sub->value + "]" + post;
            return nullptr;
        }
        *subscript = sub;
        for (auto item: IR->fields)
            if (item.fldName == expr->value) {
                size = atoi(item.vecCount.c_str());
                break;
            }
        return expr;
    }
    for (auto item: expr->operands)
        if (ACCExpr *ret = findSubscript(IR, item, size, fieldName, subscript, post))
            return ret;
    return nullptr;
}

static std::string updateCount(std::string count, std::list<PARAM_MAP> &paramMap)
{
    for (auto item: paramMap)
         if (count == item.value) {
             count = item.name;
             break;
         }
    return count;
}

static std::string updateType(std::string type, std::list<PARAM_MAP> &paramMap)
{
    for (auto item: paramMap)
        if (type == "Bit(" + item.value + ")")
            return "@" + item.name;
    return type;
};

static ACCExpr *walkToGeneric (ACCExpr *expr, std::list<PARAM_MAP> &paramMap)
{
    if (!expr)
        return expr;
    ACCExpr *ret = allocExpr(updateCount(expr->value, paramMap));
    for (auto item: expr->operands)
        ret->operands.push_back(walkToGeneric(item, paramMap));
    return ret;
}

static void copyGenericMethod(ModuleIR *genericIR, MethodInfo *MI, std::list<PARAM_MAP> &paramMap)
{
    MethodInfo *newMI = allocMethod(MI->name);
    addMethod(genericIR, newMI);
    newMI->type = updateType(MI->type, paramMap);
    newMI->guard = MI->guard;
    newMI->rule = MI->rule;
    newMI->action = MI->action;
    newMI->storeList = MI->storeList;
    newMI->letList = MI->letList;
    newMI->callList = MI->callList;
    newMI->printfList = MI->printfList;
    newMI->type = updateType(MI->type, paramMap);
    newMI->alloca = MI->alloca;
    for (auto item : MI->generateFor) {
        newMI->generateFor.push_back(GenerateForItem{
walkToGeneric(item.cond, paramMap), item.var, walkToGeneric(item.init, paramMap), walkToGeneric(item.limit, paramMap), walkToGeneric(item.incr, paramMap), item.body});
    }
    //newMI->meta = MI->meta;
    for (auto item : MI->params)
        newMI->params.push_back(ParamElement{item.name, updateType(item.type, paramMap), ""});
}

static ModuleIR *buildGeneric(ModuleIR *IR, std::string irName, std::list<PARAM_MAP> &paramMap, bool isInterface = false)
{
    IR->transformGeneric = true;
    ModuleIR *genericIR = allocIR(irName, isInterface);
    genericIR->genvarCount = IR->genvarCount;
    genericIR->metaList = IR->metaList;
    genericIR->softwareName = IR->softwareName;
    genericIR->priority = IR->priority;
    genericIR->params = IR->params;
    genericIR->unionList = IR->unionList;
    genericIR->interfaceConnect = IR->interfaceConnect;
    genericIR->isInterface = isInterface;
    for (auto item : IR->fields)
        genericIR->fields.push_back(FieldElement{item.fldName,
            updateCount(item.vecCount, paramMap), updateType(item.type, paramMap),
            item.isPtr, item.isInput, item.isOutput, item.isInout,
            item.isParameter, item.isShared, item.isLocalInterface});
    for (auto FI : IR->generateBody)
        copyGenericMethod(genericIR, FI.second, paramMap);
    for (auto MI : IR->methods)
        copyGenericMethod(genericIR, MI, paramMap);
    for (auto item : IR->interfaces) {
        std::string iname = "l_ainterface_OC_" + irName + MODULE_SEPARATOR + item.fldName;
        buildGeneric(lookupInterface(item.type), iname, paramMap, true);
        genericIR->interfaces.push_back(FieldElement{item.fldName,
             item.vecCount, iname, item.isPtr, item.isInput,
             item.isOutput, item.isInout,
             item.isParameter, item.isShared, item.isLocalInterface});
    }
    return genericIR;
}

void preprocessMethod(ModuleIR *IR, MethodInfo *MI)
{
    static int bodyIndex = 99;
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
    // subscript processing requires that we defactor the entire statement,
    // not just add a condition expression into the tree
//bool moved = false;
    auto expandTree = [&] (int sort, ACCExpr **condp, ACCExpr *expandArg, bool isAction = false, ACCExpr *value = nullptr, std::string type = "") -> bool {
        int size = -1;
        ACCExpr *cond = *condp, *subscript = nullptr;
        std::string fieldName, post;
        if (ACCExpr *expr = findSubscript(IR, expandArg, size, fieldName, &subscript, post)) {
printf("[%s:%d] sort %d FORINDE %d expandard %s expr %s subscr %s\n", __FUNCTION__, __LINE__, sort, bodyIndex, tree2str(expandArg).c_str(),
tree2str(expr).c_str(), tree2str(subscript).c_str());
            ACCExpr *var = allocExpr(GENVAR_NAME "99");
            cond = cleanupBool(allocExpr("&", allocExpr("==", var, subscript), cond));
            cond = cleanupBool(allocExpr("&", allocExpr(methodName), allocExpr(getRdyName(methodName)), cond));
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
                 allocExpr("0"), allocExpr("<", var, allocExpr(autostr(size))),
                 allocExpr("+", var, allocExpr("1")), body.substr(0, body.length() - 5)});
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
        if (expandTree(2, &(*item)->cond, (*item)->dest, false, (*item)->value, (*item)->type))
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

    // now replace __bitconcat, __bitsubstr, __phi
    MI->guard = cleanupBool(MI->guard);
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
}

void preprocessIR(std::list<ModuleIR *> &irSeq)
{
    for (auto IR = irSeq.begin(), IRE = irSeq.end(); IR != IRE;) {
        FieldElement field;
        if ((*IR)->interfaceConnect.size()
         || (*IR)->unionList.size()
         || (*IR)->params.size()
         || (*IR)->priority.size()
         || (*IR)->softwareName.size()
         || (*IR)->metaList.size()
         || (*IR)->fields.size() != 1)
            goto skipLab;
        {
        field = (*IR)->fields.front();
        ModuleIR *fieldIR = lookupIR(field.type);
        if (!fieldIR || field.vecCount != "" || field.isPtr || field.isInput
          || field.isOutput || field.isInout || field.isParameter != "" || field.isLocalInterface)
            goto skipLab;
        for (auto MI: (*IR)->methods) {
            if (endswith(MI->name, "__RDY") && !MI->callList.size())
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
    for (auto IR : irSeq) {
        std::string modName = IR->name;
        int ind = modName.find(PARAM_MARKER);
        if (ind > 0) {
            std::string irName = modName.substr(0, ind);
            std::string parg = modName.substr(ind + strlen(PARAM_MARKER));
            std::string pname;
            std::list<PARAM_MAP> paramMap;
            while (parg != "") {
                int indVal = parg.find(MODULE_SEPARATOR);
                if (indVal <= 0)
                    break;
                pname = parg.substr(0, indVal);
                std::string pvalue = parg.substr(indVal+1);
                parg = "";
                int indNext = pvalue.find(PARAM_MARKER);
                if (indNext > 0) {
                    parg = pvalue.substr(indNext + strlen(PARAM_MARKER));
                    pvalue = pvalue.substr(0, indNext);
                }
                paramMap.push_back(PARAM_MAP{pname, pvalue});
            }
            ModuleIR *genericIR = buildGeneric(IR, irName, paramMap);
            irSeq.push_back(genericIR);
            genericModule[irName] = 1;
            ModuleIR *paramIR = allocIR(irName+MODULE_SEPARATOR+"PARAM", true);
            paramIR->isInterface = true;
            genericIR->interfaces.push_back(FieldElement{"", "", paramIR->name, false, false, false, false, ""/*not param*/, false, false});
            for (auto item: paramMap)
                paramIR->fields.push_back(FieldElement{item.name, "", "Bit(32)", false, false, false, false, item.value, false, false});
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
    for (auto IR : irSeq) {
        std::map<std::string, int> localConnect;
        for (auto IC : IR->interfaceConnect)
            if (!IC.isForward) {
                localConnect[IC.target] = 1;
                localConnect[IC.source] = 1;
            }
        for (auto item = IR->interfaces.begin(); item != IR->interfaces.end(); item++)
            if (localConnect[item->fldName])
                item->isLocalInterface = true; // interface declaration that is used to connect to local objects (does not appear in module signature)
        // expand all subscript calculations before processing the module
        for (auto item: IR->generateBody)
            preprocessMethod(IR, item.second);
        for (auto MI: IR->methods)
            preprocessMethod(IR, MI);
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
static void replaceMethodExpr(MethodInfo *MI, ACCExpr *pattern, ACCExpr *replacement, bool replaceLetDest = false)
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
    for (auto item: MI->callList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement);
    }
    for (auto item: MI->printfList) {
        item->value = walkReplaceExpr(item->value, pattern, replacement);
        item->cond = walkReplaceExpr(item->cond, pattern, replacement);
    }
}
static void postParseCleanup(MethodInfo *MI)
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
        if (!(*item)->cond && MI->alloca.find(dest) != MI->alloca.end() && !MI->alloca[dest].noReplace) {
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
        ACCExpr *itemList = allocExpr("{");
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
        for (auto MI: IR->methods)
            postParseCleanup(MI);
        for (auto item: IR->generateBody)
            postParseCleanup(item.second);
    }
}
