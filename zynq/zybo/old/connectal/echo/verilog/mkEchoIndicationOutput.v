`define BSV_RESET_VALUE 1'b0
`define BSV_RESET_EDGE negedge
module mkEchoIndicationOutput(input  CLK, input  RST_N,
  input  [15 : 0] portalIfc_messageSize_size_methodNumber,
  output [15 : 0] portalIfc_messageSize_size, output RDY_portalIfc_messageSize_size,
  output [31 : 0] portalIfc_indications_0_first, output RDY_portalIfc_indications_0_first,
  input  EN_portalIfc_indications_0_deq, output RDY_portalIfc_indications_0_deq,
  output portalIfc_indications_0_notEmpty, output RDY_portalIfc_indications_0_notEmpty,
  output [31 : 0] portalIfc_indications_1_first, output RDY_portalIfc_indications_1_first,
  input  EN_portalIfc_indications_1_deq, output RDY_portalIfc_indications_1_deq,
  output portalIfc_indications_1_notEmpty, output RDY_portalIfc_indications_1_notEmpty,
  output portalIfc_intr_status, output RDY_portalIfc_intr_status,
  output [31 : 0] portalIfc_intr_channel, output RDY_portalIfc_intr_channel,
  input  [31 : 0] ifc_heard_v, input  EN_ifc_heard, output RDY_ifc_heard,
  input  [15 : 0] ifc_heard2_a, input  [15 : 0] ifc_heard2_b,
  input  EN_ifc_heard2, output RDY_ifc_heard2);

  mkEchoIndicationOutputPipes indicationPipes(.CLK(CLK), .RST_N(RST_N),
    .methods_heard2_enq_v({ ifc_heard2_a, ifc_heard2_b }), .methods_heard_enq_v(ifc_heard_v),
    .portalIfc_messageSize_size_methodNumber(portalIfc_messageSize_size_methodNumber),
    .EN_methods_heard_enq(EN_ifc_heard), .EN_methods_heard2_enq(EN_ifc_heard2),
    .EN_portalIfc_indications_0_deq(EN_portalIfc_indications_0_deq),
    .EN_portalIfc_indications_1_deq(EN_portalIfc_indications_1_deq),
    .RDY_methods_heard_enq(RDY_ifc_heard), .methods_heard_notFull(), .RDY_methods_heard_notFull(),
    .RDY_methods_heard2_enq(RDY_ifc_heard2), .methods_heard2_notFull(), .RDY_methods_heard2_notFull(),
    .portalIfc_messageSize_size(portalIfc_messageSize_size), .RDY_portalIfc_messageSize_size(),
    .portalIfc_indications_0_first(portalIfc_indications_0_first),
    .RDY_portalIfc_indications_0_first(RDY_portalIfc_indications_0_first),
    .RDY_portalIfc_indications_0_deq(RDY_portalIfc_indications_0_deq),
    .portalIfc_indications_0_notEmpty(portalIfc_indications_0_notEmpty), .RDY_portalIfc_indications_0_notEmpty(),
    .portalIfc_indications_1_first(portalIfc_indications_1_first),
    .RDY_portalIfc_indications_1_first(RDY_portalIfc_indications_1_first),
    .RDY_portalIfc_indications_1_deq(RDY_portalIfc_indications_1_deq),
    .portalIfc_indications_1_notEmpty(portalIfc_indications_1_notEmpty), .RDY_portalIfc_indications_1_notEmpty(),
    .portalIfc_intr_status(portalIfc_intr_status), .RDY_portalIfc_intr_status(),
    .portalIfc_intr_channel(portalIfc_intr_channel), .RDY_portalIfc_intr_channel());

  assign RDY_portalIfc_messageSize_size = 1'd1 ;
  assign RDY_portalIfc_indications_0_notEmpty = 1'd1 ;
  assign RDY_portalIfc_indications_1_notEmpty = 1'd1 ;
  assign RDY_portalIfc_intr_status = 1'd1 ;
  assign RDY_portalIfc_intr_channel = 1'd1 ;
endmodule  // mkEchoIndicationOutput
