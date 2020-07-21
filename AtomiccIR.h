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
#ifndef __ATOMICIR_H__
#define __ATOMICIR_H__
#include <stdio.h>
#include <string>
#include <list>
#include <set>
#include <map>

#define MODULE_SEPARATOR "$"

#define MAX_READ_LINE 100000

static inline std::string autostr(uint64_t X, bool isNeg = false) {
  char Buffer[21];
  char *BufPtr = std::end(Buffer);

  if (X == 0) *--BufPtr = '0';  // Handle special case...

  while (X) {
    *--BufPtr = '0' + char(X % 10);
    X /= 10;
  }

  if (isNeg) *--BufPtr = '-';   // Add negative sign...
  return std::string(BufPtr, std::end(Buffer));
}

static bool inline endswith(std::string str, std::string suffix)
{
    int skipl = str.length() - suffix.length();
    return skipl >= 0 && str.substr(skipl) == suffix;
}
static bool inline startswith(std::string str, std::string suffix)
{
    return str.substr(0, suffix.length()) == suffix;
}

typedef struct ACCExpr {
    std::list<ACCExpr *>operands;
    std::string value;
} ACCExpr;

typedef struct {
    ACCExpr *target;
    ACCExpr *source;
    std::string type;
    bool        isForward;
} InterfaceConnectType;

enum {MetaNone, MetaRead, MetaInvoke, MetaMax};

typedef std::set<std::string> MetaSet;
typedef std::map<std::string,MetaSet> MetaRef;

typedef struct {
    ACCExpr *dest;
    ACCExpr *value;
    ACCExpr *cond;
} StoreListElement;

typedef struct {
    ACCExpr *dest;
    ACCExpr *value;
    ACCExpr *cond;
    std::string type;
} LetListElement;

typedef struct {
    ACCExpr *value;
    ACCExpr *cond;
} AssertListElement;

typedef struct {
    ACCExpr *value;
    ACCExpr *cond;
    bool isAction;
} CallListElement;

typedef struct {
    std::string name;
    std::string type;
    std::string init;
} ParamElement;

typedef struct {
    std::string name;
    std::string type;
} UnionItem;

typedef struct {
    std::string type;
    bool        noReplace;
} AllocaItem;

typedef struct {
    ACCExpr *cond;
    std::string var;
    ACCExpr *init;
    ACCExpr *limit;
    ACCExpr *incr;
    std::string body;
} GenerateForItem;

typedef struct {
    ACCExpr *cond;
    std::string var;
    ACCExpr *init;
    ACCExpr *limit;
    ACCExpr *incr;
    ACCExpr *sub;
    std::string body;
} InstantiateForItem;

typedef struct {
    ACCExpr                   *guard;
    ACCExpr                   *subscript;
    std::string                generateSection;
    std::string                name;
    bool                       rule;
    bool                       action;
    std::list<StoreListElement *> storeList;
    std::list<LetListElement *> letList;
    std::list<AssertListElement *> assertList;
    std::list<CallListElement *> callList;
    std::list<CallListElement *> printfList;
    std::string                type;
    std::list<ParamElement>    params;
    std::list<GenerateForItem> generateFor;
    std::list<InstantiateForItem> instantiateFor;
    std::map<std::string, AllocaItem> alloca;
    std::list<InterfaceConnectType>   interfaceConnect; // from __connectInterface() processing
    MetaRef                    meta[MetaMax];
} MethodInfo;

typedef struct {
    std::string fldName;
    std::string vecCount;
    std::string type;
    bool        isPtr;
    bool        isInput; // used for verilog interfaces
    bool        isOutput; // used for verilog interfaces
    bool        isInout; // used for verilog interfaces
    std::string isParameter; // used for verilog interfaces (initial value)
    bool        isShared;    // used for __shared (common CSE) support
    bool        isLocalInterface; // interface declaration that is used to connect to local objects (does not appear in module signature)
    bool        isExternal;  // used for non-heirarchical references
} FieldElement;

typedef struct ModuleIR {
    std::string                       name;
    std::list<std::string>            metaList;
    std::list<std::string>            softwareName;
    std::list<MethodInfo *> methods;
    std::map<std::string, MethodInfo *> generateBody;
    std::map<std::string, std::string> priority; // indexed by rulename, result is 'high'/etc
    std::list<FieldElement>           fields;
    std::map<std::string, std::string> params;
    std::list<UnionItem>              unionList;
    std::list<FieldElement>           interfaces;
    std::list<FieldElement>           parameters;
    std::list<InterfaceConnectType>   interfaceConnect;
    int                               genvarCount;
    bool                              isInterface;
    bool                              isStruct;
    bool                              isSerialize;
    bool                              transformGeneric;
    std::string                       interfaceVecCount;
    std::string                       interfaceName;
} ModuleIR;
#endif /* __ATOMICIR_H__ */
