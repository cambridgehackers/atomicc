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

std::string myGlobalName;       // imported from main.cpp
static int trace_readIR;//= 1;
static char buf[MAX_READ_LINE];
static const char *bufp;
static int lineNumber = 0;
static FILE *OStrGlobal;

static std::string getExpressionString(char terminator = 0)
{
    while (*bufp == ' ')
        bufp++;
    std::string ret = getBoundedString(&bufp, terminator);
    if (trace_readIR)
        printf("[%s:%d] '%s'\n", __FUNCTION__, __LINE__, ret.c_str());
    return ret;
}

static std::string changeSeparator(std::string value)
{
    int ind = value.rfind(DOLLAR);
    if (value.length() && isIdChar(value[0]) && ind > 0
     && !startswith(value, "__inst$Genvar")
     && !startswith(value, "FOR$")
     && !startswith(value, "RULE$"))
        value = value.substr(0, ind) + PERIOD + value.substr(ind+1);
    return value;
}

static ACCExpr *inputExpression(std::string inStr)
{
    ACCExpr *expr = str2tree(inStr);
    walkFixup(expr);
    return expr;
}

static ACCExpr *getExpression(char terminator = 0)
{
    std::string ret = getExpressionString(terminator);
    while (*bufp == ' ')
        bufp++;
    return inputExpression(ret);
}

static bool checkItem(const char *val)
{
    while (*bufp == ' ')
        bufp++;
    int len = strlen(val);
    if (trace_readIR)
        printf("[%s:%d] val %s len %d bufp '%s'\n", __FUNCTION__, __LINE__, val, len, bufp);
    bool ret = !strncmp(bufp, val, len);
    if (ret)
        bufp += len;
    while (*bufp == ' ')
        bufp++;
    if (trace_readIR)
        printf("[%s:%d] val %s ret %d\n", __FUNCTION__, __LINE__, val, ret);
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
    while (1) {
        if (fgets(buf, sizeof(buf), OStrGlobal) == NULL) {
            if (trace_readIR)
                printf("[%s:%d] end of input file.  feof %d ferror %d\n", __FUNCTION__, __LINE__, feof(OStrGlobal), ferror(OStrGlobal));
            return false;
        }
        if (strlen(buf) >= sizeof(buf) - 1) {
            printf("%s: error: IR maximum file line length exceeded (%ld)\n", __FUNCTION__, strlen(buf));
            exit(-1);
        }
        do 
            buf[strlen(buf) - 1] = 0; // strip trailing ' '
        while (buf[strlen(buf) - 1] == ' ');
        bufp = buf;
        lineNumber++;
        if (buf[0])                   // skip blank lines
            break;
    }
    if (trace_readIR)
        printf("[%s:%d] inline '%s'\n", __FUNCTION__, __LINE__, bufp);
    return true;
}
static std::string getToken()
{
    const char *startp = bufp;
    while (*bufp == ' ')
        bufp++;
    while (*bufp && *bufp != ' ')
        bufp++;
    std::string ret = std::string(startp, bufp);
    while (*bufp == ' ')
        bufp++;
    if (trace_readIR)
        printf("[%s:%d] '%s'\n", __FUNCTION__, __LINE__, ret.c_str());
    return ret;
}

static std::string specializeTypename(std::string ret)
{
    //int ind = ret.find("_OC_"); // look for 'compilation specific' template instantiations
    //if (ind > 0 && myGlobalName != "")
        //ret = ret.substr(0, ind+1) + myGlobalName + ret.substr(ind+3);
    return ret;
}

static std::string getType()
{
    const char *startp = bufp;
    while (*bufp == ' ')
        bufp++;
    while (*bufp && *bufp != ' ' && *bufp != '(')
        bufp++;
    std::string ret = std::string(startp, bufp);
    if (*bufp == '(')
        ret += getExpressionString();
    while (*bufp == ' ')
        bufp++;
    if (trace_readIR)
        printf("[%s:%d] '%s'\n", __FUNCTION__, __LINE__, ret.c_str());
    return specializeTypename(ret);
}

static ACCExpr *cleanInterface(ACCExpr *expr)
{
    expr->value = replacePeriod(expr->value);
    std::string name = expr->value;
    if (name == "_")
        name = "";
    else if (endswith(name, "$_"))
        name = name.substr(0, name.length()-1);
    expr->value = name;
    return expr;
}

static void walkRemoveParameterMarker (ACCExpr *expr)
{
    if (!expr)
        return;
    if (expr->value == "{")
        expr->value = "(";   // change from PARAMETER_MARKER
    for (auto item: expr->operands)
        walkRemoveParameterMarker(item);
}

static void readMethodInfo(ModuleIR *IR, MethodInfo *MI, MethodInfo *MIRdy)
{
    if (checkItem("(")) {
        bool first = true;
        while (!checkItem(")")) {
            if (!first)
                ParseCheck(checkItem(","), "',' missing");
            std::string type = getType();
            MI->params.push_back(ParamElement{getToken(), type, ""});
            first = false;
        }
    }
    bool foundOpenBrace = checkItem("{");
    bool foundIf = false;
    if (!foundOpenBrace) {
        foundIf = checkItem("if");
        if (!foundIf)
            MI->type = getType();
    }
    if (checkItem("="))
        MI->guard = getExpression();
    if (MIRdy) {
    if (foundIf || (!foundOpenBrace && checkItem("if"))) {
        MIRdy->guard = getExpression();
        std::string v = MIRdy->guard->value;
        if (v != "1" && v != "==" && v != "!=" && v != "&" && v != "&&" && v != "|" && v != "||")
            MIRdy->guard = allocExpr("!=", allocExpr("0"), MIRdy->guard);
    }
    else
        MIRdy->guard = allocExpr("1");
    }
    if (trace_readIR)
        printf("[%s:%d] foundOpenBrace %d guard %s remainingbuf %s\n", __FUNCTION__, __LINE__, foundOpenBrace, tree2str(MI->guard).c_str(), bufp);
    if (foundOpenBrace || checkItem("{")) {
        while (readLine() && !checkItem("}")) {
            if (checkItem("ALLOCA")) {
                std::string type = getType();
                std::string name = getToken();
                MI->alloca[name] = AllocaItem{type, false};
            }
            else if (checkItem("STORE")) {
                ACCExpr *cond = getExpression(':');
                ParseCheck(checkItem(":"), "':' missing");
                ACCExpr *dest = getExpression('=');
                ParseCheck(checkItem("="), "store = missing");
                ACCExpr *expr = inputExpression(bufp);
                MI->storeList.push_back(new StoreListElement{dest, expr, cond});
            }
            else if (checkItem("LET")) {
                std::string type = getType();
                ACCExpr *cond = getExpression(':');
                ParseCheck(checkItem(":"), "':' missing");
                ACCExpr *dest = getExpression('=');
                ParseCheck(checkItem("="), "store = missing");
                ACCExpr *expr = inputExpression(bufp);
                MI->letList.push_back(new LetListElement{dest, expr, cond, type});
            }
            else if (checkItem("ASSERT")) {
                ACCExpr *cond = getExpression(':');
                ParseCheck(checkItem(":"), "':' missing");
                ACCExpr *expr = inputExpression(bufp);
                walkRemoveParameterMarker (cond);
                MI->assertList.push_back(new AssertListElement{expr, cond});
            }
            else if (checkItem("CALL")) {
                bool isAction = checkItem("/Action");
                ACCExpr *cond = getExpression(':');
                ParseCheck(checkItem(":"), "':' missing");
                ACCExpr *value = inputExpression(bufp), *subscript = nullptr, *param = nullptr;
                // TODO: make this processing more general
                value->value = replacePeriod(value->value);
                int ind = value->value.rfind(DOLLAR);
                if (ind > 0)
                    value->value = value->value.substr(0, ind) + PERIOD + value->value.substr(ind+1);
#if 0
                if (value->value == PERIOD) {     // handle qualified expr case
                    ACCExpr *newValue = value->operands.front();
                    value->operands.pop_front();
                    for (auto item: value->operands) {
                        newValue->value += DOLLAR + item->value;
                        newValue->operands.push_back(item->operands.front());
                    }
                    value = newValue;
                }
#endif
                for (auto item: value->operands) {
                     if (item->value == "[")
                         subscript = item;
                     else if (item->value == "{")
                         param = item;
                     else if (isIdChar(item->value[0]) && item->operands.size() == 0)
                         value->value += DOLLAR + item->value;
                     else {
                         printf("%s: ERROR: unknown member of call expression\n", __FUNCTION__);
                         dumpExpr("ITEM", item);
                         exit(-1);
                     }
                }
                value->operands.clear();
                if (subscript)
                    value->operands.push_back(subscript);
                if (param)
                    value->operands.push_back(param);
                MI->callList.push_back(new CallListElement{value, cond, isAction});
            }
            else if (checkItem("PRINTF")) {
                ACCExpr *cond = getExpression(':');
                ParseCheck(checkItem(":"), "':' missing");
                ACCExpr *expr = inputExpression(bufp);
                MI->printfList.push_back(new CallListElement{expr, cond, false});
            }
            else if (checkItem("GENERATE")) {
                ACCExpr *cond = getExpression(':');
                ParseCheck(checkItem(":"), "':' missing");
                ACCExpr *var = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                ACCExpr *init = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                ACCExpr *term = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                ACCExpr *incr = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                std::string body = bufp;
                MI->generateFor.push_back(GenerateForItem{cond, var->value, init, term, incr, baseMethodName(body)});
                IR->genvarCount = 1;
            }
            else if (checkItem("INSTANTIATE")) {
                ACCExpr *cond = getExpression(':');
                ParseCheck(checkItem(":"), "':' missing");
                ACCExpr *var = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                ACCExpr *init = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                ACCExpr *term = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                ACCExpr *incr = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                ACCExpr *sub = getExpression(',');
                ParseCheck(checkItem(","), "generate ',' missing");
                std::string body = bufp;
                MI->instantiateFor.push_back(InstantiateForItem{cond, var->value, init, term, incr, sub, baseMethodName(body)});
                IR->genvarCount = 1;
            }
            else if (checkItem("INTERFACECONNECT")) {
                bool        isForward = checkItem("/Forward");
                ACCExpr *target = cleanInterface(getExpression(' '));
                ACCExpr *source = cleanInterface(getExpression(' '));
                std::string type = getType();
                MI->interfaceConnect.push_back(InterfaceConnectType{target, source, type, isForward});
            }
            else
                ParseCheck(false, "unknown method item");
        }
    }
}

static void readModuleIR(std::list<ModuleIR *> &irSeq, std::list<std::string> &fileList, FILE *OStr)
{
    OStrGlobal = OStr;
    while (readLine()) {
        if (trace_readIR)
            printf("[%s:%d] BEFOREMOD remain '%s'\n", __FUNCTION__, __LINE__, bufp);
        bool ext = checkItem("EMODULE"), interface = false, isStruct = false, isSerialize = false;
        if (!ext)
            interface = checkItem("INTERFACE");
        if (!ext && !interface)
            isStruct = checkItem("STRUCT");
        if (!ext && !interface && !isStruct)
            isSerialize = checkItem("SERIALIZE");
        if (!ext && !interface && !isStruct && !isSerialize) {
            if (checkItem("FILE")) {
                while (*bufp == ' ')
                    bufp++;
                fileList.push_back(bufp);
                continue;
            }
        }
        ParseCheck(ext || interface || isStruct || isSerialize || checkItem("MODULE"), "Module header missing");
        std::string name = specializeTypename(getToken());
        ModuleIR *IR = allocIR(name, interface);
        IR->isExt = ext;
        IR->isInterface = interface;
        IR->isStruct = isStruct;
        IR->isSerialize = isSerialize;
        if (!ext && !interface && !isStruct)
            irSeq.push_back(IR);
        if (trace_readIR)
            printf("[%s:%d] MODULE %s ifc %d struct %d ser %d ext %d\n", __FUNCTION__, __LINE__, IR->name.c_str(), 
interface, isStruct, isSerialize, ext);
        if (!checkItem("{")) {
            IR->interfaceName = specializeTypename(getToken());
            ParseCheck(checkItem("{"), "Module '{' missing");
        }
        while (readLine() && !checkItem("}")) {
            if (checkItem("SOFTWARE")) {
                IR->softwareName.push_back(getToken());
            }
            else if (checkItem("PRIORITY")) {
                std::string rule = getToken();
                IR->priority[rule] = getToken();
            }
            else if (checkItem("INTERFACECONNECT")) {
                bool        isForward = checkItem("/Forward");
                ACCExpr *target = cleanInterface(getExpression(' '));
                ACCExpr *source = cleanInterface(getExpression(' '));
                std::string type = getType();
                IR->interfaceConnect.push_back(InterfaceConnectType{target, source, type, isForward});
            }
            else if (checkItem("UNION")) {
                std::string type = getType();
                IR->unionList.push_back(UnionItem{getToken(), type});
            }
            else if (checkItem("FIELD")) {
                std::string vecCount;
                bool        isPtr = checkItem("/Ptr");
                bool        isInput = checkItem("/input");
                bool        isOutput = checkItem("/output");
                bool        isInout = checkItem("/inout");
                bool        isParameter = checkItem("/parameter");
                bool        isShared = checkItem("/shared");
                bool        isExternal = checkItem("/external");
                if (checkItem("/Count")) {
                    vecCount = tree2str(getExpression(' '));
                    IR->genvarCount = 1;
                }
                std::string type = getType();
                std::string fldName = getToken();
                IR->fields.push_back(FieldElement{fldName, vecCount, type, isPtr, isInput, isOutput, isInout, isParameter ? " " : "", isShared, false, isExternal});
            }
            else if (checkItem("PARAMS")) {
                std::string fldName = getToken();
                IR->params[fldName] = bufp;
            }
            else if (checkItem("INTERFACE")) {
                std::string vecCount;
                bool        isPtr = checkItem("/Ptr");
                if (checkItem("/Count")) {
                    vecCount = tree2str(getExpression(' '));
                }
                std::string type = getType();
                std::string fldName = getToken();
                if (fldName == "_")
                    fldName = "";
                IR->interfaces.push_back(FieldElement{fldName, vecCount, type, isPtr, false, false, false, ""/*not param*/, false, false, false});
            }
            else if (checkItem("METHOD")) {
                bool rule = false, action = false;
                if (checkItem("/Rule"))
                    rule = true;
                if (checkItem("/Action"))
                    action = true;
                std::string methodName = changeSeparator(getToken());
                MethodInfo *MI = allocMethod(methodName), *MIRdy = nullptr;
                MI->rule = rule;
                MI->action = action;
                if (addMethod(IR, MI)) {
                    std::string rdyName = getRdyName(methodName);
                    MIRdy = allocMethod(rdyName);
                    addMethod(IR, MIRdy);
                    MIRdy->rule = MI->rule;
                    MIRdy->type = "Bit(1)";
                }
                readMethodInfo(IR, MI, MIRdy);
            }
            else
                ParseCheck(false, "unknown module item");
        }
        if (trace_readIR)
            dumpModule("finishread", IR);
    }
    if (trace_readIR)
        printf("[%s:%d] after readmoduleIR\n", __FUNCTION__, __LINE__);
}

void readIR(std::list<ModuleIR *> &irSeq, std::list<std::string> &fileList, std::string OutputDir)
{
    FILE *OStrIRread = fopen((OutputDir + ".IR").c_str(), "r");
    if (!OStrIRread) {
        printf("veriloggen: unable to open '%s'\n", (OutputDir + ".IR").c_str());
        exit(-1);
    }
    readModuleIR(irSeq, fileList, OStrIRread);
    fclose(OStrIRread);
}
