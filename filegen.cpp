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
#include "AtomiccIR.h"
#include "common.h"

std::map<std::string, int> genvarMap;

std::string finishString(std::string arg)
{
    auto item = syncPins.find(arg);
    if (item != syncPins.end())
        if (!item->second.out)
            return item->second.name;
    return arg;
}

ACCExpr *replacePins(ACCExpr *expr)
{
    if (!expr)
        return expr;
    normalizeIdentifier(expr);
    expr->value = finishString(expr->value);
    for (auto operand: expr->operands)
        replacePins(operand);
    return expr;
}

std::string finishExpr(ACCExpr *expr)
{
    return tree2str(cleanupExprBuiltin(replacePins(expr), "0", true));
}

void generateModuleHeader(FILE *OStr, ModList &modLine, bool isTopModule)
{
    std::string sep;
    bool handleCLK = false;
    bool paramSeen = false;
    for (auto mitem: modLine) {
        static const char *dirStr[] = {"input wire", "output wire"};
        if (mitem.moduleStart) {
            fprintf(OStr, "module %s ", mitem.value.c_str());
            handleCLK = mitem.clockValue != "";
            if (handleCLK)
                sep = "(";
            else
                sep = "(\n    ";
        }
        else if (mitem.isparam != "") {
            if (!paramSeen) {
                fprintf(OStr, "#(\n    ");
                sep = "";
                paramSeen = true;
            }
            std::string typ = "UNKNOWN_PARAM_TYPE[" + mitem.type + "] ";
            if (mitem.type == "FLOAT") {
                typ = "real ";
            }
            else if (mitem.type == "POINTER")
                typ = "";
            else if (startswith(mitem.type, "Bit(")) {
                typ = "integer ";
            }
            fprintf(OStr, "%sparameter %s%s = %s", sep.c_str(), typ.c_str(), mitem.value.c_str(), mitem.isparam.c_str());
            sep = ",\n    ";
        }
        else {
            if (paramSeen) {
                fprintf(OStr, ")(\n    ");
                sep = "";
                paramSeen = false;
            }
            fprintf(OStr, "%s", sep.c_str());
            sep = ",\n    ";
            if (handleCLK) {
                fprintf(OStr, "input wire CLK, input wire nRST,\n    ");
                handleCLK = false;
            }
            std::string dirs = dirStr[mitem.out];
            if (mitem.inout)
                dirs = "inout wire";
            if (mitem.vecCount == "")
                mitem.vecCount = convertType(mitem.type, 2);
            std::string array;
            if (mitem.vecCount != "")
                array = "[" + mitem.vecCount + " - 1:0]";
            ModuleIR *IIR = lookupInterface(mitem.type);
            if (IIR) {
                std::string vtype = cleanupModuleType(mitem.type);   // strip off module parameters -- not allowed on formals
                int ind = vtype.find("(");
                if (ind > 0)
                    vtype = vtype.substr(0, ind);
                if (mitem.out)
                    vtype += ".client";
                else
                    vtype += ".server";
                fprintf(OStr, "%s %s%s", vtype.c_str(), mitem.value.c_str(), array.c_str());
            }
            else {
                std::string sizeStr = sizeProcess(mitem.type);
                fprintf(OStr, "%s %s%s%s", dirs.c_str(), sizeStr.c_str(), mitem.value.c_str(), array.c_str());
            }
        }
    }
    if (handleCLK)
        fprintf(OStr, "%sinput CLK, input nRST", sep.c_str());
    if (isTopModule)
        fprintf(OStr, "\n    `TopAppendPort ");
    fprintf(OStr, ");\n");
}

static void genAssign(FILE *OStr, std::string target, ACCExpr *source, std::string type)
{
    ModuleIR *IR = lookupInterface(type);
    if (IR && IR->isStruct)
        IR = nullptr;
    if (!refList[target].done && !IR && source) {
        std::string tstr = finishString(target);
        std::string sstr = finishExpr(source);
        if (tstr != sstr)
            fprintf(OStr, "    assign %s = %s;\n", tstr.c_str(), sstr.c_str());
    }
    refList[target].done = true; // mark that assigns have already been output
}

static std::string declareInstance(std::string type, std::string vecCountStr, std::string params)
{
    std::string ret;

    if (auto IR = lookupIR(type)) {
    }
    else if (auto IR = lookupInterface(type)) {
        ret = "()";
    }
    else
        return "";
    return genericModuleParam(type, params, nullptr) + " " + vecCountStr + ret;
}

static bool walkSearch (ACCExpr *expr, std::string search)
{
    if (!expr)
        return false;
    if (expr->value == search)
        return true;
    for (auto item: expr->operands)
        if (walkSearch(item, search))
            return true;
    return false;
}

void generateVerilogOutput(FILE *OStr)
{
    std::map<std::string, std::list<std::string>> resetList;
    // generate local state element declarations and wires
    refList["CLK"].done = false;
    refList["nRST"].done = false;
    for (auto item: refList) {
        if (trace_assign || trace_interface)
            printf("[%s:%d] ref %s pin %d count %d done %d out %d inout %d type %s vecCount %s\n", __FUNCTION__, __LINE__, item.first.c_str(), item.second.pin, item.second.count, item.second.done, item.second.out, item.second.inout, item.second.type.c_str(), item.second.vecCount.c_str());
        if (item.second.pin == PIN_REG) {
            // HACK HACK HACK HACK
            if (item.second.vecCount == "")
                item.second.vecCount = convertType(item.second.type, 2);
            std::string vecCountStr = " [" + item.second.vecCount + " - 1:0]";
            if (item.second.vecCount == "") {
                vecCountStr = "";
                std::string alwaysName = ALWAYS_CLOCKED + item.second.clockName + ")";
printf("[%s:%d] RESEEEEEEEEEEEEEEEEEEEEEEE %s clock %s\n", __FUNCTION__, __LINE__, item.first.c_str(), alwaysName.c_str());
                resetList[alwaysName].push_back(item.first);
                (void) condLines[""].always[alwaysName].cond[""].guard;  // dummy to ensure 'always' generation loop is executed at least once
            }
            vecCountStr = item.first + vecCountStr;
            std::string inst = declareInstance(item.second.type, vecCountStr, ""); //std::string params; //IR->params[item.fldName]
            if (inst != "") {
                fprintf(OStr, "    %s;\n", inst.c_str());
            }
            else
            fprintf(OStr, "    reg %s;\n", (sizeProcess(item.second.type) + vecCountStr).c_str());
        }
    }
    for (auto item: refList) {
        std::string temp = item.first;
        if (item.second.pin == PIN_WIRE || item.second.pin == PIN_OBJECT) {
        if (item.second.count) {
            if (item.second.vecCount == "")
                item.second.vecCount = convertType(item.second.type, 2);
            std::string vecCountStr = " [" + item.second.vecCount + " - 1:0]";
            if (item.second.vecCount == "")
                vecCountStr = "";
            vecCountStr = item.first + vecCountStr;
            std::string inst = declareInstance(item.second.type, vecCountStr, "");
            if (inst != "") {
                fprintf(OStr, "    %s;\n", inst.c_str());
            }
            else
            fprintf(OStr, "    logic %s;\n", (sizeProcess(item.second.type) + vecCountStr).c_str());
if (trace_assign && item.second.out) {
printf("[%s:%d] JJJJ outputwire %s\n", __FUNCTION__, __LINE__, item.first.c_str());
//exit(-1);
}
        }
        else if (trace_assign)
            printf("[%s:%d] WIRENOTNEEDED %s\n", __FUNCTION__, __LINE__, item.first.c_str());
        }
    }
    std::string endStr, sep;
    std::list<std::string> tempOutput;
    auto flushOut = [&](void) -> void {
        for (auto item: tempOutput)
            fprintf(OStr, "%s", item.c_str());
        fprintf(OStr, "%s", endStr.c_str());
        tempOutput.clear();
    };
    bool moduleSyncFF = false;
    for (auto mitem: modNew) {
        if (mitem.moduleStart) {
            flushOut();
            moduleSyncFF = (mitem.value == "SyncFF");  // do not perform cleanupExpr on this instantiation (will replace syncPins)
            if (mitem.vecCount == "")
                mitem.vecCount = convertType(mitem.type, 2);
            std::string instName = mitem.argName;
            // HACK HACK HACK HACK
            std::string vecCountStr;
            if (mitem.vecCount != "")
                vecCountStr = " [" + mitem.vecCount + " - 1:0]";
            tempOutput.push_back("    " + mitem.value + " " + instName + vecCountStr + " (");
            if (mitem.clockValue != "") {
                std::string clockName = mitem.clockValue, resetName;
                int ind = clockName.find(":");
                resetName = clockName.substr(ind+1);
                clockName = clockName.substr(0,ind);
                tempOutput.push_back(".CLK(" + clockName + "), .nRST(" + resetName + "),");
            }
            sep = "";
        }
        else {
            std::string val = moduleSyncFF ? mitem.value : finishExpr(cleanupExpr(str2tree(mitem.value)));
            tempOutput.push_back(sep + "\n        ." + mitem.argName + "(" + val + ")");
            sep = ",";
        }
        endStr = ");\n";
    }
    flushOut();

    for (auto item: refList) {
        std::string temp = item.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if ((item.second.out || item.second.inout) && (item.second.pin == PIN_MODULE || item.second.pin == PIN_OBJECT)) {
            if (!assignList[item.first].value)
                for (auto alitem: assignList)
                    if (ACCExpr *val = alitem.second.value)
                    if (isIdChar(val->value[0]) && val->value == item.first)
                        goto next;
            if (!refList[temp].done && refList[temp].count) {
                if (assignList[item.first].value)
                    genAssign(OStr, item.first, assignList[item.first].value, refList[item.first].type);
                else if (refList[temp].vecCount == "") {
                    if (!lookupInterface(refList[temp].type) && !lookupIR(refList[temp].type))
                    fprintf(OStr, "    assign %s = 0; //MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE\n", finishString(item.first).c_str());
                }
            }
            else if (trace_skipped)
                fprintf(OStr, "    //skippedassign %s = %s; //temp = '%s', count = %d, pin = %d done %d\n", finishString(item.first).c_str(), finishExpr(assignList[item.first].value).c_str(), temp.c_str(), refList[temp].count, item.second.pin, refList[temp].done);
next:;
            refList[item.first].done = true; // mark that assigns have already been output
        }
    }
    bool seen = false;
    for (auto item: assignList) {
        std::string temp = item.first;
        int ind = temp.find('[');
        if (ind != -1)
            temp = temp.substr(0,ind);
        if (trace_assign)
            printf("[%s:%d] assign %s value %s count %d pin %d done %d\n", __FUNCTION__, __LINE__, item.first.c_str(), tree2str(item.second.value).c_str(), refList[temp].count, refList[temp].pin, refList[item.first].done);
        if (lookupInterface(refList[temp].type) == nullptr)
        if (item.second.value && (refList[temp].count || !refList[temp].pin) && !refList[item.first].done) {
            if (!seen)
                fprintf(OStr, "    // Extra assigments, not to output wires\n");
            seen = true;
            genAssign(OStr, item.first, item.second.value, refList[temp].type);
        }
    }
    for (auto ctop: condAssignList) {
        fprintf(OStr, "%s\n", finishString(ctop.first).c_str());
        for (auto item: ctop.second) {
            genAssign(OStr, item.first, item.second.value, item.second.type);
        }
        fprintf(OStr, "    end;\n");
    }

    // combine mux'ed assignments into a single 'assign' statement
    for (auto &top: muxValueList) {
        bool endFlag = false;
        for (auto &item: top.second) {
            if (top.first != "" && item.second.phi) {
                fprintf(OStr, "%s\n", finishString(top.first).c_str());
                endFlag = true;
            }
            refList[item.first].done = false;
            if (item.second.phi) {
#if 0
            genAssign(OStr, item.first, item.second.phi, refList[item.first].type);
#else
            fprintf(OStr, "    always_comb begin\n    %s = 0;\n    unique case(1'b1)\n", item.first.c_str());
            for (auto param: item.second.phi->operands.front()->operands)
                fprintf(OStr, "    %s: %s = %s;\n", tree2str(getRHS(param, 0)).c_str(), item.first.c_str(), tree2str(getRHS(param, 1)).c_str());
            fprintf(OStr, "    default:;\n"); // not all cases needed
            fprintf(OStr, "    endcase\n    end\n");
#endif
            }
        }
        if (endFlag)
            fprintf(OStr, "    end;\n");
    }

    // generate clocked updates to state elements
    std::list<std::string> initItemList;
    for (auto &ctop : condLines) { // process all generate sections
    for (auto &alwaysGroup: ctop.second.always) {
        std::list<std::string> alwaysLines;
        bool hasElse = false;
        if (ctop.first != "")
            fprintf(OStr, "\n    %s\n", finishString(ctop.first).c_str()); // generate 'for' line
        std::string alwaysClause = alwaysGroup.first, resetName;
        int ind = alwaysClause.find(":");
        if (ind > 0) {
            resetName = alwaysClause.substr(0, alwaysClause.length()-1).substr(ind+1);
            alwaysClause = alwaysClause.substr(0,ind) + ")";
        }
        fprintf(OStr, "\n    %s begin\n      if (!%s) begin\n", alwaysClause.c_str(), resetName.c_str());
printf("[%s:%d]REEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEWWWWWWWWWWWW %s count %d\n", __FUNCTION__, __LINE__, alwaysGroup.first.c_str(), (int)resetList[alwaysGroup.first].size());
        for (auto item: resetList[alwaysGroup.first]) {
            ACCExpr *zero = allocExpr("0");
            updateWidth(zero, convertType(refList[item].type));
            std::string initAssign = item + " <= " + tree2str(zero);
            fprintf(OStr, "        %s;\n", initAssign.c_str());
            initItemList.push_back(initAssign);
        }
        resetList[alwaysGroup.first].clear();
        fprintf(OStr, "      end // nRST\n");
        for (auto tcond: alwaysGroup.second.cond) {
            std::string methodName = tcond.first, endStr;
            if (!tcond.second.guard)
                continue;
            if (!hasElse)
                fprintf(OStr, "      else begin\n");
            hasElse = true;
            if (checkInteger(tcond.second.guard, "1"))
                alwaysLines.push_back("// " + methodName);
            else {
                alwaysLines.push_back("if (" + finishExpr(tcond.second.guard) + ") begin // " + methodName);
                endStr = "end; ";
            }
            for (auto item: tcond.second.info) {
                std::string endStr;
                std::string temp;
                if (item.second.cond && !checkInteger(item.second.cond, "1")) {
                    temp = "    if (" + finishExpr(item.second.cond) + ")";
                    if (item.second.info.size() > 1) {
                        temp += " begin";
                        endStr = "    end;";
                    }
                    alwaysLines.push_back(temp);
                }
                for (auto citem: item.second.info) {
                    if (citem.dest) // dest non-null -> assignment statement, otherwise call statement
                        alwaysLines.push_back("    " + finishExpr(citem.dest) + " <= " + finishExpr(citem.value) + ";");
                    else if (citem.value->value == "$finish;")
                        alwaysLines.push_back("    $finish;");
                    else
                        alwaysLines.push_back("    $display" + finishExpr(citem.value->operands.front()) + ";");
                }
                if (endStr != "")
                    alwaysLines.push_back(endStr);
            }
            alwaysLines.push_back(endStr + "// End of " + methodName);
        }
        for (auto info: alwaysLines)
            fprintf(OStr, "        %s\n", info.c_str());
        if (hasElse)
            fprintf(OStr, "      end\n");
        fprintf(OStr, "    end // always @ (posedge CLK)\n");
        if (ctop.first != "")
            fprintf(OStr, "   end // end of forloop\n");
    }
    if (ctop.second.assertList.size()) {
        fprintf(OStr, "`ifdef	FORMAL\n");
        if (initItemList.size()) {
            fprintf(OStr, "    initial begin\n");
            for (auto item: initItemList)
                fprintf(OStr, "        %s;\n", item.c_str());
            fprintf(OStr, "    end\n");
        }
    }
    for (auto item: ctop.second.assertList) {
        std::string sensitivity = ALWAYS_STAR;
        if (walkSearch(item.cond, "$past") || walkSearch(item.value, "$past"))
            sensitivity = ALWAYS_CLOCKED "CLK)";
        fprintf(OStr, "    %s\n", sensitivity.c_str());
        std::string indent;
        std::string condStr = finishExpr(item.cond);
        if (condStr != "" && condStr != "1") {
            fprintf(OStr, "    %s\n", ("    if (" + condStr + ")").c_str());
            indent = "    ";
        }
        fprintf(OStr, "    %s\n", ("    " + indent + finishExpr(item.value) + ";").c_str());
    }
    if (ctop.second.assertList.size())
        fprintf(OStr, "`endif\n");
    }
}
