
module mkCnocTop(input  CLK, input  RST_N,
  input  [63 : 0] requests_0_message_enq_v, input  EN_requests_0_message_enq,
  output RDY_requests_0_message_enq,
  output [31 : 0] indications_0_message_first,
  input  EN_indications_0_message_deq, output RDY_indication,
  output indIntrChannel);

  reg lEIONoc_bpState;
  wire lEIONoc_bpState_D_IN,
       lEIONoc_bpState_EN;
  reg [15 : 0] lEIONoc_messageWordsReg;
  wire [15 : 0] lEIONoc_messageWordsReg_D_IN;
  wire lEIONoc_messageWordsReg_EN;
  reg [7 : 0] lEIONoc_methodIdReg;
  wire [7 : 0] lEIONoc_methodIdReg_D_IN;
  wire lEIONoc_methodIdReg_EN;
  wire [31 : 0] lEIO_ifc_heard_v,
                lEIO_portalIfc_indications_0_first,
                lEIO_portalIfc_indications_1_first;
  wire [15 : 0] lEIO_ifc_heard2_a,
                lEIO_ifc_heard2_b,
                lEIO_portalIfc_messageSize_size,
                lEIO_portalIfc_messageSize_size_methodNumber;
  wire lEIO_EN_ifc_heard,
       lEIO_EN_ifc_heard2,
       lEIO_EN_portalIfc_indications_0_deq,
       lEIO_EN_portalIfc_indications_1_deq,
       lEIO_RDY_ifc_heard,
       lEIO_RDY_ifc_heard2,
       lEIO_RDY_portalIfc_indications_0_deq,
       lEIO_RDY_portalIfc_indications_0_first,
       lEIO_RDY_portalIfc_indications_1_deq,
       lEIO_RDY_portalIfc_indications_1_first,
       lEIO_portalIfc_indications_0_notEmpty,
       lEIO_portalIfc_indications_1_notEmpty;
  wire [31 : 0] lEIONoc_fifoMsgSource_D_IN,
                lEIONoc_fifoMsgSource_D_OUT;
  wire lEIONoc_fifoMsgSource_DEQ,
       lEIONoc_fifoMsgSource_EMPTY_N,
       lEIONoc_fifoMsgSource_ENQ,
       lEIONoc_fifoMsgSource_FULL_N;
  wire [31 : 0] lERI_pipes_say2_PipeOut_first,
                lERI_pipes_say_PipeOut_first,
                lERI_portalIfc_requests_0_enq_v,
                lERI_portalIfc_requests_1_enq_v,
                lERI_portalIfc_requests_2_enq_v;
  wire [15 : 0] lERI_portalIfc_messageSize_size_methodNumber;
  wire lERI_EN_pipes_say2_PipeOut_deq,
       lERI_EN_pipes_say_PipeOut_deq,
       lERI_EN_pipes_setLeds_PipeOut_deq,
       lERI_EN_portalIfc_requests_0_enq,
       lERI_EN_portalIfc_requests_1_enq,
       lERI_EN_portalIfc_requests_2_enq,
       lERI_RDY_pipes_say2_PipeOut_deq,
       lERI_RDY_pipes_say2_PipeOut_first,
       lERI_RDY_pipes_say_PipeOut_deq,
       lERI_RDY_pipes_say_PipeOut_first,
       lERI_RDY_pipes_setLeds_PipeOut_deq,
       lERI_RDY_portalIfc_requests_0_enq,
       lERI_RDY_portalIfc_requests_1_enq,
       lERI_RDY_portalIfc_requests_2_enq;
  wire 
       lERINoc_fifoMsgSink_EMPTY_N,
       lERINoc_fifoMsgSink_FULL_N;
  wire [31 : 0] lEcho_delay_D_IN, lEcho_delay_D_OUT;
  wire lEcho_delay_DEQ,
       lEcho_delay_EMPTY_N,
       lEcho_delay_ENQ,
       lEcho_delay_FULL_N;
  wire [31 : 0] lEcho_delay2_D_IN, lEcho_delay2_D_OUT;
  wire lEcho_delay2_DEQ,
       lEcho_delay2_EMPTY_N,
       lEcho_delay2_ENQ,
       lEcho_delay2_FULL_N;
  wire RULE_lEIONoc_sendHeader,
       RULE_lEIONoc_sendMessage,
       RULE_lERINoc_receiveMessage,
       RULE_lERINoc_receiveMessageHeader;
  reg [31 : 0] MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2;
  wire [31 : 0] MUX_lEIONoc_fifoMsgSource_enq_1__VAL_1;
  wire [15 : 0] MUX_lEIONoc_messageWordsReg_write_1__VAL_2;
  wire MUX_lEIONoc_bpState_write_1__SEL_1;
  wire [15 : 0] methodNumber__h1378,
                numWords__h1336,
                roundup__h1335,
                x__h1485;
  wire [7 : 0] readyChannel__h1054;
  assign indications_0_message_first = lEIONoc_fifoMsgSource_D_OUT ;
  assign RDY_indication = lEIONoc_fifoMsgSource_EMPTY_N ;
  assign indIntrChannel = lEIONoc_fifoMsgSource_EMPTY_N ;
  mkEchoIndicationOutput lEIO(.CLK(CLK), .RST_N(RST_N),
        .ifc_heard2_a(lEIO_ifc_heard2_a),
        .ifc_heard2_b(lEIO_ifc_heard2_b),
        .ifc_heard_v(lEIO_ifc_heard_v),
        .portalIfc_messageSize_size_methodNumber(lEIO_portalIfc_messageSize_size_methodNumber),
        .EN_portalIfc_indications_0_deq(lEIO_EN_portalIfc_indications_0_deq),
        .EN_portalIfc_indications_1_deq(lEIO_EN_portalIfc_indications_1_deq),
        .EN_ifc_heard(lEIO_EN_ifc_heard),
        .EN_ifc_heard2(lEIO_EN_ifc_heard2),
        .portalIfc_messageSize_size(lEIO_portalIfc_messageSize_size),
        .RDY_portalIfc_messageSize_size(),
        .portalIfc_indications_0_first(lEIO_portalIfc_indications_0_first),
        .RDY_portalIfc_indications_0_first(lEIO_RDY_portalIfc_indications_0_first),
        .RDY_portalIfc_indications_0_deq(lEIO_RDY_portalIfc_indications_0_deq),
        .portalIfc_indications_0_notEmpty(lEIO_portalIfc_indications_0_notEmpty),
        .RDY_portalIfc_indications_0_notEmpty(),
        .portalIfc_indications_1_first(lEIO_portalIfc_indications_1_first),
        .RDY_portalIfc_indications_1_first(lEIO_RDY_portalIfc_indications_1_first),
        .RDY_portalIfc_indications_1_deq(lEIO_RDY_portalIfc_indications_1_deq),
        .portalIfc_indications_1_notEmpty(lEIO_portalIfc_indications_1_notEmpty),
        .RDY_portalIfc_indications_1_notEmpty(),
        .portalIfc_intr_status(),
        .RDY_portalIfc_intr_status(),
        .portalIfc_intr_channel(),
        .RDY_portalIfc_intr_channel(),
        .RDY_ifc_heard(lEIO_RDY_ifc_heard),
        .RDY_ifc_heard2(lEIO_RDY_ifc_heard2));
  FIFO2 #(.width(32), .guarded(1)) lEIONoc_fifoMsgSource(.RST(RST_N), .CLK(CLK),
        .D_IN(lEIONoc_fifoMsgSource_D_IN),
        .ENQ(lEIONoc_fifoMsgSource_ENQ),
        .DEQ(lEIONoc_fifoMsgSource_DEQ),
        .CLR(0),
        .D_OUT(lEIONoc_fifoMsgSource_D_OUT),
        .FULL_N(lEIONoc_fifoMsgSource_FULL_N),
        .EMPTY_N(lEIONoc_fifoMsgSource_EMPTY_N));
  mkEchoRequestInput lERI(.CLK(CLK),
        .RST_N(RST_N),
        .portalIfc_messageSize_size_methodNumber(lERI_portalIfc_messageSize_size_methodNumber),
        .portalIfc_requests_0_enq_v(lERI_portalIfc_requests_0_enq_v),
        .portalIfc_requests_1_enq_v(lERI_portalIfc_requests_1_enq_v),
        .portalIfc_requests_2_enq_v(lERI_portalIfc_requests_2_enq_v),
        .EN_portalIfc_requests_0_enq(lERI_EN_portalIfc_requests_0_enq),
        .EN_portalIfc_requests_1_enq(lERI_EN_portalIfc_requests_1_enq),
        .EN_portalIfc_requests_2_enq(lERI_EN_portalIfc_requests_2_enq),
        .EN_pipes_say_PipeOut_deq(lERI_EN_pipes_say_PipeOut_deq),
        .EN_pipes_say2_PipeOut_deq(lERI_EN_pipes_say2_PipeOut_deq),
        .EN_pipes_setLeds_PipeOut_deq(lERI_EN_pipes_setLeds_PipeOut_deq),
        .portalIfc_messageSize_size(),
        .RDY_portalIfc_messageSize_size(),
        .RDY_portalIfc_requests_0_enq(lERI_RDY_portalIfc_requests_0_enq),
        .portalIfc_requests_0_notFull(),
        .RDY_portalIfc_requests_0_notFull(),
        .RDY_portalIfc_requests_1_enq(lERI_RDY_portalIfc_requests_1_enq),
        .portalIfc_requests_1_notFull(),
        .RDY_portalIfc_requests_1_notFull(),
        .RDY_portalIfc_requests_2_enq(lERI_RDY_portalIfc_requests_2_enq),
        .portalIfc_requests_2_notFull(),
        .RDY_portalIfc_requests_2_notFull(),
        .portalIfc_intr_status(),
        .RDY_portalIfc_intr_status(),
        .portalIfc_intr_channel(),
        .RDY_portalIfc_intr_channel(),
        .pipes_say_PipeOut_first(lERI_pipes_say_PipeOut_first),
        .RDY_pipes_say_PipeOut_first(lERI_RDY_pipes_say_PipeOut_first),
        .RDY_pipes_say_PipeOut_deq(lERI_RDY_pipes_say_PipeOut_deq),
        .pipes_say_PipeOut_notEmpty(),
        .RDY_pipes_say_PipeOut_notEmpty(),
        .pipes_say2_PipeOut_first(lERI_pipes_say2_PipeOut_first),
        .RDY_pipes_say2_PipeOut_first(lERI_RDY_pipes_say2_PipeOut_first),
        .RDY_pipes_say2_PipeOut_deq(lERI_RDY_pipes_say2_PipeOut_deq),
        .pipes_say2_PipeOut_notEmpty(),
        .RDY_pipes_say2_PipeOut_notEmpty(),
        .pipes_setLeds_PipeOut_first(),
        .RDY_pipes_setLeds_PipeOut_first(),
        .RDY_pipes_setLeds_PipeOut_deq(lERI_RDY_pipes_setLeds_PipeOut_deq),
        .pipes_setLeds_PipeOut_notEmpty(),
        .RDY_pipes_setLeds_PipeOut_notEmpty());
  SizedFIFO #(.p1width(32), .p2depth(8), .p3cntr_width(3), .guarded(1)) lEcho_delay(.RST(RST_N), .CLK(CLK),
        .D_IN(lEcho_delay_D_IN),
        .ENQ(lEcho_delay_ENQ),
        .DEQ(lEcho_delay_DEQ),
        .CLR(0),
        .D_OUT(lEcho_delay_D_OUT),
        .FULL_N(lEcho_delay_FULL_N),
        .EMPTY_N(lEcho_delay_EMPTY_N));
  SizedFIFO #(.p1width(32), .p2depth(8), .p3cntr_width(3), .guarded(1)) lEcho_delay2(.RST(RST_N), .CLK(CLK),
        .D_IN(lEcho_delay2_D_IN),
        .ENQ(lEcho_delay2_ENQ),
        .DEQ(lEcho_delay2_DEQ),
        .CLR(0),
        .D_OUT(lEcho_delay2_D_OUT),
        .FULL_N(lEcho_delay2_FULL_N),
        .EMPTY_N(lEcho_delay2_EMPTY_N));
  assign RULE_lEIONoc_sendHeader = lEIONoc_fifoMsgSource_FULL_N && !lEIONoc_bpState &&
             (lEIO_portalIfc_indications_0_notEmpty || lEIO_portalIfc_indications_1_notEmpty) ;
  assign RULE_lEIONoc_sendMessage = lEIONoc_fifoMsgSource_FULL_N &&
  (lEIO_RDY_portalIfc_indications_0_deq || lEIO_RDY_portalIfc_indications_1_deq) && lEIONoc_bpState ;
  assign RULE_lERINoc_receiveMessageHeader = EN_requests_0_message_enq ;
  assign RULE_lERINoc_receiveMessage = EN_requests_0_message_enq ;
  assign RDY_requests_0_message_enq = lERI_RDY_portalIfc_requests_0_enq && lERI_RDY_portalIfc_requests_1_enq && lERI_RDY_portalIfc_requests_2_enq;
  
  assign MUX_lEIONoc_bpState_write_1__SEL_1 = RULE_lEIONoc_sendMessage && lEIONoc_messageWordsReg == 16'd1 ;
  assign MUX_lEIONoc_fifoMsgSource_enq_1__VAL_1 = { methodNumber__h1378, x__h1485 } ;
  always@(lEIONoc_methodIdReg or lEIO_portalIfc_indications_0_first or lEIO_portalIfc_indications_1_first)
  begin
    case (lEIONoc_methodIdReg)
      8'd0: MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2 = lEIO_portalIfc_indications_0_first;
      8'd1: MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2 = lEIO_portalIfc_indications_1_first;
      default: MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2 = 32'hAAAAAAAA /* unspecified value */ ;
    endcase
  end
  assign MUX_lEIONoc_messageWordsReg_write_1__VAL_2 = lEIONoc_messageWordsReg - 16'd1 ;
  assign lEIONoc_bpState_D_IN = !MUX_lEIONoc_bpState_write_1__SEL_1 ;
  assign lEIONoc_bpState_EN = RULE_lEIONoc_sendMessage && lEIONoc_messageWordsReg == 16'd1 || RULE_lEIONoc_sendHeader ;
  assign lEIONoc_messageWordsReg_D_IN = RULE_lEIONoc_sendHeader ?
               numWords__h1336 : MUX_lEIONoc_messageWordsReg_write_1__VAL_2 ;
  assign lEIONoc_messageWordsReg_EN = RULE_lEIONoc_sendHeader || RULE_lEIONoc_sendMessage ;
  assign lEIONoc_methodIdReg_D_IN = readyChannel__h1054 ;
  assign lEIONoc_methodIdReg_EN = RULE_lEIONoc_sendHeader ;
  assign lEIO_ifc_heard2_a = lEcho_delay2_D_OUT[15:0] ;
  assign lEIO_ifc_heard2_b = lEcho_delay2_D_OUT[31:16] ;
  assign lEIO_ifc_heard_v = lEcho_delay_D_OUT ;
  assign lEIO_portalIfc_messageSize_size_methodNumber = methodNumber__h1378 ;
  assign lEIO_EN_portalIfc_indications_0_deq = RULE_lEIONoc_sendMessage && lEIONoc_methodIdReg == 8'd0 ;
  assign lEIO_EN_portalIfc_indications_1_deq = RULE_lEIONoc_sendMessage && lEIONoc_methodIdReg == 8'd1 ;
  assign lEIO_EN_ifc_heard = lEIO_RDY_ifc_heard && lEcho_delay_EMPTY_N ;
  assign lEIO_EN_ifc_heard2 = lEIO_RDY_ifc_heard2 && lEcho_delay2_EMPTY_N ;
  assign lEIONoc_fifoMsgSource_D_IN = RULE_lEIONoc_sendHeader ?
               MUX_lEIONoc_fifoMsgSource_enq_1__VAL_1 : MUX_lEIONoc_fifoMsgSource_enq_1__VAL_2 ;
  assign lEIONoc_fifoMsgSource_ENQ = RULE_lEIONoc_sendHeader ||
             RULE_lEIONoc_sendMessage ;
  assign lEIONoc_fifoMsgSource_DEQ = EN_indications_0_message_deq ;
  assign lERI_portalIfc_messageSize_size_methodNumber = 16'h0 ;
  assign lERI_portalIfc_requests_0_enq_v = requests_0_message_enq_v[63:32] ;
  assign lERI_portalIfc_requests_1_enq_v = requests_0_message_enq_v[63:32] ;
  assign lERI_portalIfc_requests_2_enq_v = requests_0_message_enq_v[63:32] ;
  wire [7 : 0] lERINoc_methodIdReg;
  assign lERINoc_methodIdReg = requests_0_message_enq_v[23:16];
  assign lERI_EN_portalIfc_requests_0_enq = RULE_lERINoc_receiveMessage && lERINoc_methodIdReg == 8'd0 ;
  assign lERI_EN_portalIfc_requests_1_enq = RULE_lERINoc_receiveMessage && lERINoc_methodIdReg == 8'd1 ;
  assign lERI_EN_portalIfc_requests_2_enq = RULE_lERINoc_receiveMessage && lERINoc_methodIdReg == 8'd2 ;
  assign lERI_EN_pipes_say_PipeOut_deq = lERI_RDY_pipes_say_PipeOut_first &&
             lERI_RDY_pipes_say_PipeOut_deq && lEcho_delay_FULL_N ;
  assign lERI_EN_pipes_say2_PipeOut_deq = lERI_RDY_pipes_say2_PipeOut_first &&
             lERI_RDY_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N ;
  assign lERI_EN_pipes_setLeds_PipeOut_deq = lERI_RDY_pipes_setLeds_PipeOut_deq ;
  assign lEcho_delay_D_IN = lERI_pipes_say_PipeOut_first ;
  assign lEcho_delay_ENQ = lERI_RDY_pipes_say_PipeOut_first && lERI_RDY_pipes_say_PipeOut_deq && lEcho_delay_FULL_N ;
  assign lEcho_delay_DEQ = lEIO_RDY_ifc_heard && lEcho_delay_EMPTY_N ;
  assign lEcho_delay2_D_IN = lERI_pipes_say2_PipeOut_first ;
  assign lEcho_delay2_ENQ = lERI_RDY_pipes_say2_PipeOut_first &&
             lERI_RDY_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N ;
  assign lEcho_delay2_DEQ = lEIO_RDY_ifc_heard2 && lEcho_delay2_EMPTY_N ;
  assign methodNumber__h1378 = { 8'd0, readyChannel__h1054 } ;
  assign numWords__h1336 = { 5'd0, lEIO_portalIfc_messageSize_size[15:5] } + roundup__h1335 ;
  assign readyChannel__h1054 = lEIO_portalIfc_indications_0_notEmpty ?
               8'd0 : (lEIO_portalIfc_indications_1_notEmpty ?  8'd1 : 8'd255) ;
  assign roundup__h1335 = (lEIO_portalIfc_messageSize_size[4:0] == 5'd0) ?  16'd0 : 16'd1 ;
  assign x__h1485 = numWords__h1336 + 16'd1 ;
  always@(posedge CLK)
  begin
    if (RST_N == 0)
      begin
        lEIONoc_bpState <=  1'd0;
        lEIONoc_messageWordsReg <= 16'd0;
        lEIONoc_methodIdReg <=  8'd0;
      end
    else
      begin
        if (lEIONoc_bpState_EN)
          lEIONoc_bpState <= lEIONoc_bpState_D_IN;
        if (lEIONoc_messageWordsReg_EN)
          lEIONoc_messageWordsReg <= lEIONoc_messageWordsReg_D_IN;
        if (lEIONoc_methodIdReg_EN)
          lEIONoc_methodIdReg <= lEIONoc_methodIdReg_D_IN;
      end
  end
endmodule  // mkCnocTop
