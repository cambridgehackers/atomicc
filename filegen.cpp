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
    if (syncPins.find(arg) != syncPins.end())
        return syncPins[arg].name;
    return arg;
}

ACCExpr *replacePins(ACCExpr *expr)
{
    if (expr) {
        if (syncPins.find(expr->value) != syncPins.end())
            expr->value = finishString(expr->value);
        for (auto operand: expr->operands)
            replacePins(operand);
    }
    return expr;
}

std::string finishExpr(ACCExpr *expr)
{
    return tree2str(cleanupExprBuiltin(replacePins(expr), "0", true));
}

std::string stripModuleParam(std::string value)
{
    int ind = value.find("(");
    if (ind > 0)
        return value.substr(0, ind);
    return value;
}
void generateModuleHeader(FILE *OStr, std::list<ModData> &modLine)
{
    std::string sep;
    bool handleCLK = false;
    bool paramSeen = false;
    for (auto mitem: modLine) {
        static const char *dirStr[] = {"input wire", "output wire"};
        if (mitem.moduleStart) {
            fprintf(OStr, "module %s ", mitem.value.c_str());
            handleCLK = !mitem.noDefaultClock;
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
                std::string vtype = stripModuleParam(mitem.type);
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
    fprintf(OStr, ");\n");
}

static void genAssign(FILE *OStr, std::string target, ACCExpr *source)
{
    if (!refList[target].done) {
        std::string tstr = finishString(target);
        std::string sstr = finishExpr(source);
        if (tstr != sstr)
            fprintf(OStr, "    assign %s = %s;\n", tstr.c_str(), sstr.c_str());
    }
    refList[target].done = true; // mark that assigns have already been output
}

void generateVerilogOutput(FILE *OStr)
{
    std::list<std::string> resetList;
    // generate local state element declarations and wires
    for (auto item: refList) {
        if (trace_assign)
            printf("[%s:%d] ref %s pin %d count %d done %d out %d inout %d type %s vecCount %s\n", __FUNCTION__, __LINE__, item.first.c_str(), item.second.pin, item.second.count, item.second.done, item.second.out, item.second.inout, item.second.type.c_str(), item.second.vecCount.c_str());
        if (item.second.pin == PIN_REG) {
            // HACK HACK HACK HACK
            if (item.second.vecCount == "")
                item.second.vecCount = convertType(item.second.type, 2);
            std::string vecCountStr = " [" + item.second.vecCount + " - 1:0]";
            if (item.second.vecCount == "") {
                vecCountStr = "";
                resetList.push_back(item.first);
            }
            vecCountStr = item.first + vecCountStr;
            if (auto IR = lookupIR(item.second.type)) {
                fprintf(OStr, "    %s %s;\n", genericModuleParam(item.second.type).c_str(), vecCountStr.c_str());
            }
            else if (auto IR = lookupInterface(item.second.type)) {
                fprintf(OStr, "    %s %s();\n", genericModuleParam(item.second.type).c_str(), vecCountStr.c_str());
            }
            else
            fprintf(OStr, "    reg %s;\n", (sizeProcess(item.second.type) + vecCountStr).c_str());
        }
    }
    for (auto item: refList) {
        std::string temp = item.first;
        if (item.second.pin == PIN_WIRE || item.second.pin == PIN_OBJECT || item.second.pin == PIN_LOCAL) {
        if (item.second.count && !item.second.isGenerated) {
            if (item.second.vecCount == "")
                item.second.vecCount = convertType(item.second.type, 2);
            std::string vecCountStr = " [" + item.second.vecCount + " - 1:0]";
            if (item.second.vecCount == "")
                vecCountStr = "";
            vecCountStr = item.first + vecCountStr;
            if (auto IR = lookupIR(item.second.type)) {
                fprintf(OStr, "    %s %s;\n", genericModuleParam(item.second.type).c_str(), vecCountStr.c_str());
            }
            else if (auto IR = lookupInterface(item.second.type)) {
                fprintf(OStr, "    %s %s();\n", genericModuleParam(item.second.type).c_str(), vecCountStr.c_str());
            }
            else
            fprintf(OStr, "    wire %s;\n", (sizeProcess(item.second.type) + vecCountStr).c_str());
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
    if (genvarMap.size() > 0) {
        const char *sep = "    genvar ";
        for (auto gitem: genvarMap) {
            fprintf(OStr, "%s%s", sep, gitem.first.c_str());
            sep = ", ";
        }
        fprintf(OStr, ";\n");
    }
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
            if (!mitem.noDefaultClock)
                tempOutput.push_back(".CLK(CLK), .nRST(nRST),");
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
        if ((item.second.out || item.second.inout) && (item.second.pin == PIN_MODULE || item.second.pin == PIN_OBJECT || item.second.pin == PIN_LOCAL)) {
            if (!assignList[item.first].value)
                for (auto alitem: assignList)
                    if (ACCExpr *val = alitem.second.value)
                    if (isIdChar(val->value[0]) && val->value == item.first)
                        goto next;
            if (!refList[temp].done && refList[temp].count) {
                if (assignList[item.first].value)
                    genAssign(OStr, item.first, assignList[item.first].value);
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
        if (item.second.value && (refList[temp].count || !refList[temp].pin) && !refList[item.first].done) {
            if (!seen)
                fprintf(OStr, "    // Extra assigments, not to output wires\n");
            seen = true;
            genAssign(OStr, item.first, item.second.value);
        }
    }
    for (auto ctop: condAssignList) {
        fprintf(OStr, "%s\n", finishString(ctop.first).c_str());
        for (auto item: ctop.second) {
            genAssign(OStr, item.first, item.second.value);
        }
        fprintf(OStr, "    end;\n");
    }

    // generate clocked updates to state elements
    auto ctop = condLines.begin(), ctopEnd = condLines.end();
    for (; ctop != ctopEnd || resetList.size(); ctop++) { // process all generate sections
    if (resetList.size() || ctop->second.always.size()) {
        std::string ctopLoop = ctop != ctopEnd ? ctop->first : "";
        if (ctopLoop != "")
            fprintf(OStr, "\n    %s\n", finishString(ctopLoop).c_str());
        fprintf(OStr, "\n    always @( posedge CLK) begin\n      if (!nRST) begin\n");
        for (auto item: resetList)
            fprintf(OStr, "        %s <= 0;\n", item.c_str());
        resetList.clear();
        fprintf(OStr, "      end // nRST\n");
        if (ctop != ctopEnd && ctop->second.always.size() > 0) {
            fprintf(OStr, "      else begin\n");
            std::list<std::string> alwaysLines;
            for (auto tcond: ctop->second.always) {
                std::string methodName = tcond.first, endStr;
                if (checkInteger(tcond.second.guard, "1"))
                    alwaysLines.push_back("// " + methodName);
                else {
                    alwaysLines.push_back("if (" + finishExpr(tcond.second.guard) + ") begin // " + methodName);
                    endStr = "end; ";
                }
                for (auto item: tcond.second.info) {
                    std::string endStr;
                    std::string temp;
                    if (item.first && !checkInteger(item.first, "1")) {
                        temp = "    if (" + finishExpr(item.first) + ")";
                        if (item.second.size() > 1) {
                            temp += " begin";
                            endStr = "    end;";
                        }
                        alwaysLines.push_back(temp);
                    }
                    for (auto citem: item.second) {
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
            fprintf(OStr, "      end\n");
        }
        fprintf(OStr, "    end // always @ (posedge CLK)\n");
        if (ctopLoop != "")
            fprintf(OStr, "   end // end of forloop\n");
        if (ctop == ctopEnd)
            break;
    }
    if (ctop->second.assert.size())
        fprintf(OStr, "`ifdef	FORMAL\n");
    for (auto item: ctop->second.assert)
        fprintf(OStr, "    %s\n", item.c_str());
    if (ctop->second.assert.size())
        fprintf(OStr, "`endif\n");
    }
}
