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

typedef std::list<std::string> StrList;
static StrList interfaceList;
static std::map<std::string, bool> interfaceSeen;

static void generateVerilog(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir)
{
    std::string baseDir = OutputDir;
    int ind = baseDir.rfind('/');
    if (ind > 0)
        baseDir = baseDir.substr(0, ind+1);
    for (auto IR : irSeq) {
        static ModList modLineTop;
        modLineTop.clear();
        generateModuleDef(IR, modLineTop);         // Collect/process verilog info

        std::string name = IR->name;
        int ind = name.find("(");
        if (ind > 0)
            name = name.substr(0, ind);
        FILE *OStrV = fopen((baseDir + name + ".sv").c_str(), "w");
        if (!OStrV) {
            printf("veriloggen: unable to open '%s'\n", (baseDir + name + ".sv").c_str());
            exit(-1);
        }
        fprintf(OStrV, "`include \"%s.generated.vh\"\n\n", myName.c_str());
        fprintf(OStrV, "`default_nettype none\n");
        generateModuleHeader(OStrV, modLineTop);
        generateVerilogOutput(OStrV);
        if (IR->isTopModule)
            fprintf(OStrV, "`include \"%s.linker.vh\"\n", name.c_str());
        fprintf(OStrV, "endmodule\n\n`default_nettype wire    // set back to default value\n");
        fclose(OStrV);
    }
    if (printfFormat.size()) {
    FILE *OStrP = fopen((OutputDir + ".printf").c_str(), "w");
    for (auto item: printfFormat) {
        fprintf(OStrP, "%s ", item.format.c_str());
        for (auto witem: item.width)
            fprintf(OStrP, "%d ", witem);
        fprintf(OStrP, "\n");
    }
    fclose(OStrP);
    }
}

static std::string modportNames(std::string first, StrList &inname, std::string second, StrList &outname, StrList &inoutname)
{
    std::string ret, sep;
    if (inname.size())
        ret += first + " ";
    for (auto item: inname) {
        ret += sep + item;
        sep = ", ";
    }
    if (sep != "")
        sep = ",\n                    ";
    if (outname.size()) {
        ret += sep + second + " ";
        sep = "";
    }
    for (auto item: outname) {
        ret += sep + item;
        sep = ", ";
    }
    if (inoutname.size()) {
        ret += sep + "inout ";
        sep = "";
    }
    for (auto item: inoutname) {
        ret += sep + item;
        sep = ", ";
    }
    return ret;
}

static void generateVerilogInterface(std::string name, FILE *OStrVH)
{
    StrList inname, outname, inoutname, fields;
    ModuleIR *IR = lookupInterface(name);
    name = cleanupModuleType(name);
    for (auto fitem: IR->fields) {
        if (fitem.isParameter != "") {
            continue;
        }
        fields.push_back(typeDeclaration(fitem.type) + " " + fitem.fldName);
        if (fitem.isInput)
            inname.push_back(fitem.fldName);
        if (fitem.isOutput)
            outname.push_back(fitem.fldName);
        if (fitem.isInout)
            inoutname.push_back(fitem.fldName);
    }
    for (auto MI: IR->methods) {
         std::string methodName = MI->name;
         std::string rdyMethodName = getRdyName(methodName);
         if (methodName == rdyMethodName)
             continue;
        std::string type = "logic";
        if (MI->type != "") {
            type = typeDeclaration(MI->type);
            outname.push_back(methodName);
        }
        else
            inname.push_back(methodName);
        fields.push_back(type + " " + methodName);
        for (auto pitem: MI->params) {
            std::string pname = baseMethodName(methodName) + DOLLAR + pitem.name;
            fields.push_back(typeDeclaration(pitem.type) + " " + pname);
            inname.push_back(pname);
        }
        fields.push_back("logic " + rdyMethodName);
        outname.push_back(rdyMethodName);
    }
    if (fields.size()) {
        MapNameValue mapValue;
        extractParam("INTERFACE__" + name, name, mapValue);
        int ind = name.find("(");
        if (ind > 0)
            name = name.substr(0, ind);
        std::string defname = "__" + name + "_DEF__";
        if (mapValue.size()) {
            name += "#(";
            std::string sep;
            for (auto item: mapValue) {
                name += sep + item.first;
                if (item.second != "")
                    name += " = " + item.second;
                sep = ", ";
            }
            //dumpModule("paramet", IR);
            name += ")";
        }
        fprintf(OStrVH, "`ifndef %s\n`define %s\ninterface %s;\n", defname.c_str(), defname.c_str(), name.c_str());
        for (auto item: fields)
            fprintf(OStrVH, "    %s;\n", item.c_str());
#if 1 // yosys can't handle modport/inout
        inoutname.clear();
#endif
        if (inname.size() + outname.size() + inoutname.size()) {
        fprintf(OStrVH, "    modport server (%s);\n", modportNames("input ", inname, "output", outname, inoutname).c_str());
        fprintf(OStrVH, "    modport client (%s);\n", modportNames("output", inname, "input ", outname, inoutname).c_str());
        }
        fprintf(OStrVH, "endinterface\n`endif\n");
    }
}

static bool shouldNotOuput(ModuleIR *IR)
{
    std::string source = IR->sourceFilename;
    if (source.find("lib/") != std::string::npos)
        return true;        // imported from library
    if ((source == "atomicc.h" && myGlobalName != "atomicc")
     || (source == "fifo.h" && myGlobalName != "fifo"))
        return true;
    return false;
}

static void appendInterface(std::string orig, std::string params)
{
    MapNameValue mapValue;
    MapNameValue mapValueExclude;
    extractParam("APPEND_" + orig, orig, mapValueExclude);
    std::string name = genericModuleParam(orig, params, &mapValue); // if we inherit parameters, use them (unless we already had some)
    std::string shortName = cleanupModuleType(name);
    int ind = shortName.find_first_of("(" "#");
    if (ind > 0)
        shortName = shortName.substr(0, ind);
    if (shortName.find("_OC_") != std::string::npos)       // discard specializations
        return;
    if (interfaceSeen[shortName])
        return;
    interfaceSeen[shortName] = true;
    ModuleIR *IR = lookupInterface(orig);
    assert(IR);
    if (shouldNotOuput(IR))
        return;
    for (auto item: IR->interfaces) {
        MapNameValue mapValueInterface;
        extractParam("APPENDI_" + item.type, item.type, mapValueInterface);
        // if the value of any of the parameters in this instantiation depends on
        // a parameter from the referencing module, do not generate (for example FunnelBase and PipeIn)
        for (auto item: mapValueInterface)
            if (mapValueExclude.find(item.second) != mapValueExclude.end())
                goto nextItem;
        appendInterface(item.type, params);
nextItem:;
    }
    interfaceList.push_back(orig);
}

int main(int argc, char **argv)
{
    std::list<ModuleIR *> irSeq;
    std::list<std::string> fileList;
    bool noVerilator = false;
noVerilator = true;
printf("[%s:%d] VERILOGGGEN\n", __FUNCTION__, __LINE__);
    int argIndex = 1;
    if (argc == 3 && !strcmp(argv[argIndex], "-n")) {
        argIndex++;
        noVerilator = true;
    }
    if (argc == 3 && !strcmp(argv[argIndex], "-v")) {
        argIndex++;
        noVerilator = false;
    }
    if (argc - 1 != argIndex) {
        printf("[%s:%d] veriloggen <outputFileStem>\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    std::string myName = argv[argIndex];
    std::string OutputDir = myName + ".generated";
    int ind = myName.rfind('/');
    if (ind > 0)
        myName = myName.substr(ind+1);
    myGlobalName = myName;

    readIR(irSeq, fileList, OutputDir);
    cleanupIR(irSeq);
    flagErrorsCleanup = 1;
    if (int ret = generateSoftware(irSeq, argv[0], OutputDir))
        return ret;
    processInterfaces(irSeq);
    preprocessIR(irSeq);
    generateVerilog(irSeq, myName, OutputDir);

    FILE *OStrVH = fopen((OutputDir + ".vh").c_str(), "w");
    std::string defname = myName + "_GENERATED_";
    fprintf(OStrVH, "`ifndef __%s_VH__\n`define __%s_VH__\n", defname.c_str(), defname.c_str());
    fprintf(OStrVH, "`include \"atomicclib.vh\"\n\n");
    for (auto item: mapAllModule) {
        ModuleIR *IR = item.second;
        if (shouldNotOuput(IR))
            continue;
        if (IR->isStruct) {
            std::string defname = "__" + IR->name + "_DEF__";
            fprintf(OStrVH, "`ifndef %s\n`define %s\ntypedef struct packed {\n", defname.c_str(), defname.c_str());
            std::list<std::string> lineList;
            for (auto fitem: IR->fields) // reverse order of fields
                lineList.push_front(typeDeclaration(fitem.type) + " " + fitem.fldName);
            for (auto item: lineList)
                fprintf(OStrVH, "    %s;\n", item.c_str());
            fprintf(OStrVH, "} %s;\n`endif\n", IR->name.c_str());
        }
        else if (!IR->isInterface
         && !endswith(IR->name, "_UNION") && IR->name.find("_VARIANT_") == std::string::npos) {
            if (IR->interfaceName == "") {
                dumpModule("Missing interfaceName", IR);
                exit(-1);
            }
            std::string params;
            int ind = IR->name.find("(");
            if (ind > 0)
                params = IR->name.substr(ind);
//printf("[%s:%d]////////////////////////////////////////////////////////////////////////////////////// %s params %s\n", __FUNCTION__, __LINE__, IR->interfaceName.c_str(), params.c_str());
//dumpModule("INTERFACE", IR);
            if (!IR->isVerilog)
            appendInterface(IR->interfaceName, params);
        }
    }
#if 1 // support of System Verilog structs
    for (auto item: interfaceList)
        generateVerilogInterface(item, OStrVH);
#endif
    for (auto IR : irSeq)
        metaGenerateModule(IR, OStrVH); // now generate the verilog header file '.vh'
    fprintf(OStrVH, "`endif\n");
    fclose(OStrVH);
    generateKami(irSeq, myName, OutputDir);

    if (!noVerilator) {
        std::string commandLine = "verilator --cc " + OutputDir + ".sv";
        int ret = system(commandLine.c_str());
        printf("[%s:%d] RETURN from '%s' %d\n", __FUNCTION__, __LINE__, commandLine.c_str(), ret);
        if (ret)
            return -1; // force error return to be propagated
    }
    return 0;
}
