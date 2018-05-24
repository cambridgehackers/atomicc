#include "GeneratedTypes.h"

#ifndef NO_CPP_PORTAL_CODE
extern const uint32_t ifcNamesNone = IfcNamesNone;
extern const uint32_t ifcNames_EchoIndicationH2S = IfcNames_EchoIndicationH2S;
extern const uint32_t ifcNames_EchoRequestS2H = IfcNames_EchoRequestS2H;

/************** Start of EchoRequestWrapper CPP ***********/
#include "EchoRequest.h"
int EchoRequestdisconnect_cb (struct PortalInternal *p) {
    (static_cast<EchoRequestWrapper *>(p->parent))->disconnect();
    return 0;
};
int EchoRequestsay_cb (  struct PortalInternal *p, const uint32_t v ) {
    (static_cast<EchoRequestWrapper *>(p->parent))->say ( v);
    return 0;
};
int EchoRequestsay2_cb (  struct PortalInternal *p, const uint16_t a, const uint16_t b ) {
    (static_cast<EchoRequestWrapper *>(p->parent))->say2 ( a, b);
    return 0;
};
int EchoRequestsetLeds_cb (  struct PortalInternal *p, const uint8_t v ) {
    (static_cast<EchoRequestWrapper *>(p->parent))->setLeds ( v);
    return 0;
};
EchoRequestCb EchoRequest_cbTable = {
    EchoRequestdisconnect_cb,
    EchoRequestsay_cb,
    EchoRequestsay2_cb,
    EchoRequestsetLeds_cb,
};

/************** Start of EchoIndicationWrapper CPP ***********/
#include "EchoIndication.h"
int EchoIndicationdisconnect_cb (struct PortalInternal *p) {
    (static_cast<EchoIndicationWrapper *>(p->parent))->disconnect();
    return 0;
};
int EchoIndicationheard_cb (  struct PortalInternal *p, const uint32_t v ) {
    (static_cast<EchoIndicationWrapper *>(p->parent))->heard ( v);
    return 0;
};
int EchoIndicationheard2_cb (  struct PortalInternal *p, const uint16_t a, const uint16_t b ) {
    (static_cast<EchoIndicationWrapper *>(p->parent))->heard2 ( a, b);
    return 0;
};
EchoIndicationCb EchoIndication_cbTable = {
    EchoIndicationdisconnect_cb,
    EchoIndicationheard_cb,
    EchoIndicationheard2_cb,
};
#endif //NO_CPP_PORTAL_CODE
