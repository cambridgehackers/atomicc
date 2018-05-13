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
#include <assert.h>
#include "portal.h"
#include <string>
#include <list>
#include <map>
#define MAX_READ_LINE 1024

static void memdump(unsigned char *p, int len, const char *title)
{
int i;

    i = 0;
    while (len > 0) {
        if (!(i & 0xf)) {
            if (i > 0)
                fprintf(stderr, "\n");
            fprintf(stderr, "%s: ",title);
        }
        fprintf(stderr, "%02x ", *p++);
        i++;
        len--;
    }
    fprintf(stderr, "\n");
}
typedef struct {
    std::string format;
    std::list<int> width;
} PrintfInfo;
std::map<int, PrintfInfo> printfFormat;

static void atomiccPrintfHandler(struct PortalInternal *p, unsigned int header)
{
    int len = header & 0xffff;
    int tmpfd;
    p->transport->recv(p, &p->map_base[1], len - 1, &tmpfd);
    unsigned short *data = ((unsigned short *)p->map_base) + 2;
    int printfNumber = *data++;
    assert(printfNumber >= 0);
    std::string forstr = "RUNTIME: " + printfFormat[printfNumber].format;
    const char *format = forstr.c_str();
    //printf("[%s:%d] header %x format %d = '%s'\n", __FUNCTION__, __LINE__, header, printfNumber, format);
//memdump((unsigned char *)p->map_base, len * sizeof(p->map_base[0]), "PRINTIND");
    int params[100], *pparam = params, *pdata = (int *)data;
    for (auto item: printfFormat[printfNumber].width) {
        memcpy(pparam, pdata, sizeof(*pparam));
        pparam++;
        pdata++;
    }
    printf(format, params[0], params[1], params[2], params[3],
        params[4], params[5], params[6]);
}

void atomiccPrintfInit(const char *filename)
{
printf("[%s:%d] %s\n", __FUNCTION__, __LINE__, filename);
    char buf[MAX_READ_LINE];
    char *bufp;
    int lineNumber = 0;
    int printfNumber = 1;
    connectalPrintfHandler = atomiccPrintfHandler;
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("atomiccPrintfInit: unable to open '%s'\n", filename);
        exit(-1);
    }
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        buf[strlen(buf) - 1] = 0;
        bufp = buf;
        lineNumber++;
        if (*bufp++ != '"') {
            printf("[%s:%d] formaterror '%s'\n", __FUNCTION__, __LINE__, buf);
            exit(-1);
        }
        while (*bufp && *bufp != '"') {
            bufp++;
            if (*bufp == '\\')
                bufp += 2;
        }
        if (*bufp != '"') {
            printf("[%s:%d] formaterror '%s'\n", __FUNCTION__, __LINE__, buf);
            exit(-1);
        }
        *bufp++ = 0;
        std::string format(&buf[1]);
        int ind;
        while ((ind = format.find("\\n")) != -1) {
            format = format.substr(0,ind) + "\n" + format.substr(ind+2);
        }
        printfFormat[printfNumber] = PrintfInfo{format};
        while (*bufp) {
            while (*bufp == ' ')
                bufp++;
            if (isdigit(*bufp))
                printfFormat[printfNumber].width.push_back(atoi(bufp));
            while(isdigit(*bufp))
                bufp++;
        }
        //printf("[%s:%d] format %d = '%s'", __FUNCTION__, __LINE__, printfNumber, printfFormat[printfNumber].format.c_str());
        for (auto item: printfFormat[printfNumber].width)
             printf(" width=%d", item);
        printf("\n");
        printfNumber++;
    }
    fclose(fp);
}
