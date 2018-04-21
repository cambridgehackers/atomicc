/*
   Copyright (C) 2018 John Ankcorn

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
#define MAX_EXPR_DEPTH 20

static std::string lexString;
static unsigned lexIndex;
static char lexChar;
static bool lexAllowRange;
static ACCExpr *repeatGet1Token;

static bool isIdChar(char ch)
{
    return isalpha(ch) || ch == '_' || ch == '$';
}

static bool isParenChar(char ch)
{
    return ch == '[' || ch == '(' || ch == '{';
}

static std::string treePost(const ACCExpr *arg)
{
    if (arg->value == "[")
        return " ]";
    else if (arg->value == "(")
        return " )";
    else if (arg->value == "{")
        return " }";
    return "";
}

static bool checkOperand(std::string s)
{
    return isIdChar(s[0]) || isdigit(s[0]) || s == "(" || s == "{";
}

static inline void dumpExpr(std::string tag, ACCExpr *next)
{
    printf("DE: %s %p %s\n", tag.c_str(), next, next ? next->value.c_str() : "");
    int i = 0;
    if (next)
    for (auto item: next->operands) {
        dumpExpr(tag + "_" + autostr(i), item);
        i++;
    }
}

static std::string tree2str(const ACCExpr *arg)
{
    std::string ret;
    if (!arg)
        return "";
    std::string sep, op = arg->value;
    if (isParenChar(op[0]) || isIdChar(op[0])) {
        ret += op + " ";
        op = "";
    }
    if (!arg->operands.size())
        ret += op;
    for (auto item: arg->operands) {
        ret += sep;
        bool operand = checkOperand(item->value) || item->value == "," || item->value == "?" || arg->operands.size() == 1;
        if (!operand)
            ret += "( ";
        ret += tree2str(item);
        if (!operand)
            ret += " )";
        sep = " " + op + " ";
        if (op == "?")
            op = ":";
    }
    ret += treePost(arg);
    return ret;
}

static ACCExpr *allocExpr(std::string value, ACCExpr *argl = nullptr, ACCExpr *argr = nullptr)
{
    ACCExpr *ret = new ACCExpr;
    ret->value = value;
    ret->operands.clear();
    if (argl)
        ret->operands.push_back(argl);
    if (argr)
        ret->operands.push_back(argr);
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

static ACCExpr *getExprList(ACCExpr *head, std::string terminator, bool repeatCurrentToken);
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
    else if (isParenChar(lexChar) || lexChar == '/' || lexChar == '%'
        || lexChar == ']' || lexChar == '}' || lexChar == ')' || lexChar == '^'
        || lexChar == ',' || lexChar == '?' || lexChar == ':' || lexChar == ';')
        getNext();
    else {
        printf("[%s:%d] lexString '%s' unknown lexChar %c %x\n", __FUNCTION__, __LINE__, lexString.c_str(), lexChar, lexChar);
        exit(-1);
    }
    ret = allocExpr(lexToken);
    if (isParenChar(ret->value[0]))
        return getExprList(ret, treePost(ret).substr(1), false);
    return ret;
}

static bool checkOperator(std::string s)
{
    return s == "==" || s == "&" || s == "+" || s == "-" || s == "*" || s == "%" || s == "!="
      || s == "?" || s == ":" || s == "^" || s == ","
      || s == "|" || s == "||" || s == "<" || s == ">";
}

static ACCExpr *getRHS(ACCExpr *expr)
{
     int i = 0;
     for (auto item: expr->operands)
         if (i++ == 1)
             return item;
     return nullptr;
}

static ACCExpr *invertExpr(ACCExpr *expr)
{
    if (!expr)
        return allocExpr("1");
    ACCExpr *lhs = expr->operands.front();
    std::string v = expr->value;
    if (v == "^" && getRHS(expr)->value == "1")
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

static void updateWidth(ACCExpr *item, int len)
{
    if (len > 0 && item->value.find("'") == std::string::npos)
        item->value = autostr(len) + "'d" + item->value;
}

static ACCExpr *cleanupExpr(ACCExpr *expr)
{
    if (!expr)
        return expr;
    ACCExpr *lhs = expr->operands.front();
    std::string v = expr->value;
    if (v == "^" && getRHS(expr)->value == "1")
        return invertExpr(cleanupExpr(lhs));
    ACCExpr *ret = allocExpr(expr->value);
    for (auto item: expr->operands) {
         ACCExpr *titem = cleanupExpr(item);
         if (titem->value != ret->value || ret->value == "?")
             ret->operands.push_back(titem);
         else
             for (auto oitem: titem->operands)
                 ret->operands.push_back(oitem);
    }
    if (ret->value == "&") {
        ACCExpr *nret = allocExpr(ret->value);
        std::string checkName;
        for (auto item: ret->operands) {
             if (item->value == "0") {
                 nret = item;
                 break;
             }
             if (item->value == "==")
                 checkName = item->operands.front()->value;
             else if (item->value == "!=" && checkName == item->operands.front()->value)
                 continue;
             else if (item->value == "1" && ret->operands.size() > 1)
                 continue;
             nret->operands.push_back(item);
        }
        ret = nret;
    }
    if (ret->value == "|") {
        ACCExpr *nret = allocExpr(ret->value);
        std::string checkName;
        for (auto item: ret->operands) {
             if (item->value == "1") {
                 nret = item;
                 break;
             }
             else if (item->value == "0" && ret->operands.size() > 1)
                 continue;
             nret->operands.push_back(item);
        }
        ret = nret;
    }
    if (ret->operands.size() == 1 && (ret->value == "&" || ret->value == "|"))
        ret = ret->operands.front();
    if (ret->value == "==") {
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
            else if (isdigit(item->value[0]))
                updateWidth(item, leftLen);
        }
    }
    if (ret->value == "!=") {
        ACCExpr *rhs = getRHS(ret);
        if (ret->operands.front()->value == "0" && rhs->value == "^" && getRHS(rhs)->value == "1") {
            ret->value = "==";
            ret->operands.pop_back();
            ret->operands.push_back(rhs->operands.front());
        }
    }
    return ret;
}

static ACCExpr *getExprList(ACCExpr *head, std::string terminator, bool repeatCurrentToken)
{
    bool parseState = false;
    ACCExpr *currentOperand = nullptr;
    ACCExpr *tok;
    ACCExpr *exprStack[MAX_EXPR_DEPTH];
    int exprStackIndex = 0;
#define TOP exprStack[exprStackIndex]
    TOP = nullptr;
    if (head) {
        while ((tok = get1Token()) && tok->value != terminator) {
            if ((parseState = !parseState)) {    /* Operand */
                ACCExpr *tnext = tok;
                if (repeatCurrentToken)
                    tok = head;
                else
                    tnext = get1Token();
                repeatCurrentToken = false;
                if (!checkOperand(tok->value) && !checkOperator(tok->value)) {
                    printf("[%s:%d] OPERAND CHECKFAILLLLLLLLLLLLLLL %s from %s\n", __FUNCTION__, __LINE__, tree2str(tok).c_str(), lexString.c_str());
                    exit(-1);
                }
                while (tnext && (tnext->value == "{" || tnext->value == "[" || isIdChar(tnext->value[0]))) {
                    assert(isIdChar(tok->value[0]));
                    tok->operands.push_back(tnext);
                    tnext = get1Token();
                }
                if (isIdChar(tok->value[0]) && tok->operands.size()
                  && tok->operands.front()->value == "["
                  && tok->operands.front()->operands.size()) {
                    std::string sub = tok->operands.front()->operands.front()->value;
                    if (isdigit(sub[0])) {
                        tok->value += sub;
                        tok->operands.pop_front();
                        if (tok->operands.size() && isIdChar(tok->operands.front()->value[0])) {
                            tok->value += tok->operands.front()->value;
                            tok->operands.pop_front();
                        }
                    }
                }
                repeatGet1Token = tnext;
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
        if (head->value == "(" && head->operands.size() == 1)
            head = head->operands.front();
    }
    head = cleanupExpr(head);
    return head;
}

static ACCExpr *str2tree(std::string arg, bool allowRangeParam = false)
{
    lexString = arg;
    lexIndex = 0;
    lexChar = lexString[lexIndex++];
    lexAllowRange = allowRangeParam;
    return getExprList(get1Token(), "", true);
}
