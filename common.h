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

#define SUBSCRIPT_MARKER "@["
#define PARAMETER_MARKER "@{"
#define SUBSCRIPT_CLOSE "@]"
#define PARAMETER_CLOSE "@}"
#define ASSIGN_SIZE_LIMIT 6

typedef struct {
    std::string argName;
    std::string value;
    std::string type;
    bool        moduleStart;
    bool        noDefaultClock;
    int         out;
    bool        inout;
    bool        isparam;
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
    bool        noRecursion;
} AssignItem;
enum {PIN_NONE, PIN_MODULE, PIN_OBJECT, PIN_REG, PIN_WIRE, PIN_ALIAS, PIN_LOCAL};
typedef struct {
    int         count;
    std::string type;
    bool        out;
    bool        inout;
    int         pin;
    bool        done;
} RefItem;
typedef struct {
    uint64_t    upper;
    ACCExpr    *value;
    std::string type;
} BitfieldPart;
typedef struct {
    ACCExpr *dest;
    ACCExpr *value;
} CondInfo;
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
std::string fixupQualPin(ModuleIR *searchIR, std::string searchStr);
void getFieldList(std::list<FieldItem> &fieldList, std::string name, std::string base, std::string type, bool out, bool force = true, uint64_t offset = 0, bool alias = false, bool init = true);
ModuleIR *allocIR(std::string name);
MethodInfo *allocMethod(std::string name);
void addMethod(ModuleIR *IR, MethodInfo *MI);
void dumpModule(std::string name, ModuleIR *IR);
std::string findType(std::string name);

// expr.cpp
std::string tree2str(ACCExpr *expr, bool *changed = nullptr, bool assignReplace = false);
ACCExpr *allocExpr(std::string value, ACCExpr *argl = nullptr, ACCExpr *argr = nullptr, ACCExpr *argt = nullptr);
bool isIdChar(char ch);
void dumpExpr(std::string tag, ACCExpr *next);
ACCExpr *cleanupExpr(ACCExpr *expr, bool preserveParen = false, bool replaceBuiltin = false);
ACCExpr *cleanupExprBit(ACCExpr *expr);
ACCExpr *str2tree(std::string arg, bool allowRangeParam = false);
std::string treePost(const ACCExpr *arg);
bool checkOperand(std::string s);
ACCExpr *invertExpr(ACCExpr *expr);
void updateWidth(ACCExpr *item, int len);
ACCExpr *getRHS(ACCExpr *expr, int match = 1);
void walkRef (ACCExpr *expr);
void decRef(std::string name);
bool matchExpr(ACCExpr *lhs, ACCExpr *rhs);
int walkCount (ACCExpr *expr);

// readIR.cpp
void readIR(std::list<ModuleIR *> &irSeq, std::string OutputDir);

// processInterfaces
void processInterfaces(std::list<ModuleIR *> &irSeq);

// generateSoftware
int generateSoftware(std::list<ModuleIR *> &irSeq, const char *exename, std::string outName);

// metaGen.cpp
void generateMeta(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir);

// preprocessIR.cpp
void preprocessIR(std::list<ModuleIR *> &irSeq);

// verilog.cpp
void generateVerilog(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir);

extern int trace_assign;
extern int trace_expand;
extern std::map<std::string, RefItem> refList;
extern std::map<std::string, AssignItem> assignList;
extern std::map<std::string, ModuleIR *> mapIndex;
extern std::map<std::string, int> replaceBlock;
extern int globalExprCleanup;
extern int flagErrorsCleanup;
