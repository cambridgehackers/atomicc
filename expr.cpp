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
static std::string lexString;
static unsigned lexIndex;
static char lexChar;
static bool lexAllowRange;
static ACCExpr *repeatGet1Token;
std::map<std::string, int> replaceBlock;

bool isIdChar(char ch)
{
    return isalpha(ch) || ch == '_' || ch == '$';
}

static bool isParen(std::string ch)
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
     for (auto item: expr->operands)
         if (i++ == match)
             return item;
     return nullptr;
}

std::string tree2str(ACCExpr *expr, bool *changed, bool assignReplace)
{
    if (!expr)
        return "";
    std::string ret, sep, op = expr->value;
    if (op == "__bitconcat") {
        ACCExpr *list = expr->operands.front();
        if (list->value == PARAMETER_MARKER)
            list->value = "{";
        return tree2str(list, changed, assignReplace);
    }
    if (op == "__bitsubstr") {
        std::string extra;
        ACCExpr *list = expr->operands.front();
        if (list->operands.size() == 3)
            extra = ":" + tree2str(getRHS(list, 2), changed, assignReplace);
        ACCExpr *bitem = list->operands.front();
        if (!isIdChar(bitem->value[0])) {
            printf("[%s:%d] can only do __bitsubstr on elementary items\n", __FUNCTION__, __LINE__);
            dumpExpr("BITSUB", expr);
            exit(-1);
        }
        std::string base = tree2str(list->operands.front(), changed, false);  // can only do bit select on net or reg (not expressions)
        return base + "[" + tree2str(getRHS(list), changed, assignReplace) + extra + "]";
    }
    if (isParen(op)) {
        ret += op;
        if (expr->operands.size())
            ret += " ";
        op = ",";
    }
    else if (isIdChar(op[0])) {
        ret += op;
        if (assignReplace) {
        ACCExpr *assignValue = assignList[op].value;
if (trace_assign)
printf("[%s:%d] check '%s' exprtree %p\n", __FUNCTION__, __LINE__, op.c_str(), (void *)assignValue);
        if (assignValue && !expr->operands.size() && !assignList[op].noRecursion && !assignList[op].noReplace && (assignValue->value == "{" || walkCount(assignValue) < ASSIGN_SIZE_LIMIT)) {
        if (replaceBlock[op]++ < 5 ) {
if (trace_assign)
printf("[%s:%d] changed %s -> %s\n", __FUNCTION__, __LINE__, op.c_str(), tree2str(assignValue).c_str());
            decRef(op);
            ret = tree2str(assignValue, changed, assignReplace);
            if (changed)
                *changed = true;
            else
                walkRef(assignValue);
        } 
        else {
printf("[%s:%d] excessive replace of %s with %s top %s\n", __FUNCTION__, __LINE__, op.c_str(), tree2str(assignValue).c_str(), tree2str(expr).c_str());
if (replaceBlock[op] > 7)
exit(-1);
        }
        }
        }
    }
    else if (!expr->operands.size() || ((op == "-" || op == "!")/*unary*/ && expr->operands.size() == 1))
        ret += op;
bool dumpOutput = false;
    bool topOp = checkOperand(expr->value) || expr->value == "," || expr->value == "[" || expr->value == PARAMETER_MARKER;
    for (auto item: expr->operands) {
        ret += sep;
        bool oldCond = !checkOperand(item->value) && item->value != ",";
        bool addParen = !topOp && oldCond;
bool orig = item->value != "?" || expr->operands.size() != 1;
if (addParen != (oldCond && orig)) dumpOutput = true;
        if (addParen)
            ret += "( ";
        ret += tree2str(item, changed, assignReplace);
        if (addParen)
            ret += " )";
        sep = " " + op + " ";
        if (op == "?")
            op = ":";
    }
    ret += treePost(expr);
//if (dumpOutput) {
//printf("[%s:%d]TTTTTTTT top '%s' expr %s\n", __FUNCTION__, __LINE__, expr->value.c_str(), ret.c_str());
//}
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
if (trace_assign)
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

static bool checkOperator(std::string s)
{
    return s == "==" || s == "&" || s == "+" || s == "-" || s == "*" || s == "%" || s == "!="
      || s == "?" || s == ":" || s == "^" || s == "," || s == "!"
      || s == "|" || s == "||" || s == "<" || s == ">"
      || s == "<<" || s == ">>";
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

void updateWidth(ACCExpr *item, int len)
{
    if (len > 0 && item->value.find("'") == std::string::npos)
        item->value = autostr(len) + "'d" + item->value;
}

bool matchExpr(ACCExpr *lhs, ACCExpr *rhs)
{
    if (!lhs && !rhs)
        return true;
    if (!lhs || !rhs || lhs->value != rhs->value || lhs->operands.size() != rhs->operands.size())
        return false;
    for (auto lcur = lhs->operands.begin(), lend = lhs->operands.end(), rcur = rhs->operands.begin(); lcur != lend; lcur++, rcur++)
        if (!matchExpr(*lcur, *rcur))
            return false;
    return true;
}

ACCExpr *cleanupExpr(ACCExpr *expr, bool preserveParen)
{
    if (!expr)
        return expr;
    if (trace_expr)
        dumpExpr("cleanupExprSTART", expr);
    if (expr->operands.size() == 1 && expr->operands.front()->value != "," && ((!preserveParen && expr->value == "(") ||
         expr->value == "&&" || expr->value == "&" || expr->value == "||" || expr->value == "|"))
        expr = expr->operands.front();
    if (isParen(expr->value) && expr->operands.size() == 1 && expr->operands.front()->value == ",")
        expr->operands = expr->operands.front()->operands;
    ACCExpr *lhs = expr->operands.front();
    std::string v = expr->value;
    if (v == "^" && checkInteger(getRHS(expr), "1"))
        return invertExpr(cleanupExpr(lhs));
    ACCExpr *ret = allocExpr(expr->value);
    for (auto item: expr->operands) {
         ACCExpr *titem = cleanupExpr(item, isIdChar(expr->value[0]));
         if (trace_expr)
             printf("[%s:%d] item %p titem %p ret %p\n", __FUNCTION__, __LINE__, (void *)item, (void *)titem, (void *)ret);
         if (titem->value != ret->value || ret->value == "?" || isParen(ret->value)
            || (  titem->value != "&" && titem->value != "|"
               && titem->value != "&&" && titem->value != "||"
               && titem->value != "+" && titem->value != "*"))
             ret->operands.push_back(titem);
         else {
             if (trace_expr)
                 printf("[%s:%d] combine tree expressions: op %s uppersize %d lowersize %d\n", __FUNCTION__, __LINE__, titem->value.c_str(), (int)ret->operands.size(), (int)titem->operands.size());
             for (auto oitem: titem->operands)
                 ret->operands.push_back(oitem);
         }
    }
    if (ret->value == "?" && checkInteger(ret->operands.front(), "1"))
        ret = getRHS(ret);
    if (ret->value == "?" && checkInteger(getRHS(ret, 2), "0"))
        ret = allocExpr("&", ret->operands.front(), getRHS(ret));
    if (ret->value == "&") {
        bool restartFlag = false;
        do {
        restartFlag = false;
        ACCExpr *nret = allocExpr(ret->value);
        std::string checkName;
        for (auto item: ret->operands) {
             if (checkInteger(item, "0")) {
                 nret = item;
                 break;
             }
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
             else if (checkInteger(item, "1") && ret->operands.size() > 1)
                 continue;
             else for (auto pitem: nret->operands)
                 if (matchExpr(pitem, item))  // see if we already have this operand
                     goto nexta;
             nret->operands.push_back(item);
nexta:;
        }
        ret = nret;
        } while (restartFlag);
    }
    if (ret->value == "|") {
        ACCExpr *nret = allocExpr(ret->value);
        std::string checkName;
        for (auto item: ret->operands) {
             if (checkInteger(item, "1")) {
                 nret = item;
                 break;
             }
             else if (checkInteger(item, "0") && ret->operands.size() > 1)
                 continue;
             else for (auto pitem: nret->operands)
                 if (matchExpr(pitem, item))  // see if we already have this operand
                     goto nexto;
             nret->operands.push_back(item);
nexto:;
        }
        ret = nret;
    }
    if (ret->operands.size() == 1 && (ret->value == "&" || ret->value == "|"))
        ret = ret->operands.front();
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
            if (isIdChar(item->value[0])) {
                if(!refList[item->value].pin) {
printf("[%s:%d] unknown %s in '=='\n", __FUNCTION__, __LINE__, item->value.c_str());
//exit(-1);
                }
                else
                    leftLen = convertType(refList[item->value].type);
            }
            else if (isdigit(item->value[0])) {
                if (checkInteger(item, "0") && leftLen == 1) {
                    ret = allocExpr("!", ret->operands.front());
                    break;
                }
                updateWidth(item, leftLen);
            }
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
            if (isIdChar(item->value[0])) {
                if(!refList[item->value].pin) {
printf("[%s:%d] unknown %s in '=='\n", __FUNCTION__, __LINE__, item->value.c_str());
//exit(-1);
                }
                else
                    leftLen = convertType(refList[item->value].type);
            }
            else if (isdigit(item->value[0])) {
                if (checkInteger(item, "0") && leftLen == 1) {
                    ret = ret->operands.front();
                    break;
                }
                updateWidth(item, leftLen);
            }
        }
    }
    if (ret->value == "&" && !ret->operands.size())
        ret->value = "1";
    if (ret->value == "|" && !ret->operands.size())
        ret->value = "0";
    return ret;
}
ACCExpr *cleanupExprBit(ACCExpr *expr)
{
    if (!expr)
        return expr;
    ACCExpr *temp = cleanupExpr(expr);
    //return temp;
    std::string bitTemp = tree2str(temp);  //HACK HACK HACK HACK to trigger __bitsubstr processing
//printf("[%s:%d] '%s'\n", __FUNCTION__, __LINE__, bitTemp.c_str());
//dumpExpr("EXPR", expr);
    return str2tree(bitTemp);              //HACK HACK HACK HACK to trigger __bitsubstr processing
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
