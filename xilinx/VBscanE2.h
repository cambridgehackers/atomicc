class MBSCANE2IFC {
    __output __uint(1)        CAPTURE;
    __parameter const char *     DISABLE_JTAG;
    __output __uint(1)        DRCK;
    __parameter int              JTAG_CHAIN;
    __output __uint(1)        RESET;
    __output __uint(1)        RUNTEST;
    __output __uint(1)        SEL;
    __output __uint(1)        SHIFT;
    __output __uint(1)        TCK;
    __output __uint(1)        TDI;
    __input  __uint(1)        TDO;
    __output __uint(1)        TMS;
    __output __uint(1)        UPDATE;
};
class BSCANE2 __implements MBSCANE2IFC;
