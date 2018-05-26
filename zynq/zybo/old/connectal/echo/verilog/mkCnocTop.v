`define BSV_RESET_VALUE 1'b0
`define BSV_RESET_EDGE negedge
module mkCnocTop( input  CLK, input  RST_N,
  output [31 : 0] requests_0_id, output RDY_requests_0_id,
  input  [31 : 0] requests_0_message_enq_v, input  EN_requests_0_message_enq, output RDY_requests_0_message_enq,
  output requests_0_message_notFull, output RDY_requests_0_message_notFull,
  output [31 : 0] indications_0_id, output RDY_indications_0_id,
  output [31 : 0] indications_0_message_first, output RDY_indications_0_message_first,
  input  EN_indications_0_message_deq, output RDY_indications_0_message_deq,
  output indications_0_message_notEmpty, output RDY_indications_0_message_notEmpty);

  wire [31 : 0] lEIO_portalIfc_indications_0_first, lEIO_portalIfc_indications_1_first;
  wire [15 : 0] lEIO_portalIfc_messageSize_size;
  wire lEIO_RDY_ifc_heard, lEIO_RDY_ifc_heard2,
       lEIO_RDY_portalIfc_indications_0_deq, lEIO_RDY_portalIfc_indications_0_first,
       lEIO_RDY_portalIfc_indications_1_deq, lEIO_RDY_portalIfc_indications_1_first,
       lEIO_portalIfc_indications_0_notEmpty, lEIO_portalIfc_indications_1_notEmpty;
  wire lEIONoc_fifoMsgSource_EMPTY_N, lEIONoc_fifoMsgSource_FULL_N;
  wire [31 : 0] lERI_pipes_say2_PipeOut_first, lERI_pipes_say_PipeOut_first;
  wire lERI_RDY_pipes_say2_PipeOut_deq, lERI_RDY_pipes_say2_PipeOut_first,
       lERI_RDY_pipes_say_PipeOut_deq, lERI_RDY_pipes_say_PipeOut_first,
       lERI_RDY_pipes_setLeds_PipeOut_deq, lERI_RDY_portalIfc_requests_0_enq,
       lERI_RDY_portalIfc_requests_1_enq, lERI_RDY_portalIfc_requests_2_enq;
  wire [31 : 0] lERINoc_fifoMsgSink_D_OUT;
  wire lERINoc_fifoMsgSink_EMPTY_N, lERINoc_fifoMsgSink_FULL_N;
  wire [31 : 0] lEcho_delay_D_OUT; wire lEcho_delay_EMPTY_N, lEcho_delay_FULL_N;
  wire [31 : 0] lEcho_delay2_D_OUT; wire lEcho_delay2_EMPTY_N, lEcho_delay2_FULL_N;
  wire RULElEIONoc_sendHeader, RULElEIONoc_sendMessage,
       RULElERINoc_receiveMessage, RULElERINoc_receiveMessageHeader;
  wire [15 : 0] methodNumber__h1378, numWords__h1336;
  wire [7 : 0] _theResult____h1942, readyChannel__h1054;

  reg lEIONoc_bpState;
  reg [15 : 0] lEIONoc_messageWordsReg;
  reg [7 : 0] lEIONoc_methodIdReg;
  reg lERINoc_bpState;
  reg [7 : 0] lERINoc_messageWordsReg;
  reg [7 : 0] lERINoc_methodIdReg;
  reg [31 : 0] MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2;
  reg CASE_lEIONoc_methodIdReg_0_lE_ETC__q1,
      CASE_lEIONoc_methodIdReg_0_lE_ETC__q2,
      CASE_lERINoc_methodIdReg_0_lEchoR_ETC__q3;

  FIFO2 #(.width(32'd32), .guarded(32'd1)) lERINoc_fifoMsgSink(.RST(RST_N), .CLK(CLK),
    .D_IN(requests_0_message_enq_v), .ENQ(EN_requests_0_message_enq),
    .DEQ(RULElERINoc_receiveMessage || RULElERINoc_receiveMessageHeader),
    .CLR(0), .D_OUT(lERINoc_fifoMsgSink_D_OUT),
    .FULL_N(lERINoc_fifoMsgSink_FULL_N), .EMPTY_N(lERINoc_fifoMsgSink_EMPTY_N));

  FIFO2 #(.width(32'd32), .guarded(32'd1)) lEIONoc_fifoMsgSource(.RST(RST_N), .CLK(CLK),
    .D_IN(RULElEIONoc_sendHeader ?
        { methodNumber__h1378, numWords__h1336 + 16'd1 } : MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2),
    .ENQ(RULElEIONoc_sendHeader || RULElEIONoc_sendMessage),
    .DEQ(EN_indications_0_message_deq), .CLR(0), .D_OUT(indications_0_message_first),
    .FULL_N(lEIONoc_fifoMsgSource_FULL_N),
    .EMPTY_N(lEIONoc_fifoMsgSource_EMPTY_N));

  mkEchoRequestInput lERI(.CLK(CLK), .RST_N(RST_N),
      .portalIfc_messageSize_size_methodNumber(0),
      .portalIfc_requests_0_enq_v(lERINoc_fifoMsgSink_D_OUT),
      .portalIfc_requests_1_enq_v(lERINoc_fifoMsgSink_D_OUT),
      .portalIfc_requests_2_enq_v(lERINoc_fifoMsgSink_D_OUT),
      .EN_portalIfc_requests_0_enq(RULElERINoc_receiveMessage && lERINoc_methodIdReg == 8'd0),
      .EN_portalIfc_requests_1_enq(RULElERINoc_receiveMessage && lERINoc_methodIdReg == 8'd1),
      .EN_portalIfc_requests_2_enq(RULElERINoc_receiveMessage && lERINoc_methodIdReg == 8'd2),
      .EN_pipes_say_PipeOut_deq(lERI_RDY_pipes_say_PipeOut_first &&
         lERI_RDY_pipes_say_PipeOut_deq && lEcho_delay_FULL_N),
      .EN_pipes_say2_PipeOut_deq(lERI_RDY_pipes_say2_PipeOut_first &&
         lERI_RDY_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N),
      .EN_pipes_setLeds_PipeOut_deq(lERI_RDY_pipes_setLeds_PipeOut_deq),
      .portalIfc_messageSize_size(), .RDY_portalIfc_messageSize_size(),
      .RDY_portalIfc_requests_0_enq(lERI_RDY_portalIfc_requests_0_enq),
      .portalIfc_requests_0_notFull(), .RDY_portalIfc_requests_0_notFull(),
      .RDY_portalIfc_requests_1_enq(lERI_RDY_portalIfc_requests_1_enq),
      .portalIfc_requests_1_notFull(), .RDY_portalIfc_requests_1_notFull(),
      .RDY_portalIfc_requests_2_enq(lERI_RDY_portalIfc_requests_2_enq),
      .portalIfc_requests_2_notFull(), .RDY_portalIfc_requests_2_notFull(),
      .portalIfc_intr_status(), .RDY_portalIfc_intr_status(),
      .portalIfc_intr_channel(), .RDY_portalIfc_intr_channel(),
      .pipes_say_PipeOut_first(lERI_pipes_say_PipeOut_first),
      .RDY_pipes_say_PipeOut_first(lERI_RDY_pipes_say_PipeOut_first),
      .RDY_pipes_say_PipeOut_deq(lERI_RDY_pipes_say_PipeOut_deq),
      .pipes_say_PipeOut_notEmpty(), .RDY_pipes_say_PipeOut_notEmpty(),
      .pipes_say2_PipeOut_first(lERI_pipes_say2_PipeOut_first),
      .RDY_pipes_say2_PipeOut_first(lERI_RDY_pipes_say2_PipeOut_first),
      .RDY_pipes_say2_PipeOut_deq(lERI_RDY_pipes_say2_PipeOut_deq),
      .pipes_say2_PipeOut_notEmpty(), .RDY_pipes_say2_PipeOut_notEmpty(),
      .pipes_setLeds_PipeOut_first(), .RDY_pipes_setLeds_PipeOut_first(),
      .RDY_pipes_setLeds_PipeOut_deq(lERI_RDY_pipes_setLeds_PipeOut_deq),
      .pipes_setLeds_PipeOut_notEmpty(), .RDY_pipes_setLeds_PipeOut_notEmpty());

  mkEchoIndicationOutput lEIO(.CLK(CLK), .RST_N(RST_N),
    .ifc_heard2_a(lEcho_delay2_D_OUT[15:0]), .ifc_heard2_b(lEcho_delay2_D_OUT[31:16]), .ifc_heard_v(lEcho_delay_D_OUT),
    .portalIfc_messageSize_size_methodNumber(methodNumber__h1378),
    .EN_portalIfc_indications_0_deq(RULElEIONoc_sendMessage && lEIONoc_methodIdReg == 8'd0),
    .EN_portalIfc_indications_1_deq(RULElEIONoc_sendMessage && lEIONoc_methodIdReg == 8'd1),
    .EN_ifc_heard(lEIO_RDY_ifc_heard && lEcho_delay_EMPTY_N), .EN_ifc_heard2(lEIO_RDY_ifc_heard2 && lEcho_delay2_EMPTY_N),
    .portalIfc_messageSize_size(lEIO_portalIfc_messageSize_size), .RDY_portalIfc_messageSize_size(),
    .portalIfc_indications_0_first(lEIO_portalIfc_indications_0_first),
    .RDY_portalIfc_indications_0_first(lEIO_RDY_portalIfc_indications_0_first),
    .RDY_portalIfc_indications_0_deq(lEIO_RDY_portalIfc_indications_0_deq),
    .portalIfc_indications_0_notEmpty(lEIO_portalIfc_indications_0_notEmpty),
    .RDY_portalIfc_indications_0_notEmpty(),
    .portalIfc_indications_1_first(lEIO_portalIfc_indications_1_first),
    .RDY_portalIfc_indications_1_first(lEIO_RDY_portalIfc_indications_1_first),
    .RDY_portalIfc_indications_1_deq(lEIO_RDY_portalIfc_indications_1_deq),
    .portalIfc_indications_1_notEmpty(lEIO_portalIfc_indications_1_notEmpty),
    .RDY_portalIfc_indications_1_notEmpty(), .portalIfc_intr_status(), .RDY_portalIfc_intr_status(),
    .portalIfc_intr_channel(), .RDY_portalIfc_intr_channel(),
    .RDY_ifc_heard(lEIO_RDY_ifc_heard), .RDY_ifc_heard2(lEIO_RDY_ifc_heard2));

  SizedFIFO #(.p1width(32'd32), .p2depth(32'd8), .p3cntr_width(32'd3), .guarded(32'd1)) lEcho_delay(
      .RST(RST_N), .CLK(CLK), .D_IN(lERI_pipes_say_PipeOut_first),
      .ENQ(lERI_RDY_pipes_say_PipeOut_first && lERI_RDY_pipes_say_PipeOut_deq && lEcho_delay_FULL_N),
      .DEQ(lEIO_RDY_ifc_heard && lEcho_delay_EMPTY_N), .CLR(0), .D_OUT(lEcho_delay_D_OUT),
      .FULL_N(lEcho_delay_FULL_N), .EMPTY_N(lEcho_delay_EMPTY_N));

  SizedFIFO #(.p1width(32'd32), .p2depth(32'd8), .p3cntr_width(32'd3), .guarded(32'd1)) lEcho_delay2(
      .RST(RST_N), .CLK(CLK), .D_IN(lERI_pipes_say2_PipeOut_first),
      .ENQ(lERI_RDY_pipes_say2_PipeOut_first && lERI_RDY_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N),
      .DEQ(lEIO_RDY_ifc_heard2 && lEcho_delay2_EMPTY_N), .CLR(0), .D_OUT(lEcho_delay2_D_OUT),
      .FULL_N(lEcho_delay2_FULL_N), .EMPTY_N(lEcho_delay2_EMPTY_N));

  assign requests_0_id = 32'd6;
  assign RDY_requests_0_id = 1'd1;
  assign RDY_requests_0_message_enq = lERINoc_fifoMsgSink_FULL_N;
  assign requests_0_message_notFull = lERINoc_fifoMsgSink_FULL_N;
  assign RDY_requests_0_message_notFull = 1'd1;
  assign indications_0_id = 32'd5;
  assign RDY_indications_0_id = 1'd1;
  assign RDY_indications_0_message_first = lEIONoc_fifoMsgSource_EMPTY_N;
  assign RDY_indications_0_message_deq = lEIONoc_fifoMsgSource_EMPTY_N;
  assign indications_0_message_notEmpty = lEIONoc_fifoMsgSource_EMPTY_N;
  assign RDY_indications_0_message_notEmpty = 1'd1;
  assign RULElEIONoc_sendHeader = lEIONoc_fifoMsgSource_FULL_N && !lEIONoc_bpState && (lEIO_portalIfc_indications_0_notEmpty || lEIO_portalIfc_indications_1_notEmpty);
  assign RULElEIONoc_sendMessage = lEIONoc_fifoMsgSource_FULL_N &&
         CASE_lEIONoc_methodIdReg_0_lE_ETC__q1 && CASE_lEIONoc_methodIdReg_0_lE_ETC__q2 && lEIONoc_bpState;
  assign RULElERINoc_receiveMessageHeader = lERINoc_fifoMsgSink_EMPTY_N && !lERINoc_bpState;
  assign RULElERINoc_receiveMessage = (lERINoc_fifoMsgSink_EMPTY_N && CASE_lERINoc_methodIdReg_0_lEchoR_ETC__q3) && lERINoc_bpState;
  assign _theResult____h1942 = (lERINoc_fifoMsgSink_D_OUT[7:0] == 8'd1) ?
           lERINoc_fifoMsgSink_D_OUT[7:0] : (lERINoc_fifoMsgSink_D_OUT[7:0] - 8'd1);
  assign numWords__h1336 = { 5'd0, lEIO_portalIfc_messageSize_size[15:5] } + ((lEIO_portalIfc_messageSize_size[4:0] == 5'd0) ?  16'd0 : 16'd1);
  assign readyChannel__h1054 = lEIO_portalIfc_indications_0_notEmpty ?  8'd0 :
           (lEIO_portalIfc_indications_1_notEmpty ?  8'd1 : 8'd255);
  assign methodNumber__h1378 = { 8'd0, readyChannel__h1054 };

  always@(lEIONoc_methodIdReg or lEIO_portalIfc_indications_0_first or lEIO_portalIfc_indications_1_first)
  begin
  case (lEIONoc_methodIdReg)
  8'd0: MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2 = lEIO_portalIfc_indications_0_first;
  8'd1: MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2 = lEIO_portalIfc_indications_1_first;
  default: MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2 = 32'hAAAAAAAA /* unspecified value */;
  endcase
  end

  always@(lEIONoc_methodIdReg or lEIO_RDY_portalIfc_indications_0_deq or lEIO_RDY_portalIfc_indications_1_deq)
  begin
  case (lEIONoc_methodIdReg)
  8'd0: CASE_lEIONoc_methodIdReg_0_lE_ETC__q1 = lEIO_RDY_portalIfc_indications_0_deq;
  8'd1: CASE_lEIONoc_methodIdReg_0_lE_ETC__q1 = lEIO_RDY_portalIfc_indications_1_deq;
  default: CASE_lEIONoc_methodIdReg_0_lE_ETC__q1 = 1'd1;
  endcase
  end
  always@(lEIONoc_methodIdReg or lEIO_RDY_portalIfc_indications_0_first or lEIO_RDY_portalIfc_indications_1_first)
  begin
  case (lEIONoc_methodIdReg)
  8'd0: CASE_lEIONoc_methodIdReg_0_lE_ETC__q2 = lEIO_RDY_portalIfc_indications_0_first;
  8'd1: CASE_lEIONoc_methodIdReg_0_lE_ETC__q2 = lEIO_RDY_portalIfc_indications_1_first;
  default: CASE_lEIONoc_methodIdReg_0_lE_ETC__q2 = 1'd1;
  endcase
  end
  always@(lERINoc_methodIdReg or lERI_RDY_portalIfc_requests_0_enq or
      lERI_RDY_portalIfc_requests_1_enq or lERI_RDY_portalIfc_requests_2_enq)
  begin
  case (lERINoc_methodIdReg)
  8'd0: CASE_lERINoc_methodIdReg_0_lEchoR_ETC__q3 = lERI_RDY_portalIfc_requests_0_enq;
  8'd1: CASE_lERINoc_methodIdReg_0_lEchoR_ETC__q3 = lERI_RDY_portalIfc_requests_1_enq;
  8'd2: CASE_lERINoc_methodIdReg_0_lEchoR_ETC__q3 = lERI_RDY_portalIfc_requests_2_enq;
  default: CASE_lERINoc_methodIdReg_0_lEchoR_ETC__q3 = 1'd1;
  endcase
  end
  always@(posedge CLK)
  begin
    if (RST_N == `BSV_RESET_VALUE)
      begin
        lEIONoc_bpState <= 1'd0;
        lEIONoc_messageWordsReg <= 16'd0;
        lEIONoc_methodIdReg <= 8'd0;
        lERINoc_bpState <= 1'd0;
        lERINoc_messageWordsReg <= 8'd0;
        lERINoc_methodIdReg <= 8'd0;
      end
    else
      begin
        if (RULElEIONoc_sendMessage && lEIONoc_messageWordsReg == 16'd1 || RULElEIONoc_sendHeader)
          lEIONoc_bpState <= !(RULElEIONoc_sendMessage && lEIONoc_messageWordsReg == 16'd1);
        if (RULElEIONoc_sendHeader || RULElEIONoc_sendMessage)
          lEIONoc_messageWordsReg <= RULElEIONoc_sendHeader ?  numWords__h1336 : (lEIONoc_messageWordsReg - 16'd1);
        if (RULElEIONoc_sendHeader)
          lEIONoc_methodIdReg <= readyChannel__h1054;
        if (RULElERINoc_receiveMessageHeader && _theResult____h1942 != 8'd0 || RULElERINoc_receiveMessage && lERINoc_messageWordsReg == 8'd1)
          lERINoc_bpState <= RULElERINoc_receiveMessageHeader && _theResult____h1942 != 8'd0;
        if (RULElERINoc_receiveMessageHeader || RULElERINoc_receiveMessage)
          lERINoc_messageWordsReg <= RULElERINoc_receiveMessageHeader ?  _theResult____h1942 : (lERINoc_messageWordsReg - 8'd1);
        if (RULElERINoc_receiveMessageHeader)
          lERINoc_methodIdReg <= lERINoc_fifoMsgSink_D_OUT[23:16];
      end
  end
  initial
  begin
    lEIONoc_bpState = 1'h0;
    lEIONoc_messageWordsReg = 16'hAAAA;
    lEIONoc_methodIdReg = 8'hAA;
    lERINoc_bpState = 1'h0;
    lERINoc_messageWordsReg = 8'hAA;
    lERINoc_methodIdReg = 8'hAA;
  end
endmodule  // mkCnocTop
