
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <map>

//#include "cJSON.h"
#include "cJSON.c"

#define BUFFER_LENGTH 20000000
static char buffer[BUFFER_LENGTH];
static void dumpJson(const cJSON *arg, std::string &master, int depth);
static void dumpSingle(const cJSON *p, std::string &master, int depth);
FILE *outfile;

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
static bool inline startswith(std::string str, std::string suffix)
{
    return str.substr(0, suffix.length()) == suffix;
}

std::string getString(const cJSON *arg, const char *name)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(arg, name);
    if (item && item->valuestring)
        return item->valuestring;
    return "";
}
std::string getStringSpace(const cJSON *arg, const char *name)
{
    std::string val = getString(arg, name);
    int ind = val.find(" ");
    if (ind > 0)
        val = val.substr(ind+1);
    return val;
}
std::map<std::string, std::string> dataMap;
static std::string renameFile(std::string temp, std::string base)
{
    auto item = dataMap.find(temp);
    if (item != dataMap.end())
        return item->second;
    dataMap[temp] = base;
    fprintf(outfile, "**** %s ****\n%s\n", base.c_str(), temp.c_str());
    return base;
}

static std::string typeAlias(const cJSON *arg)
{
    std::string str = getString(arg, "target");
    if (startswith(str, "struct")) {
        int ind = str.rfind("}");
        if (ind != -1)
            str = str.substr(0, ind+1);
    }
    return renameFile(str, "TYPE_" + getString(arg, "name"));
}
std::string processExpr(const cJSON *item)
{
     if (!item)
         return "";
     std::string kind = getString(item, "kind");
     std::string op = getString(item, "op");
     std::string type = getString(item, "type");
     std::string left = processExpr(cJSON_GetObjectItemCaseSensitive(item, "left"));
     std::string right = processExpr(cJSON_GetObjectItemCaseSensitive(item, "right"));
     std::string val;
     if (kind == "Assignment") {
         val = "=";
         if (const cJSON *nonBlock = cJSON_GetObjectItemCaseSensitive(item, "isNonBlocking"))
         if (nonBlock->type & cJSON_True)
             val = "<=";
     }
     else if (kind == "IntegerLiteral")
         val = getString(item, "constant");
     else if (kind == "UnaryOp") {
         if (op == "LogicalNot")
             val = "~";
         if (op == "Minus")
             val = "-";
         val += processExpr(cJSON_GetObjectItemCaseSensitive(item, "operand"));
     }
     else if (kind == "BinaryOp") {
         if (op == "LogicalAnd")
             val = "&&";
         else if (op == "BinaryAnd")
             val = "&";
         else if (op == "LogicalOr")
             val = "||";
         else if (op == "BinaryOr")
             val = "|";
         else if (op == "Add")
             val = "+";
         else if (op == "Subtract")
             val = "-";
         else if (op == "Equality")
             val = "==";
         else if (op == "Inequality")
             val = "!=";
         else if (op == "LessThanEqual")
             val = "<";
         else if (op == "LogicalShiftLeft")
             val = "<<";
     }
     else if (kind == "NamedValue") {
         val = getStringSpace(item, "symbol");
     }
     else if (kind == "StringLiteral") {
         val = "\"" + getStringSpace(item, "literal") + "\"";;
     }
     else if (kind == "Concatenation") {
         const cJSON *aitem = NULL;
         std::string sep;
         cJSON_ArrayForEach(aitem, cJSON_GetObjectItemCaseSensitive(item, "operands")) {
             val += sep + processExpr(aitem);
             sep = ", ";
         }
         return "{" + val + "}";
     }
     else if (kind == "Conversion")
         val = "(" + getString(item, "type") + ")" + processExpr(cJSON_GetObjectItemCaseSensitive(item, "operand"));
     else if (kind == "RangeSelect") {
         return processExpr(cJSON_GetObjectItemCaseSensitive(item, "value")) + "[" + left + ":" + right + "]";
     }
     else if (kind == "MemberAccess") {
         val = processExpr(cJSON_GetObjectItemCaseSensitive(item, "value")) + "." + getStringSpace(item, "member");
     }
     else if (kind == "ElementSelect") {
         val = processExpr(cJSON_GetObjectItemCaseSensitive(item, "value")) + "[" + processExpr(cJSON_GetObjectItemCaseSensitive(item, "selector")) + "]";
     }
     else if (kind == "Call") {
         std::string sep;
         val = "CALL " + getStringSpace(item, "subroutine") + "(";
         const cJSON *aitem = NULL;
         cJSON_ArrayForEach(aitem, cJSON_GetObjectItemCaseSensitive(item, "arguments")) {
             val += sep + processExpr(aitem);
             sep = ", ";
         }
         val += ")";
     }
     else if (kind == "EmptyArgument") {
         val = "EA";
     }
     else if (kind == "ConditionalOp") {
         val = "CONDOP ( " + processExpr(cJSON_GetObjectItemCaseSensitive(item, "pred")) + " ) ? " + left + " : " + right;
     }
     else if (kind == "HierarchicalValue") {
         val = "HV:" + getString(item, "symbol");
//printf("[%s:%d] val %s left %s righ %s kind %s op %s type %s\n", __FUNCTION__, __LINE__, val.c_str(), left.c_str(), right.c_str(), kind.c_str(), op.c_str(), type.c_str());
//dumpJson(item, stdout, 0);
     }
if (val == "") {
printf("[%s:%d] val %s left %s righ %s kind %s op %s type %s\n", __FUNCTION__, __LINE__, val.c_str(), left.c_str(), right.c_str(), kind.c_str(), op.c_str(), type.c_str());
//dumpJson(item, stdout, 0);
exit(-1);
}
     return left + " " + val + " " + right;
}
extern "C" void jca(){}
static void processBlock(std::string startString, const cJSON *p, std::string &master, int depth)
{
    std::string kind = getString(p, "kind");
    if (!p || kind == "Empty")
        return;
    master += startString;
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(p, "body");
    std::string bkind = getString(body, "kind");
//fprintf(d, "[%s:%d] p %p body %p kind %s bkind %s\n", __FUNCTION__, __LINE__, p, body, kind.c_str(), bkind.c_str());
    if (kind == "Conditional") {
        master += "CONDBLOCK: if (" + processExpr(cJSON_GetObjectItemCaseSensitive(p, "cond")) + ") ";
        processBlock("begin\n", cJSON_GetObjectItemCaseSensitive(p, "ifTrue"), master, depth+1);
        processBlock("else begin\n", cJSON_GetObjectItemCaseSensitive(p, "ifFalse"), master, depth+1);
    }
    else if (kind == "ExpressionStatement") {
        master += "EXPRSTMT: " + processExpr(cJSON_GetObjectItemCaseSensitive(p, "expr")) + "\n";
    }
    else if (kind == "Case") {
        master += "CASE: " + getString(p, "check") + " " + processExpr(cJSON_GetObjectItemCaseSensitive(p, "expr")) + "\n";
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(p, "items")) {
            master += "    case ";
            std::string sep;
            const cJSON *expr = NULL;
            cJSON_ArrayForEach(expr, cJSON_GetObjectItemCaseSensitive(item, "expressions")) {
                master += sep + processExpr(expr);
                sep = ", ";
            }
            master += ":\n";
            processBlock("", cJSON_GetObjectItemCaseSensitive(item, "stmt"), master, depth+1);
        }
        master += "ENDCASE\n";
    }
    else if (bkind == "List") {
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(body, "list")) {
            processBlock("", item, master, depth+1);
        }
    }
    else if (bkind == "Conditional") {
        master += "CONDITIONAL: ";
        dumpJson(p, master, depth+1);
    }
    else {
        master += "BLOCKCKKC " + bkind + "\n";
        dumpJson(p, master, depth+1);
    }
    if (startString != "")
        master += "end\n";
}
static std::string setDefinition(const cJSON *arg)
{
    std::string master;
    std::string objectName = getString(arg, "definition");
    const cJSON *memb = cJSON_GetObjectItemCaseSensitive(arg, "members");
    std::string val, sep;
    bool inParam = false;
    bool inPort = true;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, memb) {
        std::string name = getString(item, "name");
        const cJSON *init = cJSON_GetObjectItemCaseSensitive(item, "initializer");
        std::string constVal = getString(init, "constant");
        int ind = constVal.find("'");
        int ind2 = constVal.find(".");
        if (ind < 0 && ind2 < 0 && name != "" && constVal != "" && objectName.length() < 40)
            objectName += "__" + name + "_" + constVal;
        std::string kind = getString(item, "kind");
        std::string type = getString(item, "type");
        std::string direction = getString(item, "direction");
        std::string v = getString(item, "value");
        if (kind == "Port") {
            if (inParam) {
                val = "#(" + val + ")(";
                sep = "";
            }
            else if (sep == "")
                val += "(";
            inParam = false;
            val += sep + direction + " " + type + " " + name;
            if (v != "")
                val += "=" + v; 
            sep = ", ";
        }
        else if (kind == "Parameter") {
            inParam = true;
            val = sep + name;
            if (v != "")
                val += "=" + v; 
            sep = ", ";
        }
        else if (kind == "InterfacePort" || direction != "") {
            if (inParam) {
                val = "#(" + val + ")(";
                sep = "";
            }
            else if (sep == "")
                val += "(";
            inParam = false;
            if (direction != "") {
                val += sep + direction + " " + type + " " + name;
            }
            else
                val += sep + getString(item, "interfaceDef") + "." + getString(item, "modport") + " " + name;
            sep = ", ";
        }
    }
    if (inPort)
        val += ")\n";
    master += "MODULE: " + val + "\n";
    cJSON_ArrayForEach(item, memb) {
        std::string kind = getString(item, "kind");
        std::string direction = getString(item, "direction");
        if (kind == "Port" || kind == "Parameter" || kind == "InterfacePort" || direction != "" || kind == "Net")
            continue;
        if (kind == "Variable") {
            std::string name = getString(item, "name");
            std::string type = getString(item, "type");
            std::string val = type + " " + name + ";";
            master += val + "\n";
        }
        else if (kind == "Modport") {
            std::string sep;
            master += "modport (";
            const cJSON *mitem = NULL;
            cJSON_ArrayForEach(mitem, cJSON_GetObjectItemCaseSensitive(item, "members")) {
                master += sep + getString(mitem, "direction") + " " //+ getString(mitem, "type") + " "
                           + getString(mitem, "name");
                sep = ", ";
            }
            std::string name = getString(item, "name");
            master += ") " + name + ";\n";
        }
        else
            dumpSingle(item, master, 1);
    }
    //dumpJson(arg->child, master, 0);
    return renameFile(master, "DEF_" + objectName);
}

static void dumpSingle(const cJSON *p, std::string &master, int depth)
{
        int type = p->type;
        std::string name, vstring;
        if (p->string)
            name = p->string;
        if (p->valuestring)
            vstring = p->valuestring;
        if (name == "addr")
            return;
        if (name == "name" && vstring == "")
            return;
        for (int i = 0; i < depth; i++)
            master += "    ";
        master += name + ": ";
        if (type & cJSON_False) {
            master += "FALSE\n";
        }
        if (type & cJSON_True) {
            master += "TRUE\n";
        }
        if (type & cJSON_NULL) {
            master += "NULL\n";
        }
        if (p && (type & cJSON_Raw)) {
            master += "RAW\n";
        }
        if (type & cJSON_Number) {
            master += autostr(p->valueint) + "\n";
        }
        if (type & cJSON_String) {
            master += "'" + vstring + "'\n";
        }
        if (type & cJSON_Array) {
            master += "ARR\n";
            dumpJson(p->child, master, depth+1);
        }
        if (type & cJSON_Object) {
            std::string kind = getString(p, "kind");
            std::string definition = getString(p, "definition");
            if (kind == "TypeAlias") {
                master += "FILEALIAS: " + typeAlias(p) + "\n";
            }
            else if (kind == "Root") {
                dumpJson(cJSON_GetObjectItemCaseSensitive(p, "members"), master, depth+1);
            }
            else if (kind == "ContinuousAssign") {
                master += "ASSIGN: " + processExpr(cJSON_GetObjectItemCaseSensitive(p, "assignment")) + "\n";
            }
            else if (kind == "Instance") {
                master += "INSTANCE: " + setDefinition(cJSON_GetObjectItemCaseSensitive(p, "body")) + " " + getString(p, "name") + "\n";
                //connections
            }
            else if (kind == "InstanceArray") {
                const cJSON *item = NULL;
                std::string name = getString(p, "name");
                cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(p, "members")) {
                    master += "INSTANCEARRAY: " + setDefinition(cJSON_GetObjectItemCaseSensitive(item, "body")) + " " + name + "\n";
                    //dumpJson(item, master, depth+1);
                }
            }
            else if (kind == "Block") {
                processBlock("", p, master, depth+1);
            }
            else if (kind == "ProceduralBlock") {
    //const cJSON *body = cJSON_GetObjectItemCaseSensitive(p, "body");
master += "BLOCKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n";
                dumpJson(p->child, master, 0);
            }
            else if (definition != "") {
                master += "FILEDEF: " + setDefinition(p) + "\n";
            }
            else if (kind == "SignalEvent") {
                std::string edge = getString(p, "edge");
                master += "always @ " + edge + " " + processExpr(cJSON_GetObjectItemCaseSensitive(p, "expr")) + "\n";
            }
            else {
                master += "OBJ: " + kind + "\n";
                dumpJson(p->child, master, depth+1);
            }
        }
}
static void dumpJson(const cJSON *arg, std::string &master, int depth)
{
    const cJSON *pnext = arg;
    while (pnext) {
         dumpSingle(pnext, master, depth);
         pnext = pnext->next;
    }
}

int main(int argc, char *argv[])
{
    std::string outputFile = "yy.json.dump";
    int inFile = open("xx.json", O_RDONLY);
    unsigned long len = read(inFile, buffer, sizeof(buffer));
    if (inFile == -1 || len <= 0 || len >= sizeof(buffer) - 10) {
printf("[%s:%d] error reading\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    int status = 0;
printf("[%s:%d] value %p len %ld \n", __FUNCTION__, __LINE__, (void *)buffer, len);
    cJSON *json = cJSON_ParseWithLength(buffer, len);
printf("[%s:%d]cjson %p\n", __FUNCTION__, __LINE__, (void *)json);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
static char foo[1000];
memcpy(foo, error_ptr, 800);
            printf("Error before: %s\n", foo);
        }
        status = 0;
        return -1;
    } 
    outfile = fopen(outputFile.c_str(), "w");
    std::string master;
    dumpJson(json, master, 0);
    fprintf(outfile, "%s\n", master.c_str());
    cJSON_Delete(json);
    fclose(outfile);
    return 0;
}
