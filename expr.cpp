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
#define MAX_EXPR_DEPTH 20

static int trace_expr;//=1;
static int cleanupTraceLevel;//=1; //-1;
#define TRACE_CLEANUP_EXPR (cleanupTraceLevel && (cleanupTraceLevel == -1 || (level <= cleanupTraceLevel)))
static std::string lexString;
static unsigned lexIndex;
static char lexChar;
static bool lexAllowRange;
static ACCExpr *repeatGet1Token;
std::map<std::string, int> replaceBlock;
int flagErrorsCleanup;

bool isIdChar(char ch)
{
    return isalpha(ch) || ch == '_' || ch == '$';
}

bool isParen(std::string ch)
{
    return ch == "[" || ch == "(" || ch == "{" || ch == SUBSCRIPT_MARKER || ch == PARAMETER_MARKER;
}
static bool isParen(char ch)
{
    static char item[2];
    item[0] = ch;
    return isParen(item);
}

std::string treePost(const ACCExpr *arg)
{
    std::string space = " ";
    if (arg->value == "[")
        return " ]";
    else if (arg->value == "(")
        return " )";
    else if (arg->value == "{")
        return " }";
    else if (arg->value == SUBSCRIPT_MARKER)
        return space + SUBSCRIPT_CLOSE;
    else if (arg->value == PARAMETER_MARKER)
        return space + PARAMETER_CLOSE;
    return "";
}

static bool booleanBinop(std::string s)
{
    return s == "^" || s == "&" || s == "|" || s == "&&" || s == "||";
}
static bool shiftOp(std::string s)
{
    return s == "<<" || s == ">>";
}
static bool arithOp(std::string s)
{
    return s == "+" || s == "-" || s == "*" || s == "%" || s == "?" || s == ":";
}

static bool relationalOp(std::string s)
{
    return s == "==" || s == "!=" || s == "<" || s == ">";
}

static bool checkOperator(std::string s)
{
    return booleanBinop(s) || shiftOp(s) ||  arithOp(s) || relationalOp(s) || s == "," || s == "!";
}

bool checkOperand(std::string s)
{
    return isIdChar(s[0]) || isdigit(s[0]) || s == "(" || s == "{" || s[0] == '"';
}

void dumpExpr(std::string tag, ACCExpr *next)
{
    printf("DE: %s %p %s\n", tag.c_str(), (void *)next, next ? next->value.c_str() : "");
    int i = 0;
    if (next)
    for (auto item: next->operands) {
        dumpExpr(tag + "_" + autostr(i), item);
        i++;
    }
}

ACCExpr *getRHS(ACCExpr *expr, int match)
{
     int i = 0;
     if (match == -1)
         match = expr->operands.size() - 1;
     for (auto item: expr->operands)
         if (i++ == match)
             return item;
     return nullptr;
}

std::string tree2str(ACCExpr *expr)
{
    if (!expr)
        return "";
    std::string ret, sep, op = expr->value;
    if (isParen(op)) {
        ret += op;
        if (expr->operands.size())
            ret += " ";
        op = ",";
    }
    else if (isIdChar(op[0])) {
        ret += op;
    }
    else if (!expr->operands.size() || ((op == "-" || op == "!")/*unary*/ && expr->operands.size() == 1))
        ret += op;
    bool topOp = checkOperand(expr->value) || expr->value == "," || expr->value == "[" || expr->value == PARAMETER_MARKER;
    for (auto item: expr->operands) {
        ret += sep;
        bool addParen = !topOp && !checkOperand(item->value) && item->value != ",";
        if (addParen)
            ret += "( ";
        ret += tree2str(item);
        if (addParen)
            ret += " )";
        sep = " " + op + " ";
        if (op == "?")
            op = ":";
    }
    ret += treePost(expr);
    return ret;
}

void decRef(std::string name)
{
//return;
    if (refList[name].count > 0)
        refList[name].count--;
}

int walkCount (ACCExpr *expr)
{
    if (!expr)
        return 0;
    int count = 1;  // include this node in count
    for (auto item: expr->operands)
        count += walkCount(item);
    return count;
}

void walkRef (ACCExpr *expr)
{
    if (!expr)
        return;
    std::string item = expr->value;
    if (isIdChar(item[0])) {
        std::string base = item;
        int ind = base.find("[");
        if (ind > 0)
            base = base.substr(0, ind);
        if (!refList[item].pin)
            printf("[%s:%d] refList[%s] definition missing\n", __FUNCTION__, __LINE__, item.c_str());
        if (base != item)
{
if (trace_expr)
printf("[%s:%d] RRRRREFFFF %s -> %s\n", __FUNCTION__, __LINE__, expr->value.c_str(), item.c_str());
item = base;
}
        assert(refList[item].pin);
        refList[item].count++;
        ACCExpr *temp = assignList[item].value;
        if (temp && refList[item].count == 1)
            walkRef(temp);
    }
    for (auto item: expr->operands)
        walkRef(item);
}

ACCExpr *allocExpr(std::string value, ACCExpr *argl, ACCExpr *argr, ACCExpr *argt)
{
    ACCExpr *ret = new ACCExpr;
    ret->value = value;
    ret->operands.clear();
    if (argl)
        ret->operands.push_back(argl);
    if (argr)
        ret->operands.push_back(argr);
    if (argt)
        ret->operands.push_back(argt);
    return ret;
}

static int findPrec(std::string s)
{
static struct {
    const char *op;
    int         prec;
} opPrec[] = {
    {"," , 1},

    {"?", 10}, {":", 10},

    {"&", 12}, {"|", 12},
    {"&&", 17}, {"||", 17},
    {"^", 18},

    {"==", 20}, {"!=" , 20}, {"<", 20}, {">", 20}, {"<=", 20}, {">=", 20},

    {">>", 25}, {"<<", 25},

    {"+", 30}, {"-", 30},
    {"*", 40}, {"%", 40},

    {nullptr, -1}};
    int ind = 0;
    while (opPrec[ind].op && opPrec[ind].op != s)
        ind++;
    if (s != "" && !opPrec[ind].op) {
        printf("[%s:%d] PPPPPPPPPPPP %s\n", __FUNCTION__, __LINE__, s.c_str());
        exit(-1);
    }
    return opPrec[ind].prec;
}

static ACCExpr *getExprList(ACCExpr *head, std::string terminator, bool repeatCurrentToken, bool preserveParen);
static ACCExpr *get1Token(void)
{
    std::string lexToken;
    auto getNext = [&] (void) -> void {
        lexToken += lexChar;
        lexChar = lexString[lexIndex++];
    };

    ACCExpr *ret = repeatGet1Token;
    repeatGet1Token = nullptr;
    if (ret)
        return ret;
    while (lexChar == ' ' || lexChar == '\t')
        lexChar = lexString[lexIndex++];
    if(lexIndex > lexString.length() || lexChar == 0)
        return nullptr;
    if (isIdChar(lexChar)) {
        do {
            getNext();
        } while (isIdChar(lexChar) || isdigit(lexChar));
        if (lexAllowRange && lexChar == '[') {
            do {
                getNext();
            } while (lexChar != ']');
            getNext();
        }
        if (lexToken == "__defaultClock")
            lexToken = "CLK";
        else if (lexToken == "__defaultnReset")
            lexToken = "nRST";
    }
    else if (isdigit(lexChar))
        do {
            getNext();
        } while (isdigit(lexChar) || lexChar == '.' || lexChar == '\'' || lexChar == 'b'
            || lexChar == 'h' || lexChar == 'd' || lexChar == 'o');
    else if (lexChar == '+' || lexChar == '-' || lexChar == '*' || lexChar == '&' || lexChar == '|')
        do {
            getNext();
        } while (lexChar == lexToken[0]);
    else if (lexChar == '=' || lexChar == '<' || lexChar == '>' || lexChar == '!')
        do {
            getNext();
        } while (lexChar == '=' || lexChar == '<' || lexChar == '>');
    else if (isParen(lexChar) || lexChar == '/' || lexChar == '%'
        || lexChar == ']' || lexChar == '}' || lexChar == ')' || lexChar == '^'
        || lexChar == ',' || lexChar == '?' || lexChar == ':' || lexChar == ';')
        getNext();
    else if (lexChar == '@') { // special 'escape' character for internal SUBSCRIPT_MARKER/PARAMETER_MARKER sequences
        getNext();
        getNext();
    }
    else if (lexChar == '"') {
        do {
            if (lexChar == '\\')
                getNext();
            getNext();
        } while (lexChar != '"');
        getNext();
    }
    else {
        printf("[%s:%d] lexString '%s' unknown lexChar %c %x\n", __FUNCTION__, __LINE__, lexString.c_str(), lexChar, lexChar);
        exit(-1);
    }
    ret = allocExpr(lexToken);
    if (isParen(ret->value)) {
        std::string val = ret->value;
        if (trace_expr)
            printf("[%s:%d] before subparse of '%s'\n", __FUNCTION__, __LINE__, ret->value.c_str());
        ret = getExprList(ret, treePost(ret).substr(1), false, true);
        if (ret->value != val)
            ret = allocExpr(val, ret); // over optimization of '(<singleItem>)'
        if (trace_expr) {
            printf("[%s:%d] after subparse of '%s'\n", __FUNCTION__, __LINE__, ret->value.c_str());
            dumpExpr("SUBPAREN", ret);
        }
    }
    return ret;
}

static bool checkInteger(ACCExpr *expr, std::string pattern)
{
    if (!expr)
        return false;
    std::string val = expr->value;
    if (!isdigit(val[0]))
        return false;
    int ind = val.find("'");
    if (ind > 0)    // strip off radix
        val = val.substr(ind+2);
    return val == pattern;
}

ACCExpr *invertExpr(ACCExpr *expr)
{
    if (!expr)
        return allocExpr("1");
    ACCExpr *lhs = expr->operands.front();
    std::string v = expr->value;
    if (checkInteger(expr, "1"))
        return allocExpr("0");
    if (checkInteger(expr, "0"))
        return allocExpr("1");
    if (v == "!")
        return lhs;
    if (v == "^" && checkInteger(getRHS(expr), "1"))
        return lhs;
    if (v == "==") {
        if (expr->operands.size() == 1)
            return allocExpr("0");
        return allocExpr("!=", lhs, getRHS(expr));
    }
    if (v == "!=") {
        if (expr->operands.size() == 1)
            return allocExpr("1");
        return allocExpr("==", lhs, getRHS(expr));
    }
    if (v == "&" || v == "|") {
        ACCExpr *temp = allocExpr(v == "&" ? "|" : "&");
        for (auto item: expr->operands)
            temp->operands.push_back(invertExpr(item));
        return temp;
    }
    return allocExpr("^", expr, allocExpr("1"));
}

int exprWidth(ACCExpr *expr)
{
    if (relationalOp(expr->value))
        return 1;
    int ind = expr->value.find("'");
    if (isdigit(expr->value[0]) && ind > 0) {
        std::string temp = expr->value.substr(0, ind - 1);
        return atoi(temp.c_str());
    }
    if (isIdChar(expr->value[0])) {
        ACCExpr *lhs = getRHS(expr, 0);
        if (lhs && lhs->value == "[" && lhs->operands.size() > 0) {
            ACCExpr *first = getRHS(lhs, 0);
            if (first->value == ":") {
                ACCExpr *second = getRHS(first);
                first = getRHS(first, 0);
                if (!second)
                    return 1;
                if (isdigit(first->value[0]) && isdigit(second->value[0]))
                    return atoi(first->value.c_str()) - atoi(second->value.c_str()) + 1;
            }
            else if (isdigit(first->value[0]))
                return 1;
        }
        return convertType(refList[expr->value].type);
    }
    if (expr->value == "?") {
        if (int len = exprWidth(getRHS(expr, 1)))
            return len;
        if (int len = exprWidth(getRHS(expr, 2)))
            return len;
    }
    return 0;
}

void updateWidth(ACCExpr *expr, int len)
{
    int ilen = exprWidth(expr);
    if (isdigit(expr->value[0]) && len > 0 && expr->value.find("'") == std::string::npos)
        expr->value = autostr(len) + "'d" + expr->value;
    else if (isIdChar(expr->value[0])) {
        if (ilen > len) {
            ACCExpr *subexpr = allocExpr(":", allocExpr(autostr(len-1)));
            if (len > 1)
                subexpr->operands.push_back(allocExpr("0"));
            expr->operands.push_back(allocExpr("[", subexpr));
        }
    }
    else if (expr->value == "?") {
        updateWidth(getRHS(expr, 0), len);
        updateWidth(getRHS(expr), len);
    }
    else if (arithOp(expr->value))
        for (auto item: expr->operands)
            updateWidth(item, len);
}

bool matchExpr(ACCExpr *lhs, ACCExpr *rhs)
{
    if (!lhs && !rhs)
        return true;
    if (!lhs || !rhs || lhs->value != rhs->value || lhs->operands.size() != rhs->operands.size())
        return false;
    if ((lhs->value == "&" || lhs->value == "|") && lhs->operands.size() == 2) // check commutativity
        if (matchExpr(getRHS(lhs), getRHS(rhs, 0)) && matchExpr(getRHS(lhs, 0), getRHS(rhs)))
            return true;
    for (auto lcur = lhs->operands.begin(), lend = lhs->operands.end(), rcur = rhs->operands.begin(); lcur != lend; lcur++, rcur++)
        if (!matchExpr(*lcur, *rcur))
            return false;
    return true;
}

static void replaceValue(ACCExpr *expr, std::string value)
{
    expr->value = value;
    expr->operands.clear();
}

ACCExpr *dupExpr(ACCExpr *expr)
{
    auto ext = allocExpr(expr->value);
    for (auto item: expr->operands)
        ext->operands.push_back(dupExpr(item));
    return ext;
}

void walkReplaceBuiltin(ACCExpr *expr)
{
    while(1) {
    if (expr->value == "__bitconcat") {
        ACCExpr *list = expr->operands.front();
        if (list->value == PARAMETER_MARKER)
            list->value = "{";
        expr->value = list->value;
        expr->operands = list->operands;
    }
    else if (expr->value == "__bitsubstr") {
        ACCExpr *list = expr->operands.front();
        ACCExpr *bitem = list->operands.front();
        if (!isIdChar(bitem->value[0])) {  // can only do bit select on net or reg (not expressions)
            printf("[%s:%d] can only do __bitsubstr on elementary items\n", __FUNCTION__, __LINE__);
            dumpExpr("BITSUB", expr);
            exit(-1);
        }
        bitem->operands.push_back(allocExpr("[", allocExpr(":", getRHS(list), getRHS(list, 2))));
        expr->value = bitem->value;
        expr->operands = bitem->operands;
    }
    else if (expr->value == "__phi") {
        ACCExpr *list = expr->operands.front();
        int size = list->operands.size();
        ACCExpr *lhs = getRHS(list, 0), *rhs = getRHS(list);
        ACCExpr *newe = nullptr;
        if (size == 2 && matchExpr(getRHS(lhs, 0), invertExpr(getRHS(rhs, 0))))
            newe = allocExpr("?", getRHS(lhs, 0), getRHS(lhs), getRHS(rhs));
        else if (size == 2 && getRHS(lhs, 0)->value == "__default" && exprWidth(getRHS(rhs)) == 1)
            newe = allocExpr("&", getRHS(rhs, 0), getRHS(rhs));
        else {
            //dumpExpr("PHI", list);
            newe = allocExpr("|");
            for (auto item: list->operands) {
                item->value = "?"; // Change from ':' -> '?'
                item->operands.push_back(allocExpr("0"));
                newe->operands.push_back(item);
            }
        }
        expr->value = newe->value;
        expr->operands = newe->operands;
    }
    else
        break;
    }
    for (auto item: expr->operands)
        walkReplaceBuiltin(item);
}

bool factorExpr(ACCExpr *expr)
{
    bool changed = false;
    if (!expr)
        return false;
static int level = 999;
    level++;
if (level == 1)
dumpExpr("FACT" + autostr(level), expr);
    while(1) {
        bool found = false;
    if (expr->value == "|") {
        for (auto itemo = expr->operands.begin(), iend = expr->operands.end(); itemo != iend;) {
            if (checkInteger(*itemo, "0")) {
                 itemo = expr->operands.erase(itemo);
                 changed = true;
                 continue;
            }
            ACCExpr *invertItem = invertExpr(*itemo);
            for (auto jtemo = expr->operands.begin(); jtemo != itemo; jtemo++)
                 if (matchExpr(*jtemo, invertItem)) {
                     replaceValue(expr, "1");
                     changed = true;
                     goto skiplabel1;
                 }
            itemo++;
        }
    }
skiplabel1:;
    if (expr->value == "&") {
        for (auto itemo = expr->operands.begin(), iend = expr->operands.end(); itemo != iend;) {
            if (checkInteger(*itemo, "1")) {
                 itemo = expr->operands.erase(itemo);
                 changed = true;
                 continue;
            }
            ACCExpr *invertItem = invertExpr(*itemo);
            for (auto jtemo = expr->operands.begin(); jtemo != itemo; jtemo++)
                 if (matchExpr(*jtemo, invertItem)) {
                     replaceValue(expr, "0");
                     changed = true;
                     goto skiplabel2;
                 }
            itemo++;
        }
    }
skiplabel2:;
    if (expr->value == "|") {
        ACCExpr *matchItem = nullptr;
        for (auto itemo : expr->operands) {
            if (itemo->value == "&" && itemo->operands.size()) {
                ACCExpr *thisLast = getRHS(itemo, -1); // get last item
                if (matchExpr(matchItem, thisLast))
                    found = true;
                else if (!found)
                    matchItem = thisLast;
            }
        }
        if (found) {
            ACCExpr *factorp = allocExpr("|");
            for (auto itemo = expr->operands.begin(), iend = expr->operands.end(); itemo != iend;) {
                ACCExpr *thisLast = getRHS(*itemo, -1); // get last item
                if ((*itemo)->value == "&" && matchExpr(matchItem, thisLast)) {
                    ACCExpr *andp = allocExpr("&");
                    for (auto itema : (*itemo)->operands)
                        if (!matchExpr(matchItem, itema))
                            andp->operands.push_back(itema);
                    if (andp->operands.size())
                        factorp->operands.push_back(andp);
                    itemo = expr->operands.erase(itemo);
                    changed = true;
                }
                else
                    itemo++;
            }
            changed |= factorExpr(factorp);
            expr->operands.push_back(allocExpr("&", factorp, matchItem));
            continue;
        }
    }
    ACCExpr *lhs = getRHS(expr, 0);
    if (expr->value == "|" && expr->operands.size() >= 2 && lhs->value == "&" && lhs->operands.size()) {
        auto itemo = expr->operands.begin(), iend = expr->operands.end();
        ACCExpr *andp = *itemo++, *used = allocExpr("");
        for (; itemo != iend; itemo++) {
            ACCExpr *itemoInvert = invertExpr(*itemo);
            auto itema = andp->operands.begin(), aend = andp->operands.end();
            for (; itema != aend;) {
                if (matchExpr(*itema, itemoInvert)) {
                    used->operands.push_back(*itema);
                    itema = andp->operands.erase(itema);
                    found |= true;
                }
                else
                    itema++;
            }
        }
    }
    if (expr->value == "&")
        for (auto item = expr->operands.begin(), iend = expr->operands.end(); item != iend;) {
            for (auto jitem = expr->operands.begin(); jitem != item; jitem++)
                if (matchExpr(*item, *jitem)) {
                    item = expr->operands.erase(item);
                    changed = true;
                    goto skipitem1;
                }
            if (checkInteger(*item, "0")) {
                replaceValue(expr, "0");
                goto nextlab;
            }
            item++;
skipitem1:;
        }
    if (expr->value == "|")
        for (auto item = expr->operands.begin(), iend = expr->operands.end(); item != iend;) {
            for (auto jitem = expr->operands.begin(); jitem != item; jitem++)
                if (matchExpr(*item, *jitem)) {
                    item = expr->operands.erase(item);
                    changed = true;
                    goto skipitem2;
                }
            if (checkInteger(*item, "1")) {
                replaceValue(expr, "1");
                goto nextlab;
            }
            item++;
skipitem2:;
        }
    for (auto item: expr->operands)
        found |= factorExpr(item);
    if (found)
        goto nextlab;
    if (expr->operands.size() == 1 && booleanBinop(expr->value)) {
        ACCExpr *lhs = getRHS(expr, 0);
        expr->value = lhs->value;
        expr->operands = lhs->operands;
    }
    else if (expr->value == "&" && !expr->operands.size())
        replaceValue(expr, "1");
    else if (expr->value == "|" && !expr->operands.size())
        replaceValue(expr, "0");
    else
        break;  // all done, no more updates
nextlab:;
    changed = true;
    }
if (level == 1 && changed && expr->operands.size())
dumpExpr("FOVER" + autostr(level), expr);
    level--;
    return changed;
}

ACCExpr *cleanupExpr(ACCExpr *expr, bool preserveParen)
{
    if (!expr)
        return expr;
static int level;
    level++;
    if (TRACE_CLEANUP_EXPR)
        dumpExpr("cleanupExprSTART" + autostr(level), expr);
    if (expr->operands.size() == 1 && expr->operands.front()->value != "," && (!preserveParen && expr->value == "("))
        expr = expr->operands.front();
    if (isParen(expr->value) && expr->operands.size() == 1 && expr->operands.front()->value == ",")
        expr->operands = expr->operands.front()->operands;
    ACCExpr *lhs = expr->operands.front();
    if (expr->value == "^" && checkInteger(getRHS(expr), "1"))
        expr = invertExpr(cleanupExpr(lhs));
    if (expr->value == "&" && expr->operands.size() == 2 && matchExpr(getRHS(expr, 0), invertExpr(getRHS(expr))))
        expr = allocExpr("0");
    ACCExpr *ret = allocExpr(expr->value);
    for (auto item: expr->operands) {
         ACCExpr *titem = cleanupExpr(item, isIdChar(expr->value[0]));
         if (TRACE_CLEANUP_EXPR)
             printf("[%s:%d] item %p titem %p ret %p\n", __FUNCTION__, __LINE__, (void *)item, (void *)titem, (void *)ret);
         if (titem->value != ret->value || ret->value == "?" || isParen(ret->value)
            || (  titem->value != "&" && titem->value != "|"
               && titem->value != "&&" && titem->value != "||"
               && titem->value != "+" && titem->value != "*"))
             ret->operands.push_back(titem);
         else {
             if (TRACE_CLEANUP_EXPR)
                 printf("[%s:%d] combine tree expressions: op %s uppersize %d lowersize %d\n", __FUNCTION__, __LINE__, titem->value.c_str(), (int)ret->operands.size(), (int)titem->operands.size());
             for (auto oitem: titem->operands)
                 ret->operands.push_back(oitem);
         }
    }
    factorExpr(ret);
    if (ret->value == "?" && checkInteger(ret->operands.front(), "1"))
        ret = getRHS(ret);
    if (ret->value == "?" && checkInteger(getRHS(ret, 2), "0")) {
        ACCExpr *rhs = getRHS(ret,1);
        int len = exprWidth(rhs);
        if (checkInteger(rhs, "1"))
            ret = ret->operands.front();
        else if (len == 1)
            ret = allocExpr("&", ret->operands.front(), getRHS(ret));
    }
    if (ret->value == "&") {
        bool restartFlag = false;
        do {
        restartFlag = false;
        ACCExpr *nret = allocExpr(ret->value);
        std::string checkName;
        for (auto item: ret->operands) {
             if (item->value == "==")
                 checkName = item->operands.front()->value;
             else if (item->value == "&") {
                 for (auto pitem: item->operands)
                     nret->operands.push_back(pitem);
                 restartFlag = true;
                 continue;
             }
             else if (item->value == "!=" && checkName == item->operands.front()->value)
                 continue;
             nret->operands.push_back(item);
        }
        ret = nret;
        } while (restartFlag);
    }
    if (ret->value == "!=") {
        if (isdigit(ret->operands.front()->value[0])) { // move constants to RHS
            ACCExpr *lhs = ret->operands.front();
            ret->operands.pop_front();
            ret->operands.push_back(lhs);
        }
        ACCExpr *rhs = getRHS(ret), *lhs = ret->operands.front();
        if (checkInteger(rhs, "0") && lhs->value == "^" && checkInteger(getRHS(lhs), "1")) {
            ret->value = "==";
            ret->operands.clear();
            ret->operands.push_back(lhs->operands.front());
            ret->operands.push_back(rhs);
        }
    }
    if (ret->value == "==") {
        int leftLen = -1;
        if (isdigit(ret->operands.front()->value[0])) { // move constants to RHS
            ACCExpr *lhs = ret->operands.front();
            ret->operands.pop_front();
            ret->operands.push_back(lhs);
        }
        for (auto item: ret->operands) {
            if (isdigit(item->value[0])) {
                if (checkInteger(item, "0") && leftLen == 1) {
                    ret = allocExpr("!", ret->operands.front());
                    break;
                }
                updateWidth(item, leftLen);
            }
            else if (int len = exprWidth(item))
                leftLen = len;
        }
        ACCExpr *rhs = getRHS(ret), *lhs = ret->operands.front();
        if (checkInteger(rhs, "0")) {
        if (lhs->value == "==") {
            ret = lhs;
            ret->value = "!=";
        }
        }
    }
    if (ret->value == "!=") {
        int leftLen = -1;
        for (auto item: ret->operands) {
            if (isdigit(item->value[0])) {
                if (checkInteger(item, "0") && leftLen == 1) {
                    ret = ret->operands.front();
                    break;
                }
                updateWidth(item, leftLen);
            }
            else if (int len = exprWidth(item))
                leftLen = len;
        }
    }
    if (ret->value == "&" && ret->operands.size() >= 2) {
        auto aitem = ret->operands.begin(), aend = ret->operands.end();
        ACCExpr *first = *aitem++;
        ACCExpr *invertFirst = invertExpr(first);
        bool found = false;
        for (;aitem != aend; aitem++) {
            if ((*aitem)->value == "|") {
                for (auto oitem: (*aitem)->operands) {
                    if (matchExpr(oitem, invertFirst)) {
                        replaceValue(oitem, "0");
                        found = true;
                    }
                    else if (matchExpr(oitem, first)) {
                        replaceValue(oitem, "1");
                        found = true;
                    }
                }
            }
        }
        if (found)
            ret = cleanupExpr(ret);
    }
    if (ret->value == "&" && ret->operands.size() > 1) {
        auto aitem = ret->operands.begin(), aend = ret->operands.end();
        ACCExpr *lhs = *aitem++;
        if (lhs->value == "==") {
        ACCExpr *invertLhs = invertExpr(lhs);
        ACCExpr *matchItem = getRHS(lhs, 0);
        bool found = false;
        for (; aitem != aend; aitem++) {
            if ((*aitem)->value == "|")
                for (auto oitem: (*aitem)->operands) {
                     if (matchExpr(lhs, oitem)) {
                         replaceValue(oitem, "1");
                         found = true;
                     }
                     else if (matchExpr(invertLhs, oitem)) {
                         replaceValue(oitem, "0");
                         found = true;
                     }
                     else if (oitem->value == "!=" && matchExpr(matchItem, getRHS(oitem,0))) {
                         replaceValue(oitem, "0");
                         found = true;
                     }
                }
        }
        if (found)
            ret = cleanupExpr(ret);
        }
    }
    if (ret->value == "|" && ret->operands.size() > 1) {
        auto aitem = ret->operands.begin(), aend = ret->operands.end();
        ACCExpr *lhs = *aitem++;
        ACCExpr *invertLhs = invertExpr(lhs);
        bool found = false;
        for (; aitem != aend; aitem++) {
            if ((*aitem)->value == "&")
                for (auto pitem: (*aitem)->operands) {
                     if (matchExpr(invertLhs, pitem)) {
                         replaceValue(pitem, "1");
                         found = true;
                     }
                }
        }
        if (found)
            ret = cleanupExpr(ret);
    }
    if (TRACE_CLEANUP_EXPR)
        dumpExpr("cleanupExprEND" + autostr(level), ret);
    level--;
    return ret;
}
ACCExpr *cleanupExprBit(ACCExpr *expr)
{
    if (!expr)
        return expr;
    walkReplaceBuiltin(expr);
    return cleanupExpr(expr);
}

static ACCExpr *getExprList(ACCExpr *head, std::string terminator, bool repeatCurrentToken, bool preserveParen)
{
    bool parseState = false;
    ACCExpr *currentOperand = nullptr;
    ACCExpr *tok;
    ACCExpr *exprStack[MAX_EXPR_DEPTH];
    int exprStackIndex = 0;
#define TOP exprStack[exprStackIndex]
    TOP = nullptr;
    if (trace_expr)
        printf("[%s:%d] head %s\n", __FUNCTION__, __LINE__, head ? head->value.c_str() : "(nil)");
    if (head) {
        while ((tok = get1Token()) && tok->value != terminator) {
            if (trace_expr)
                printf("[%s:%d] parseState %d tok->value %s repeat %d\n", __FUNCTION__, __LINE__, parseState, tok->value.c_str(), repeatCurrentToken);
            if ((parseState = !parseState)) {    /* Operand */
                ACCExpr *unary = nullptr;
                ACCExpr *tnext = tok;
                if (repeatCurrentToken)
                    tok = head;
                else
                    tnext = get1Token();
                repeatCurrentToken = false;
                if ((tok->value == "-" || tok->value == "!") && !tok->operands.size()) { // unary '-'
                    unary = tok;
                    tok = tnext;
                    tnext = get1Token();
                    if (trace_expr)
                        printf("[%s:%d] unary '-' unary %p tok %p tnext %p\n", __FUNCTION__, __LINE__, (void *)unary, (void *)tok, (void *)tnext);
                }
                if (!checkOperand(tok->value) && !checkOperator(tok->value)) {
                    printf("[%s:%d] OPERAND CHECKFAILLLLLLLLLLLLLLL %s from %s\n", __FUNCTION__, __LINE__, tree2str(tok).c_str(), lexString.c_str());
                    exit(-1);
                }
                while (tnext && (isParen(tnext->value) || isIdChar(tnext->value[0]))) {
                    assert(isIdChar(tok->value[0]));
                    tok->operands.push_back(tnext);
                    tnext = get1Token();
                }
                repeatGet1Token = tnext;
                if (unary) {
                    unary->operands.push_back(tok);
                    tok = unary;
                }
                currentOperand = tok;
            }
            else {                        /* Operator */
                std::string L = TOP ? TOP->value : "", R = tok->value;
                if (!checkOperator(R)) {
                    printf("[%s:%d] OPERATOR CHECKFAILLLLLLLLLLLLLLL %s from %s\n", __FUNCTION__, __LINE__, R.c_str(), lexString.c_str());
                    exit(-1);
                }
                else if (!((L == R && L != "?") || (L == "?" && R == ":"))) {
                    if (TOP) {
                        int lprec = findPrec(L), rprec = findPrec(R);
                        if (lprec < rprec) {
                            exprStackIndex++;
                            TOP = nullptr;
                        }
                        else {
                            TOP->operands.push_back(currentOperand);
                            currentOperand = TOP;
                            while (exprStackIndex > 0 && lprec >= rprec) {
                                exprStackIndex--;
                                TOP->operands.push_back(currentOperand);
                                currentOperand = TOP;
                                L = TOP->value;
                                lprec = findPrec(L);
                            }
                        }
                    }
                    TOP = tok;
                }
                TOP->operands.push_back(currentOperand);
                currentOperand = nullptr;
            }
        }
        while (exprStackIndex != 0) {
            TOP->operands.push_back(currentOperand);
            currentOperand = TOP;
            exprStackIndex--;
        }
        if (currentOperand) {
            if (TOP)
                TOP->operands.push_back(currentOperand);
            else
                TOP = currentOperand;
        }
        if (TOP) {
            if (terminator != "")
                head->operands.push_back(TOP); // the first item in a recursed list
            else
                head = TOP;
        }
    }
    factorExpr(head);
    head = cleanupExpr(head, preserveParen);
    return head;
}

ACCExpr *str2tree(std::string arg, bool allowRangeParam)
{
    lexString = arg;
    lexIndex = 0;
    lexChar = lexString[lexIndex++];
    lexAllowRange = allowRangeParam;
    return getExprList(get1Token(), "", true, false);
}
