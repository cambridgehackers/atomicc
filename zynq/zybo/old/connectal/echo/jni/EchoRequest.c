#include "GeneratedTypes.h"

int EchoRequest_say ( struct PortalInternal *p, const uint32_t v )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_EchoRequest_say, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_EchoRequest_say, "EchoRequest_say")) return 1;
    p->transport->write(p, &temp_working_addr, v);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_EchoRequest_say << 16) | 2, -1);
    return 0;
};

int EchoRequest_say2 ( struct PortalInternal *p, const uint16_t a, const uint16_t b )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_EchoRequest_say2, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_EchoRequest_say2, "EchoRequest_say2")) return 1;
    p->transport->write(p, &temp_working_addr, b|(((unsigned long)a)<<16));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_EchoRequest_say2 << 16) | 2, -1);
    return 0;
};

int EchoRequest_setLeds ( struct PortalInternal *p, const uint8_t v )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_EchoRequest_setLeds, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_EchoRequest_setLeds, "EchoRequest_setLeds")) return 1;
    p->transport->write(p, &temp_working_addr, v);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_EchoRequest_setLeds << 16) | 2, -1);
    return 0;
};

EchoRequestCb EchoRequestProxyReq = {
    portal_disconnect,
    EchoRequest_say,
    EchoRequest_say2,
    EchoRequest_setLeds,
};
EchoRequestCb *pEchoRequestProxyReq = &EchoRequestProxyReq;

const uint32_t EchoRequest_reqinfo = 0x30008;
const char * EchoRequest_methodSignatures()
{
    return "{\"setLeds\": [\"long\"], \"say\": [\"long\"], \"say2\": [\"long\", \"long\"]}";
}

int EchoRequest_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    EchoRequestData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    volatile unsigned int* temp_working_addr = p->transport->mapchannelInd(p, channel);
    switch (channel) {
    case CHAN_NUM_EchoRequest_say: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.say.v = (uint32_t)(((tmp)&0xfffffffful));
        ((EchoRequestCb *)p->cb)->say(p, tempdata.say.v);
      } break;
    case CHAN_NUM_EchoRequest_say2: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.say2.b = (uint16_t)(((tmp)&0xfffful));
        tempdata.say2.a = (uint16_t)(((tmp>>16)&0xfffful));
        ((EchoRequestCb *)p->cb)->say2(p, tempdata.say2.a, tempdata.say2.b);
      } break;
    case CHAN_NUM_EchoRequest_setLeds: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setLeds.v = (uint8_t)(((tmp)&0xfful));
        ((EchoRequestCb *)p->cb)->setLeds(p, tempdata.setLeds.v);
      } break;
    default:
        PORTAL_PRINTF("EchoRequest_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("EchoRequest_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
