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

int trace_expr;//=1;
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

static std::string treePost(std::string val)
{
    if (val == "[")
        return "]";
    else if (val == "(")
        return ")";
    else if (val == "{")
        return "}";
    else if (val == SUBSCRIPT_MARKER)
        return SUBSCRIPT_CLOSE;
    else if (val == PARAMETER_MARKER)
        return PARAMETER_CLOSE;
    return "";
}

bool bitOp(std::string s)
{
    return s == "^" || s == "&" || s == "|";
}
bool booleanBinop(std::string s)
{
    return bitOp(s) || s == "&&" || s == "||";
}
static bool shiftOp(std::string s)
{
    return s == "<<" || s == ">>";
}
bool arithOp(std::string s)
{
    return bitOp(s) || s == "+" || s == "-" || s == "*" || s == "%" || s == "?" || s == ":" || s == "/";
}

bool relationalOp(std::string s)
{
    return s == "==" || s == "!=" || s == "<" || s == ">" || s == "<=" || s == ">=";
}

static bool checkOperator(std::string s)
{
    return booleanBinop(s) || shiftOp(s) ||  arithOp(s) || relationalOp(s) || s == "," || s == "!" || s == "." || s == "=" || s == "'";
}

bool checkOperand(std::string s)
{
    return isIdChar(s[0]) || isdigit(s[0]) || s == "(" || s == "{" || s[0] == '"';
}

bool isConstExpr(ACCExpr *expr)
{
    if (!isdigit(expr->value[0]) && !startswith(expr->value, GENVAR_NAME) && !checkOperator(expr->value))
        return false;
    for (auto item: expr->operands)
        if (!isConstExpr(item))
            return false;
    return true;
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

std::string tree2str(ACCExpr *expr, bool addSpaces)
{
    if (!expr)
        return "";
    std::string ret, sep, orig = expr->value, space;
    if (addSpaces)
        space = " ";
    if (orig[0] == '@')
        orig = orig.substr(1);
    std::string op = orig;
    if (isParen(op)) {
        ret += op;
        if (expr->operands.size())
            ret += space;
        op = ",";
    }
    else if (isIdChar(op[0])) {
        ret += op;
        op = "";
    }
    else if (((op == "-" || op == "<<" || expr->value[0] == '@')/*unary*/ && expr->operands.size() == 1))
        ret += op;
    else if (op == "!" || !expr->operands.size())
        ret += op;
    bool topOp = checkOperand(expr->value) || expr->value == "," || isParen(expr->value);
    if (expr->value == "$past" && expr->operands.size()) { // runtime functions actually 'called' in generated code
        expr->operands.front()->value = "(";
    }
    bool leftOpParen = (op == "'" && expr->operands.size() > 1);
    for (auto item: expr->operands) {
        ret += sep;
        bool addParen = leftOpParen || (!topOp && !checkOperand(item->value) && item->value != "," && item->value != PERIOD && !isParen(item->value));
        if (addParen)
            ret += "(" + space;
        std::string val = tree2str(item, addSpaces);
        if (ret.length() && val.length() && ret.substr(ret.length()-1) == PERIOD && val[0] == '[')
            ret = ret.substr(0, ret.length()-1);
        ret += val;
        if (addParen)
            ret += space + ")";
        if (op == PERIOD)
            sep = op;
        else
            sep = space + op + space;
        if (op == "?")
            op = ":";
        //leftOpParen = false;
    }
    std::string post = treePost(orig);
    if (post != "")
        ret += space + post;
    return ret;
}

int walkCount (ACCExpr *expr)
{
    if (!expr)
        return 0;
    int count = 1;  // include this node in count
    if (expr->value != PERIOD)
    for (auto item: expr->operands)
        count += walkCount(item);
    return count;
}

std::string replacePeriod(std::string value)
{
    int ind;
    while ((ind = value.find(PERIOD)) > 0)
        value = value.substr(0, ind) + DOLLAR + value.substr(ind+1);
    return value;
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

    {"," , 2},

    {"=" , 7},

    {"?", 10}, {":", 10},

    {"&", 12}, {"|", 12},
    {"&&", 17}, {"||", 17},
    {"^", 18},

    {"==", 20}, {"!=" , 20}, {"<", 20}, {">", 20}, {"<=", 20}, {">=", 20},

    {">>", 25}, {"<<", 25},

    {"+", 30}, {"-", 30},
    {"*", 40}, {"%", 40}, {"/", 40},
    {".", 99},

    {nullptr, -1}};
    int ind = 0;
    while (opPrec[ind].op && opPrec[ind].op != s)
        ind++;
    if (s != "" && !opPrec[ind].op) {
        printf("[%s] ERROR: failed to find precedence for operator %s\n", __FUNCTION__, s.c_str());
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
    else if (isdigit(lexChar)) {
        do {
            getNext();
        } while (isdigit(lexChar) || lexChar == '.' || lexChar == '\'' || lexChar == 'b'
            || lexChar == 'h' || lexChar == 'd' || lexChar == 'o');
        if (lexToken[lexToken.length()-1] == '\'') {
        if (lexChar == 'x') {
            do {
                getNext();
            } while (isdigit(lexChar) || (lexChar >= 'a' && lexChar <= 'f'));
            if (!startswith(lexToken, "0x")) {
                printf("[%s:%d] error: invalid characters in hex string '%s'\n", __FUNCTION__, __LINE__, lexToken.c_str());
                exit(-1);
            }
            lexToken = lexToken.substr(2);
            lexToken = autostr(4 * lexToken.length()) + "'h" + lexToken;
        }
        else if (lexChar == 'U') // unsigned
            lexChar = lexString[lexIndex++]; // skip 'U'
        else if (lexChar == '(') {
            while(lexChar != ')')
                getNext();
            getNext();
        }
        }
    }
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
        || lexChar == ',' || lexChar == '?' || lexChar == ':' || lexChar == ';'
        || lexChar == '\'')
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
        std::string post = treePost(val);
        if (trace_expr)
            printf("[%s:%d] before subparse of '%s' post '%s' bufferlen %d\n", __FUNCTION__, __LINE__, ret->value.c_str(), post.c_str(), (int)strlen(&lexString[lexIndex]));
        ret = getExprList(ret, post, false, true);
        if (ret->value != val)
            ret = allocExpr(val, ret); // over optimization of '(<singleItem>)'
        if (trace_expr) {
            printf("[%s:%d] after subparse of '%s'\n", __FUNCTION__, __LINE__, ret->value.c_str());
            dumpExpr("SUBPAREN", ret);
        }
    }
    return ret;
}

bool checkIntegerString(std::string val, std::string pattern)
{
    if (!isdigit(val[0]))
        return false;
    int ind = val.find("'");
    if (ind > 0)    // strip off radix
        val = val.substr(ind+2);
    return val == pattern;
}

bool checkInteger(ACCExpr *expr, std::string pattern)
{
    return expr && checkIntegerString(expr->value, pattern);
}

bool plainInteger(std::string val)
{
    return val.length() > 0 && isdigit(val[0]) && val.find("'") == std::string::npos;
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
    if (v == "&" || v == "&&" || v == "|" || v == "||") {
        ACCExpr *temp = allocExpr((v == "&" || v == "&&") ? "||" : "&&");
        for (auto item: expr->operands)
            temp->operands.push_back(invertExpr(item));
        return temp;
    }
    return allocExpr("^", expr, allocExpr("1"));
}

bool matchExpr(ACCExpr *lhs, ACCExpr *rhs)
{
    if (!lhs && !rhs)
        return true;
    if (!lhs || !rhs || lhs->value != rhs->value || lhs->operands.size() != rhs->operands.size())
        return false;
    if ((lhs->value == "&" || lhs->value == "&&" || lhs->value == "|" || lhs->value == "||") && lhs->operands.size() == 2) // check commutativity
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

ACCExpr *cleanupExpr(ACCExpr *expr, bool preserveParen)
{
    if (!expr)
        return expr;
    if (expr->operands.size() == 1 && expr->operands.front()->value != "," && (!preserveParen && expr->value == "("))
        expr = expr->operands.front();
    if (isParen(expr->value) && expr->operands.size() == 1 && expr->operands.front()->value == ",")
        expr->operands = expr->operands.front()->operands;
    if (expr->value == "|" && expr->operands.size() == 1)
        expr = expr->operands.front();
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
    if (ret->value == "|" && !ret->operands.size())
        ret->value = "0";
    if (ret->value == "?" && checkInteger(getRHS(ret, 0), "1"))
        return getRHS(ret, 1);
    if (ret->value == "&&" && checkInteger(getRHS(ret, 0), "1")) {
        if (ret->operands.size() > 2)
            ret->operands.pop_front();
        else
            return getRHS(ret, 1);
    }
    return ret;
}

ACCExpr *cleanupInteger(ACCExpr *expr)
{
    ACCExpr *ret = cleanupExpr(expr);
    std::string op = ret->value;
    int64_t total = op == "*" ? 1 : 0;
    if (op == "+" || op == "*") {
        for (auto item: ret->operands) {
            if (!plainInteger(item->value))
                return ret;
            int64_t val = atoi(item->value.c_str());
            if (op == "+")
                total += val;
            else if (op == "*")
                total *= val;
        }
        ret->value = autostr(total);
        ret->operands.clear();
    }
    return ret;
}

ACCExpr *cleanupModuleParam(std::string param)
{
    ACCExpr *expr = cleanupExprBuiltin(str2tree(param));
    if (expr->value != "(")
        expr = allocExpr("(", expr);
    for (auto item: expr->operands)
        assert(item->value == "=");
    return expr;
}

typedef struct {
    int index;
    DdNode *node;
} MapItem;
typedef std::map<std::string, MapItem *> VarMap;

#define MAX_NAME_COUNT 1000

static bool boolPossible(ACCExpr *expr)
{
    return expr && (isdigit(expr->value[0]) || exprWidth(expr, true) == "1");
}

int traceBDD;//=1;
static DdNode *tree2BDD(DdManager *mgr, ACCExpr *expr, VarMap &varMap)
{
    DdNode *ret = nullptr, *trueOperand = nullptr;
    if (traceBDD)
        dumpExpr("ENTERTREE2BDD", expr);
    if (expr->value == "?" && checkInteger(getRHS(expr,2), "0")) {
        expr->value = "&";
        expr->operands.pop_back();
    }
    std::string op = expr->value;
    if (op == "&&")
        op = "&";
    else if (op == "||")
        op = "|";
    if (op == "?" && expr->operands.size() != 3) {
        printf("[%s:%d]ERROR in '?' operand '%s'\n", __FUNCTION__, __LINE__, tree2str(expr).c_str());
        exit(-1);
    }
    if (checkInteger(expr, "1"))
        ret = Cudd_ReadOne(mgr);
    else if (checkInteger(expr, "0"))
        ret = Cudd_ReadLogicZero(mgr);  // logic 0 != arithmetic 0
    else if (op == "!")
        return Cudd_Not(tree2BDD(mgr, expr->operands.front(), varMap)); // Not passes through ref count
    else if (op == "&" || op == "|" || op == "^" || op == "?") { // handled below
    }
    else {
        if (op == "!=" || op == "==") {
            ACCExpr *lhs = getRHS(expr, 0);
            if (trace_bool)
                printf("[%s:%d] boolnot %d %d = %s\n", __FUNCTION__, __LINE__, boolPossible(getRHS(expr,0)), boolPossible(getRHS(expr,1)), tree2str(expr).c_str());
            if (lhs && isIdChar(lhs->value[0])) {
                if (trace_bool)
                    printf("[%s:%d] name %s type %s\n", __FUNCTION__, __LINE__, lhs->value.c_str(), refList[lhs->value].type.c_str());
            }
        }
        if (op == "!=")    // normalize comparison strings
            expr->value = "==";
        std::string name = tree2str(expr);
        if (!isIdChar(expr->value[0]))
            name = "( " + name + " )";
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
             else if (op == "?") { // in BDD, called 'Ite' -> 'if-then-else' operator.  In cpp, called 'trinary'
                 if (!trueOperand) {
                     trueOperand = operand;
                     continue;
                 }
                 next = Cudd_bddIte(mgr, ret, trueOperand, operand); // "?" operator
                 Cudd_RecursiveDeref(mgr, trueOperand);
             }
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

ACCExpr *finalBool(ACCExpr *expr)
{
    ACCExpr *ret = allocExpr(expr->value);
    if (ret->value == "!") {
        ACCExpr *op = getRHS(expr, 0);
        if (op->value == "!") {
            return finalBool(getRHS(op, 0));
        }
        if (op->value == "^" && checkInteger(getRHS(op, 1), "1")) {
            return finalBool(getRHS(op, 0));
        }
        if (op->value == "||" || op->value == "&&") {
            ret->value = (op->value == "||") ? "&&" : "||";
            for (auto item: op->operands)
                ret->operands.push_back(allocExpr("!", item));
            return finalBool(ret);
        }
        if (op->value == "==") {
            ret->value = "!=";
            for (auto item: op->operands)
                ret->operands.push_back(item);
            return finalBool(ret);
        }
    }
    if (ret->value == "==" || ret->value == "!=") {
        ACCExpr *opl = getRHS(expr, 0);
        ACCExpr *opr = getRHS(expr, 1);
        if (isdigit(opl->value[0]) && !isdigit(opr->value[0])) {
            ret->operands.push_back(opr);
            ret->operands.push_back(opl);
            return finalBool(ret);
        }
        if (checkInteger(opr, "0")) {
        if (opl->value == "^") {
            ACCExpr *argl = getRHS(opl, 0);
            ACCExpr *argr = getRHS(opl, 1);
            if (checkInteger(argr, "1")) {
                if (ret->value == "==")
                    return finalBool(argl);
                else
                    return finalBool(allocExpr("!", argl));
            }
        }
        else if (convertType(refList[opl->value].type) == "1" || isRdyName(opl->value) || isEnaName(opl->value)) {
            if (ret->value == "==")
                return finalBool(allocExpr("!", opl));
            else
                return finalBool(opl);
        }
        }
    }
    for (auto item: expr->operands) {
        item = finalBool(item);
        if ((ret->value == "||" || ret->value == "&&") && ret->value == item->value) {
            for (auto iitem: item->operands)
                ret->operands.push_back(iitem);
            continue;
        }
        ret->operands.push_back(item);
    }
    return ret;
}

ACCExpr *cleanupBool(ACCExpr *expr)
{
    if (!expr)
        return expr;
    inBool++; // can be invoked recursively!
    walkReplaceBuiltin(expr, "0");
    updateWidth(expr, "1");
    inBool--;
//#define MAX_EXPR_COMPLEXITY 40
//if (walkCount(expr) > MAX_EXPR_COMPLEXITY) {
    //return expr;
//}
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
    ret = finalBool(ret);
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
                std::string val = tok->value;
                if (val[0] == '@') {
printf("[%s:%d]AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA '%s'\n", __FUNCTION__, __LINE__, val.c_str());
                    val = val.substr(1);
                }
                if ((val == "-" || val == "!" || bitOp(val) || val == "<<" || val == "'") && !tok->operands.size()) { // unary '-'
                    if (val != "!")
                        tok->value = "@" + tok->value;
                    unary = tok;
                    tok = tnext;
                    tnext = get1Token();
                    if (trace_expr)
                        printf("[%s:%d] unary '-' unary %p tok %p tnext %p\n", __FUNCTION__, __LINE__, (void *)unary, (void *)tok, (void *)tnext);
                }
                ACCExpr *dotExpr = nullptr;
                if (!checkOperand(tok->value) && !checkOperator(tok->value)) {
                    printf("[%s:%d] OPERAND CHECKFAILLLLLLLLLLLLLLL %s from %s\n", __FUNCTION__, __LINE__, tree2str(tok).c_str(), lexString.c_str());
                    exit(-1);
                }
                while (tnext && tok->value != "'" && (tnext->value == PERIOD || isParen(tnext->value) || isIdChar(tnext->value[0]))) {
                while (tnext && tnext->value == PERIOD) {
                    if (!dotExpr)
                        dotExpr = tnext;
                    dotExpr->operands.push_back(tok);
                    tok = get1Token();
                    tnext = get1Token();
                }
                while (tnext && (isParen(tnext->value) || isIdChar(tnext->value[0]))) {
                    if(!isIdChar(tok->value[0]) && tok->value[0] != '.') {
                        printf("[%s:%d] ERROR: operand %s\n", __FUNCTION__, __LINE__, tok->value.c_str());
                        exit(-1);
                    }
                    //assert(isIdChar(tok->value[0]) || tok->value[0] == '.'); // hack for fifo[i].out
                    tok->operands.push_back(tnext);
                    tnext = get1Token();
                }
                }
                if (dotExpr) {
                    if (tok)
                        dotExpr->operands.push_back(tok);
                    tok = dotExpr;
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
                            if (TOP->value == "," && tok->value == ",")
                                goto lll;
                        }
                    }
                    TOP = tok;
                }
                TOP->operands.push_back(currentOperand);
lll:;
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
    if (trace_expr)
        printf("[%s:%d] arg '%s' allowRangeParam '%d'\n", __FUNCTION__, __LINE__, arg.c_str(), allowRangeParam);
std::string orig = arg;
    lexString = arg;
    lexIndex = 0;
    lexChar = lexString[lexIndex++];
    lexAllowRange = allowRangeParam;
    auto ret = getExprList(get1Token(), "", true, false);
//printf("[%s:%d] in '%s'\n", __FUNCTION__, __LINE__, orig.c_str());
//dumpExpr("OUTEX", ret);
    return ret;
}
