#include "GeneratedTypes.h"

#ifndef NO_CPP_PORTAL_CODE
extern const uint32_t ifcNames_EchoIndicationH2S = IfcNames_EchoIndicationH2S;
extern const uint32_t ifcNames_EchoRequestS2H = IfcNames_EchoRequestS2H;

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
int EchoIndicationheard3_cb (  struct PortalInternal *p, const uint16_t a, const uint32_t b, const uint32_t c, const uint16_t d ) {
    (static_cast<EchoIndicationWrapper *>(p->parent))->heard3 ( a, b, c, d);
    return 0;
};
EchoIndicationCb EchoIndication_cbTable = {
    EchoIndicationdisconnect_cb,
    EchoIndicationheard_cb,
    EchoIndicationheard2_cb,
    EchoIndicationheard3_cb,
};
#endif //NO_CPP_PORTAL_CODE
