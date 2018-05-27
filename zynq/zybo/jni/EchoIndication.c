#include "GeneratedTypes.h"

const uint32_t EchoIndication_reqinfo = 0x30010;
const char * EchoIndication_methodSignatures()
{
    return "{\"heard2\": [\"long\", \"long\"], \"heard\": [\"long\"], \"heard3\": [\"long\", \"long\", \"long\", \"long\"]}";
}

int EchoIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int tmpfd __attribute__ ((unused));
    EchoIndicationData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    unsigned int temp_working_addr[REQINFO_SIZE(EchoIndication_reqinfo)];
    switch (channel) {
    case CHAN_NUM_EchoIndication_heard: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tempdata.heard.v = (uint32_t)(((temp_working_addr[0])&0xfffffffful));
        ((EchoIndicationCb *)p->cb)->heard(p, tempdata.heard.v);
      } break;
    case CHAN_NUM_EchoIndication_heard2: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tempdata.heard2.b = (uint16_t)(((temp_working_addr[0])&0xfffful));
        tempdata.heard2.a = (uint16_t)(((temp_working_addr[0]>>16)&0xfffful));
        ((EchoIndicationCb *)p->cb)->heard2(p, tempdata.heard2.a, tempdata.heard2.b);
      } break;
    case CHAN_NUM_EchoIndication_heard3: {
        p->transport->recv(p, temp_working_addr, 3, &tmpfd);
        tempdata.heard3.b = (uint32_t)(((uint32_t)(((temp_working_addr[0])&0xfffful))<<16));
        tempdata.heard3.a = (uint16_t)(((temp_working_addr[0]>>16)&0xfffful));
        tempdata.heard3.c = (uint32_t)(((uint32_t)(((temp_working_addr[1])&0xfffful))<<16));
        tempdata.heard3.b |= (uint32_t)(((temp_working_addr[1]>>16)&0xfffful));
        tempdata.heard3.d = (uint16_t)(((temp_working_addr[2])&0xfffful));
        tempdata.heard3.c |= (uint32_t)(((temp_working_addr[2]>>16)&0xfffful));
        ((EchoIndicationCb *)p->cb)->heard3(p, tempdata.heard3.a, tempdata.heard3.b, tempdata.heard3.c, tempdata.heard3.d);
      } break;
    default:
        PORTAL_PRINTF("EchoIndication_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("EchoIndication_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
