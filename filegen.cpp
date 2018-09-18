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
//#include <string.h>
//#include <assert.h>
#include "AtomiccIR.h"
#include "common.h"

static void generateModuleHeader(std::list<ModData> &modLine, FILE *OStr)
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
        else if (mitem.isparam) {
            if (!paramSeen) {
                fprintf(OStr, "#(\n    ");
                sep = "";
                paramSeen = true;
            }
            std::string typ = "UNKNOWN_PARAM_TYPE[" + mitem.type + "] ";
            std::string init = "\"FALSE\"";
            if (mitem.type == "FLOAT") {
                typ = "real ";
                init = "0.0";
            }
            else if (mitem.type == "POINTER")
                typ = "";
            else if (startswith(mitem.type, "INTEGER_")) {
                typ = "integer ";
                init = "0";
            }
            fprintf(OStr, "%sparameter %s%s = %s", sep.c_str(), typ.c_str(), mitem.value.c_str(), init.c_str());
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
            fprintf(OStr, "%s %s%s", dirs.c_str(), sizeProcess(mitem.type).c_str(), mitem.value.c_str());
        }
    }
    if (handleCLK)
        fprintf(OStr, "%sinput CLK, input nRST", sep.c_str());
    fprintf(OStr, ");\n");
}

void generateVerilogOutput(FILE *OStr, std::list<ModData> &modLineTop)
{
    generateModuleHeader(modLineTop, OStr);
    std::list<std::string> resetList;
    // generate local state element declarations and wires
    for (auto item: refList) {
        if (trace_assign)
            printf("[%s:%d] ref %s pin %d count %d done %d out %d inout %d type %s\n", __FUNCTION__, __LINE__, item.first.c_str(), item.second.pin, item.second.count, item.second.done, item.second.out, item.second.inout, item.second.type.c_str());
        if (item.second.pin == PIN_REG) {
            fprintf(OStr, "    reg %s;\n", (sizeProcess(item.second.type) + item.first).c_str());
            resetList.push_back(item.first);
        }
    }
    for (auto item: refList) {
        std::string temp = item.first;
        if (item.second.pin == PIN_WIRE || item.second.pin == PIN_OBJECT || item.second.pin == PIN_LOCAL) {
        if (item.second.count && !item.second.isGenerated) {
            fprintf(OStr, "    wire %s;\n", (sizeProcess(item.second.type) + item.first).c_str());
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
    bool isGenerate = false;
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
    for (auto mitem: modNew) {
        if (mitem.moduleStart) {
            flushOut();
            isGenerate = mitem.vecCount != -1;
            std::string instName = mitem.argName;
            if (isGenerate) {
                fprintf(OStr, "    genvar __inst$Genvar;\n");
                fprintf(OStr, "    generateFor(__inst$Genvar = 0; __inst$Genvar < %d; __inst$Genvar = __inst$Genvar + 1) begin %s:\n", mitem.vecCount, mitem.argName.c_str());
                instName = "data";
            }
            tempOutput.push_back("    " + mitem.value + " " + instName + " (");
            if (!mitem.noDefaultClock)
                tempOutput.push_back(".CLK(CLK), .nRST(nRST),");
            sep = "";
        }
        else {
            std::string val = tree2str(cleanupExpr(str2tree(mitem.value)));
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
            if (item.second.inout && !assignList[item.first].value)
                for (auto alitem: assignList)
                    if (ACCExpr *val = alitem.second.value)
                    if (isIdChar(val->value[0]) && val->value == item.first)
                        goto next;
            if (refList[temp].done)
                continue;
            if (refList[temp].count) {
                if (assignList[item.first].value)
                    fprintf(OStr, "    assign %s = %s;\n", item.first.c_str(), tree2str(assignList[item.first].value).c_str());
                else
                    fprintf(OStr, "    assign %s = 0; //MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE\n", item.first.c_str());
            }
            else if (trace_skipped)
                fprintf(OStr, "    //skippedassign %s = %s; //temp = '%s', count = %d, pin = %d done %d\n", item.first.c_str(), tree2str(assignList[item.first].value).c_str(), temp.c_str(), refList[temp].count, item.second.pin, refList[temp].done);
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
        if (item.second.value && refList[temp].count && !refList[temp].done) {
            if (!seen)
                fprintf(OStr, "    // Extra assigments, not to output wires\n");
            seen = true;
            fprintf(OStr, "    assign %s = %s;\n", item.first.c_str(), tree2str(item.second.value).c_str());
            refList[temp].done = true; // mark that assigns have already been output
        }
    }

    // generate clocked updates to state elements
    if (condLines.size() || resetList.size()) {
        fprintf(OStr, "\n    always @( posedge CLK) begin\n      if (!nRST) begin\n");
        for (auto item: resetList)
            fprintf(OStr, "        %s <= 0;\n", item.c_str());
        fprintf(OStr, "      end // nRST\n");
        if (condLines.size() > 0) {
            fprintf(OStr, "      else begin\n");
            std::list<std::string> alwaysLines;
            for (auto tcond: condLines) {
                std::string methodName = tcond.first, endStr;
                if (checkInteger(tcond.second.guard, "1"))
                    alwaysLines.push_back("// " + methodName);
                else {
                    alwaysLines.push_back("if (" + tree2str(tcond.second.guard) + ") begin // " + methodName);
                    endStr = "end; ";
                }
                for (auto item: tcond.second.info) {
                    std::string endStr;
                    std::string temp;
                    if (item.first) {
                        temp = "    if (" + tree2str(item.first) + ")";
                        if (item.second.size() > 1) {
                            temp += " begin";
                            endStr = "    end;";
                        }
                        alwaysLines.push_back(temp);
                    }
                    for (auto citem: item.second) {
                        if (citem.dest)
                            alwaysLines.push_back("    " + tree2str(citem.dest) + " <= " + tree2str(citem.value) + ";");
                        else
                            alwaysLines.push_back("    $display" + tree2str(citem.value->operands.front()) + ";");
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
    }
    fprintf(OStr, "endmodule \n\n");
}
