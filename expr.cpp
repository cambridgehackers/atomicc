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
#include "cudd.h"   // BDD library
#include "AtomiccIR.h"
#include "common.h"
#define MAX_EXPR_DEPTH 20

static int trace_expr;//=1;
static int trace_bool;//=1;
static std::string lexString;
static unsigned lexIndex;
static char lexChar;
static bool lexAllowRange;
static ACCExpr *repeatGet1Token;
std::map<std::string, int> replaceBlock;
int flagErrorsCleanup;
static int inBool;  // allow for recursive invocation!
std::map<std::string, RefItem> refList;

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

static bool bitOp(std::string s)
{
    return s == "^" || s == "&" || s == "|";
}
static bool booleanBinop(std::string s)
{
    return bitOp(s) || s == "&&" || s == "||";
}
static bool shiftOp(std::string s)
{
    return s == "<<" || s == ">>";
}
static bool arithOp(std::string s)
{
    return bitOp(s) || s == "+" || s == "-" || s == "*" || s == "%" || s == "?" || s == ":";
}

static bool relationalOp(std::string s)
{
    return s == "==" || s == "!=" || s == "<" || s == ">" || s == "<=" || s == ">=";
}

static bool checkOperator(std::string s)
{
    return booleanBinop(s) || shiftOp(s) ||  arithOp(s) || relationalOp(s) || s == "," || s == "!" || s == ".";
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

int walkCount (ACCExpr *expr)
{
    if (!expr)
        return 0;
    int count = 1;  // include this node in count
    for (auto item: expr->operands)
        count += walkCount(item);
    return count;
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
    else if (isParen(lexChar) || lexChar == '/' || lexChar == '%' || lexChar == '.'
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

bool checkInteger(ACCExpr *expr, std::string pattern)
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

int exprWidth(ACCExpr *expr, bool forceNumeric)
{
    if (!expr)
        return 0;
    std::string op = expr->value;
    if (relationalOp(op))
        return 1;
    if (isdigit(op[0])) {
        int ind = op.find("'");
        if (ind > 0) {
            std::string temp = op.substr(0, ind);
            return atoi(temp.c_str());
        }
        else if (forceNumeric)
            return 1;
    }
    if (isIdChar(op[0])) {
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
        return convertType(refList[op].type);
    }
    if (op == "?") {
        if (int len = exprWidth(getRHS(expr, 1), forceNumeric))
            return len;
        if (int len = exprWidth(getRHS(expr, 2), forceNumeric))
            return len;
    }
    if (op == "&" || op == "|" || op == "^") {
        for (auto item: expr->operands)
            if (exprWidth(item, forceNumeric) != 1)
                goto nextand;
        return 1;
nextand:;
    }
    if (op == "!") {
        return exprWidth(expr->operands.front(), forceNumeric);
    }
    return 0;
}

void updateWidth(ACCExpr *expr, int len)
{
    int ilen = exprWidth(expr);
    if (ilen < 0 || len < 0) {
        printf("[%s:%d] len %d ilen %d tree %s\n", __FUNCTION__, __LINE__, len, ilen, tree2str(expr).c_str());
        exit(-1);
    }
    if (isdigit(expr->value[0]) && len > 0 && expr->value.find("'") == std::string::npos)
        expr->value = autostr(len) + "'d" + expr->value;
    else if (isIdChar(expr->value[0])) {
        if (trace_expr)
            printf("[%s:%d] ID %s ilen %d len %d\n", __FUNCTION__, __LINE__, tree2str(expr).c_str(), ilen, len);
        if (ilen > len && len > 0 && !expr->operands.size()) {
            ACCExpr *subexpr = allocExpr(":", allocExpr(autostr(len-1)));
            if (len > 1)
                subexpr->operands.push_back(allocExpr("0"));
            expr->operands.push_back(allocExpr("[", subexpr));
        }
    }
    else if (expr->value == ":") // for __phi
        updateWidth(getRHS(expr), len);
    else if (expr->value == "?") {
        updateWidth(getRHS(expr), len);
        updateWidth(getRHS(expr, 2), len);
    }
    else if (arithOp(expr->value) || expr->value == "(") {
        if (expr->value == "-" && expr->operands.size() == 1
            && isdigit(expr->operands.front()->value[0]) && len == 1) {
            /* hack to update width on "~foo", which translates to "foo ^ -1" in the IR */
            expr->value = expr->operands.front()->value;
            expr->operands.clear();
            updateWidth(expr, len);
        }
        else
        for (auto item: expr->operands)
            updateWidth(item, len);
    }
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
        ACCExpr *list = expr->operands.front(); // get "(" list of [":", cond, value] items
        int size = list->operands.size();
        ACCExpr *firstInList = getRHS(list, 0), *secondInList = getRHS(list);
        ACCExpr *newe = nullptr;
        if (size == 2 && matchExpr(getRHS(firstInList, 0), invertExpr(getRHS(secondInList, 0))))
            newe = allocExpr("?", getRHS(firstInList, 0), getRHS(firstInList), getRHS(secondInList));
        else if (size == 2 && getRHS(firstInList, 0)->value == "__default" && exprWidth(getRHS(secondInList)) == 1)
            newe = allocExpr("&", getRHS(secondInList, 0), getRHS(secondInList));
        else if (size == 1)
            newe = getRHS(firstInList);
        else {
            //dumpExpr("PHI", list);
            newe = allocExpr("|");
            for (auto item: list->operands) {
                if (trace_expr)
                    dumpExpr("PHIELEMENTBEF", item);
                if (checkInteger(getRHS(item), "0"))
                    continue;    // default value is already '0'
                item->value = "?"; // Change from ':' -> '?'
                item->operands.push_back(allocExpr("0"));
                updateWidth(item, exprWidth(getRHS(item)));
                newe->operands.push_back(item);
                if (trace_expr)
                    dumpExpr("PHIELEMENT", item);
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

ACCExpr *cleanupExpr(ACCExpr *expr, bool preserveParen)
{
    if (!expr)
        return expr;
static int level;
    level++;
    if (expr->operands.size() == 1 && expr->operands.front()->value != "," && (!preserveParen && expr->value == "("))
        expr = expr->operands.front();
    if (isParen(expr->value) && expr->operands.size() == 1 && expr->operands.front()->value == ",")
        expr->operands = expr->operands.front()->operands;
    ACCExpr *ret = allocExpr(expr->value);
    bool booleanCond = expr->value == "?";
    for (auto item: expr->operands) {
         if (booleanCond)
             item = cleanupBool(item);
         ACCExpr *titem = cleanupExpr(item, isIdChar(expr->value[0]));
         booleanCond = false;
         if (titem->value != ret->value || ret->value == "?" || isParen(ret->value)
            || (  titem->value != "&" && titem->value != "|"
               && titem->value != "&&" && titem->value != "||"
               && titem->value != "+" && titem->value != "*"))
             ret->operands.push_back(titem);
         else {
             for (auto oitem: titem->operands)
                 ret->operands.push_back(oitem);
         }
    }
    level--;
    return ret;
}
ACCExpr *cleanupExprBuiltin(ACCExpr *expr)
{
    if (!expr)
        return expr;
    walkReplaceBuiltin(expr);
    return cleanupExpr(expr);
}

typedef struct {
    int index;
    DdNode *node;
} MapItem;
typedef std::map<std::string, MapItem *> VarMap;

#define MAX_NAME_COUNT 1000

static bool boolPossible(ACCExpr *expr)
{
    return expr && (isdigit(expr->value[0]) || exprWidth(expr, true) == 1);
}

static DdNode *tree2BDD(DdManager *mgr, ACCExpr *expr, VarMap &varMap)
{
    std::string op = expr->value;
    DdNode *ret = nullptr;
    if (op == "&&")
        op = "&";
    else if (op == "||")
        op = "|";
    if (checkInteger(expr, "1"))
        ret = Cudd_ReadOne(mgr);
    else if (checkInteger(expr, "0"))
        ret = Cudd_ReadLogicZero(mgr);
    else if (op == "!")
        return Cudd_Not(tree2BDD(mgr, expr->operands.front(), varMap)); // Not passes through ref count
    else if (op != "&" && op != "|" && op != "^") {
        if ((op == "!=" || op == "==")) {
            ACCExpr *lhs = getRHS(expr, 0);
            if (boolPossible(lhs) && boolPossible(getRHS(expr,1)))
                goto next; // we can analyze relops on booleans
            if (trace_bool)
                printf("[%s:%d] boolnot %d %d = %s\n", __FUNCTION__, __LINE__, boolPossible(getRHS(expr,0)), boolPossible(getRHS(expr,1)), tree2str(expr).c_str());
            if (isIdChar(lhs->value[0])) {
                if (trace_bool)
                    printf("[%s:%d] name %s type %s\n", __FUNCTION__, __LINE__, lhs->value.c_str(), refList[lhs->value].type.c_str());
            }
        }
        if (op == "!=")    // normalize comparison strings
            expr->value = "==";
        std::string name = "( " + tree2str(expr) + " )";
        if (!varMap[name]) {
            varMap[name] = new MapItem;
            varMap[name]->index = varMap.size();
            varMap[name]->node = Cudd_bddIthVar(mgr, varMap[name]->index);
        }
        ret = varMap[name]->node;
        if (op == "!=") {   // normalize comparison strings
            expr->value = op; // restore
            ret = Cudd_Not(ret);
        }
    }
    if (ret) {
        Cudd_Ref(ret);
        return ret;
    }
next:;
    for (auto item: expr->operands) {
         DdNode *operand = tree2BDD(mgr, item, varMap), *next;
         if (!ret)
             ret = operand;
         else {
             if (op == "&")
                 next = Cudd_bddAnd(mgr, ret, operand);
             else if (op == "|")
                 next = Cudd_bddOr(mgr, ret, operand);
             else if (op == "^" || op == "!=")
                 next = Cudd_bddXor(mgr, ret, operand);
             else if (op == "==")
                 next = Cudd_bddXnor(mgr, ret, operand);
             else {
                 printf("[%s:%d] unknown operator\n", __FUNCTION__, __LINE__);
                 exit(-1);
             }
             Cudd_Ref(next);
             Cudd_RecursiveDeref(mgr, operand);
             Cudd_RecursiveDeref(mgr, ret);
             ret = next;
         }
    }
    return ret;
}

ACCExpr *cleanupBool(ACCExpr *expr)
{
    if (!expr)
        return expr;
    inBool++; // can be invoked recursively!
    walkReplaceBuiltin(expr);
    inBool--;
    int varIndex = 0;
    VarMap varMap;
    DdManager * mgr = Cudd_Init(MAX_NAME_COUNT,0,CUDD_UNIQUE_SLOTS,CUDD_CACHE_SLOTS,0);
    varIndex = 0;
    varMap.clear();
    DdNode *bdd = tree2BDD(mgr, expr, varMap);
    char const *inames[MAX_NAME_COUNT];
    for (auto item: varMap)
        inames[item.second->index] = strdup(item.first.c_str());
    char * fform = Cudd_FactoredFormString(mgr, bdd, inames);
    Cudd_RecursiveDeref(mgr, bdd);
    int err = Cudd_CheckZeroRef(mgr);
    if (trace_bool)
        printf("%s: expr '%s' val = %s\n", __FUNCTION__, tree2str(expr).c_str(), fform);
    assert(err == 0 && "Reference counting");
    ACCExpr *ret = str2tree(fform);
    free(fform);
    Cudd_Quit(mgr);
    return ret;
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
