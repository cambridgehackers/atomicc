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

std::map<std::string, int> genericModule;
std::string genericName(std::string name)
{
    int ind = name.find(MODULE_SEPARATOR "__PARAM__" MODULE_SEPARATOR);
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
    }
    if (isdigit(subscript->value[0])) {
        expr->value += subscript->value + post;
        return;
    }
    int size = -1;
    for (auto item: IR->fields)
        if (item.fldName == fieldName) {
            size = item.vecCount;
            break;
        }
printf("[%s:%d] ARRAAA size %d '%s' post '%s' subscriptval %s\n", __FUNCTION__, __LINE__, size, fieldName.c_str(), post.c_str(), subscript->value.c_str());
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
    if (isIdChar(expr->value[0]) && expr->operands.size() && expr->operands.front()->value == SUBSCRIPT_MARKER) {
        fieldName = expr->value;
        ACCExpr *sub = expr->operands.front()->operands.front();
        expr->operands.pop_front();
        if (expr->operands.size() && isIdChar(expr->operands.front()->value[0])) {
            post = expr->operands.front()->value;
            expr->operands.pop_front();
        }
        if (isdigit(sub->value[0])) {
            expr->value += sub->value + post;
            return nullptr;
        }
        *subscript = sub;
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

static ModuleIR *buildGeneric(ModuleIR *IR, std::string irName, std::string pname)
{
    auto updateType = [&](std::string type) -> std::string {
        if (type == PLACE_NAME)
            return "@" + pname;
        return type;
    };

    ModuleIR *genericIR = allocIR(irName);
    genericIR->metaList = IR->metaList;
    genericIR->softwareName = IR->softwareName;
    genericIR->priority = IR->priority;
    genericIR->params = IR->params;
    genericIR->unionList = IR->unionList;
    genericIR->interfaceConnect = IR->interfaceConnect;
    for (auto item : IR->fields)
        genericIR->fields.push_back(FieldElement{item.fldName,
            item.vecCount, updateType(item.type), item.isPtr,
            item.isInput, item.isOutput, item.isInout,
            item.isParameter, item.isLocalInterface});
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        MethodInfo *newMI = allocMethod(MI->name);
        addMethod(genericIR, newMI);
        newMI->type = updateType(MI->type);
        newMI->guard = MI->guard;
        newMI->rule = MI->rule;
        newMI->storeList = MI->storeList;
        newMI->letList = MI->letList;
        newMI->callList = MI->callList;
        newMI->printfList = MI->printfList;
        newMI->type = updateType(MI->type);
        newMI->alloca = MI->alloca;
        //newMI->meta = MI->meta;
        for (auto item : MI->params)
            newMI->params.push_back(ParamElement{item.name, updateType(item.type)});
    }
    for (auto item : IR->interfaces) {
        std::string iname = "l_ainterface_OC_" + irName + MODULE_SEPARATOR + item.fldName;
        buildGeneric(lookupIR(item.type), iname, pname);
        genericIR->interfaces.push_back(FieldElement{item.fldName,
             item.vecCount, iname, item.isPtr, item.isInput,
             item.isOutput, item.isInout,
             item.isParameter, item.isLocalInterface});
    }
    return genericIR;
}

void preprocessIR(std::list<ModuleIR *> &irSeq)
{
    for (auto IR = irSeq.begin(), IRE = irSeq.end(); IR != IRE;) {
#if 0
typedef struct {
    std::string name;
    std::string type;
} ParamElement;
typedef struct {
    ACCExpr                   *guard;
    std::string                name;
    std::string                type;
    std::list<ParamElement>    params;
} MethodInfo;
#endif
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
        if (!fieldIR || field.vecCount != -1 || field.isPtr || field.isInput
          || field.isOutput || field.isInout || field.isParameter || field.isLocalInterface)
            goto skipLab;
        for (auto FI: (*IR)->method) {
            MethodInfo *MI = FI.second;
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
        IR = irSeq.erase(IR);
        continue;
        }
skipLab:;
        IR++;
    }
    for (auto IR : irSeq) {
        int ind = IR->name.find(MODULE_SEPARATOR "__PARAM__" MODULE_SEPARATOR);
        if (ind > 0)
        if (endswith(IR->name, MODULE_SEPARATOR GENERIC_INT_TEMPLATE_FLAG_STRING)) {
            std::string irName = IR->name.substr(0, ind);
            std::string parg = IR->name.substr(ind + 11);
            std::string pname;
            while (parg != "") {
                int ind = parg.find(MODULE_SEPARATOR);
                if (ind <= 0)
                    break;
                pname = parg.substr(0, ind);
                parg = parg.substr(ind + strlen(MODULE_SEPARATOR GENERIC_INT_TEMPLATE_FLAG_STRING));
            }
            ModuleIR *genericIR = buildGeneric(IR, irName, pname);
            irSeq.push_back(genericIR);
            genericModule[irName] = 1;
            ModuleIR *paramIR = allocIR(irName+MODULE_SEPARATOR+"PARAM");
            genericIR->interfaces.push_back(FieldElement{"", -1, paramIR->name, false, false, false, false, false, false});
            paramIR->fields.push_back(FieldElement{pname, -1, "INTEGER_32", false, false, false, false, true, false});
            //dumpModule("GENERIC", genericIR);
        }
    }
    for (auto IR = irSeq.begin(), IRE = irSeq.end(); IR != IRE;) {
        if (genericName((*IR)->name) != "")
            IR = irSeq.erase(IR);
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

            // now replace __bitconcat, __bitsubstr, __phi
            MI->guard = cleanupExprBit(MI->guard);
            for (auto info: MI->storeList) {
                walkSubst(IR, info->dest);
                walkSubst(IR, info->cond);
                walkSubst(IR, info->value);
                info->dest = cleanupExprBit(info->dest);
                info->cond = cleanupExprBit(info->cond);
                info->value = cleanupExprBit(info->value);
            }
            for (auto info: MI->printfList) {
                walkSubst(IR, info->cond);
                walkSubst(IR, info->value);
                info->cond = cleanupExprBit(info->cond);
                info->value = cleanupExprBit(info->value);
            }
            for (auto info: MI->callList) {
                walkSubst(IR, info->cond);
                walkSubst(IR, info->value);
                info->cond = cleanupExprBit(info->cond);
                info->value = cleanupExprBit(info->value);
            }
            for (auto info: MI->letList) {
                walkSubst(IR, info->dest);
                walkSubst(IR, info->cond);
                walkSubst(IR, info->value);
                info->dest = cleanupExprBit(info->dest);
                info->cond = cleanupExprBit(info->cond);
                info->value = cleanupExprBit(info->value);
            }
        }
    }
}
