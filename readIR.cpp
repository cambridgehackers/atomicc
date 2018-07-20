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

static char buf[MAX_READ_LINE];
static char *bufp;
static int lineNumber = 0;
static FILE *OStrGlobal;

static ACCExpr *getExpression(void)
{
    const char *startp = bufp;
    int level = 0;
    bool inQuote = false;
    while (*bufp == ' ')
        bufp++;
    while (*bufp && ((*bufp != ' ' && *bufp != ':' && *bufp != ',') || level != 0)) {
        if (inQuote) {
            if (*bufp == '"')
                inQuote = false;
            if (*bufp == '\\')
                bufp++;
        }
        else {
        if (*bufp == '"')
            inQuote = true;
        else if (*bufp == '(')
            level++;
        else if (*bufp == ')')
            level--;
        }
        bufp++;
    }
    while (*bufp == ' ')
        bufp++;
    return str2tree(std::string(startp, bufp - startp));
}

static bool checkItem(const char *val)
{
     while (*bufp == ' ')
         bufp++;
     int len = strlen(val);
//printf("[%s:%d] val %s len %d bufp %s\n", __FUNCTION__, __LINE__, val, len, bufp);
     bool ret = !strncmp(bufp, val, len);
     if (ret)
         bufp += len;
     while (*bufp == ' ')
         bufp++;
     return ret;
}
static void ParseCheck(bool ok, std::string message)
{
    if (!ok) {
        printf("[%s:%d] ERROR: %s: residual %s\n", __FUNCTION__, __LINE__, message.c_str(), bufp);
        printf("full line number %d: %s\n", lineNumber, buf);
        exit(-1);
    }
}
static bool readLine(void)
{
    if (fgets(buf, sizeof(buf), OStrGlobal) == NULL)
        return false;
    buf[strlen(buf) - 1] = 0;
    bufp = buf;
    lineNumber++;
    return true;
}
static std::string getToken()
{
    char *startp = bufp;
    while (*bufp == ' ')
        bufp++;
    while (*bufp && *bufp != ' ')
        bufp++;
    std::string ret = std::string(startp, bufp);
    while (*bufp == ' ')
        bufp++;
    return ret;
}

static std::string cleanInterface(std::string name)
{
    if (name == "_")
        name = "";
    else if (endswith(name, "$_"))
        name = name.substr(0, name.length()-1);
    return name;
}

/*
 * rewrite method call parameter and subscript markers, since '[' and '{'
 * are needed in verilog output
 */
static void rewriteExpr(ACCExpr *expr)
{
    if (!expr)
        return;
    if (expr->value == "[")
        expr->value = SUBSCRIPT_MARKER;
    else if (expr->value == "{")
        expr->value = PARAMETER_MARKER;
    for (auto item: expr->operands)
        rewriteExpr(item);
}

static void postParseCleanup(MethodInfo *MI)
{
    rewriteExpr(MI->guard);
    for (auto item: MI->storeList) {
        rewriteExpr(item->dest);
        rewriteExpr(item->value);
        rewriteExpr(item->cond);
    }
    for (auto item: MI->letList) {
        rewriteExpr(item->dest);
        rewriteExpr(item->value);
        rewriteExpr(item->cond);
    }
    for (auto item: MI->callList) {
        rewriteExpr(item->value);
        rewriteExpr(item->cond);
    }
    for (auto item: MI->printfList) {
        rewriteExpr(item->value);
        rewriteExpr(item->cond);
    }
}

static void readModuleIR(std::list<ModuleIR *> &irSeq, FILE *OStr)
{
    OStrGlobal = OStr;
    while (readLine()) {
        bool ext = checkItem("EMODULE");
        ParseCheck(ext || checkItem("MODULE"), "Module header missing");
        std::string name = getToken();
        ModuleIR *IR = allocIR(name);
        if (!ext)
            irSeq.push_back(IR);
        ParseCheck(checkItem("{"), "Module '{' missing");
        while (readLine() && !checkItem("}")) {
            if (checkItem("SOFTWARE")) {
                IR->softwareName.push_back(getToken());
            }
            else if (checkItem("PRIORITY")) {
                std::string rule = getToken();
                IR->priority[rule] = getToken();
            }
            else if (checkItem("INTERFACECONNECT")) {
                std::string target = cleanInterface(getToken());
                std::string source = cleanInterface(getToken());
                std::string type = getToken();
                IR->interfaceConnect.push_back(InterfaceConnectType{target, source, type});
            }
            else if (checkItem("UNION")) {
                std::string type = getToken();
                IR->unionList.push_back(UnionItem{getToken(), type});
            }
            else if (checkItem("FIELD")) {
                int64_t     vecCount = -1;
                bool        isPtr = checkItem("/Ptr");
                bool        isInput = checkItem("/input");
                bool        isOutput = checkItem("/output");
                bool        isInout = checkItem("/inout");
                bool        isParameter = checkItem("/parameter");
                if (checkItem("/Count"))
                    vecCount = atoi(getToken().c_str());
                std::string type = getToken();
                std::string fldName = getToken();
                IR->fields.push_back(FieldElement{fldName, vecCount, type, isPtr, isInput, isOutput, isInout, isParameter});
            }
            else if (checkItem("PARAMS")) {
                std::string fldName = getToken();
                IR->params[fldName] = bufp;
            }
            else if (checkItem("INTERFACE")) {
                int64_t     vecCount = -1;
                bool        isPtr = checkItem("/Ptr");
                if (checkItem("/Count"))
                    vecCount = atoi(getToken().c_str());
                std::string type = getToken();
                std::string fldName = getToken();
                if (fldName == "_")
                    fldName = "";
                IR->interfaces.push_back(FieldElement{fldName, vecCount, type, isPtr, false, false, false, false});
            }
            else if (checkItem("METHOD")) {
                bool rule = false;
                if (checkItem("/Rule"))
                    rule = true;
                std::string methodName = getToken();
                MethodInfo *MI = allocMethod(methodName);
                MI->rule = rule;
                addMethod(IR, MI);
                if (checkItem("(")) {
                    bool first = true;
                    while (!checkItem(")")) {
                        if (!first)
                            ParseCheck(checkItem(","), "',' missing");
                        std::string type = getToken();
                        MI->params.push_back(ParamElement{getToken(), type});
                        first = false;
                    }
                }
                bool foundOpenBrace = checkItem("{");
                bool foundIf = false;
                if (!foundOpenBrace) {
                    foundIf = checkItem("if");
                    if (!foundIf)
                        MI->type = getToken();
                }
                if (checkItem("="))
                    MI->guard = getExpression();
                std::string rdyName = getRdyName(methodName);
                MethodInfo *MIRdy = allocMethod(rdyName);
                addMethod(IR, MIRdy);
                MIRdy->rule = MI->rule;
                MIRdy->type = "INTEGER_1";
                if (foundIf || (!foundOpenBrace && checkItem("if"))) {
                    MIRdy->guard = getExpression();
                    std::string v = MIRdy->guard->value;
                    if (v != "1" && v != "==" && v != "!=" && v != "&" && v != "|")
                        MIRdy->guard = allocExpr("!=", allocExpr("0"), MIRdy->guard);
                }
                else
                    MIRdy->guard = allocExpr("1");
                if (foundOpenBrace || checkItem("{")) {
                    while (readLine() && !checkItem("}")) {
                        if (checkItem("ALLOCA")) {
                            std::string type = getToken();
                            std::string name = getToken();
                            MI->alloca[name] = type;
                        }
                        else if (checkItem("STORE")) {
                            ACCExpr *cond = getExpression();
                            ParseCheck(checkItem(":"), "':' missing");
                            ACCExpr *dest = getExpression();
                            ParseCheck(checkItem("="), "store = missing");
                            ACCExpr *expr = str2tree(bufp);
                            MI->storeList.push_back(new StoreListElement{dest, expr, cond});
                        }
                        else if (checkItem("LET")) {
                            std::string type = getToken();
                            ACCExpr *cond = getExpression();
                            ParseCheck(checkItem(":"), "':' missing");
                            ACCExpr *dest = getExpression();
                            ParseCheck(checkItem("="), "store = missing");
                            ACCExpr *expr = str2tree(bufp);
                            MI->letList.push_back(new LetListElement{dest, expr, cond, type});
                        }
                        else if (checkItem("CALL")) {
                            bool isAction = checkItem("/Action");
                            ACCExpr *cond = getExpression();
                            ParseCheck(checkItem(":"), "':' missing");
                            ACCExpr *expr = str2tree(bufp);
                            MI->callList.push_back(new CallListElement{expr, cond, isAction});
                        }
                        else if (checkItem("PRINTF")) {
                            ACCExpr *cond = getExpression();
                            ParseCheck(checkItem(":"), "':' missing");
                            ACCExpr *expr = str2tree(bufp);
                            MI->printfList.push_back(new CallListElement{expr, cond, false});
                        }
                        else
                            ParseCheck(false, "unknown method item");
                    }
                }
                postParseCleanup(MI);
            }
            else
                ParseCheck(false, "unknown module item");
        }
    }
}

void readIR(std::list<ModuleIR *> &irSeq, std::string OutputDir)
{
    FILE *OStrIRread = fopen((OutputDir + ".IR").c_str(), "r");
    if (!OStrIRread) {
        printf("veriloggen: unable to open '%s'\n", (OutputDir + ".IR").c_str());
        exit(-1);
    }
    readModuleIR(irSeq, OStrIRread);
    fclose(OStrIRread);
}
