
`include "ProjectDefines.vh"
module mkZynqTop #(parameter width = 64) (
  inout  [14 : 0] DDR_Addr, inout  [2 : 0] DDR_BankAddr,
  inout  DDR_CAS_n, inout  DDR_CKE, inout  DDR_CS_n, inout  DDR_Clk_n, inout  DDR_Clk_p,
  inout  [3 : 0] DDR_DM, inout  [31 : 0] DDR_DQ,
  inout  [3 : 0] DDR_DQS_n, inout  [3 : 0] DDR_DQS_p,
  inout  DDR_DRSTB, inout  DDR_ODT, inout  DDR_RAS_n,
  inout  FIXED_IO_ddr_vrn, inout  FIXED_IO_ddr_vrp, inout  DDR_WEB, inout  [53 : 0] MIO,
  inout  FIXED_IO_ps_clk, inout  FIXED_IO_ps_porb, inout  FIXED_IO_ps_srstb);

  wire CLK, RST_N;
  wire ps7_clockGen_clkout0buffer_O;
  wire ps7_clockGen_pll_CLKFBOUT, ps7_clockGen_pll_CLKOUT0, ps7_clockGen_pll_CLKOUT0B;
  wire [3 : 0] ps7_ps7_foo_FCLKCLK, ps7_ps7_foo_FCLKRESETN, maxigp0ARLEN, maxigp0AWLEN;

  wire ps7_b2c_0_OUT1, ps7_b2c_0_OUT2;
  CONNECTNET2 ps7_b2c_0(.IN1(ps7_ps7_foo_FCLKCLK[0]), .IN2(ps7_ps7_foo_FCLKRESETN[0]), .OUT1(ps7_b2c_0_OUT1), .OUT2(ps7_b2c_0_OUT2));
  BUFG ps7_fclk_0_c(.I(ps7_b2c_0_OUT1), .O(CLK));
  BUFG ps7_freset_0_r(.I(ps7_b2c_0_OUT2), .O(RST_N));
  BUFG ps7_clockGen_clkout0buffer(.I(ps7_clockGen_pll_CLKOUT0), .O(ps7_clockGen_clkout0buffer_O));
  wire ps7_clockGen_pll_clkfbbuf_O, ps7_clockGen_pll_reset_RESET_OUT;
/* verilator lint_off PINMISSING */
  MMCME2_ADV #(.BANDWIDTH("OPTIMIZED"), .CLKFBOUT_USE_FINE_PS("FALSE"), .CLKOUT0_USE_FINE_PS("FALSE"),
        .CLKOUT1_USE_FINE_PS("FALSE"), .CLKOUT2_USE_FINE_PS("FALSE"), .CLKOUT3_USE_FINE_PS("FALSE"),
        .CLKOUT4_CASCADE("FALSE"), .CLKOUT4_USE_FINE_PS("FALSE"), .CLKOUT5_USE_FINE_PS("FALSE"),
        .CLKOUT6_USE_FINE_PS("FALSE"), .COMPENSATION("ZHOLD"), .STARTUP_WAIT("FALSE"),
        .CLKFBOUT_MULT_F(10.0), .CLKFBOUT_PHASE(0.0), .CLKIN1_PERIOD(10.0),
        .CLKIN2_PERIOD(0.0), .DIVCLK_DIVIDE(32'd1), .CLKOUT0_DIVIDE_F(5.0),
        .CLKOUT0_DUTY_CYCLE(0.5), .CLKOUT0_PHASE(0.0), .CLKOUT1_DIVIDE(32'd10),
        .CLKOUT1_DUTY_CYCLE(0.5), .CLKOUT1_PHASE(0.0), .CLKOUT2_DIVIDE(32'd10),
        .CLKOUT2_DUTY_CYCLE(0.5), .CLKOUT2_PHASE(0.0), .CLKOUT3_DIVIDE(32'd10),
        .CLKOUT3_DUTY_CYCLE(0.5), .CLKOUT3_PHASE(0.0), .CLKOUT4_DIVIDE(32'd10),
        .CLKOUT4_DUTY_CYCLE(0.5), .CLKOUT4_PHASE(0.0), .CLKOUT5_DIVIDE(32'd10),
        .CLKOUT5_DUTY_CYCLE(0.5), .CLKOUT5_PHASE(0.0), .CLKOUT6_DIVIDE(32'd10),
        .CLKOUT6_DUTY_CYCLE(0.5), .CLKOUT6_PHASE(0.0), .REF_JITTER1(1.0e-2),
        .REF_JITTER2(1.0e-2)) ps7_clockGen_pll(.CLKIN1(CLK),
        .RST(ps7_clockGen_pll_reset_RESET_OUT), .CLKIN2(1'd0), .CLKINSEL(1'd1),
        .DADDR(7'd0), .DCLK(1'd0), .DEN(1'd0), .DI(16'd0), .DWE(1'd0), .PSCLK(1'd0),
        .PSEN(1'd0), .PSINCDEC(1'd0), .PWRDWN(1'd0), .CLKFBIN(ps7_clockGen_pll_clkfbbuf_O), .LOCKED(),
        .CLKFBOUT(ps7_clockGen_pll_CLKFBOUT), .CLKFBOUTB(),
        .CLKOUT0(ps7_clockGen_pll_CLKOUT0), .CLKOUT0B(ps7_clockGen_pll_CLKOUT0B),
        .CLKOUT1(), .CLKOUT1B(), .CLKOUT2(), .CLKOUT2B(), .CLKOUT3(), .CLKOUT3B(),
        .CLKOUT4(), .CLKOUT5(), .CLKOUT6());
/* verilator lint_on PINMISSING */
  BUFG ps7_clockGen_pll_clkfbbuf(.I(ps7_clockGen_pll_CLKFBOUT), .O(ps7_clockGen_pll_clkfbbuf_O));
  ResetInverter ps7_clockGen_pll_reset(.RESET_IN(RST_N), .RESET_OUT(ps7_clockGen_pll_reset_RESET_OUT));

  wire [31 : 0] maxigp0ARADDR, maxigp0AWADDR, maxigp0WDATA;
  wire [11 : 0] maxigp0ARID, maxigp0AWID, maxigp0WID;
  wire maxigp0ARVALID, maxigp0AWVALID, maxigp0RREADY, maxigp0BREADY, maxigp0WLAST, maxigp0WVALID;
  wire maxigp0AWREADY, maxigp0WREADY, read_reqFifo_EMPTY_N, maxigp0RVALID;
  wire RDY_WriteDone;
  wire [31 : 0] maxigp0RDATA;
  wire [13 : 0] maxigp0BRESP;
  wire interrupt_0__read;
  wire [5 : 0] maxigp0RID;
  wire reqwriteDataFifo_FULL_N, write_reqFifo_FULL_N, reqws_FULL_N;
  reg CMRlastWriteDataSeen;
  assign maxigp0AWREADY = reqws_FULL_N && write_reqFifo_FULL_N;
  assign maxigp0WREADY = !CMRlastWriteDataSeen && reqwriteDataFifo_FULL_N ;
/* verilator lint_off PINMISSING */
  PS7 ps7_ps7_foo(.MAXIGP0ACLK(CLK),
        .MAXIGP1ACLK(CLK), .SAXIACPACLK(CLK), .SAXIGP0ACLK(CLK),
        .SAXIGP1ACLK(CLK), .SAXIHP0ACLK(CLK), .SAXIHP1ACLK(CLK),
        .SAXIHP2ACLK(CLK), .SAXIHP3ACLK(CLK),
        .DDRARB(0), .EMIOGPIOI(0), .EMIOI2C0SCLI(0), .EMIOI2C0SDAI(0), .EMIOI2C1SCLI(0),
        .EMIOI2C1SDAI(0), .EMIOSRAMINTIN(0), .EVENTEVENTI(0), .FCLKCLKTRIGN(0), .FPGAIDLEN(1),
        .IRQF2P({ 19'b0, interrupt_0__read}),

        .MAXIGP0ARADDR(maxigp0ARADDR), .MAXIGP0ARID(maxigp0ARID), .MAXIGP0ARLEN(maxigp0ARLEN),
        .MAXIGP0ARVALID(maxigp0ARVALID), .MAXIGP0ARREADY(!read_reqFifo_EMPTY_N),

        .MAXIGP0RDATA(maxigp0RDATA), .MAXIGP0RRESP(0), .MAXIGP0RLAST(1),
        .MAXIGP0RID({6'b0, maxigp0RID}), .MAXIGP0RREADY(maxigp0RREADY), .MAXIGP0RVALID(maxigp0RVALID),

        .MAXIGP0AWADDR(maxigp0AWADDR), .MAXIGP0AWID(maxigp0AWID), .MAXIGP0AWLEN(maxigp0AWLEN),
        .MAXIGP0AWVALID(maxigp0AWVALID), .MAXIGP0AWREADY(maxigp0AWREADY),

        .MAXIGP0WDATA(maxigp0WDATA), .MAXIGP0WID(maxigp0WID), .MAXIGP0WLAST(maxigp0WLAST),
        .MAXIGP0WVALID(maxigp0WVALID), .MAXIGP0WREADY(maxigp0WREADY),

        .MAXIGP0BRESP(maxigp0BRESP[13:12]), .MAXIGP0BID(maxigp0BRESP[11:0]),
        .MAXIGP0BVALID(RDY_WriteDone && maxigp0BREADY), .MAXIGP0BREADY(maxigp0BREADY),
        .MAXIGP0ARBURST(), .MAXIGP0ARCACHE(), .MAXIGP0ARESETN(),
        .MAXIGP0ARLOCK(), .MAXIGP0ARPROT(), .MAXIGP0ARQOS(), .MAXIGP0ARSIZE(),
        .MAXIGP0AWBURST(), .MAXIGP0AWCACHE(), .MAXIGP0AWLOCK(), .MAXIGP0AWPROT(),
        .MAXIGP0AWQOS(), .MAXIGP0AWSIZE(), .MAXIGP0WSTRB(),

        .MAXIGP1ARREADY(0), .MAXIGP1AWREADY(0), .MAXIGP1BID(0), .MAXIGP1BRESP(0), .MAXIGP1BVALID(0),
        .MAXIGP1RDATA(0), .MAXIGP1RID(0), .MAXIGP1RLAST(0), .MAXIGP1RRESP(0), .MAXIGP1RVALID(0), .MAXIGP1WREADY(0),
        .SAXIACPARADDR(0), .SAXIACPARBURST(0), .SAXIACPARCACHE(0), .SAXIACPARID(0), .SAXIACPARLEN(0),
        .SAXIACPARLOCK(0), .SAXIACPARPROT(0), .SAXIACPARQOS(0), .SAXIACPARSIZE(0), .SAXIACPARUSER(0),
        .SAXIACPARVALID(0), .SAXIACPAWADDR(0), .SAXIACPAWBURST(0), .SAXIACPAWCACHE(0), .SAXIACPAWID(0),
        .SAXIACPAWLEN(0), .SAXIACPAWLOCK(0), .SAXIACPAWPROT(0), .SAXIACPAWQOS(0), .SAXIACPAWSIZE(0),
        .SAXIACPAWUSER(0), .SAXIACPAWVALID(0), .SAXIACPBREADY(0), .SAXIACPRREADY(0), .SAXIACPWDATA(0),
        .SAXIACPWID(0), .SAXIACPWLAST(0), .SAXIACPWSTRB(0), .SAXIACPWVALID(0),
        .SAXIGP0ARADDR(0), .SAXIGP0ARBURST(0), .SAXIGP0ARCACHE(0), .SAXIGP0ARID(0),
        .SAXIGP0ARLEN(0), .SAXIGP0ARLOCK(0), .SAXIGP0ARPROT(0), .SAXIGP0ARQOS(0),
        .SAXIGP0ARSIZE(0), .SAXIGP0ARVALID(0), .SAXIGP0AWADDR(0), .SAXIGP0AWBURST(0),
        .SAXIGP0AWCACHE(0), .SAXIGP0AWID(0), .SAXIGP0AWLEN(0), .SAXIGP0AWLOCK(0),
        .SAXIGP0AWPROT(0), .SAXIGP0AWQOS(0), .SAXIGP0AWSIZE(0), .SAXIGP0AWVALID(0),
        .SAXIGP0BREADY(0), .SAXIGP0RREADY(0), .SAXIGP0WDATA(0), .SAXIGP0WID(0),
        .SAXIGP0WLAST(0), .SAXIGP0WSTRB(0), .SAXIGP0WVALID(0), .SAXIGP1ARADDR(0),
        .SAXIGP1ARBURST(0), .SAXIGP1ARCACHE(0), .SAXIGP1ARID(0), .SAXIGP1ARLEN(0),
        .SAXIGP1ARLOCK(0), .SAXIGP1ARPROT(0), .SAXIGP1ARQOS(0), .SAXIGP1ARSIZE(0),
        .SAXIGP1ARVALID(0), .SAXIGP1AWADDR(0), .SAXIGP1AWBURST(0), .SAXIGP1AWCACHE(0),
        .SAXIGP1AWID(0), .SAXIGP1AWLEN(0), .SAXIGP1AWLOCK(0), .SAXIGP1AWPROT(0),
        .SAXIGP1AWQOS(0), .SAXIGP1AWSIZE(0), .SAXIGP1AWVALID(0), .SAXIGP1BREADY(0),
        .SAXIGP1RREADY(0), .SAXIGP1WDATA(0), .SAXIGP1WID(0), .SAXIGP1WLAST(0),
        .SAXIGP1WSTRB(0), .SAXIGP1WVALID(0), .SAXIHP0ARADDR(0), .SAXIHP0ARBURST(0),
        .SAXIHP0ARCACHE(0), .SAXIHP0ARID(0), .SAXIHP0ARLEN(0), .SAXIHP0ARLOCK(0),
        .SAXIHP0ARPROT(0), .SAXIHP0ARQOS(0), .SAXIHP0ARSIZE(0), .SAXIHP0ARVALID(0),
        .SAXIHP0AWADDR(0), .SAXIHP0AWBURST(0), .SAXIHP0AWCACHE(0), .SAXIHP0AWID(0),
        .SAXIHP0AWLEN(0), .SAXIHP0AWLOCK(0), .SAXIHP0AWPROT(0), .SAXIHP0AWQOS(0),
        .SAXIHP0AWSIZE(0), .SAXIHP0AWVALID(0), .SAXIHP0BREADY(0), .SAXIHP0RDISSUECAP1EN(0),
        .SAXIHP0RREADY(0), .SAXIHP0WDATA(0), .SAXIHP0WID(0), .SAXIHP0WLAST(0),
        .SAXIHP0WRISSUECAP1EN(0), .SAXIHP0WSTRB(0), .SAXIHP0WVALID(0), .SAXIHP1ARADDR(0),
        .SAXIHP1ARBURST(0), .SAXIHP1ARCACHE(0), .SAXIHP1ARID(0), .SAXIHP1ARLEN(0),
        .SAXIHP1ARLOCK(0), .SAXIHP1ARPROT(0), .SAXIHP1ARQOS(0), .SAXIHP1ARSIZE(0),
        .SAXIHP1ARVALID(0), .SAXIHP1AWADDR(0), .SAXIHP1AWBURST(0), .SAXIHP1AWCACHE(0),
        .SAXIHP1AWID(0), .SAXIHP1AWLEN(0), .SAXIHP1AWLOCK(0), .SAXIHP1AWPROT(0),
        .SAXIHP1AWQOS(0), .SAXIHP1AWSIZE(0), .SAXIHP1AWVALID(0), .SAXIHP1BREADY(0),
        .SAXIHP1RDISSUECAP1EN(0), .SAXIHP1RREADY(0), .SAXIHP1WDATA(0), .SAXIHP1WID(0),
        .SAXIHP1WLAST(0), .SAXIHP1WRISSUECAP1EN(0), .SAXIHP1WSTRB(0), .SAXIHP1WVALID(0),
        .SAXIHP2ARADDR(0), .SAXIHP2ARBURST(0), .SAXIHP2ARCACHE(0), .SAXIHP2ARID(0),
        .SAXIHP2ARLEN(0), .SAXIHP2ARLOCK(0), .SAXIHP2ARPROT(0), .SAXIHP2ARQOS(0),
        .SAXIHP2ARSIZE(0), .SAXIHP2ARVALID(0), .SAXIHP2AWADDR(0), .SAXIHP2AWBURST(0),
        .SAXIHP2AWCACHE(0), .SAXIHP2AWID(0), .SAXIHP2AWLEN(0), .SAXIHP2AWLOCK(0),
        .SAXIHP2AWPROT(0), .SAXIHP2AWQOS(0), .SAXIHP2AWSIZE(0), .SAXIHP2AWVALID(0),
        .SAXIHP2BREADY(0), .SAXIHP2RDISSUECAP1EN(0), .SAXIHP2RREADY(0), .SAXIHP2WDATA(0),
        .SAXIHP2WID(0), .SAXIHP2WLAST(0), .SAXIHP2WRISSUECAP1EN(0), .SAXIHP2WSTRB(0),
        .SAXIHP2WVALID(0), .SAXIHP3ARADDR(0), .SAXIHP3ARBURST(0), .SAXIHP3ARCACHE(0),
        .SAXIHP3ARID(0), .SAXIHP3ARLEN(0), .SAXIHP3ARLOCK(0), .SAXIHP3ARPROT(0),
        .SAXIHP3ARQOS(0), .SAXIHP3ARSIZE(0), .SAXIHP3ARVALID(0), .SAXIHP3AWADDR(0),
        .SAXIHP3AWBURST(0), .SAXIHP3AWCACHE(0), .SAXIHP3AWID(0), .SAXIHP3AWLEN(0),
        .SAXIHP3AWLOCK(0), .SAXIHP3AWPROT(0), .SAXIHP3AWQOS(0), .SAXIHP3AWSIZE(0),
        .SAXIHP3AWVALID(0), .SAXIHP3BREADY(0), .SAXIHP3RDISSUECAP1EN(0), .SAXIHP3RREADY(0),
        .SAXIHP3WDATA(0), .SAXIHP3WID(0), .SAXIHP3WLAST(0), .SAXIHP3WRISSUECAP1EN(0),
        .SAXIHP3WSTRB(0), .SAXIHP3WVALID(0), .EMIOGPIOO(), .EMIOGPIOTN(),
        .EMIOI2C0SCLO(), .EMIOI2C0SCLTN(), .EMIOI2C0SDAO(), .EMIOI2C0SDATN(),
        .EMIOI2C1SCLO(), .EMIOI2C1SCLTN(), .EMIOI2C1SDAO(), .EMIOI2C1SDATN(),
        .EVENTEVENTO(), .EVENTSTANDBYWFE(), .EVENTSTANDBYWFI(), .IRQP2F(),
        .MAXIGP1ARADDR(), .MAXIGP1ARBURST(), .MAXIGP1ARCACHE(), .MAXIGP1ARESETN(),
        .MAXIGP1ARID(), .MAXIGP1ARLEN(), .MAXIGP1ARLOCK(), .MAXIGP1ARPROT(),
        .MAXIGP1ARQOS(), .MAXIGP1ARSIZE(), .MAXIGP1ARVALID(), .MAXIGP1AWADDR(),
        .MAXIGP1AWBURST(), .MAXIGP1AWCACHE(), .MAXIGP1AWID(), .MAXIGP1AWLEN(),
        .MAXIGP1AWLOCK(), .MAXIGP1AWPROT(), .MAXIGP1AWQOS(), .MAXIGP1AWSIZE(),
        .MAXIGP1AWVALID(), .MAXIGP1BREADY(), .MAXIGP1RREADY(), .MAXIGP1WDATA(),
        .MAXIGP1WID(), .MAXIGP1WLAST(), .MAXIGP1WSTRB(), .MAXIGP1WVALID(),
        .SAXIACPARESETN(), .SAXIACPARREADY(), .SAXIACPAWREADY(), .SAXIACPBID(),
        .SAXIACPBRESP(), .SAXIACPBVALID(), .SAXIACPRDATA(), .SAXIACPRID(),
        .SAXIACPRLAST(), .SAXIACPRRESP(), .SAXIACPRVALID(), .SAXIACPWREADY(),
        .SAXIGP0ARESETN(), .SAXIGP0ARREADY(), .SAXIGP0AWREADY(), .SAXIGP0BID(),
        .SAXIGP0BRESP(), .SAXIGP0BVALID(), .SAXIGP0RDATA(), .SAXIGP0RID(),
        .SAXIGP0RLAST(), .SAXIGP0RRESP(), .SAXIGP0RVALID(), .SAXIGP0WREADY(),
        .SAXIGP1ARESETN(), .SAXIGP1ARREADY(), .SAXIGP1AWREADY(), .SAXIGP1BID(),
        .SAXIGP1BRESP(), .SAXIGP1BVALID(), .SAXIGP1RDATA(), .SAXIGP1RID(),
        .SAXIGP1RLAST(), .SAXIGP1RRESP(), .SAXIGP1RVALID(), .SAXIGP1WREADY(),
        .SAXIHP0ARESETN(), .SAXIHP0ARREADY(), .SAXIHP0AWREADY(), .SAXIHP0BID(),
        .SAXIHP0BRESP(), .SAXIHP0BVALID(), .SAXIHP0RDATA(), .SAXIHP0RID(),
        .SAXIHP0RLAST(), .SAXIHP0RRESP(), .SAXIHP0RVALID(), .SAXIHP0RACOUNT(),
        .SAXIHP0RCOUNT(), .SAXIHP0WACOUNT(), .SAXIHP0WCOUNT(), .SAXIHP0WREADY(),
        .SAXIHP1ARESETN(), .SAXIHP1ARREADY(), .SAXIHP1AWREADY(), .SAXIHP1BID(),
        .SAXIHP1BRESP(), .SAXIHP1BVALID(), .SAXIHP1RDATA(), .SAXIHP1RID(),
        .SAXIHP1RLAST(), .SAXIHP1RRESP(), .SAXIHP1RVALID(), .SAXIHP1RACOUNT(),
        .SAXIHP1RCOUNT(), .SAXIHP1WACOUNT(), .SAXIHP1WCOUNT(), .SAXIHP1WREADY(),
        .SAXIHP2ARESETN(), .SAXIHP2ARREADY(), .SAXIHP2AWREADY(), .SAXIHP2BID(),
        .SAXIHP2BRESP(), .SAXIHP2BVALID(), .SAXIHP2RDATA(), .SAXIHP2RID(),
        .SAXIHP2RLAST(), .SAXIHP2RRESP(), .SAXIHP2RVALID(), .SAXIHP2RACOUNT(),
        .SAXIHP2RCOUNT(), .SAXIHP2WACOUNT(), .SAXIHP2WCOUNT(), .SAXIHP2WREADY(),
        .SAXIHP3ARESETN(), .SAXIHP3ARREADY(), .SAXIHP3AWREADY(), .SAXIHP3BID(),
        .SAXIHP3BRESP(), .SAXIHP3BVALID(), .SAXIHP3RDATA(), .SAXIHP3RID(),
        .SAXIHP3RLAST(), .SAXIHP3RRESP(), .SAXIHP3RVALID(), .SAXIHP3RACOUNT(),
        .SAXIHP3RCOUNT(), .SAXIHP3WACOUNT(), .SAXIHP3WCOUNT(), .SAXIHP3WREADY(),
        .FCLKCLK(ps7_ps7_foo_FCLKCLK), .FCLKRESETN(ps7_ps7_foo_FCLKRESETN),
        .DDRA(DDR_Addr), .DDRBA(DDR_BankAddr), .DDRCASB(DDR_CAS_n),
        .DDRCKE(DDR_CKE), .DDRCKN(DDR_Clk_n), .DDRCKP(DDR_Clk_p),
        .DDRCSB(DDR_CS_n), .DDRDM(DDR_DM), .DDRDQ(DDR_DQ),
        .DDRDQSN(DDR_DQS_n), .DDRDQSP(DDR_DQS_p), .DDRDRSTB(DDR_DRSTB),
        .DDRODT(DDR_ODT), .DDRRASB(DDR_RAS_n), .DDRVRN(FIXED_IO_ddr_vrn),
        .DDRVRP(FIXED_IO_ddr_vrp), .DDRWEB(DDR_WEB), .PSCLK(FIXED_IO_ps_clk),
        .PSPORB(FIXED_IO_ps_porb), .PSSRSTB(FIXED_IO_ps_srstb), .MIO(MIO));
/* verilator lint_on PINMISSING */

  reg ctrlPort_0_interruptEnableReg;
  reg readFirst, readLast, selectRIndReq, portalRControl, selectWIndReq, portalWControl;
  reg [31 : 0] requestValue, portalCtrlInfo;
  reg [9 : 0] readCount;
  reg [4 : 0] readAddr;

  wire requestNotFull;
  wire RDY_indication, RDY_requestEnq, ReadDataFifo_FULL_N;
  wire reqPortal_EMPTY_N, reqPortal_FULL_N, read_reqFifo_FULL_N;
  wire reqrs_EMPTY_N, reqrs_FULL_N, reqwriteDataFifo_EMPTY_N, reqws_EMPTY_N;
  wire readFirstNext, readAddr_EN, RULEread, reqPortal_D_OUT_last;
  wire [31 : 0]indicationData, requestData, zzIntrChannel;
  wire [9 : 0] reqPortal_D_OUT_base, read_reqFifo_D_OUT_count, readburstCount;
  wire [5 : 0] reqPortal_D_OUT_id, read_reqFifo_D_OUT_id;
  wire [4 : 0] reqPortal_D_OUT_addr, read_reqFifo_D_OUT_addr, readAddrupdate;
  wire [1 : 0] selectIndication, selectRequest;
  assign zzIntrChannel = selectRIndReq ? 32'd0 : RDY_indication;

  assign interrupt_0__read = RDY_indication && ctrlPort_0_interruptEnableReg;
  assign readFirstNext = readFirst ? read_reqFifo_D_OUT_count == 4  : readLast ;
  assign RULEread = reqPortal_EMPTY_N && ReadDataFifo_FULL_N && (selectRIndReq ?
        ((!(!portalRControl && reqPortal_D_OUT_addr == 4) && !reqPortal_D_OUT_last) || reqrs_EMPTY_N)
       : ((!(!portalRControl && reqPortal_D_OUT_addr == 0) || RDY_indication) && reqrs_EMPTY_N));

  always@(reqPortal_D_OUT_addr or indicationData or requestNotFull)
  begin
    if (selectRIndReq)
      requestValue = { 31'd0, (reqPortal_D_OUT_addr == 4 && requestNotFull) };
    else
    case (reqPortal_D_OUT_addr)
      0: requestValue = indicationData;
      4: requestValue = { 31'd0, requestNotFull};
      default: requestValue = 32'd0;
    endcase
  end
  always@(reqPortal_D_OUT_addr or ctrlPort_0_interruptEnableReg or zzIntrChannel or selectRIndReq)
  begin
    case (reqPortal_D_OUT_addr)
      0: portalCtrlInfo = {31'd0,  zzIntrChannel != 0}; // PORTAL_CTRL_INTERRUPT_STATUS 0
      //4: portalCtrlInfo = 0;//{31'd0, ctrlPort_0_interruptEnableReg;
      8: portalCtrlInfo = 1;                         // PORTAL_CTRL_NUM_TILES        2
      5'h0C: portalCtrlInfo = zzIntrChannel;         // PORTAL_CTRL_IND_QUEUE_STATUS 3
      5'h10: portalCtrlInfo = selectRIndReq ? 6 : 5; // PORTAL_CTRL_PORTAL_ID        4
      5'h14: portalCtrlInfo = 2;                     // PORTAL_CTRL_NUM_PORTALS      5
      //5'h18: portalCtrlInfo = 0;
      //5'h1C: portalCtrlInfo = 0;
      default: portalCtrlInfo = 32'h005A05A0;
// PORTAL_CTRL_INTERRUPT_ENABLE 1
// PORTAL_CTRL_COUNTER_MSB      6
// PORTAL_CTRL_COUNTER_LSB      7
    endcase
  end

  assign readAddr_EN = read_reqFifo_EMPTY_N && reqPortal_FULL_N;

  FIFO1 #(.width(2), .guarded(1)) reqrs(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(maxigp0ARADDR[6:5] - 2'd1), .ENQ(maxigp0ARVALID),
        .D_OUT(selectIndication), .DEQ(RULEread && (!selectRIndReq || reqPortal_D_OUT_last)),
        .FULL_N(reqrs_FULL_N), .EMPTY_N(reqrs_EMPTY_N));
  always@(posedge CLK) begin
        if (maxigp0ARVALID) begin
            portalRControl <= maxigp0ARADDR[11:5] == 7'd0;
            selectRIndReq <= maxigp0ARADDR[12];
        end
  end
  FIFO1 #(.width(21), .guarded(1)) reqArs(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({ maxigp0ARADDR[4:0], { 4'd0, maxigp0ARLEN + 4'd1, 2'd0 }, maxigp0ARID[5:0]}), .ENQ(maxigp0ARVALID),
        .D_OUT({read_reqFifo_D_OUT_addr, read_reqFifo_D_OUT_count, read_reqFifo_D_OUT_id}),
        .DEQ(readAddr_EN && readFirstNext), .FULL_N(read_reqFifo_FULL_N), .EMPTY_N(read_reqFifo_EMPTY_N));
  assign readAddrupdate = readFirst ?  read_reqFifo_D_OUT_addr : readAddr ;
  assign readburstCount = readFirst ?  { 2'd0, read_reqFifo_D_OUT_count[9:2] } : readCount ;
  FIFO2 #(.width(22), .guarded(1)) reqPortal(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({readAddrupdate, readburstCount, read_reqFifo_D_OUT_id, readFirstNext}), .ENQ(readAddr_EN),
        .D_OUT({reqPortal_D_OUT_addr, reqPortal_D_OUT_base, reqPortal_D_OUT_id, reqPortal_D_OUT_last}),
        .DEQ(RULEread), .FULL_N(reqPortal_FULL_N), .EMPTY_N(reqPortal_EMPTY_N));
  FIFO2 #(.width(38), .guarded(1)) ReadDataFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({portalRControl ? portalCtrlInfo : requestValue, reqPortal_D_OUT_id}), .ENQ(RULEread),
        .D_OUT({maxigp0RDATA, maxigp0RID}), .DEQ(maxigp0RREADY && maxigp0RVALID),
        .FULL_N(ReadDataFifo_FULL_N), .EMPTY_N(maxigp0RVALID));

//write
  reg writeFirst, writeLast;
  reg [9 : 0] writeCount;
  reg [4 : 0] writeAddr;

  wire writeAddr_EN, writeFirstNext, WriteDone_FULL_N, RULEwrite, EN_WriteReq, EN_WriteData;
  wire writeFifo_EMPTY_N, writeFifo_FULL_N, write_reqFifo_EMPTY_N, reqdoneFifo_ENQ; wire writeFifo_D_OUT_last;
  wire [9 : 0] writeFifo_D_OUT_count, write_reqFifo_D_OUT_count, writeburstCount;
  wire [5 : 0] writeFifo_D_OUT_id, write_reqFifo_D_OUT_id;
  wire [4 : 0] writeFifo_D_OUT_addr, write_reqFifo_D_OUT_addr, writeAddrupdate;

  assign EN_WriteReq = maxigp0AWVALID && maxigp0AWREADY;
  assign RULEwrite = reqwriteDataFifo_EMPTY_N && writeFifo_EMPTY_N && (!writeFifo_D_OUT_last || WriteDone_FULL_N)
            && (!selectWIndReq || portalWControl || (reqws_EMPTY_N && RDY_requestEnq));
  assign writeAddr_EN = write_reqFifo_EMPTY_N && writeFifo_FULL_N ;

  assign reqdoneFifo_ENQ = RULEwrite && writeFifo_D_OUT_last;
  assign writeFirstNext = writeFirst ?  write_reqFifo_D_OUT_count == 4 : writeLast ;
  FIFO1 #(.width(21), .guarded(1)) write_reqFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({maxigp0AWADDR[4:0], { 4'd0, maxigp0AWLEN + 4'd1, 2'd0 }, maxigp0AWID[5:0] }), .ENQ(EN_WriteReq),
        .D_OUT({write_reqFifo_D_OUT_addr, write_reqFifo_D_OUT_count, write_reqFifo_D_OUT_id}),
        .DEQ(writeAddr_EN && writeFirstNext), .FULL_N(write_reqFifo_FULL_N), .EMPTY_N(write_reqFifo_EMPTY_N));
  FIFO1 #(.width(2), .guarded(1)) reqws(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(maxigp0AWADDR[6:5] - 2'd1), .ENQ(EN_WriteReq),
        .D_OUT(selectRequest), .DEQ(reqdoneFifo_ENQ), .FULL_N(reqws_FULL_N), .EMPTY_N(reqws_EMPTY_N));
  always@(posedge CLK) begin
        if (EN_WriteReq) begin
            portalWControl <= maxigp0AWADDR[11:5] == 7'd0;
            selectWIndReq <= maxigp0AWADDR[12];
        end
  end
  assign EN_WriteData = maxigp0WVALID && maxigp0WREADY;
  FIFO2 #(.width(32), .guarded(1)) reqwriteDataFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(maxigp0WDATA), .ENQ(EN_WriteData),
        .D_OUT(requestData), .DEQ(RULEwrite), .FULL_N(reqwriteDataFifo_FULL_N), .EMPTY_N(reqwriteDataFifo_EMPTY_N));
  assign writeAddrupdate = writeFirst ?  write_reqFifo_D_OUT_addr : writeAddr ;
  assign writeburstCount = writeFirst ?  { 2'd0, write_reqFifo_D_OUT_count[9:2] } : writeCount ;
  FIFO2 #(.width(22), .guarded(1)) writeFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({ writeAddrupdate, writeburstCount, write_reqFifo_D_OUT_id, writeFirstNext }), .ENQ(writeAddr_EN),
        .D_OUT({writeFifo_D_OUT_addr, writeFifo_D_OUT_count, writeFifo_D_OUT_id, writeFifo_D_OUT_last}),
        .DEQ(RULEwrite), .FULL_N(writeFifo_FULL_N), .EMPTY_N(writeFifo_EMPTY_N));
  FIFO1 #(.width(14), .guarded(1)) CMRdoneFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN( { 8'd0, writeFifo_D_OUT_id}), .ENQ(reqdoneFifo_ENQ),
        .D_OUT(maxigp0BRESP), .DEQ(maxigp0BREADY), .FULL_N(WriteDone_FULL_N), .EMPTY_N(RDY_WriteDone));

  always@(posedge CLK)
  begin
    if (RST_N == 0)
      begin
        ctrlPort_0_interruptEnableReg <=  1'd0;
        CMRlastWriteDataSeen <=  1'd0;
        readAddr <= 0;
        readCount <= 0;
        readFirst <= 1'd1;
        readLast <= 1'd0;
        writeAddr <= 0;
        writeCount <= 0;
        writeFirst <= 1'd1;
        writeLast <= 1'd0;
      end
    else
      begin
        if (RULEwrite && portalWControl && writeFifo_D_OUT_addr == 4)
          ctrlPort_0_interruptEnableReg <= requestData[0];
        if (EN_WriteData && maxigp0WLAST || EN_WriteReq)
          CMRlastWriteDataSeen <= !EN_WriteReq;
        if (readAddr_EN) begin
          readAddr <= readAddrupdate + 4 ;
          readCount <= readburstCount - 1 ;
          readFirst <= readFirstNext;
          readLast <= readburstCount == 2 ;
        end

        if (writeAddr_EN) begin
          writeAddr <= writeAddrupdate + 4 ;
          writeCount <= writeburstCount - 1 ;
          writeFirst <= writeFirstNext ;
          writeLast <= writeburstCount == 2 ;
        end
      end
  end

mkUser user(.CLK(CLK), .nRST(RST_N),
  .Userwrite(RULEwrite && !portalWControl), .writeBeatData(requestData), .requestLast(writeFifo_D_OUT_addr != 0),
  .RDY_writeBeat(requestNotFull),
  .Userread(RULEread && !portalRControl), .readBeatData(indicationData), .RDY_readBeat(RDY_indication));
endmodule  // mkZynqTop

module mkUser (input CLK, input nRST,
  input Userwrite, input [`MAX_BUS_WIDTH-1:0] writeBeatData, input requestLast, output RDY_writeBeat,
  input Userread, output [`MAX_BUS_WIDTH-1:0] readBeatData, output RDY_readBeat);

  wire EN_outgoing, RDY_incoming, EN_incoming, RDY_echo_in_enq, EN_echo_out_enq, RDY_echo_out_enq;
  wire [`MAX_OUT_WIDTH-1 : 0] echoData, incomingData;
  assign RDY_readBeat = !RDY_echo_out_enq;
  l_module_OC_AdapterToBus radapter_0(.CLK(CLK), .nRST(nRST),
   .in$enq__ENA(EN_echo_out_enq), .in$enq__RDY(RDY_echo_out_enq), .in$enq$v(echoData), .in$enq$length(echoData[15:0]-1),
   .out$enq__ENA(EN_outgoing), .out$enq__RDY(Userread && RDY_readBeat), .out$enq$v(readBeatData), .out$enq$last());
  l_module_OC_AdapterFromBus wadapter_0(.CLK(CLK), .nRST(nRST),
    .in$enq__ENA(Userwrite), .in$enq$v(writeBeatData), .in$enq$last(requestLast), .in$enq__RDY(RDY_writeBeat),
    .out$enq__ENA(EN_incoming), .out$enq$v(incomingData), .out$enq$length(), .out$enq__RDY(RDY_incoming));
  l_top ctop( .CLK (CLK ), .nRST(nRST),
    .request$enq$v (incomingData), .request$enq__ENA (EN_incoming), .request$enq__RDY(RDY_incoming),
    .indication$enq$v (echoData), .indication$enq__ENA(EN_echo_out_enq), .indication$enq__RDY(RDY_echo_out_enq));
endmodule  // mkUser
