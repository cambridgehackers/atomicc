#include "GeneratedTypes.h"
#ifndef _ECHOINDICATION_H_
#define _ECHOINDICATION_H_
#include "portal.h"

class EchoIndicationProxy : public Portal {
    EchoIndicationCb *cb;
public:
    EchoIndicationProxy(int id, int tile = DEFAULT_TILE, EchoIndicationCb *cbarg = &EchoIndicationProxyReq, int bufsize = EchoIndication_reqinfo, PortalPoller *poller = 0) :
        Portal(id, tile, bufsize, NULL, NULL, this, poller), cb(cbarg) {};
    EchoIndicationProxy(int id, PortalTransportFunctions *transport, void *param, EchoIndicationCb *cbarg = &EchoIndicationProxyReq, int bufsize = EchoIndication_reqinfo, PortalPoller *poller = 0) :
        Portal(id, DEFAULT_TILE, bufsize, NULL, NULL, transport, param, this, poller), cb(cbarg) {};
    EchoIndicationProxy(int id, PortalPoller *poller) :
        Portal(id, DEFAULT_TILE, EchoIndication_reqinfo, NULL, NULL, NULL, NULL, this, poller), cb(&EchoIndicationProxyReq) {};
    int heard ( const uint32_t v ) { return cb->heard (&pint, v); };
    int heard2 ( const uint16_t a, const uint16_t b ) { return cb->heard2 (&pint, a, b); };
};

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
};
#endif // _ECHOINDICATION_H_
