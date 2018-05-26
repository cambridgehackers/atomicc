`define BSV_RESET_VALUE 1'b0
`define BSV_RESET_EDGE negedge
module mkEchoIndicationOutputPipes( input  CLK, input  RST_N,
  input  [31 : 0] methods_heard_enq_v, input  EN_methods_heard_enq, output RDY_methods_heard_enq,
  output methods_heard_notFull, output RDY_methods_heard_notFull,
  input  [31 : 0] methods_heard2_enq_v, input  EN_methods_heard2_enq, output RDY_methods_heard2_enq,
  output methods_heard2_notFull, output RDY_methods_heard2_notFull,
  input  [15 : 0] portalIfc_messageSize_size_methodNumber,
  output [15 : 0] portalIfc_messageSize_size, output RDY_portalIfc_messageSize_size,
  output [31 : 0] portalIfc_indications_0_first, output RDY_portalIfc_indications_0_first,
  input  EN_portalIfc_indications_0_deq, output RDY_portalIfc_indications_0_deq,
  output portalIfc_indications_0_notEmpty, output RDY_portalIfc_indications_0_notEmpty,
  output [31 : 0] portalIfc_indications_1_first, output RDY_portalIfc_indications_1_first,
  input  EN_portalIfc_indications_1_deq, output RDY_portalIfc_indications_1_deq,
  output portalIfc_indications_1_notEmpty, output RDY_portalIfc_indications_1_notEmpty,
  output portalIfc_intr_status, output RDY_portalIfc_intr_status,
  output [31 : 0] portalIfc_intr_channel, output RDY_portalIfc_intr_channel);

  reg [31 : 0] heard2_responseAdapter_bits;
  reg heard2_responseAdapter_notEmptyReg;
  reg [31 : 0] heard_responseAdapter_bits;
  reg heard_responseAdapter_notEmptyReg;

  assign RDY_methods_heard_enq = !heard_responseAdapter_notEmptyReg ;
  assign methods_heard_notFull = !heard_responseAdapter_notEmptyReg ;
  assign RDY_methods_heard_notFull = 1 ;
  assign RDY_methods_heard2_enq = !heard2_responseAdapter_notEmptyReg ;
  assign methods_heard2_notFull = !heard2_responseAdapter_notEmptyReg ;
  assign RDY_methods_heard2_notFull = 1 ;
  assign portalIfc_messageSize_size = 16'd32 ;
  assign RDY_portalIfc_messageSize_size = 1 ;
  assign portalIfc_indications_0_first = heard_responseAdapter_bits ;
  assign RDY_portalIfc_indications_0_first = heard_responseAdapter_notEmptyReg ;
  assign RDY_portalIfc_indications_0_deq = heard_responseAdapter_notEmptyReg ;
  assign portalIfc_indications_0_notEmpty = heard_responseAdapter_notEmptyReg ;
  assign RDY_portalIfc_indications_0_notEmpty = 1 ;
  assign portalIfc_indications_1_first = heard2_responseAdapter_bits ;
  assign RDY_portalIfc_indications_1_first = heard2_responseAdapter_notEmptyReg ;
  assign RDY_portalIfc_indications_1_deq = heard2_responseAdapter_notEmptyReg ;
  assign portalIfc_indications_1_notEmpty = heard2_responseAdapter_notEmptyReg ;
  assign RDY_portalIfc_indications_1_notEmpty = 1 ;
  assign portalIfc_intr_status = heard_responseAdapter_notEmptyReg || heard2_responseAdapter_notEmptyReg ;
  assign RDY_portalIfc_intr_status = 1 ;
  assign portalIfc_intr_channel = heard_responseAdapter_notEmptyReg ?  32'd0 : (heard2_responseAdapter_notEmptyReg ? 32'd1 : 32'hFFFFFFFF) ;
  assign RDY_portalIfc_intr_channel = 1 ;
  always@(posedge CLK)
  begin
    if (RST_N == `BSV_RESET_VALUE)
      begin
        heard2_responseAdapter_bits <= 32'd0;
        heard2_responseAdapter_notEmptyReg <= 0;
        heard_responseAdapter_bits <= 32'd0;
        heard_responseAdapter_notEmptyReg <= 0;
      end
    else
      begin
        if (EN_methods_heard2_enq)
          heard2_responseAdapter_bits <= methods_heard2_enq_v;
        if (EN_portalIfc_indications_1_deq || EN_methods_heard2_enq)
          heard2_responseAdapter_notEmptyReg <= !EN_portalIfc_indications_1_deq;
        if (EN_methods_heard_enq)
          heard_responseAdapter_bits <= methods_heard_enq_v;
        if (EN_portalIfc_indications_0_deq || EN_methods_heard_enq)
          heard_responseAdapter_notEmptyReg <= !EN_portalIfc_indications_0_deq;
      end
  end
  initial
  begin
    heard2_responseAdapter_bits = 32'hAAAAAAAA;
    heard2_responseAdapter_notEmptyReg = 0;
    heard_responseAdapter_bits = 32'hAAAAAAAA;
    heard_responseAdapter_notEmptyReg = 0;
  end
endmodule  // mkEchoIndicationOutputPipes
