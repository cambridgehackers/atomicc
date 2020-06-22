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
#include <ctype.h>
#include "portal.h"
#include "readElfSection.h"
#define MAX_READ_LINE 1024
#define MAX_WIDTH 10
#define MAX_FORMAT 1000

typedef struct {
    int   index;
    char *format;
    int wCount;
    int width[MAX_WIDTH];
} PrintfInfo;
static PrintfInfo printfFormat[MAX_FORMAT];
static int printfFormatIndex;

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

int lookupFormat(int printfNumber)
{
    for (int i = 0; i < printfFormatIndex; i++) {
         if (printfFormat[i].index == printfNumber)
             return i;
    }
printf("[%s:%d] Missing format %d\n", __FUNCTION__, __LINE__, printfNumber);
    return 0;
}

static void atomiccPrintfHandler(struct PortalInternal *p, unsigned int header)
{
    int len = header & 0xffff;
    int tmpfd;
    p->transport->recv(p, &p->map_base[1], len - 1, &tmpfd);
    unsigned short *data = ((unsigned short *)p->map_base) + 2;
    int printfNumber = *data++;
    assert(printfNumber > 0);
    char format[MAX_READ_LINE];
    int findex = lookupFormat(printfNumber);
    sprintf(format, "RUNTIME: %s", printfFormat[findex].format);
    printf("[%s:%d] len %x header %x format %d = '%s'\n", __FUNCTION__, __LINE__, len, header, printfNumber, format);
memdump((unsigned char *)p->map_base, len * sizeof(p->map_base[0]), "PRINTIND");
    int params[100], *pparam = params, *pdata = (int *)data;
    for (int i = 0; i < printfFormat[findex].wCount; i++) {
        (void) memdump;
        memcpy(pparam, pdata, sizeof(*pparam));
        pparam++;
        pdata++;
    }
    printf(format, params[0], params[1], params[2], params[3],
        params[4], params[5], params[6]);
}

#ifdef ANDROID
extern "C"
#endif
char *getExecutionFilename(char *buf, int buflen);
void atomiccPrintfInit(const char *filename)
{
    connectalPrintfHandler = atomiccPrintfHandler;
    printfFormat[printfFormatIndex++].format = "NONE";
    char buf[MAX_READ_LINE];
    char *bufp;
    uint8_t *currentp, *bufend;
    int lineNumber = 0;
    unsigned long buflen = 0;
    char fbuffer[MAX_READ_LINE];
    char *fname = getExecutionFilename(fbuffer, sizeof(fbuffer)-1);
printf("[%s:%d] %s fn %s\n", __FUNCTION__, __LINE__, filename, fname);
    if (readElfSection(fname, "printfdata", &currentp, &buflen)) {
        printf("atomiccPrintfInit: unable to find printdata in '%s'\n", fname);
        return;
    }
    bufend = currentp + buflen;
    while (1) {
        bufp = buf;
        do {
            while (*currentp && *currentp != '\n' && currentp < bufend)
                *bufp++ = *currentp++;
            *bufp = 0;
            currentp++;
printf("[%s:%d] read '%s'\n", __FUNCTION__, __LINE__, buf);
        } while (currentp < bufend && !buf[0]);
        bufp = buf;
        lineNumber++;
        if (!buf[0])
            break;
        if (*bufp++ != '"') {
            printf("[%s:%d] formaterror '%s' current %p end %p\n", __FUNCTION__, __LINE__, buf, currentp, bufend);
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
        printfFormat[printfFormatIndex].index = printfFormatIndex; // starts at 1;
        printfFormat[printfFormatIndex].format = strdup(&buf[1]);
        printfFormat[printfFormatIndex].wCount = 0;
        while (*bufp) {
            while (*bufp == ' ')
                bufp++;
            if (isdigit(*bufp)) {
                printfFormat[printfFormatIndex].width[printfFormat[printfFormatIndex].wCount++] = atoi(bufp);
            }
            while(isdigit(*bufp))
                bufp++;
        }
        printf("[%s:%d] format %d = '%s'", __FUNCTION__, __LINE__, printfFormatIndex, printfFormat[printfFormatIndex].format);
        for (int i = 0; i < printfFormat[printfFormatIndex].wCount; i++)
             printf(" width=%d", printfFormat[printfFormatIndex].width[i]);
        printf("\n");
        printfFormatIndex++;
    }
}
