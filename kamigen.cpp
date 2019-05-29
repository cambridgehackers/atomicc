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

static std::string kamiType(std::string type)
{
    std::string ret = type;
    int i = ret.find("(");
    if (i > 0)
        ret = ret.substr(0, i) + " " + ret.substr(i + 1, ret.length() - i - 2);
    return ret;
}

static ACCExpr *kamiChanges(ACCExpr *expr)
{
    std::string value = expr->value;
    if (isIdChar(value[0]))
        value = "#" + value + "_v";
    else if (isdigit(value[0]))
        value = "$$ (* intwidth *) (natToWord 32 " + value + ")";
    auto ext = allocExpr(value);
    for (auto item: expr->operands)
        ext->operands.push_back(kamiChanges(item));
    return ext;
}

static std::string kamiValue(ACCExpr *expr)
{
    return tree2str(kamiChanges(expr));
}

static std::string kamiCall(ACCExpr *expr)
{
    return tree2str(expr);
}

static void generateKami(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir)
{
    FILE *OStrV = fopen((OutputDir + ".kami").c_str(), "w");
    if (!OStrV) {
        printf("kamigen: unable to open '%s'\n", (OutputDir + ".kami").c_str());
        exit(-1);
    }
    fprintf(OStrV, "Require Import Bool String List Arith.\n"
                   "Require Import Omega.\n"
                   "Require Import Kami.All.\n"
                   "Require Import Bsvtokami.\n\n"
                   "Require Import FunctionalExtensionality.\n\n"
                   "Set Implicit Arguments.\n\n\n");

    for (auto item: interfaceIndex) {
        std::list<std::string> hints;
        std::string name = item.first;
        ModuleIR *IR = item.second;
printf("[%s:%d] interface %s = %s\n", __FUNCTION__, __LINE__, name.c_str(), IR->name.c_str());
        fprintf(OStrV, "(* * interface %s *)\nRecord %s := {\n    %s'mod: Mod;\n",
             IR->name.c_str(), name.c_str(), name.c_str());
        for (auto iitem: IR->method) {
             std::string methodName = iitem.first;
             //MethodInfo *MI = iitem.second;
             char buf[200];
             if (endswith(methodName, "__RDY"))
                 continue;
             snprintf(buf, sizeof(buf), "Hint Unfold %s'%s : ModuleDefs.", name.c_str(), methodName.c_str());
             hints.push_back(buf);
             fprintf(OStrV, "    %s'%s : string;\n", name.c_str(), methodName.c_str());
        }
        fprintf(OStrV, "}.\n\n");
        for (auto iitem: hints)
            fprintf(OStrV, "%s\n", iitem.c_str());
    }
    for (auto IR : irSeq) {
        std::string name = IR->name;
        int i = name.find("(");
        if (i > 0)
            name = name.substr(0, i);
printf("[%s:%d] module '%s'\n", __FUNCTION__, __LINE__, IR->name.c_str());
        fprintf(OStrV, "\nModule module'mk%s.\n", name.c_str());
        fprintf(OStrV, "    Section Section'mk%s.\n", name.c_str());
        fprintf(OStrV, "    Variable instancePrefix: string.\n"
                       "        (* method bindings *)\n");
        std::list<std::string> letList;
        for (auto iitem: IR->fields) {
            std::string tname = iitem.type;
            int i = tname.find("(");
            if (i > 0)
                tname = tname.substr(0, i);
printf("[%s:%d] tname %s\n", __FUNCTION__, __LINE__, tname.c_str());
            auto IIR = lookupIR(tname);
            if (IIR) {
                fprintf(OStrV, "    Let (* action binding *) %s := mk%s (instancePrefix--\"%s\").\n",
                    iitem.fldName.c_str(), tname.c_str(), iitem.fldName.c_str());
                for (auto interfaceItem: IIR->interfaces) {
                auto interfaceIR = lookupInterface(interfaceItem.type);
printf("[%s:%d] III %p\n", __FUNCTION__, __LINE__, interfaceIR);
                for (auto mitem: interfaceIR->method) {
                    std::string methodName = mitem.first;
                    //MethodInfo *MI = mitem.second;
                    if (endswith(methodName, "__RDY"))
                        continue;
                    char buf[200];
                    snprintf(buf, sizeof(buf), "    Let %s'%s : string := (%s'%s %s).", iitem.fldName.c_str(),
                         methodName.c_str(), tname.c_str(), methodName.c_str(), iitem.fldName.c_str());
                    letList.push_back(buf);
                }
                }
            }
            else
                fprintf(OStrV, "    Let %s : string := instancePrefix--\"%s\".\n",
                    iitem.fldName.c_str(), iitem.fldName.c_str());
        }
        fprintf(OStrV, "    (* instance methods *)\n");
        for (auto iitem: letList)
            fprintf(OStrV, "%s\n", iitem.c_str());
        std::string interfaceName = "Empty";
        for (auto iitem: IR->interfaces) {
            auto II = lookupInterface(iitem.type);
printf("[%s:%d] interfacename %s lookup %s\n", __FUNCTION__, __LINE__, iitem.type.c_str(), II ? II->name.c_str() : "none");
            interfaceName = iitem.fldName;
        }
        fprintf(OStrV, "    Local Open Scope kami_expr.\n\n");
        fprintf(OStrV, "    Definition mk%sModule: Mod :=\n", name.c_str());
        fprintf(OStrV, "         (BKMODULE {\n");
        std::string sep = "   ";
        for (auto iitem: IR->fields) {
            std::string tname = iitem.type;
            int i = tname.find("(");
            if (i > 0)
                tname = tname.substr(0, i);
printf("[%s:%d] tname %s\n", __FUNCTION__, __LINE__, tname.c_str());
            auto IIR = lookupIR(tname);
            if (IIR)
                fprintf(OStrV, "    %s (BKMod (%s'mod %s :: nil))\n",
                    sep.c_str(), tname.c_str(), iitem.fldName.c_str());
            else
                fprintf(OStrV, "    %s Register (instancePrefix--\"%s\") : %s <- Default\n",
                    sep.c_str(), iitem.fldName.c_str(), kamiType(iitem.type).c_str());
            sep = "with";
        }
        for (auto iitem: IR->method) {
             std::string methodName = iitem.first;
             MethodInfo *MI = iitem.second;
             if (endswith(methodName, "__RDY"))
                 continue;
             MethodInfo *MIRdy = lookupMethod(IR, getRdyName(methodName));
             fprintf(OStrV, "    %s %s instancePrefix--\"%s\" :=\n    (\n", sep.c_str(),
                 MI->rule ? "Rule" : "Method", methodName.c_str());
             for (auto pitem: MI->params) {
                  fprintf(OStrV, "        Read %s_v: %s <- (instancePrefix--\"%s\") ;\n",
                 pitem.name.c_str(), kamiType(pitem.type).c_str(), pitem.name.c_str());
             }
             fprintf(OStrV, "        Assert(%s);\n", kamiValue(MIRdy->guard).c_str());
             int unusedNumber = 1;
             for (auto citem: MI->callList) {
                  fprintf(OStrV, "               BKCall unused%d : Void %s <- %s  ;\n",
                     unusedNumber++, citem->isAction ? "(* actionBinding *)" : "",
                     kamiCall(citem->value).c_str());
             }
             for (auto sitem: MI->storeList) {
                  fprintf(OStrV, "        Write (instancePrefix--\"%s\") : %s <- %s ;\n",
                 tree2str(sitem->dest).c_str(), "BITT", kamiValue(sitem->value).c_str());
             }
             if (MI->rule)
                 fprintf(OStrV, "        Retv ) (* rule %s *)\n", methodName.c_str());
             else
                 fprintf(OStrV, "        Retv    )\n");
             sep = "with";
        }
        fprintf(OStrV, "\n    }). (* mk%s *)\n\n", name.c_str());
        fprintf(OStrV, "    Hint Unfold mk%sModule : ModuleDefs.\n", name.c_str());
        std::string mtype = IR->name;
        if (mtype == "Empty")
            mtype = "Main"; // HACK
        fprintf(OStrV, "(* Module mk%s type Module#(%s) return type %s *)\n", name.c_str(), mtype.c_str(), mtype.c_str());
        fprintf(OStrV, "    Definition mk%s := Build_%s mk%sModule.\n", name.c_str(), interfaceName.c_str(), name.c_str());
        fprintf(OStrV, "    Hint Unfold mk%s : ModuleDefs.\n", name.c_str());
        fprintf(OStrV, "    Hint Unfold mk%sModule : ModuleDefs.\n\n", name.c_str());
        fprintf(OStrV, "    End Section'mk%s.\n", name.c_str());
        fprintf(OStrV, "End module'mk%s.\n\n", name.c_str());
        fprintf(OStrV, "Definition mk%s := module'mk%s.mk%s.\n", name.c_str(), name.c_str(), name.c_str());
        fprintf(OStrV, "Hint Unfold mk%s : ModuleDefs.\n", name.c_str());
        fprintf(OStrV, "Hint Unfold module'mk%s.mk%s : ModuleDefs.\n", name.c_str(), name.c_str());
        fprintf(OStrV, "Hint Unfold module'mk%s.mk%sModule : ModuleDefs.\n", name.c_str(), name.c_str());
    }
    fclose(OStrV);
}

int main(int argc, char **argv)
{
    std::list<ModuleIR *> irSeq;
printf("[%s:%d] KAMIGEN\n", __FUNCTION__, __LINE__);
    int argIndex = 1;
    if (argc - 1 != argIndex) {
        printf("[%s:%d] kamigen <outputFileStem>\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    std::string myName = argv[argIndex];
    std::string OutputDir = myName + ".generated";
    int ind = myName.rfind('/');
    if (ind > 0)
        myName = myName.substr(ind+1);

    readIR(irSeq, OutputDir);
printf("[%s:%d] AFTERREAD\n", __FUNCTION__, __LINE__);
    //cleanupIR(irSeq);
    flagErrorsCleanup = 1;
    //preprocessIR(irSeq);
    generateKami(irSeq, myName, OutputDir);
printf("[%s:%d]DONE\n", __FUNCTION__, __LINE__);
    return 0;
}
