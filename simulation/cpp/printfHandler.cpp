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
#include "portal.h"

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
static void atomiccPrintfHandler(struct PortalInternal *p, unsigned int header)
{
    int len = header & 0xffff;
    int tmpfd;
    p->transport->recv(p, &p->map_base[1], len - 1, &tmpfd);
printf("[%s:%d] header %x\n", __FUNCTION__, __LINE__, header);
memdump((unsigned char *)p->map_base, len * sizeof(p->map_base[0]), "PRINTIND");
}

void atomiccPrintfInit(void)
{
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
connectalPrintfHandler = atomiccPrintfHandler;
}
