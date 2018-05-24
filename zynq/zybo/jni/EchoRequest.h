#include "GeneratedTypes.h"
#ifndef _ECHOREQUEST_H_
#define _ECHOREQUEST_H_
#include "portal.h"

class EchoRequestProxy : public Portal {
    EchoRequestCb *cb;
public:
    EchoRequestProxy(int id, int tile = DEFAULT_TILE, EchoRequestCb *cbarg = &EchoRequestProxyReq, int bufsize = EchoRequest_reqinfo, PortalPoller *poller = 0) :
        Portal(id, tile, bufsize, NULL, NULL, this, poller), cb(cbarg) {};
    EchoRequestProxy(int id, PortalTransportFunctions *transport, void *param, EchoRequestCb *cbarg = &EchoRequestProxyReq, int bufsize = EchoRequest_reqinfo, PortalPoller *poller = 0) :
        Portal(id, DEFAULT_TILE, bufsize, NULL, NULL, transport, param, this, poller), cb(cbarg) {};
    EchoRequestProxy(int id, PortalPoller *poller) :
        Portal(id, DEFAULT_TILE, EchoRequest_reqinfo, NULL, NULL, NULL, NULL, this, poller), cb(&EchoRequestProxyReq) {};
    int say ( const uint32_t v ) { return cb->say (&pint, v); };
    int say2 ( const uint16_t a, const uint16_t b ) { return cb->say2 (&pint, a, b); };
    int setLeds ( const uint8_t v ) { return cb->setLeds (&pint, v); };
};

extern EchoRequestCb EchoRequest_cbTable;
class EchoRequestWrapper : public Portal {
public:
    EchoRequestWrapper(int id, int tile = DEFAULT_TILE, PORTAL_INDFUNC cba = EchoRequest_handleMessage, int bufsize = EchoRequest_reqinfo, PortalPoller *poller = 0) :
           Portal(id, tile, bufsize, cba, (void *)&EchoRequest_cbTable, this, poller) {
    };
    EchoRequestWrapper(int id, PortalTransportFunctions *transport, void *param, PORTAL_INDFUNC cba = EchoRequest_handleMessage, int bufsize = EchoRequest_reqinfo, PortalPoller *poller=0):
           Portal(id, DEFAULT_TILE, bufsize, cba, (void *)&EchoRequest_cbTable, transport, param, this, poller) {
    };
    EchoRequestWrapper(int id, PortalPoller *poller) :
           Portal(id, DEFAULT_TILE, EchoRequest_reqinfo, EchoRequest_handleMessage, (void *)&EchoRequest_cbTable, this, poller) {
    };
    EchoRequestWrapper(int id, PortalTransportFunctions *transport, void *param, PortalPoller *poller):
           Portal(id, DEFAULT_TILE, EchoRequest_reqinfo, EchoRequest_handleMessage, (void *)&EchoRequest_cbTable, transport, param, this, poller) {
    };
    virtual void disconnect(void) {
        printf("EchoRequestWrapper.disconnect called %d\n", pint.client_fd_number);
    };
    virtual void say ( const uint32_t v ) = 0;
    virtual void say2 ( const uint16_t a, const uint16_t b ) = 0;
    virtual void setLeds ( const uint8_t v ) = 0;
};
#endif // _ECHOREQUEST_H_
