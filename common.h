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

typedef struct {
    std::string argName;
    std::string value;
    std::string type;
    bool        moduleStart;
    int         out;
} ModData;
typedef struct {
    ACCExpr    *cond;
    ACCExpr    *value;
} MuxValueEntry;
typedef struct {
    std::string name;
    std::string base;
    std::string type;
    bool        alias;
    uint64_t    offset;
} FieldItem;
typedef struct {
    ACCExpr    *value;
    std::string type;
    bool        noReplace;
} AssignItem;
enum {PIN_NONE, PIN_MODULE, PIN_OBJECT, PIN_REG, PIN_WIRE, PIN_ALIAS};
typedef struct {
    int         count;
    std::string type;
    bool        out;
    int         pin;
} RefItem;
typedef struct {
    uint64_t    upper;
    ACCExpr    *value;
    std::string type;
} BitfieldPart;
typedef ModuleIR *(^CBFun)(FieldElement &item, std::string fldName);
#define CBAct ^ ModuleIR * (FieldElement &item, std::string fldName)

// util.cpp
std::string getRdyName(std::string basename);
uint64_t convertType(std::string arg);
std::string getRdyName(std::string basename);
ModuleIR *lookupIR(std::string ind);
uint64_t convertType(std::string arg);
std::string sizeProcess(std::string type);
ModuleIR *iterField(ModuleIR *IR, CBFun cbWorker);
MethodInfo *lookupMethod(ModuleIR *IR, std::string name);
MethodInfo *lookupQualName(ModuleIR *searchIR, std::string searchStr);
void getFieldList(std::list<FieldItem> &fieldList, std::string name, std::string base, std::string type, bool out, bool force = true, uint64_t offset = 0, bool alias = false, bool init = true);
ModuleIR *allocIR(std::string name);
MethodInfo *allocMethod(std::string name);
void addMethod(ModuleIR *IR, MethodInfo *MI);
void dumpModule(std::string name, ModuleIR *IR);

// AtomiccExpr.h
std::string tree2str(const ACCExpr *arg);
ACCExpr *allocExpr(std::string value, ACCExpr *argl = nullptr, ACCExpr *argr = nullptr);
bool isIdChar(char ch);

// processInterfaces
void processInterfaces(std::list<ModuleIR *> &irSeq);

// generateSoftware
int jsonPrepare(std::list<ModuleIR *> &irSeq, const char *exename, std::string outName);

// metaGen.cpp
void metaGenerate(ModuleIR *IR, FILE *OStr);

// preprocessIR.cpp
void preprocessIR(std::list<ModuleIR *> &irSeq);

extern int trace_assign;
extern int trace_expand;
extern std::map<std::string, RefItem> refList;
extern std::map<std::string, ModuleIR *> mapIndex;
