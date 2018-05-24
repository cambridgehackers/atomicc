
module mkConnectalTop(input CLK, input  RST_N,
    input [1:0]selectIndication, input [1:0]selectRequest,
    input [31 : 0] requestEnqV,
    input EN_indication, input EN_request,
    output [31 : 0]reqIntrChannel, output [31 : 0]indIntrChannel,
    output requestNotFull,
    output indicationNotEmpty,
    output [31 : 0] indicationData,
    output RDY_indication,
    output RDY_requestEnq);

  reg RDY_requestEnq, requestNotFull;
  wire RDY_indicationDeq, RDY_indicationData, indicationNotEmpty;
  wire [31 : 0] indicationData;

  wire RDY_req0_enq, RDY_req1_enq, RDY_req2_enq, ind0_notEmpty, ind1_notEmpty;
  wire reqIntrStatus, indIntrStatus;
  wire [31 : 0]ind0_first, ind1_first;
  wire [31 : 0]dutreqIntrChannel, dutindIntrChannel;

  always@(selectRequest or RDY_req0_enq or RDY_req1_enq or RDY_req2_enq)
  begin
    case (selectRequest)
      2'd0: RDY_requestEnq = RDY_req0_enq;
      2'd1: RDY_requestEnq = RDY_req1_enq;
      2'd2: RDY_requestEnq = RDY_req2_enq;
      2'd3: RDY_requestEnq = 1'd1;
    endcase
  end
  always@(selectIndication or req0_notFull or req1_notFull or req2_notFull or
      ind0_first or ind1_first or RDY_ind0_first or RDY_ind1_first or
      ind0_notEmpty or ind1_notEmpty or RDY_ind0_deq or RDY_ind1_deq)
  begin
    case (selectIndication)
      2'd0: requestNotFull = req0_notFull;
      2'd1: requestNotFull = req1_notFull;
      2'd2: requestNotFull = req2_notFull;
      2'd3: requestNotFull = 1'b0 /* unspecified value */ ;
    endcase
  end
  assign indicationNotEmpty = selectIndication[0] ? ind1_notEmpty : ind0_notEmpty;
  assign indicationData = selectIndication[0] ? ind1_first : ind0_first;
  assign RDY_indicationData = selectIndication[0] ? RDY_ind1_first : RDY_ind0_first;
  assign RDY_indicationDeq = selectIndication[0] ? RDY_ind1_deq : RDY_ind0_deq;
  assign RDY_indication = RDY_indicationDeq && RDY_indicationData;
  assign reqIntrChannel = reqIntrStatus ? (dutreqIntrChannel + 32'd1) : 32'd0;
  assign indIntrChannel = indIntrStatus ? (dutindIntrChannel + 32'd1) : 32'd0;
////////////////////////////////////////////////////////////////////////////
  wire [31 : 0] lEchoIndicationOutput_ifc_heard_v;
  wire [15 : 0] lEchoIndicationOutput_ifc_heard2_a,
                lEchoIndicationOutput_ifc_heard2_b,
                lEchoIndicationOutput_portalIfc_messageSize_size_methodNumber;
  wire EN_lEchoIndicationOutput_ifc_heard,
       EN_lEchoIndicationOutput_ifc_heard2,
       EN_lEchoIndicationOutput_portalIfc_indications_0_deq,
       EN_lEchoIndicationOutput_portalIfc_indications_1_deq,
       RDY_lEchoIndicationOutput_ifc_heard,
       RDY_lEchoIndicationOutput_ifc_heard2,
       RDY_ind0_deq, RDY_ind1_deq,
       RDY_ind0_first, RDY_ind1_first;
  wire [31 : 0] lEchoRequestInput_pipes_say2_PipeOut_first,
                lEchoRequestInput_pipes_say_PipeOut_first;
  wire [15 : 0] lEchoRequestInput_portalIfc_messageSize_size_methodNumber;
  wire EN_lEchoRequestInput_pipes_say2_PipeOut_deq,
       EN_lEchoRequestInput_pipes_say_PipeOut_deq,
       EN_lEchoRequestInput_pipes_setLeds_PipeOut_deq,
       EN_lEchoRequestInput_portalIfc_requests_0_enq,
       EN_lEchoRequestInput_portalIfc_requests_1_enq,
       EN_lEchoRequestInput_portalIfc_requests_2_enq,
       RDY_lEchoRequestInput_pipes_say2_PipeOut_deq,
       RDY_lEchoRequestInput_pipes_say2_PipeOut_first,
       RDY_lEchoRequestInput_pipes_say_PipeOut_deq,
       RDY_lEchoRequestInput_pipes_say_PipeOut_first,
       RDY_lEchoRequestInput_pipes_setLeds_PipeOut_deq,
       req0_notFull, req1_notFull, req2_notFull;
  wire [31 : 0] lEcho_delay_D_IN, lEcho_delay_D_OUT;
  wire lEcho_delay_DEQ, lEcho_delay_EMPTY_N, lEcho_delay_ENQ, lEcho_delay_FULL_N;
  wire [31 : 0] lEcho_delay2_D_IN, lEcho_delay2_D_OUT;
  wire lEcho_delay2_DEQ, lEcho_delay2_EMPTY_N, lEcho_delay2_ENQ, lEcho_delay2_FULL_N;
  assign lEchoIndicationOutput_ifc_heard2_a = lEcho_delay2_D_OUT[15:0] ;
  assign lEchoIndicationOutput_ifc_heard2_b = lEcho_delay2_D_OUT[31:16] ;
  assign lEchoIndicationOutput_ifc_heard_v = lEcho_delay_D_OUT ;
  assign lEchoIndicationOutput_portalIfc_messageSize_size_methodNumber =
             16'h0 ;
  assign EN_lEchoIndicationOutput_portalIfc_indications_0_deq = EN_indication && selectIndication[0] == 1'd0;
  assign EN_lEchoIndicationOutput_portalIfc_indications_1_deq = EN_indication && selectIndication[0] == 1'd1;
  assign EN_lEchoIndicationOutput_ifc_heard =
             RDY_lEchoIndicationOutput_ifc_heard && lEcho_delay_EMPTY_N ;
  assign EN_lEchoIndicationOutput_ifc_heard2 =
             RDY_lEchoIndicationOutput_ifc_heard2 && lEcho_delay2_EMPTY_N ;
  assign lEchoRequestInput_portalIfc_messageSize_size_methodNumber = 16'h0 ;
  assign EN_lEchoRequestInput_portalIfc_requests_0_enq = EN_request && selectRequest == 2'd0;
  assign EN_lEchoRequestInput_portalIfc_requests_1_enq = EN_request && selectRequest == 2'd1;
  assign EN_lEchoRequestInput_portalIfc_requests_2_enq = EN_request && selectRequest == 2'd2;
  assign EN_lEchoRequestInput_pipes_say_PipeOut_deq = RDY_lEchoRequestInput_pipes_say_PipeOut_first && RDY_lEchoRequestInput_pipes_say_PipeOut_deq && lEcho_delay_FULL_N ;
  assign EN_lEchoRequestInput_pipes_say2_PipeOut_deq = RDY_lEchoRequestInput_pipes_say2_PipeOut_first && RDY_lEchoRequestInput_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N ;
  assign EN_lEchoRequestInput_pipes_setLeds_PipeOut_deq = RDY_lEchoRequestInput_pipes_setLeds_PipeOut_deq ;
  assign lEcho_delay_D_IN = lEchoRequestInput_pipes_say_PipeOut_first ;
  assign lEcho_delay_ENQ = RDY_lEchoRequestInput_pipes_say_PipeOut_first && RDY_lEchoRequestInput_pipes_say_PipeOut_deq && lEcho_delay_FULL_N ;
  assign lEcho_delay_DEQ = RDY_lEchoIndicationOutput_ifc_heard && lEcho_delay_EMPTY_N ;
  assign lEcho_delay2_D_IN = lEchoRequestInput_pipes_say2_PipeOut_first ;
  assign lEcho_delay2_ENQ = RDY_lEchoRequestInput_pipes_say2_PipeOut_first && RDY_lEchoRequestInput_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N ;
  assign lEcho_delay2_DEQ = RDY_lEchoIndicationOutput_ifc_heard2 && lEcho_delay2_EMPTY_N ;

  mkEchoIndicationOutput lEchoIndicationOutput(.CLK(CLK),
        .RST_N(RST_N),
        .ifc_heard2_a(lEchoIndicationOutput_ifc_heard2_a),
        .ifc_heard2_b(lEchoIndicationOutput_ifc_heard2_b),
        .ifc_heard_v(lEchoIndicationOutput_ifc_heard_v),
        .portalIfc_messageSize_size_methodNumber(lEchoIndicationOutput_portalIfc_messageSize_size_methodNumber),
        .EN_portalIfc_indications_0_deq(EN_lEchoIndicationOutput_portalIfc_indications_0_deq),
        .EN_portalIfc_indications_1_deq(EN_lEchoIndicationOutput_portalIfc_indications_1_deq),
        .EN_ifc_heard(EN_lEchoIndicationOutput_ifc_heard),
        .EN_ifc_heard2(EN_lEchoIndicationOutput_ifc_heard2),
        .portalIfc_messageSize_size(),
        .RDY_portalIfc_messageSize_size(),
        .portalIfc_indications_0_first(ind0_first),
        .RDY_portalIfc_indications_0_first(RDY_ind0_first),
        .RDY_portalIfc_indications_0_deq(RDY_ind0_deq),
        .portalIfc_indications_0_notEmpty(ind0_notEmpty),
        .RDY_portalIfc_indications_0_notEmpty(),
        .portalIfc_indications_1_first(ind1_first),
        .RDY_portalIfc_indications_1_first(RDY_ind1_first),
        .RDY_portalIfc_indications_1_deq(RDY_ind1_deq),
        .portalIfc_indications_1_notEmpty(ind1_notEmpty),
        .RDY_portalIfc_indications_1_notEmpty(),
        .portalIfc_intr_status(indIntrStatus),
        .RDY_portalIfc_intr_status(),
        .portalIfc_intr_channel(dutindIntrChannel),
        .RDY_portalIfc_intr_channel(),
        .RDY_ifc_heard(RDY_lEchoIndicationOutput_ifc_heard),
        .RDY_ifc_heard2(RDY_lEchoIndicationOutput_ifc_heard2));
  mkEchoRequestInput lEchoRequestInput(.CLK(CLK),
        .RST_N(RST_N),
        .portalIfc_messageSize_size_methodNumber(lEchoRequestInput_portalIfc_messageSize_size_methodNumber),
        .portalIfc_requests_0_enq_v(requestEnqV),
        .portalIfc_requests_1_enq_v(requestEnqV),
        .portalIfc_requests_2_enq_v(requestEnqV),
        .EN_portalIfc_requests_0_enq(EN_lEchoRequestInput_portalIfc_requests_0_enq),
        .EN_portalIfc_requests_1_enq(EN_lEchoRequestInput_portalIfc_requests_1_enq),
        .EN_portalIfc_requests_2_enq(EN_lEchoRequestInput_portalIfc_requests_2_enq),
        .EN_pipes_say_PipeOut_deq(EN_lEchoRequestInput_pipes_say_PipeOut_deq),
        .EN_pipes_say2_PipeOut_deq(EN_lEchoRequestInput_pipes_say2_PipeOut_deq),
        .EN_pipes_setLeds_PipeOut_deq(EN_lEchoRequestInput_pipes_setLeds_PipeOut_deq),
        .portalIfc_messageSize_size(),
        .RDY_portalIfc_messageSize_size(),
        .RDY_portalIfc_requests_0_enq(RDY_req0_enq),
        .portalIfc_requests_0_notFull(req0_notFull),
        .RDY_portalIfc_requests_0_notFull(),
        .RDY_portalIfc_requests_1_enq(RDY_req1_enq),
        .portalIfc_requests_1_notFull(req1_notFull),
        .RDY_portalIfc_requests_1_notFull(),
        .RDY_portalIfc_requests_2_enq(RDY_req2_enq),
        .portalIfc_requests_2_notFull(req2_notFull),
        .RDY_portalIfc_requests_2_notFull(),
        .portalIfc_intr_status(reqIntrStatus),
        .RDY_portalIfc_intr_status(),
        .portalIfc_intr_channel(dutreqIntrChannel),
        .RDY_portalIfc_intr_channel(),
        .pipes_say_PipeOut_first(lEchoRequestInput_pipes_say_PipeOut_first),
        .RDY_pipes_say_PipeOut_first(RDY_lEchoRequestInput_pipes_say_PipeOut_first),
        .RDY_pipes_say_PipeOut_deq(RDY_lEchoRequestInput_pipes_say_PipeOut_deq),
        .pipes_say_PipeOut_notEmpty(),
        .RDY_pipes_say_PipeOut_notEmpty(),
        .pipes_say2_PipeOut_first(lEchoRequestInput_pipes_say2_PipeOut_first),
        .RDY_pipes_say2_PipeOut_first(RDY_lEchoRequestInput_pipes_say2_PipeOut_first),
        .RDY_pipes_say2_PipeOut_deq(RDY_lEchoRequestInput_pipes_say2_PipeOut_deq),
        .pipes_say2_PipeOut_notEmpty(),
        .RDY_pipes_say2_PipeOut_notEmpty(),
        .pipes_setLeds_PipeOut_first(),
        .RDY_pipes_setLeds_PipeOut_first(),
        .RDY_pipes_setLeds_PipeOut_deq(RDY_lEchoRequestInput_pipes_setLeds_PipeOut_deq),
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
endmodule
