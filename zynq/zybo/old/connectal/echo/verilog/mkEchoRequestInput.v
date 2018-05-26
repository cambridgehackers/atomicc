  `define BSV_RESET_VALUE 1'b0
  `define BSV_RESET_EDGE negedge
module mkEchoRequestInput(input  CLK, input  RST_N,
        input  [15 : 0] portalIfc_messageSize_size_methodNumber,
        output [15 : 0] portalIfc_messageSize_size, output RDY_portalIfc_messageSize_size,
        input  [31 : 0] portalIfc_requests_0_enq_v,
        input  EN_portalIfc_requests_0_enq, output RDY_portalIfc_requests_0_enq,
        output portalIfc_requests_0_notFull, output RDY_portalIfc_requests_0_notFull,
        input  [31 : 0] portalIfc_requests_1_enq_v,
        input  EN_portalIfc_requests_1_enq, output RDY_portalIfc_requests_1_enq,
        output portalIfc_requests_1_notFull, output RDY_portalIfc_requests_1_notFull,
        input  [31 : 0] portalIfc_requests_2_enq_v,
        input  EN_portalIfc_requests_2_enq, output RDY_portalIfc_requests_2_enq,
        output portalIfc_requests_2_notFull, output RDY_portalIfc_requests_2_notFull,
        output portalIfc_intr_status, output RDY_portalIfc_intr_status,
        output [31 : 0] portalIfc_intr_channel, output RDY_portalIfc_intr_channel,
        output [31 : 0] pipes_say_PipeOut_first, output RDY_pipes_say_PipeOut_first,
        input  EN_pipes_say_PipeOut_deq, output RDY_pipes_say_PipeOut_deq,
        output pipes_say_PipeOut_notEmpty, output RDY_pipes_say_PipeOut_notEmpty,
        output [31 : 0] pipes_say2_PipeOut_first, output RDY_pipes_say2_PipeOut_first,
        input  EN_pipes_say2_PipeOut_deq, output RDY_pipes_say2_PipeOut_deq,
        output pipes_say2_PipeOut_notEmpty, output RDY_pipes_say2_PipeOut_notEmpty,
        output [7 : 0] pipes_setLeds_PipeOut_first, output RDY_pipes_setLeds_PipeOut_first,
        input  EN_pipes_setLeds_PipeOut_deq, output RDY_pipes_setLeds_PipeOut_deq,
        output pipes_setLeds_PipeOut_notEmpty, output RDY_pipes_setLeds_PipeOut_notEmpty);

  wire say2_requestAdapter_fifo_EMPTY_N, say2_requestAdapter_fifo_FULL_N;
  wire say_requestAdapter_fifo_EMPTY_N, say_requestAdapter_fifo_FULL_N;
  wire setLeds_requestAdapter_fifo_EMPTY_N, setLeds_requestAdapter_fifo_FULL_N;

  reg [15 : 0] portalIfc_messageSize_size;
  FIFO1 #(.width(32), .guarded(1)) say2_requestAdapter_fifo(.RST(RST_N), .CLK(CLK),
      .D_IN(portalIfc_requests_1_enq_v), .ENQ(EN_portalIfc_requests_1_enq),
      .DEQ(EN_pipes_say2_PipeOut_deq), .CLR(0), .D_OUT(pipes_say2_PipeOut_first),
      .FULL_N(say2_requestAdapter_fifo_FULL_N), .EMPTY_N(say2_requestAdapter_fifo_EMPTY_N));
  FIFO1 #(.width(32), .guarded(1)) say_requestAdapter_fifo(.RST(RST_N), .CLK(CLK),
      .D_IN(portalIfc_requests_0_enq_v), .ENQ(EN_portalIfc_requests_0_enq),
      .DEQ(EN_pipes_say_PipeOut_deq), .CLR(0), .D_OUT(pipes_say_PipeOut_first),
      .FULL_N(say_requestAdapter_fifo_FULL_N), .EMPTY_N(say_requestAdapter_fifo_EMPTY_N));
  FIFO1 #(.width(8), .guarded(1)) setLeds_requestAdapter_fifo(.RST(RST_N), .CLK(CLK),
      .D_IN(portalIfc_requests_2_enq_v[7:0]), .ENQ(EN_portalIfc_requests_2_enq),
      .DEQ(EN_pipes_setLeds_PipeOut_deq), .CLR(0), .D_OUT(pipes_setLeds_PipeOut_first),
      .FULL_N(setLeds_requestAdapter_fifo_FULL_N), .EMPTY_N(setLeds_requestAdapter_fifo_EMPTY_N));

  always@(portalIfc_messageSize_size_methodNumber)
  begin
    case (portalIfc_messageSize_size_methodNumber)
      16'd0, 16'd1: portalIfc_messageSize_size = 16'd32;
      default: portalIfc_messageSize_size = 16'd8;
    endcase
  end
  assign RDY_portalIfc_messageSize_size = 1 ;
  assign RDY_portalIfc_requests_0_enq = say_requestAdapter_fifo_FULL_N ;
  assign portalIfc_requests_0_notFull = say_requestAdapter_fifo_FULL_N ;
  assign RDY_portalIfc_requests_0_notFull = 1 ;
  assign RDY_portalIfc_requests_1_enq = say2_requestAdapter_fifo_FULL_N ;
  assign portalIfc_requests_1_notFull = say2_requestAdapter_fifo_FULL_N ;
  assign RDY_portalIfc_requests_1_notFull = 1 ;
  assign RDY_portalIfc_requests_2_enq = setLeds_requestAdapter_fifo_FULL_N ;
  assign portalIfc_requests_2_notFull = setLeds_requestAdapter_fifo_FULL_N ;
  assign RDY_portalIfc_requests_2_notFull = 1 ;
  assign portalIfc_intr_status = 0 ;
  assign RDY_portalIfc_intr_status = 1 ;
  assign portalIfc_intr_channel = 32'hFFFFFFFF ;
  assign RDY_portalIfc_intr_channel = 1 ;
  assign RDY_pipes_say_PipeOut_first = say_requestAdapter_fifo_EMPTY_N ;
  assign RDY_pipes_say_PipeOut_deq = say_requestAdapter_fifo_EMPTY_N ;
  assign pipes_say_PipeOut_notEmpty = say_requestAdapter_fifo_EMPTY_N ;
  assign RDY_pipes_say_PipeOut_notEmpty = 1 ;
  assign RDY_pipes_say2_PipeOut_first = say2_requestAdapter_fifo_EMPTY_N ;
  assign RDY_pipes_say2_PipeOut_deq = say2_requestAdapter_fifo_EMPTY_N ;
  assign pipes_say2_PipeOut_notEmpty = say2_requestAdapter_fifo_EMPTY_N ;
  assign RDY_pipes_say2_PipeOut_notEmpty = 1 ;
  assign RDY_pipes_setLeds_PipeOut_first = setLeds_requestAdapter_fifo_EMPTY_N ;
  assign RDY_pipes_setLeds_PipeOut_deq = setLeds_requestAdapter_fifo_EMPTY_N ;
  assign pipes_setLeds_PipeOut_notEmpty = setLeds_requestAdapter_fifo_EMPTY_N ;
  assign RDY_pipes_setLeds_PipeOut_notEmpty = 1 ;
endmodule  // mkEchoRequestInput
