__interface Pps7ddr {
    __inout  __int(15)       A;
    __input  __int(4)        ARB;
    __inout  __int(3)        BA;
    __inout  __int(1)        CASB;
    __inout  __int(1)        CKE;
    __inout  __int(1)        CKN;
    __inout  __int(1)        CKP;
    __inout  __int(1)        CSB;
    __inout  __int(4)        DM;
    __inout  __int(32)       DQ;
    __inout  __int(4)        DQSN;
    __inout  __int(4)        DQSP;
    __inout  __int(1)        DRSTB;
    __inout  __int(1)        ODT;
    __inout  __int(1)        RASB;
    __inout  __int(1)        VRN;
    __inout  __int(1)        VRP;
    __inout  __int(1)        WEB;
};
__interface Pps7dma {
    __input  __int(1)        ACLK;
    __input  __int(1)        DAREADY;
    __output __int(2)        DATYPE;
    __output __int(1)        DAVALID;
    __input  __int(1)        DRLAST;
    __output __int(1)        DRREADY;
    __input  __int(2)        DRTYPE;
    __input  __int(1)        DRVALID;
    __output __int(1)        RSTN;
};
__interface Pps7emiocan {
    __input  __int(1)        PHYRX;
    __output __int(1)        PHYTX;
};
__interface Pps7emioenet {
    __input  __int(1)        EXTINTIN;
    __input  __int(1)        GMIICOL;
    __input  __int(1)        GMIICRS;
    __input  __int(1)        GMIIRXCLK;
    __input  __int(8)        GMIIRXD;
    __input  __int(1)        GMIIRXDV;
    __input  __int(1)        GMIIRXER;
    __input  __int(1)        GMIITXCLK;
    __output __int(8)        GMIITXD;
    __output __int(1)        GMIITXEN;
    __output __int(1)        GMIITXER;
    __input  __int(1)        MDIOI;
    __output __int(1)        MDIOMDC;
    __output __int(1)        MDIOO;
    __output __int(1)        MDIOTN;
    __output __int(1)        PTPDELAYREQRX;
    __output __int(1)        PTPDELAYREQTX;
    __output __int(1)        PTPPDELAYREQRX;
    __output __int(1)        PTPPDELAYREQTX;
    __output __int(1)        PTPPDELAYRESPRX;
    __output __int(1)        PTPPDELAYRESPTX;
    __output __int(1)        PTPSYNCFRAMERX;
    __output __int(1)        PTPSYNCFRAMETX;
    __output __int(1)        SOFRX;
    __output __int(1)        SOFTX;
};
__interface Pps7emiogpio {
    __input  __int(64)       I;
    __output __int(64)       O;
    __output __int(64)       TN;
};
__interface Pps7emioi2c {
    __input  __int(1)        SCLI;
    __output __int(1)        SCLO;
    __output __int(1)        SCLTN;
    __input  __int(1)        SDAI;
    __output __int(1)        SDAO;
    __output __int(1)        SDATN;
};
__interface Pps7emiopjtag {
    __input  __int(1)        TCK;
    __input  __int(1)        TDI;
    __output __int(1)        TDO;
    __output __int(1)        TDTN;
    __input  __int(1)        TMS;
};
__interface Pps7emiosdio {
    __output __int(1)        BUSPOW;
    __output __int(3)        BUSVOLT;
    __input  __int(1)        CDN;
    __output __int(1)        CLK;
    __input  __int(1)        CLKFB;
    __input  __int(1)        CMDI;
    __output __int(1)        CMDO;
    __output __int(1)        CMDTN;
    __input  __int(4)        DATAI;
    __output __int(4)        DATAO;
    __output __int(4)        DATATN;
    __output __int(1)        LED;
    __input  __int(1)        WP;
};
__interface Pps7emiospi {
    __input  __int(1)        MI;
    __output __int(1)        MO;
    __output __int(1)        MOTN;
    __input  __int(1)        SCLKI;
    __output __int(1)        SCLKO;
    __output __int(1)        SCLKTN;
    __input  __int(1)        SI;
    __output __int(1)        SO;
    __input  __int(1)        SSIN;
    __output __int(1)        SSNTN;
    __output __int(3)        SSON;
    __output __int(1)        STN;
};
__interface Pps7emiosramint {
    __input  __int(1)        IN;
};
__interface Pps7emiotrace {
    __input  __int(1)        CLK;
    __output __int(1)        CTL;
    __output __int(32)       DATA;
};
__interface Pps7emiottc {
    __input  __int(3)        CLKI;
    __output __int(3)        WAVEO;
};
__interface Pps7emiouart {
    __input  __int(1)        CTSN;
    __input  __int(1)        DCDN;
    __input  __int(1)        DSRN;
    __output __int(1)        DTRN;
    __input  __int(1)        RIN;
    __output __int(1)        RTSN;
    __input  __int(1)        RX;
    __output __int(1)        TX;
};
__interface Pps7emiousb {
    __output __int(2)        PORTINDCTL;
    __input  __int(1)        VBUSPWRFAULT;
    __output __int(1)        VBUSPWRSELECT;
};
__interface Pps7emiowdt {
    __input  __int(1)        CLKI;
    __output __int(1)        RSTO;
};
__interface Pps7event {
    __input  __int(1)        EVENTI;
    __output __int(1)        EVENTO;
    __output __int(2)        STANDBYWFE;
    __output __int(2)        STANDBYWFI;
};
__interface Pps7fclk {
    __output __int(4)        CLK;
    __input  __int(4)        CLKTRIGN;
    __output __int(4)        RESETN;
};
__interface Pps7fpgaid {
    __input  __int(1)        LEN;
};
__interface Pps7ftmd {
    __input  __int(4)        TRACEINATID;
    __input  __int(1)        TRACEINCLOCK;
    __input  __int(32)       TRACEINDATA;
    __input  __int(1)        TRACEINVALID;
};
__interface Pps7ftmt {
    __input  __int(32)       F2PDEBUG;
    __input  __int(4)        F2PTRIG;
    __output __int(4)        F2PTRIGACK;
    __output __int(32)       P2FDEBUG;
    __output __int(4)        P2FTRIG;
    __input  __int(4)        P2FTRIGACK;
};
__interface Pps7irq {
    __input  __int(20)       F2P;
    __output __int(29)       P2F;
};
__interface Pps7m {
    __inout  __int(54)       IO;
};
__interface Pps7maxigp {
    __input  __int(1)        ACLK;
    __output __int(32)       ARADDR;
    __output __int(2)        ARBURST;
    __output __int(4)        ARCACHE;
    __output __int(1)        ARESETN;
    __output __int(12)       ARID;
    __output __int(4)        ARLEN;
    __output __int(2)        ARLOCK;
    __output __int(3)        ARPROT;
    __output __int(4)        ARQOS;
    __input  __int(1)        ARREADY;
    __output __int(2)        ARSIZE;
    __output __int(1)        ARVALID;
    __output __int(32)       AWADDR;
    __output __int(2)        AWBURST;
    __output __int(4)        AWCACHE;
    __output __int(12)       AWID;
    __output __int(4)        AWLEN;
    __output __int(2)        AWLOCK;
    __output __int(3)        AWPROT;
    __output __int(4)        AWQOS;
    __input  __int(1)        AWREADY;
    __output __int(2)        AWSIZE;
    __output __int(1)        AWVALID;
    __input  __int(12)       BID;
    __output __int(1)        BREADY;
    __input  __int(2)        BRESP;
    __input  __int(1)        BVALID;
    __input  __int(32)       RDATA;
    __input  __int(12)       RID;
    __input  __int(1)        RLAST;
    __output __int(1)        RREADY;
    __input  __int(2)        RRESP;
    __input  __int(1)        RVALID;
    __output __int(32)       WDATA;
    __output __int(12)       WID;
    __output __int(1)        WLAST;
    __input  __int(1)        WREADY;
    __output __int(4)        WSTRB;
    __output __int(1)        WVALID;
};
__interface Pps7ps {
    __inout  __int(1)        CLK;
    __inout  __int(1)        PORB;
    __inout  __int(1)        SRSTB;
};
__interface Pps7saxiacp {
    __input  __int(1)        ACLK;
    __input  __int(32)       ARADDR;
    __input  __int(2)        ARBURST;
    __input  __int(4)        ARCACHE;
    __output __int(1)        ARESETN;
    __input  __int(3)        ARID;
    __input  __int(4)        ARLEN;
    __input  __int(2)        ARLOCK;
    __input  __int(3)        ARPROT;
    __input  __int(4)        ARQOS;
    __output __int(1)        ARREADY;
    __input  __int(2)        ARSIZE;
    __input  __int(5)        ARUSER;
    __input  __int(1)        ARVALID;
    __input  __int(32)       AWADDR;
    __input  __int(2)        AWBURST;
    __input  __int(4)        AWCACHE;
    __input  __int(3)        AWID;
    __input  __int(4)        AWLEN;
    __input  __int(2)        AWLOCK;
    __input  __int(3)        AWPROT;
    __input  __int(4)        AWQOS;
    __output __int(1)        AWREADY;
    __input  __int(2)        AWSIZE;
    __input  __int(5)        AWUSER;
    __input  __int(1)        AWVALID;
    __output __int(3)        BID;
    __input  __int(1)        BREADY;
    __output __int(2)        BRESP;
    __output __int(1)        BVALID;
    __output __int(64)       RDATA;
    __output __int(3)        RID;
    __output __int(1)        RLAST;
    __input  __int(1)        RREADY;
    __output __int(2)        RRESP;
    __output __int(1)        RVALID;
    __input  __int(64)       WDATA;
    __input  __int(3)        WID;
    __input  __int(1)        WLAST;
    __output __int(1)        WREADY;
    __input  __int(8)        WSTRB;
    __input  __int(1)        WVALID;
};
__interface Pps7saxigp {
    __input  __int(1)        ACLK;
    __input  __int(32)       ARADDR;
    __input  __int(2)        ARBURST;
    __input  __int(4)        ARCACHE;
    __output __int(1)        ARESETN;
    __input  __int(6)        ARID;
    __input  __int(4)        ARLEN;
    __input  __int(2)        ARLOCK;
    __input  __int(3)        ARPROT;
    __input  __int(4)        ARQOS;
    __output __int(1)        ARREADY;
    __input  __int(2)        ARSIZE;
    __input  __int(1)        ARVALID;
    __input  __int(32)       AWADDR;
    __input  __int(2)        AWBURST;
    __input  __int(4)        AWCACHE;
    __input  __int(6)        AWID;
    __input  __int(4)        AWLEN;
    __input  __int(2)        AWLOCK;
    __input  __int(3)        AWPROT;
    __input  __int(4)        AWQOS;
    __output __int(1)        AWREADY;
    __input  __int(2)        AWSIZE;
    __input  __int(1)        AWVALID;
    __output __int(6)        BID;
    __input  __int(1)        BREADY;
    __output __int(2)        BRESP;
    __output __int(1)        BVALID;
    __output __int(32)       RDATA;
    __output __int(6)        RID;
    __output __int(1)        RLAST;
    __input  __int(1)        RREADY;
    __output __int(2)        RRESP;
    __output __int(1)        RVALID;
    __input  __int(32)       WDATA;
    __input  __int(6)        WID;
    __input  __int(1)        WLAST;
    __output __int(1)        WREADY;
    __input  __int(4)        WSTRB;
    __input  __int(1)        WVALID;
};
__interface Pps7saxihp {
    __input  __int(1)        ACLK;
    __input  __int(32)       ARADDR;
    __input  __int(2)        ARBURST;
    __input  __int(4)        ARCACHE;
    __output __int(1)        ARESETN;
    __input  __int(6)        ARID;
    __input  __int(4)        ARLEN;
    __input  __int(2)        ARLOCK;
    __input  __int(3)        ARPROT;
    __input  __int(4)        ARQOS;
    __output __int(1)        ARREADY;
    __input  __int(2)        ARSIZE;
    __input  __int(1)        ARVALID;
    __input  __int(32)       AWADDR;
    __input  __int(2)        AWBURST;
    __input  __int(4)        AWCACHE;
    __input  __int(6)        AWID;
    __input  __int(4)        AWLEN;
    __input  __int(2)        AWLOCK;
    __input  __int(3)        AWPROT;
    __input  __int(4)        AWQOS;
    __output __int(1)        AWREADY;
    __input  __int(2)        AWSIZE;
    __input  __int(1)        AWVALID;
    __output __int(6)        BID;
    __input  __int(1)        BREADY;
    __output __int(2)        BRESP;
    __output __int(1)        BVALID;
    __output __int(3)        RACOUNT;
    __output __int(8)        RCOUNT;
    __output __int(64)       RDATA;
    __input  __int(1)        RDISSUECAP1EN;
    __output __int(6)        RID;
    __output __int(1)        RLAST;
    __input  __int(1)        RREADY;
    __output __int(2)        RRESP;
    __output __int(1)        RVALID;
    __output __int(6)        WACOUNT;
    __output __int(8)        WCOUNT;
    __input  __int(64)       WDATA;
    __input  __int(6)        WID;
    __input  __int(1)        WLAST;
    __output __int(1)        WREADY;
    __input  __int(1)        WRISSUECAP1EN;
    __input  __int(8)        WSTRB;
    __input  __int(1)        WVALID;
};
__interface PS7 {
    __input  Pps7ddr         DDR;
    __input  Pps7dma         DMA0;
    __input  Pps7dma         DMA1;
    __input  Pps7dma         DMA2;
    __input  Pps7dma         DMA3;
    __input  Pps7emiocan     EMIOCAN0;
    __input  Pps7emiocan     EMIOCAN1;
    __input  Pps7emioenet    EMIOENET0;
    __input  Pps7emioenet    EMIOENET1;
    __input  Pps7emiogpio    EMIOGPIO;
    __input  Pps7emioi2c     EMIOI2C0;
    __input  Pps7emioi2c     EMIOI2C1;
    __input  Pps7emiopjtag   EMIOPJTAG;
    __input  Pps7emiosdio    EMIOSDIO0;
    __input  Pps7emiosdio    EMIOSDIO1;
    __input  Pps7emiospi     EMIOSPI0;
    __input  Pps7emiospi     EMIOSPI1;
    __input  Pps7emiosramint EMIOSRAMINT;
    __input  Pps7emiotrace   EMIOTRACE;
    __input  Pps7emiottc     EMIOTTC0;
    __input  Pps7emiottc     EMIOTTC1;
    __input  Pps7emiouart    EMIOUART0;
    __input  Pps7emiouart    EMIOUART1;
    __input  Pps7emiousb     EMIOUSB0;
    __input  Pps7emiousb     EMIOUSB1;
    __input  Pps7emiowdt     EMIOWDT;
    __input  Pps7event       EVENT;
    __input  Pps7fclk        FCLK;
    __input  Pps7fpgaid      FPGAID;
    __input  Pps7ftmd        FTMD;
    __input  Pps7ftmt        FTMT;
    __input  Pps7irq         IRQ;
    __input  Pps7maxigp      MAXIGP0;
    __input  Pps7maxigp      MAXIGP1;
    __input  Pps7m           M;
    __input  Pps7ps          PS;
    __input  Pps7saxiacp     SAXIACP;
    __input  Pps7saxigp      SAXIGP0;
    __input  Pps7saxigp      SAXIGP1;
    __input  Pps7saxihp      SAXIHP0;
    __input  Pps7saxihp      SAXIHP1;
    __input  Pps7saxihp      SAXIHP2;
    __input  Pps7saxihp      SAXIHP3;
};
