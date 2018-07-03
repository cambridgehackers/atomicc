// Copyright (c) 2018 The Connectal Project
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <map>
#include <list>
#include <string>
#include <algorithm>

//
// parser for .lib files
//

#define BUFFER_LEN 20000000
std::string tokval;
FILE *outfile;

class OptItem {
public:
    std::string cell, filename, ifprefix;
    std::list<std::string> notfactor;
    std::list<std::string> factor;
    std::list<std::string> notFactor;
};
OptItem options;
static char buffer[BUFFER_LEN];
char btemp[1000];
char *p = buffer;
bool eof;
typedef std::map<std::string, std::string> AttributeList;
typedef struct {
    AttributeList attr;
    std::map<int, int> pins;
} LibInfo;
std::map<std::string, LibInfo> capturePins;
std::map<std::string, LibInfo> busInfo;
typedef struct {
    std::string dir;
    std::string type;
    std::string name;
    std::string comment;
} PinInfo;
typedef std::list<PinInfo> PinList;
PinList pinList;
std::map<std::string, std::map<std::string, PinList>> interfaceList;
PinList masterList;
std::map<std::string, int> masterSeen;

typedef struct {
    std::string dir;
    std::string type;
} VPInfo;
std::map<std::string, VPInfo> vinfo;

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
static std::string ljust(std::string str, int len)
{
    int size = len - str.length();
    if (size > 0)
        str += std::string(size, ' ');
    return str;
}

bool isId(char c)
{
    return (isalpha(c) || c == '_' || c == '$');
}

std::string parsenext()
{
    static const char *special = "()=!|/*+-[]{}%:;,";
    if (eof)
        return "";
    while (isspace(*p))
        p++;
    char *start = p;
    if (!*p)
        eof = true;
    else if (isdigit(*p)) {
        while(isdigit(*p) || *p == '.')
            p++;
    }
    else if (isId(*p)) {
        while(isId(*p) || isdigit(*p))
            p++;
    }
    else if (*p == '"') {
        p++;
        while (*p && *p++ != '"')
            ;
    }
    else if (index(special, *p)) {
        p++;
    }
    else {
printf("[%s:%d] unknown char '%c'\n", __FUNCTION__, __LINE__, *p);
exit(-1);
    }
    tokval = std::string(start, p);
//printf("[%s:%d] TOKEN start %p p %p pchar %x '%s'\n", __FUNCTION__, __LINE__, start, p, *start, tokval.c_str());
    return tokval;
}

void validate_token(bool testval, std::string wanted)
{
    if (! testval) {
        printf("Error:Got: %s, wanted '%s'\n", tokval.c_str(), wanted.c_str());
        exit(-1);
    }
    parsenext();
}

std::string parseparam()
{
    std::string paramstr;
    validate_token(tokval == "(", "(");
    while (tokval != ")" && !eof) {
        paramstr = paramstr + tokval;
        parsenext();
    }
    validate_token(tokval == ")", ")");
    validate_token(tokval == "{", "{");
    return paramstr;
}
void processCell()
{
    for (auto item : capturePins) {
        int next = 0;
        bool nextok = false;
        for (auto pin: item.second.pins)
            if (pin.second && pin.first == next++)
                nextok = true;
            else {
                nextok = false;
                break;
            }
        std::string dir = item.second.attr["direction"];
        std::string type = autostr(nextok ? next : 1);
        std::string bus = item.second.attr["bus_type"];
        if (bus != "")
             type = busInfo[bus].attr["bit_width"];
        std::string name = item.first;
        std::string comment = item.second.attr["related_pin"];
        item.second.attr["direction"] = "";
        item.second.attr["related_pin"] = "";
        if (!nextok && item.second.pins.size()) {
            printf("[%s:%d] pins not consecutive\n", __FUNCTION__, __LINE__);
            exit(-1);
        }
        for (auto attr: item.second.attr)
            if (attr.second != "")
                comment += ", " + attr.first + "=" + attr.second;
        pinList.push_back(PinInfo{dir, type, name, comment});
    }
}

void addAttr(AttributeList *attr, std::string pinName, std::string name, std::string value)
{
    if (!attr)
        return;
    if (attr->find(name) != attr->end()) {
        if (name != "fpga_arc_condition" && (*attr)[name] != value) {
        if (!(value[0] == '"' && value[value.length()-1] == '"' && value.substr(1, value.length()-2) == pinName)) {
            printf("%s: pin %s attr replace [%s] was %s new %s\n", __FUNCTION__, pinName.c_str(), name.c_str(), (*attr)[name].c_str(), value.c_str());
            exit(-1);
        }
        }
    }
    (*attr)[name] = value;
}

void parse_item(bool capture, std::string pinName, AttributeList *attr)
{
    while (tokval != "}" && !eof) {
        std::string paramname = tokval;
        validate_token(isId(tokval[0]), "name");
        if (paramname == "default_intrinsic_fall" || paramname == "default_intrinsic_rise") {
            validate_token(tokval == ":", ":(paramname)");
            if (capture)
                addAttr(attr, pinName, paramname, tokval);
            validate_token(isdigit(tokval[0]), "number");
        }
        else if (paramname == "bus_type") {
            validate_token(tokval == ":", ":(bus_type)");
            if (capture)
                addAttr(attr, pinName, paramname, tokval);
            validate_token(isId(tokval[0]), "name");
        }
        else if (tokval == "(") {
            while (tokval == "(") {
                std::string paramstr = parseparam();
                bool cell = paramname == "cell" && paramstr == options.cell;
                int ind = paramstr.find("[");
                if (capture && (paramname == "pin" || paramname == "bus")) {
                    if (ind > 0 && paramstr[paramstr.length()-1] == ']') {
                        std::string sub = paramstr.substr(ind+1);
                        sub = sub.substr(0, sub.length()-1);
                        paramstr = paramstr.substr(0, ind);
                        capturePins[paramstr].pins[atol(sub.c_str())] = 1;
                    }
                    parse_item(true, paramstr, &capturePins[paramstr].attr);
                }
                else if (paramname == "type") {
                    parse_item(true, "", &busInfo[paramstr].attr);
                }
                else
                    parse_item(capture || cell, pinName, attr);
//if (paramname == "cell")
//printf("[%s:%d] paramname %s paramstr %s \n", __FUNCTION__, __LINE__, paramname.c_str(), paramstr.c_str());
                if (cell) {
                    processCell();
                    return;
                }
                paramname = tokval;
                if (!isId(tokval[0]))
                    break;
                parsenext();
            }
        }
        else {
            validate_token(tokval == ":", ":(other)");
            if (capture && paramname != "timing_type")
                addAttr(attr, pinName, paramname, tokval);
            if (isdigit(tokval[0]) || isId(tokval[0]) || tokval[0] == '"')
                parsenext();
            else
                validate_token(false, "number or name or string");
            if (tokval != "}")
                validate_token(tokval == ";", ";");
        }
    }
    validate_token(tokval == "}", "}");
}

void parse_lib(std::string filename)
{
    int inpfile = open(filename.c_str(), O_RDONLY);
    if (inpfile == -1) {
        printf("[%s:%d] unable to open '%s'\n", __FUNCTION__, __LINE__, filename.c_str());
        exit(-1);
    }
    int len = read(inpfile, buffer, sizeof(buffer));
    if (len >= sizeof(buffer) - 1) {
        printf("[%s:%d] incomplete read of '%s'\n", __FUNCTION__, __LINE__, filename.c_str());
        exit(-1);
    }
    parsenext();
    if (tokval != "library") {
        printf("[%s:%d] 'library' keyword not found\n", __FUNCTION__, __LINE__);
        exit(-1);
    }
    validate_token(isId(tokval[0]), "name");
    parseparam();
    parse_item(false, "", nullptr);
}

void getVline()
{
    while (*p && *p == ' ')
        p++;
    if (*p == '\n')
        p++;
}

std::string getVtoken()
{
    while (*p && *p == ' ')
        p++;
    char *start = p;
    while (*p && *p != ' ' && *p != '\n')
        p++;
    std::string ret = std::string(start, p);
//printf("[%s:%d] %s\n", __FUNCTION__, __LINE__, ret.c_str());
    return ret;
}

void parse_verilator(std::string filename)
{
    std::string commandLine = "verilator --atomicc --top-module " + options.cell + " " + filename;
    int rc = system(commandLine.c_str());
printf("[%s:%d] calling '%s' returned %d\n", __FUNCTION__, __LINE__, commandLine.c_str(), rc);

    std::string tempFile = "obj_dir/V" + options.cell + ".atomicc";
    int inpfile = open(tempFile.c_str(), O_RDONLY);
    if (inpfile == -1) {
        printf("[%s:%d] unable to open '%s'\n", __FUNCTION__, __LINE__, tempFile.c_str());
        exit(-1);
    }
    int len = read(inpfile, buffer, sizeof(buffer));
    if (len >= sizeof(buffer) - 1) {
        printf("[%s:%d] incomplete read of '%s'\n", __FUNCTION__, __LINE__, tempFile.c_str());
        exit(-1);
    }
    while (*p) {
        std::string dir = getVtoken();
        std::string type = getVtoken();
        std::string name = getVtoken();
        vinfo[name] = VPInfo{dir, type};
        getVline();
    }
    for (auto item: vinfo)
        pinList.push_back(PinInfo{item.second.dir, item.second.type, item.first, ""});
}
void generate_interface(std::string interfacename, std::string indexname, std::string paramlist, PinList &ilist)
{
    fprintf(outfile, "__interface %s {\n", (options.ifprefix + interfacename + paramlist).c_str());
    for (auto item : ilist) {
        if (item.dir != "input" && item.dir != "output" && item.dir != "inout" && item.dir != "parameter" && item.dir != "interface")
            continue;
        std::string typ = item.type;
        if (isdigit(typ[0]))
            typ = "__uint(" + typ + ")";
        else if (typ == "integer" || typ == "logic")
            typ = "int";
        else if (typ == "real" || typ == "DOUBLE")
            typ = "float";
        else if (typ == "GENERIC")
            typ = "const char *";
        std::string outp;
        if (item.dir == "input")
            outp = "__input  ";
        if (item.dir == "inout")
            outp = "__inout  ";
        if (item.dir == "output")
            outp = "__output ";
        if (item.dir == "parameter")
            outp = "__parameter ";
        fprintf(outfile, "    %s%s %s;\n", outp.c_str(), ljust(typ,16).c_str(), item.name.c_str());
    }
    fprintf(outfile, "};\n");
}

void regroup_items()
{
    std::string currentgroup = "";
    for (auto item: pinList) {
        std::string litem = item.name, groupname, fieldname, indexname;
        bool skipcheck = false;
//printf("[%s:%d] dir %s type %s name %s comment %s\n", __FUNCTION__, __LINE__, item.dir.c_str(), item.type.c_str(), item.name.c_str(), item.comment.c_str());
        if (item.dir != "input" && item.dir != "output" && item.dir != "inout" && item.dir != "parameter") {
            //newlist.push_back(item);
            printf("DD %s\n", item.name.c_str());
            continue;
        }
        for (auto tstring : options.factor)
            if (startswith(litem, tstring)) {
                groupname = tstring;
                litem = litem.substr(groupname.length());
                int ind = 0;
                while (isdigit(litem[ind]))
                    ind++;
                indexname = litem.substr(0,ind);
                litem = litem.substr(ind);
                fieldname = litem;
//printf("[%s:%d] group %s litem %s index %s\n", __FUNCTION__, __LINE__, groupname.c_str(), litem.c_str(), indexname.c_str());
                break;
            }
        for (auto checkitem : options.notfactor) {
            if (startswith(litem, checkitem)) {
printf("[%s:%d]notfactor\n", __FUNCTION__, __LINE__);
                skipcheck = 1;
            }
        }
        if (skipcheck || fieldname == "") {
            //newlist.push_back(item);
            masterList.push_back(PinInfo{item.dir, item.type, item.name, item.comment});
            continue;
        }
        std::string itemname = groupname + indexname;
        std::transform(groupname.begin(), groupname.end(), groupname.begin(), ::tolower);
        if (!masterSeen[itemname]) {
            masterSeen[itemname] = 1;
            masterList.push_back(PinInfo{"interface", options.ifprefix + groupname, itemname, ""});
        }
        interfaceList[groupname][indexname].push_back(PinInfo{item.dir, item.type, fieldname, item.comment});
    }
}

void generate_cpp()
{
    // generate output file
    //std::string paramlist = "";
    //for (auto item : paramnames);
        //paramlist = paramlist + ", numeric type " + item;
    //if (paramlist != "");
        //paramlist = "//(" + paramlist[2:] + ")";;
    //paramval = paramlist.replace("numeric type ", "");
    for (auto item: interfaceList)
        for (auto iitem: item.second)
            generate_interface(item.first, iitem.first, "", iitem.second);
    generate_interface(options.cell, "", "", masterList);
    fprintf(outfile, "__emodule %s {\n", options.cell.c_str());
    fprintf(outfile, "    %s _;\n", (options.ifprefix + options.cell).c_str());
    fprintf(outfile, "};\n");
}

int main(int argc, char **argv)
{
    opterr = 0;
    int c;
    while ((c = getopt (argc, argv, "o:f:n:C:P:I:")) != -1)
        switch (c) {
        case 'o':
            options.filename = optarg;
            break;
        case 'f':
            options.factor.push_back(optarg);
            break;
        case 'n':
            options.notFactor.push_back(optarg);
            break;
        case 'C':
            options.cell = optarg;
            break;
        case 'P':
            options.ifprefix = optarg;
            break;
        default:
            abort();
        }
    if (optind != argc-1 || options.filename == "" || argc == 0 || options.ifprefix == "") {;
        printf("Missing \"--o\" option, missing input filenames, missing ifname or missing ifprefix.  Run \" importbvi.py -h \" to see available options\n");
        exit(-1);
    }
    outfile = fopen(options.filename.c_str(), "w");
    if (endswith(argv[optind], ".lib"))
        parse_lib(argv[optind]);
    else
        parse_verilator(argv[optind]);
    regroup_items();
    generate_cpp();
    return 0;
}
