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

static void metaGenerateModule(ModuleIR *IR, FILE *OStr)
{
    std::map<std::string, int> exclusiveSeen;
    std::list<std::string>     metaList;
    // write out metadata comments at end of the file
    metaList.push_front("//METASTART; " + IR->name);
    for (auto item: IR->interfaces)
        if (item.isPtr)
        metaList.push_back("//METAEXTERNAL; " + item.fldName + "; " + lookupIR(item.type)->name + ";");
    for (auto item: IR->fields) {
        int64_t vecCount = item.vecCount;
        int dimIndex = 0;
        if (lookupIR(item.type))
        do {
            std::string fldName = item.fldName;
            if (vecCount != -1)
                fldName += autostr(dimIndex++);
            if (item.isPtr)
                metaList.push_back("//METAEXTERNAL; " + fldName + "; " + lookupIR(item.type)->name + ";");
            else if (!startswith(lookupIR(item.type)->name, "l_struct_OC_")
                 && !startswith(lookupIR(item.type)->name, "l_ainterface"))
                metaList.push_back("//METAINTERNAL; " + fldName + "; " + lookupIR(item.type)->name + ";");
        } while(--vecCount > 0);
    }
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        std::string gtemp = "; " + tree2str(MI->guard) + ";";
        if (endswith(methodName, "__RDY"))
            metaList.push_back("//METAGUARD; "
                + methodName.substr(0, methodName.length()-5) + gtemp);
        else if (endswith(methodName, "__READY"))
            metaList.push_back("//METAGUARDV; "
                + methodName.substr(0, methodName.length()-7) + gtemp);
        else {
            // For each method/rule of the current class,
            // gather up metadata generated by processFunction
            MetaRef *bm = MI->meta;
            std::string temp;
            for (auto titem: bm[MetaInvoke])
                for (auto item: titem.second)
                    temp += item + ":" + titem.first + ";";
            if (temp != "")
                metaList.push_back("//METAINVOKE; " + methodName + "; " + temp);
            std::map<std::string,std::string> metaBefore;
            std::map<std::string,std::string> metaConflict;
            for (auto innerFI : IR->method) {
                MethodInfo *innerMI = innerFI.second;
                std::string innermethodName = innerMI->name;
                MetaRef *innerbm = innerMI->meta;
                std::string tempConflict;
                if (innermethodName == methodName)
                    continue;
                // scan all other rule/methods of this class
                for (auto inneritem: innerMI->storeList) {
                    for (auto item: bm[MetaRead])
                        // if the current method reads a state element that
                        // is written by another method, add it to the 'before' list
                        if (item.first == inneritem->dest->value) {
printf("[%s:%d] innermethodName %s before conflict '%s' innerunc %s methodName %s\n", __FUNCTION__, __LINE__, innermethodName.c_str(), item.first.c_str(), innermethodName.c_str(), methodName.c_str());
                            metaBefore[innermethodName] = "; :";
                            break;
                        }
                    for (auto item: MI->storeList)
                        // if the current method writes a state element that
                        // is written by another method, add it to the 'conflict' list
                        if (tree2str(item->dest) == tree2str(inneritem->dest)) {
                            metaConflict[innermethodName] = "; ";
                            break;
                        }
                }
                for (auto inneritem: innerbm[MetaInvoke]) {
                    for (auto item: bm[MetaInvoke])
                        if (item.first == inneritem.first) {
//printf("[%s:%d] conflict methodName %s innermethodName %s item %s\n", __FUNCTION__, __LINE__, methodName.c_str(), innermethodName.c_str(), item.first.c_str());
                            metaConflict[innermethodName] = "; ";
                            break;
                        }
                }
            }
            std::string metaStr;
            for (auto item: metaConflict)
                 if (item.second != "" && !exclusiveSeen[item.first])
                     metaStr += item.second + item.first;
            exclusiveSeen[methodName] = 1;
            if (metaStr != "")
                metaList.push_back("//METAEXCLUSIVE; " + methodName + metaStr);
            metaStr = "";
            for (auto item: metaBefore)
                 if (item.second != "")
                     metaStr += item.second + item.first;
            if (metaStr != "")
                metaList.push_back("//METABEFORE; " + methodName + metaStr);
        }
    }
    std::string ruleNames;
    for (auto FI : IR->method) {
        MethodInfo *MI = FI.second;
        std::string methodName = MI->name;
        if (MI->rule && endswith(methodName, "__ENA"))
            ruleNames += "; " + methodName.substr(0, methodName.length()-5);
    }
    if (ruleNames != "")
        metaList.push_back("//METARULES" + ruleNames);
    for (auto item: IR->interfaceConnect) {
        std::string tname = item.target;
        std::string sname = item.source;
printf("[%s:%d] METACONNECT %s %s\n", __FUNCTION__, __LINE__, tname.c_str(), sname.c_str());
        for (auto FI: lookupIR(item.type)->method) {
            MethodInfo *MI = FI.second;
            std::string methodName = MI->name;
            metaList.push_back("//METACONNECT; " + tname + MODULE_SEPARATOR + MI->name
                                              + "; " + sname + MODULE_SEPARATOR + MI->name);
        }
    }
    for (auto item : IR->priority)
        metaList.push_back("//METAPRIORITY; " + item.first + "; " + item.second);
    for (auto item : metaList)
        fprintf(OStr, "%s\n", item.c_str());
}

void generateMeta(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir)
{
    myName += "_GENERATED_";
    FILE *OStrVH = fopen((OutputDir + ".vh").c_str(), "w");
    fprintf(OStrVH, "`ifndef __%s_VH__\n`define __%s_VH__\n\n", myName.c_str(), myName.c_str());
    for (auto IR : irSeq)
        metaGenerateModule(IR, OStrVH); // now generate the verilog header file '.vh'
    fprintf(OStrVH, "`endif\n");
    fclose(OStrVH);
}
