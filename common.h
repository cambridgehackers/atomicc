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
#define GENVAR_NAME "__inst$Genvar"
#define BLOCK_NAME "BasicBlockCond_"
#define COUNT_LIMIT             0//10

#define DOLLAR           "$"
#define PERIOD           "."

#define GENERIC_INT_TEMPLATE_FLAG         999999
#define GENERIC_INT_TEMPLATE_FLAG_STRING "999999"

typedef struct {
    std::string argName;
    std::string value;
    std::string type;
    bool        moduleStart;
    bool        noDefaultClock;
    int         out;
    bool        inout;
    //bool        isparam;
    std::string isparam;     // initial value for parameters
    std::string vecCount;
} ModData;
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
    bool        noRecursion;
    int         size;
} AssignItem;
enum {PIN_NONE, PIN_MODULE, PIN_OBJECT, PIN_REG, PIN_WIRE, PIN_ALIAS, PIN_LOCAL};
typedef struct {
    int         count;
    std::string type;
    bool        out;
    bool        inout;
    int         pin;
    bool        done;
    bool        isGenerated;
    std::string vecCount;
    bool        isArgument;
} RefItem;
typedef struct {
    long        upper;
    ACCExpr    *value;
    std::string type;
} BitfieldPart;
typedef struct {
    ACCExpr *dest;
    ACCExpr *value;
} CondInfo;
typedef struct {
    ACCExpr      *guard;
    std::map<ACCExpr *, std::list<CondInfo>> info;
} CondGroup;
typedef struct {
    std::string format;
    std::list<int> width;
} PrintfInfo;
typedef struct {
    std::string name;
    bool        out;
    bool        isPtr;
    bool        instance;
} SyncPinsInfo;

typedef struct {
    std::map<std::string, CondGroup> always;
    std::list<std::string>           assert;
} CondLineType;

typedef ModuleIR *(^CBFun)(FieldElement &item);
#define CBAct ^ ModuleIR * (FieldElement &item)

typedef std::map<std::string, std::string> MapNameValue;

// util.cpp
std::string baseMethodName(std::string pname);
std::string getRdyName(std::string basename);
std::string getEnaName(std::string basename);
bool isRdyName(std::string name);
bool isEnaName(std::string name);
void extractParam(std::string debugName, std::string replaceList, MapNameValue &mapValue);
std::string instantiateType(std::string arg, MapNameValue &mapValue);
std::string convertType(std::string arg, int arrayProcess = 0);
ModuleIR *lookupIR(std::string ind);
ModuleIR *lookupInterface(std::string ind);
std::string sizeProcess(std::string type);
std::string typeDeclaration(std::string type);
ModuleIR *iterField(ModuleIR *IR, CBFun cbWorker);
MethodInfo *lookupMethod(ModuleIR *IR, std::string name);
MethodInfo *lookupQualName(ModuleIR *searchIR, std::string searchStr, std::string &vecCount, MapNameValue &mapValue);
std::string fixupQualPin(ModuleIR *searchIR, std::string searchStr);
void getFieldList(std::list<FieldItem> &fieldList, std::string name, std::string base, std::string type, bool force, uint64_t offset, bool alias);
ModuleIR *allocIR(std::string name, bool isInterface = false);
MethodInfo *allocMethod(std::string name);
bool addMethod(ModuleIR *IR, MethodInfo *MI);
void dumpMethod(std::string name, MethodInfo *MI);
void dumpModule(std::string name, ModuleIR *IR);
std::string findType(std::string name);
std::string CBEMangle(const std::string &S);
char *getExecutionFilename(char *buf, int buflen);
void walkReplaceBuiltin(ACCExpr *expr, std::string phiDefault);
std::string exprWidth(ACCExpr *expr, bool forceNumeric = false);
std::string makeSection(std::string var, ACCExpr *init, ACCExpr *limit, ACCExpr *incr);

// expr.cpp
bool bitOp(std::string s);
bool plainInteger(std::string val);
void foldMember(ACCExpr *expr);
std::string tree2str(ACCExpr *expr, bool addSpaces = true);
ACCExpr *allocExpr(std::string value, ACCExpr *argl = nullptr, ACCExpr *argr = nullptr, ACCExpr *argt = nullptr);
bool isIdChar(char ch);
bool isParen(std::string ch);
void dumpExpr(std::string tag, ACCExpr *next);
ACCExpr *cleanupExpr(ACCExpr *expr, bool preserveParen = false);
ACCExpr *cleanupExprBuiltin(ACCExpr *expr, std::string phiDefault = "0", bool preserveParen = false);
ACCExpr *cleanupInteger(ACCExpr *expr);
ACCExpr *cleanupBool(ACCExpr *expr);
ACCExpr *str2tree(std::string arg, bool allowRangeParam = false);
bool checkOperand(std::string s);
ACCExpr *invertExpr(ACCExpr *expr);
void updateWidth(ACCExpr *item, std::string clen);
ACCExpr *getRHS(ACCExpr *expr, int match = 1);
bool matchExpr(ACCExpr *lhs, ACCExpr *rhs);
int walkCount (ACCExpr *expr);
ACCExpr *dupExpr(ACCExpr *expr);
bool checkIntegerString(std::string val, std::string pattern);
bool checkInteger(ACCExpr *expr, std::string pattern);
bool booleanBinop(std::string s);
bool arithOp(std::string s);
bool relationalOp(std::string s);
bool isConstExpr(ACCExpr *expr);
extern int trace_expr;

// readIR.cpp
void readIR(std::list<ModuleIR *> &irSeq, std::list<std::string> &fileList, std::string OutputDir);

// processInterfaces
void processInterfaces(std::list<ModuleIR *> &irSeq);

// generateSoftware
int generateSoftware(std::list<ModuleIR *> &irSeq, const char *exename, std::string outName);

// metaGen.cpp
void metaGenerateModule(ModuleIR *IR, FILE *OStr);

// preprocessIR.cpp
void preprocessIR(std::list<ModuleIR *> &irSeq);
void cleanupIR(std::list<ModuleIR *> &irSeq);

// verilog.cpp
void generateModuleDef(ModuleIR *IR, std::list<ModData> &modLineTop);

// filegen.cpp
extern std::map<std::string, int> genvarMap;
void generateModuleHeader(FILE *OStr, std::list<ModData> &modLine);
void generateVerilogOutput(FILE *OStr);

// kami.cpp
void generateKami(std::list<ModuleIR *> &irSeq, std::string myName, std::string OutputDir);

extern int trace_assign;
extern int trace_expand;
extern int trace_skipped;
extern std::map<std::string, RefItem> refList;
extern std::map<std::string, AssignItem> assignList;
extern std::map<std::string, std::map<std::string, AssignItem>> condAssignList;
extern std::map<std::string, ModuleIR *> mapIndex;
extern std::map<std::string, ModuleIR *> interfaceIndex;
extern std::map<std::string, int> replaceBlock;
extern std::list<ModData> modNew;
extern std::map<std::string, CondLineType> condLines;
extern std::list<PrintfInfo> printfFormat;
extern std::map<std::string, int> genericModule;
extern int globalExprCleanup;
extern int flagErrorsCleanup;
extern std::map<std::string, SyncPinsInfo> syncPins;    // SyncFF items needed for PipeInSync instances
extern int implementPrintf;
extern std::string myGlobalName;
