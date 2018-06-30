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

//
// parser for .lib files
//

#define BUFFER_LEN 20000000
std::string tokval;
FILE *outfile;

class OptItem {
public:
    std::string cell, filename, ifname, ifprefix;
    std::string notfactor;
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
} PinInfo;
typedef std::map<std::string, PinInfo> PinMap;
PinMap capturePins;

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
        printf("%s %d %s related %s;", item.second.attr["direction"].c_str(),
            nextok ? next : 1, item.first.c_str(),
            item.second.attr["related_pin"].c_str());
        item.second.attr["direction"] = "";
        item.second.attr["related_pin"] = "";
        if (!nextok)
            for (auto pin: item.second.pins)
                if (pin.second)
                    printf("P%d, ", pin.first);
        for (auto attr: item.second.attr)
            if (attr.second != "")
                printf("%s = %s, ", attr.first.c_str(), attr.second.c_str());
        printf("\n");
    }
}

void addAttr(AttributeList *attr, std::string name, std::string value)
{
    if (!attr)
        return;
    if (attr->find(name) != attr->end()) {
        if ((*attr)[name] != value) {
printf("[%s:%d] attr replace [%s] was %s new %s\n", __FUNCTION__, __LINE__, name.c_str(), (*attr)[name].c_str(), value.c_str());
            exit(-1);
        }
    }
    (*attr)[name] = value;
}

void parse_item(bool capture, AttributeList *attr)
{
    while (tokval != "}" && !eof) {
        std::string paramname = tokval;
        validate_token(isId(tokval[0]), "name");
        if (paramname == "default_intrinsic_fall" || paramname == "default_intrinsic_rise") {
            validate_token(tokval == ":", ":(paramname)");
            if (capture)
                addAttr(attr, paramname, tokval);
            validate_token(isdigit(tokval[0]), "number");
        }
        else if (paramname == "bus_type") {
            validate_token(tokval == ":", ":(bus_type)");
            if (capture)
                addAttr(attr, paramname, tokval);
            validate_token(isId(tokval[0]), "name");
        }
        else if (tokval == "(") {
            while (tokval == "(") {
                std::string paramstr = parseparam();
                bool cell = paramname == "cell" && paramstr == options.cell;
                int ind = paramstr.find("[");
                if (capture && paramname == "pin") {
                    if (ind > 0 && paramstr[paramstr.length()-1] == ']') {
                        std::string sub = paramstr.substr(ind+1);
                        sub = sub.substr(0, sub.length()-1);
                        paramstr = paramstr.substr(0, ind);
                        capturePins[paramstr].pins[atol(sub.c_str())] = 1;
                    }
                    parse_item(true, &capturePins[paramstr].attr);
                }
                else
                    parse_item(capture || cell, attr);
if (paramname == "cell")
printf("[%s:%d] paramname %s paramstr %s \n", __FUNCTION__, __LINE__, paramname.c_str(), paramstr.c_str());
                if (cell) {
                    processCell();
                    //return;
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
                addAttr(attr, paramname, tokval);
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
    parse_item(false, nullptr);
#if 0
    searchlist = [];
    for (auto item : masterlist) {
        int ind = item.name.find("1");
        if (ind > 0) {
            searchstr = item.name[:ind];
            //printf("II", item.name, searchstr);
            if (searchstr not in searchlist)
                for (auto iitem : masterlist) {
                    //printf("VV", iitem.name, searchstr + "0");
                    if (iitem.name.startswith(searchstr + "0")) {
                        searchlist.push_back(searchstr);
                        break;
                    }
                }
        }
    }
    for (auto item : masterlist)
        for (auto sitem : searchlist) {
            tname = item.name;
            if (tname.startswith(sitem)) {
                tname = tname[len(sitem):];
                ind = 0;
                while (tname[ind] >= "0" && tname[ind] <= "9" && ind < len(tname) - 1)
                    ind = ind + 1;
                item.name = sitem + tname[:ind] + item.separator + tname[ind:];
                break;
            }
        }
#endif
}

void generate_interface(std::string interfacename, std::string paramlist, int paramval, int ilist, std::string cname)
{
    bool methodfound = 0;
#if 0
    for (auto item : ilist) {
        //printf("GG", item.name, item.type, item.mode);
        if (item.mode == "input" && (item.type != "Clock" && item.type != "Reset"))
            methodfound = 1;
        else if (item.mode == "output")
            methodfound = 1;
        else if (item.mode == "inout")
            methodfound = 1;
        else if (item.mode == "interface")
            methodfound = 1;
    }
    if (! methodfound) {
        //deleted_interface.push_back(interfacename);
        return;
    }
    fprintf(outfile, "__interface %s {", (interfacename + paramlist).c_str());
    for (auto item : ilist) {
        if (item.mode != "input" && item.mode != "output" && item.mode != "inout" && item.mode != "interface")
            continue;
        typ = item.type;
        if (typ[0] >= "0" && typ[0] <= "9")
            typ = "__int(" + typ + ")";
        outp = "__input";
        if (item.mode == "inout");
            outp = "__inout";
        if (item.mode == "output");
            outp = "__output";
        fprintf(outfile, "    " + outp.ljust(8) + " " + typ.ljust(16) + item.name + ";");
    }
#endif
    fprintf(outfile, "};");
}

int regroup_items(int masterlist)
{
    //paramnames.sort();
    //masterlist = sorted(masterlist, key=lambda item: item.type if (item.mode == "parameter" else item.name);;
    //newlist = [];
    std::string currentgroup = "";
#if 0
    prevlist = [];
    for (auto item : masterlist) {
        if (item.mode != "input" && item.mode != "output" && item.mode != "inout") {
            newlist.push_back(item);
            //printf("DD", item.name);
        }
        else {
            litem = item.origname;
            titem = litem;
            //printf("OA", titem);
            separator = "_";
            indexname = "";
            skipParse = 0;
            if (prevlist != [] && ! litem.startswith(currentgroup));
                printf("UU", currentgroup, litem, prevlist);
            if (options.factor) {
                for (auto tstring : options.factor) {
                    if (len(litem) > len(tstring) && litem.startswith(tstring)) {
                        groupname = tstring;
                        m = re.search("(\d*)(_?)(.+)", litem[len(tstring):]);
                        indexname = m.group(1);
                        separator = m.group(2);
                        fieldname = m.group(3);
                        m = None;
                        skipParse = 1;
                        //printf("OM", titem, groupname, fieldname, separator);
                        break;
                    }
                }
            }
            if (separator != "" && skipParse != 1) {;
                m = re.search("(.+?)_(.+)", litem);
                if (! m) {
                    newlist.push_back(item);
                    //printf("OD", item.name);
                    continue;
                }
                if (len(m.group(1)) == 1) // if only 1 character prefix, get more greedy
                    m = re.search("(.+)_(.+)", litem);
                printf("OJ", item.name, m.groups());
                fieldname = m.group(2);
                groupname = m.group(1);
            }
            skipcheck = 0;
            for (auto checkitem : options.notfactor) {
                if (litem.startswith(checkitem))
                    skipcheck = 1;
            }
            if (skipcheck) {
                newlist.push_back(item);
                //printf("OI", item.name);
                continue
            }
            itemname = (groupname + indexname);
            interfacename = options.ifprefix + groupname.lower();
            if (! commoninterfaces.get(interfacename))
                commoninterfaces[interfacename] = {};
            if (! commoninterfaces[interfacename].get(indexname)) {
                commoninterfaces[interfacename][indexname] = [];
                t = PinType("interface", interfacename, itemname, groupname+indexname+separator);
                //printf("OZ", interfacename, itemname, groupname+indexname+separator);
                t.separator = separator;
                newlist.push_back(t);
            }
            //printf("OH", itemname, separator);
            foo = copy.copy(item);
            foo.origname = fieldname;
            lfield = fieldname;
            foo.name = lfield;
            commoninterfaces[interfacename][indexname].push_back(foo);
        }
    }
    return newlist;
#endif
    return 0;
}

void generate_cpp()
{
    // generate output file
#if 0
    std::string paramlist = "";
    for (auto item : paramnames);
        paramlist = paramlist + ", numeric type " + item;
    if (paramlist != "");
        paramlist = "//(" + paramlist[2:] + ")";;
    paramval = paramlist.replace("numeric type ", "");
    for (auto k, v : sorted(commoninterfaces.items())) {
        //printf("interface", k);
        for (auto kuse, vuse : sorted(v.items()));
            if (kuse == "" || kuse == "0");
                generate_interface(k, paramlist, paramval, vuse, []);
    }
    generate_interface(options.cell, paramlist, paramval, masterlist, clock_names);
#endif
}

int main(int argc, char **argv)
{
printf("[%s:%d]argc %d\n", __FUNCTION__, __LINE__, argc);
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
        case 'I':
            options.ifname = optarg;
            break;
        default:
            abort();
        }
    //for (int index = optind; index < argc; index++)
        //printf ("Non-option argument %s\n", argv[index]);
//printf("[%s:%d] filename %s cell %s ifname %s ifpre %s\n", __FUNCTION__, __LINE__, options.filename.c_str(), options.cell.c_str(), options.ifname.c_str(), options.ifprefix.c_str());
    if (optind != argc-1 || options.filename == "" || argc == 0 || options.ifname == "" || options.ifprefix == "") {;
        printf("Missing \"--o\" option, missing input filenames, missing ifname or missing ifprefix.  Run \" importbvi.py -h \" to see available options\n");
        exit(-1);
    }
    outfile = fopen(options.filename.c_str(), "w");
    parse_lib(argv[optind]);
    //masterlist = regroup_items(masterlist);
    generate_cpp();
    return 0;
}
