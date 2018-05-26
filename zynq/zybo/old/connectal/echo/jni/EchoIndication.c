#include "GeneratedTypes.h"

int EchoIndication_heard ( struct PortalInternal *p, const uint32_t v )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_EchoIndication_heard, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_EchoIndication_heard, "EchoIndication_heard")) return 1;
    p->transport->write(p, &temp_working_addr, v);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_EchoIndication_heard << 16) | 2, -1);
    return 0;
};

int EchoIndication_heard2 ( struct PortalInternal *p, const uint16_t a, const uint16_t b )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_EchoIndication_heard2, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_EchoIndication_heard2, "EchoIndication_heard2")) return 1;
    p->transport->write(p, &temp_working_addr, b|(((unsigned long)a)<<16));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_EchoIndication_heard2 << 16) | 2, -1);
    return 0;
};

EchoIndicationCb EchoIndicationProxyReq = {
    portal_disconnect,
    EchoIndication_heard,
    EchoIndication_heard2,
};
EchoIndicationCb *pEchoIndicationProxyReq = &EchoIndicationProxyReq;

const uint32_t EchoIndication_reqinfo = 0x20008;
const char * EchoIndication_methodSignatures()
{
    return "{\"heard2\": [\"long\", \"long\"], \"heard\": [\"long\"]}";
}

int EchoIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    EchoIndicationData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    volatile unsigned int* temp_working_addr = p->transport->mapchannelInd(p, channel);
    switch (channel) {
    case CHAN_NUM_EchoIndication_heard: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.heard.v = (uint32_t)(((tmp)&0xfffffffful));
        ((EchoIndicationCb *)p->cb)->heard(p, tempdata.heard.v);
      } break;
    case CHAN_NUM_EchoIndication_heard2: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.heard2.b = (uint16_t)(((tmp)&0xfffful));
        tempdata.heard2.a = (uint16_t)(((tmp>>16)&0xfffful));
        ((EchoIndicationCb *)p->cb)->heard2(p, tempdata.heard2.a, tempdata.heard2.b);
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
