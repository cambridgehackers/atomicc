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
    return tree2str(replacePins(expr));
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
            std::string sizeStr = sizeProcess(mitem.type);
            if (mitem.vecCount != "") {
                if (sizeStr == "")
                    array = "[" + mitem.vecCount + " - 1:0]";
                else
                    sizeStr = "[" + mitem.vecCount + " * " + sizeStr.substr(1);
            }
            fprintf(OStr, "%s %s%s%s", dirs.c_str(), sizeStr.c_str(), mitem.value.c_str(), array.c_str());
        }
    }
    if (handleCLK)
        fprintf(OStr, "%sinput CLK, input nRST", sep.c_str());
    fprintf(OStr, ");\n");
}

void generateVerilogOutput(FILE *OStr)
{
    bool isGenerate = false;
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
            fprintf(OStr, "    reg %s;\n", (sizeProcess(item.second.type) + item.first + vecCountStr).c_str());
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
            fprintf(OStr, "    wire %s;\n", (sizeProcess(item.second.type) + item.first + vecCountStr).c_str());
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
        if (isGenerate)
            fprintf(OStr, "  ");
        for (auto item: tempOutput)
            fprintf(OStr, "%s", item.c_str());
        fprintf(OStr, "%s", endStr.c_str());
        if (isGenerate) {
            fprintf(OStr, "    end;\n");
        }
        tempOutput.clear();
    };
    for (auto mitem: modNew)
        if (mitem.moduleStart && mitem.vecCount != "")
            genvarMap[GENVAR_NAME + autostr(1)] = 1;
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
            isGenerate = mitem.vecCount != "";
            std::string instName = mitem.argName;
            // HACK HACK HACK HACK
            std::string vecCountStr = mitem.vecCount;
            if (isGenerate) {
                std::string g = GENVAR_NAME + autostr(1);
                fprintf(OStr, "    for(%s = 0; %s < %s; %s = %s + 1) begin : %s\n",
                    g.c_str(), g.c_str(), vecCountStr.c_str(), g.c_str(), g.c_str(), mitem.argName.c_str());
                instName = "data";
            }
            tempOutput.push_back("    " + mitem.value + " " + instName + " (");
            if (!mitem.noDefaultClock)
                tempOutput.push_back(".CLK(CLK), .nRST(nRST),");
            sep = "";
        }
        else {
            std::string val = moduleSyncFF ? mitem.value : finishExpr(cleanupExpr(str2tree(mitem.value)));
            if (isGenerate)
                fprintf(OStr, "      wire %s;\n", (sizeProcess(mitem.type) + val).c_str());
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
                    fprintf(OStr, "    assign %s = %s;\n", finishString(item.first).c_str(), finishExpr(assignList[item.first].value).c_str());
                else
                    fprintf(OStr, "    assign %s = 0; //MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE\n", finishString(item.first).c_str());
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
        if (item.second.value && refList[temp].count && !refList[item.first].done) {
            if (!seen)
                fprintf(OStr, "    // Extra assigments, not to output wires\n");
            seen = true;
            fprintf(OStr, "    assign %s = %s;\n", finishString(item.first).c_str(), finishExpr(item.second.value).c_str());
            refList[item.first].done = true; // mark that assigns have already been output
        }
    }
    for (auto ctop: condAssignList) {
        fprintf(OStr, "%s\n", finishString(ctop.first).c_str());
        for (auto item: ctop.second) {
            fprintf(OStr, "        assign %s = %s;\n", finishString(item.first).c_str(), finishExpr(item.second.value).c_str());
        }
        fprintf(OStr, "    end;\n");
    }

    // generate clocked updates to state elements
    auto ctop = condLines.begin(), ctopEnd = condLines.end();
    for (; ctop != ctopEnd || resetList.size(); ctop++) { // process all generate sections
    if (resetList.size() || ctop->second.size()) {
        std::string ctopLoop = ctop != ctopEnd ? ctop->first : "";
        if (ctopLoop != "")
            fprintf(OStr, "\n    %s\n", finishString(ctopLoop).c_str());
        fprintf(OStr, "\n    always @( posedge CLK) begin\n      if (!nRST) begin\n");
        for (auto item: resetList)
            fprintf(OStr, "        %s <= 0;\n", item.c_str());
        resetList.clear();
        fprintf(OStr, "      end // nRST\n");
        if (ctop != ctopEnd && ctop->second.size() > 0) {
            fprintf(OStr, "      else begin\n");
            std::list<std::string> alwaysLines;
            for (auto tcond: ctop->second) {
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
                    if (item.first) {
                        temp = "    if (" + finishExpr(item.first) + ")";
                        if (item.second.size() > 1) {
                            temp += " begin";
                            endStr = "    end;";
                        }
                        alwaysLines.push_back(temp);
                    }
                    for (auto citem: item.second) {
                        if (citem.dest)
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
    }
}
