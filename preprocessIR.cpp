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

void preprocessIR(std::list<ModuleIR *> &irSeq)
{
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
    }
}
