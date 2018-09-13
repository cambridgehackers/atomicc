__interface MIOBUF {
    __parameter int              DRIVE;
    __input  __uint(1)        I;
    __parameter const char *     IBUF_LOW_PWR;
    __inout  __uint(1)        IO;
    __parameter const char *     IOSTANDARD;
    __output __uint(1)        O;
    __parameter const char *     SLEW;
    __input  __uint(1)        T;
};
__emodule IOBUF {
    MIOBUF _;
};
