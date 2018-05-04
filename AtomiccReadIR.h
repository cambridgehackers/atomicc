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

static char buf[MAX_READ_LINE];
static char *bufp;
static int lineNumber = 0;
static FILE *OStrGlobal;
static std::map<std::string, ModuleIR *> mapIndex;

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

void readModuleIR(std::list<ModuleIR *> &irSeq, FILE *OStr)
{
    OStrGlobal = OStr;
    while (readLine()) {
        bool ext = checkItem("EMODULE");
        ParseCheck(ext || checkItem("MODULE"), "Module header missing");
        ModuleIR *IR = new ModuleIR;
        if (!ext)
            irSeq.push_back(IR);
        IR->name = getToken();
        ParseCheck(checkItem("{"), "Module '{' missing");
        mapIndex[IR->name] = IR;
        while (readLine() && !checkItem("}")) {
            if (checkItem("SOFTWARE")) {
                IR->softwareName.push_back(getToken());
            }
            else if (checkItem("PRIORITY")) {
                std::string rule = getToken();
                IR->priority[rule] = getToken();
            }
            else if (checkItem("INTERFACECONNECT")) {
                std::string target = getToken();
                std::string source = getToken();
                std::string type = getToken();
                IR->interfaceConnect.push_back(InterfaceConnectType{target, source, type});
            }
            else if (checkItem("UNION")) {
                std::string type = getToken();
                IR->unionList.push_back(UnionItem{getToken(), type});
            }
            else if (checkItem("FIELD")) {
                int64_t     vecCount = -1;
                unsigned    arrayLen = 0;
                bool        isPtr = checkItem("/Ptr");
                if (checkItem("/Count"))
                    vecCount = atoi(getToken().c_str());
                if (checkItem("/Array"))
                    arrayLen = atoi(getToken().c_str());
                std::string type = getToken();
                std::string fldName = getToken();
                IR->fields.push_back(FieldElement{fldName, vecCount, type, arrayLen, isPtr});
            }
            else if (checkItem("INTERFACE")) {
                int64_t     vecCount = -1;
                unsigned    arrayLen = 0;
                bool        isPtr = checkItem("/Ptr");
                if (checkItem("/Count"))
                    vecCount = atoi(getToken().c_str());
                if (checkItem("/Array"))
                    arrayLen = atoi(getToken().c_str());
                std::string type = getToken();
                std::string fldName = getToken();
                IR->interfaces.push_back(FieldElement{fldName, vecCount, type, arrayLen, isPtr});
            }
            else if (checkItem("METHOD")) {
                MethodInfo *MI = new MethodInfo{nullptr};
                if (checkItem("/Rule"))
                    MI->rule = true;
                std::string methodName = getToken();
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
                IR->method[methodName] = MI;
                MethodInfo *MIRdy = new MethodInfo{nullptr};
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
                IR->method[getRdyName(methodName)] = MIRdy;
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
            }
            else
                ParseCheck(false, "unknown module item");
        }
    }
}
