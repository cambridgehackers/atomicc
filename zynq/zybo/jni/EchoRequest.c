#include "GeneratedTypes.h"

int EchoRequest_say ( struct PortalInternal *p, const uint32_t v )
{
    unsigned int temp_working_addr_start[2 + 1] = {0, (CHAN_NUM_EchoRequest_say << 16) | 2,
            (unsigned int)(v)};
    p->transport->send(p, temp_working_addr_start + 2, (CHAN_NUM_EchoRequest_say << 16) | 2, -1);
    return 0;
};

int EchoRequest_say2 ( struct PortalInternal *p, const uint16_t a, const uint16_t b )
{
    unsigned int temp_working_addr_start[2 + 1] = {0, (CHAN_NUM_EchoRequest_say2 << 16) | 2,
            (unsigned int)(b|(((unsigned long)a)<<16))};
    p->transport->send(p, temp_working_addr_start + 2, (CHAN_NUM_EchoRequest_say2 << 16) | 2, -1);
    return 0;
};

int EchoRequest_setLeds ( struct PortalInternal *p, const uint8_t v )
{
    unsigned int temp_working_addr_start[2 + 1] = {0, (CHAN_NUM_EchoRequest_setLeds << 16) | 2,
            (unsigned int)(v)};
    p->transport->send(p, temp_working_addr_start + 2, (CHAN_NUM_EchoRequest_setLeds << 16) | 2, -1);
    return 0;
};

int EchoRequest_zsay4 ( struct PortalInternal *p )
{
    unsigned int temp_working_addr_start[1 + 1] = {0, (CHAN_NUM_EchoRequest_zsay4 << 16) | 1,
    };
    p->transport->send(p, temp_working_addr_start + 2, (CHAN_NUM_EchoRequest_zsay4 << 16) | 1, -1);
    return 0;
};

EchoRequestCb EchoRequestProxyReq = {
    portal_disconnect,
    EchoRequest_say,
    EchoRequest_say2,
    EchoRequest_setLeds,
    EchoRequest_zsay4,
};
EchoRequestCb *pEchoRequestProxyReq = &EchoRequestProxyReq;

const uint32_t EchoRequest_reqinfo = 0x40008;
