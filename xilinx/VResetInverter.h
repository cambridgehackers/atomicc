__interface MResetInverterResetInverter {
    __input  __uint(1)        RESET_IN;
    __output __uint(1)        RESET_OUT;
};
__emodule ResetInverter {
    MResetInverterResetInverter _;
};
