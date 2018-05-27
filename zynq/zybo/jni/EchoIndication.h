#include "GeneratedTypes.h"
#ifndef _ECHOINDICATION_H_
#define _ECHOINDICATION_H_
#include "portal.h"

extern EchoIndicationCb EchoIndication_cbTable;
class EchoIndicationWrapper : public Portal {
public:
    EchoIndicationWrapper(int id, int tile = DEFAULT_TILE, PORTAL_INDFUNC cba = EchoIndication_handleMessage, int bufsize = EchoIndication_reqinfo, PortalPoller *poller = 0) :
           Portal(id, tile, bufsize, cba, (void *)&EchoIndication_cbTable, this, poller) {
    };
    EchoIndicationWrapper(int id, PortalTransportFunctions *transport, void *param, PORTAL_INDFUNC cba = EchoIndication_handleMessage, int bufsize = EchoIndication_reqinfo, PortalPoller *poller=0):
           Portal(id, DEFAULT_TILE, bufsize, cba, (void *)&EchoIndication_cbTable, transport, param, this, poller) {
    };
    EchoIndicationWrapper(int id, PortalPoller *poller) :
           Portal(id, DEFAULT_TILE, EchoIndication_reqinfo, EchoIndication_handleMessage, (void *)&EchoIndication_cbTable, this, poller) {
    };
    EchoIndicationWrapper(int id, PortalTransportFunctions *transport, void *param, PortalPoller *poller):
           Portal(id, DEFAULT_TILE, EchoIndication_reqinfo, EchoIndication_handleMessage, (void *)&EchoIndication_cbTable, transport, param, this, poller) {
    };
    virtual void disconnect(void) {
        printf("EchoIndicationWrapper.disconnect called %d\n", pint.client_fd_number);
    };
    virtual void heard ( const uint32_t v ) = 0;
    virtual void heard2 ( const uint16_t a, const uint16_t b ) = 0;
    virtual void heard3 ( const uint16_t a, const uint32_t b, const uint32_t c, const uint16_t d ) = 0;
};
#endif // _ECHOINDICATION_H_
