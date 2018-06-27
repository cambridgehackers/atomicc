__interface Mmcme2clk {
    __input  __int(1)        IN1;
    __input  __int(1)        IN2;
    __input  __int(1)        INSEL;
    __output __int(1)        INSTOPPED;
    __output __int(1)        OUT0;
    __output __int(1)        OUT0B;
    __output __int(1)        OUT1;
    __output __int(1)        OUT1B;
    __output __int(1)        OUT2;
    __output __int(1)        OUT2B;
    __output __int(1)        OUT3;
    __output __int(1)        OUT3B;
    __output __int(1)        OUT4;
    __output __int(1)        OUT5;
    __output __int(1)        OUT6;
};
__interface Mmcme2clkfb {
    __input  __int(1)        IN;
    __output __int(1)        OUT;
    __output __int(1)        OUTB;
    __output __int(1)        STOPPED;
};
__interface Mmcme2d {
    __input  __int(1)        ADDR;
    __input  __int(1)        CLK;
    __input  __int(1)        EN;
    __input  __int(1)        I;
    __output __int(1)        O;
    __output __int(1)        RDY;
    __input  __int(1)        WE;
};
__interface Mmcme2ps {
    __input  __int(1)        CLK;
    __output __int(1)        DONE;
    __input  __int(1)        EN;
    __input  __int(1)        INCDEC;
};
__interface MMCME2_ADV {
    __input  Mmcme2clkfb     CLKFB;
    __input  Mmcme2clk       CLK;
    __input  Mmcme2d         D;
    __output __int(1)        LOCKED;
    __input  Mmcme2ps        PS;
    __input  __int(1)        PWRDWN;
    __input  __int(1)        RST;
};
