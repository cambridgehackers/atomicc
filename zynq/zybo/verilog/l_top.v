`include "rulec.generated.vh"

module l_module_OC_Echo (input CLK, input nRST,
    input request$say__ENA,
    input [31:0]request$say$v,
    input request$say2__ENA,
    input [15:0]request$say2$a,
    input [15:0]request$say2$b,
    output request$say2__RDY,
    output request$say__RDY,
    input request$setLeds__ENA,
    input [7:0]request$setLeds$v,
    output request$setLeds__RDY,
    input request$zsay4__ENA,
    output request$zsay4__RDY,
    output indication$heard__ENA,
    output [31:0]indication$heard$v,
    output indication$heard2__ENA,
    output [15:0]indication$heard2$a,
    output [15:0]indication$heard2$b,
    input indication$heard2__RDY,
    output indication$heard3__ENA,
    output [15:0]indication$heard3$a,
    output [31:0]indication$heard3$b,
    output [31:0]indication$heard3$c,
    output [15:0]indication$heard3$d,
    input indication$heard3__RDY,
    input indication$heard__RDY);
    reg [15:0]a_delay;
    reg [15:0]a_temp;
    reg [15:0]b_delay;
    reg [15:0]b_temp;
    reg [31:0]busy;
    reg [31:0]busy_delay;
    reg [31:0]v_delay;
    reg [31:0]v_temp;
    reg [31:0]v_type;
    wire delay_rule__ENA;
    wire delay_rule__RDY;
    wire respond_rule__ENA;
    wire respond_rule__RDY;
    assign delay_rule__ENA = delay_rule__RDY ;
    assign delay_rule__RDY = ( ( busy  != 0 ) & ( busy_delay  == 32'd0 ) ) != 0;
    assign respond_rule__ENA = respond_rule__RDY ;
    assign respond_rule__RDY = ( busy_delay  != 0 ) & ( ( v_type  != 1 ) | indication$heard__RDY  ) & ( ( v_type  == 32'd1 ) | indication$heard2__RDY  );
    assign indication$heard$v = v_delay ;
    assign indication$heard2$a = a_delay ;
    assign indication$heard2$b = b_delay ;
    assign indication$heard2__ENA = ( v_type  != 1 ) & respond_rule__ENA ;
    // assign indication$heard3$a = MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE;
    // assign indication$heard3$b = MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE;
    // assign indication$heard3$c = MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE;
    // assign indication$heard3$d = MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE;
    // assign indication$heard3__ENA = MISSING_ASSIGNMENT_FOR_OUTPUT_VALUE;
    assign indication$heard__ENA = ( v_type  == 32'd1 ) & respond_rule__ENA ;
    assign request$say2__RDY = busy  == 32'd0;
    assign request$say__RDY = busy  == 32'd0;
    assign request$setLeds__RDY = 1;
    assign request$zsay4__RDY = 1;

    always @( posedge CLK) begin
      if (!nRST) begin
        a_delay <= 0;
        a_temp <= 0;
        b_delay <= 0;
        b_temp <= 0;
        busy <= 0;
        busy_delay <= 0;
        v_delay <= 0;
        v_temp <= 0;
        v_type <= 0;
      end // nRST
      else begin
        if (delay_rule__ENA) begin
            busy  <= 0;
            busy_delay  <= 1;
            v_delay  <= v_temp;
            a_delay  <= a_temp;
            b_delay  <= b_temp;
            $display( "[delay_rule:%d]Echo" , 77 );
        end; // End of delay_rule__ENA
        if (request$say__ENA) begin
            v_temp  <= request$say$v;
            busy  <= 1;
            v_type  <= 1;
            $display( "[%s:%d]Echo" , "request$say" , 53 );
        end; // End of request$say__ENA
        if (request$say2__ENA) begin
            a_temp  <= request$say2$a;
            b_temp  <= request$say2$b;
            busy  <= 1;
            v_type  <= 2;
            $display( "[%s:%d]Echo" , "request$say2" , 59 );
        end; // End of request$say2__ENA
        if (request$zsay4__ENA) begin
            $display( "[%s:%d]Echo" , "request$zsay4" , 71 );
        end; // End of request$zsay4__ENA
        if (respond_rule__ENA) begin
            busy_delay  <= 0;
            $display( "[respond_rule:%d]Echo" , 85 );
        end; // End of respond_rule__ENA
      end
    end // always @ (posedge CLK)
endmodule 

module l_module_OC_Fifo1 (input CLK, input nRST,
    input in$enq__ENA,
    input [127:0]in$enq$v,
    output in$enq__RDY,
    input out$deq__ENA,
    output out$deq__RDY,
    output [127:0]out$first,
    output out$first__RDY);
    reg [31:0]element$data0;
    reg [31:0]element$data1;
    reg [31:0]element$data2;
    reg [31:0]element$data3;
    reg full;
    assign in$enq__RDY = 0 == full ;
    assign out$deq__RDY = 0 != full ;
    assign out$first = { element$data3  , element$data2  , element$data1  , element$data0  };
    assign out$first__RDY = 0 != full ;

    always @( posedge CLK) begin
      if (!nRST) begin
        element$data0 <= 0;
        element$data1 <= 0;
        element$data2 <= 0;
        element$data3 <= 0;
        full <= 0;
      end // nRST
      else begin
        if (in$enq__ENA) begin
            { element$data3  , element$data2  , element$data1  , element$data0  } <= in$enq$v;
            full  <= 1;
        end; // End of in$enq__ENA
        if (out$deq__ENA) begin
            full  <= 0;
        end; // End of out$deq__ENA
      end
    end // always @ (posedge CLK)
endmodule 

module l_module_OC_MuxPipe (input CLK, input nRST,
    input in$enq__ENA,
    input [127:0]in$enq$v,
    output in$enq__RDY,
    input forward$enq__ENA,
    input [127:0]forward$enq$v,
    output forward$enq__RDY,
    output out$enq__ENA,
    output [127:0]out$enq$v,
    input out$enq__RDY);
    wire fifoRule__ENA;
    wire fifoRule__RDY;
    wire forwardFifo$out$deq__RDY;
    wire [127:0]forwardFifo$out$first;
    wire forwardFifo$out$first__RDY;
    assign fifoRule__ENA = fifoRule__RDY ;
    assign fifoRule__RDY = forwardFifo$out$first__RDY  & out$enq__RDY  & forwardFifo$out$deq__RDY ;
    l_module_OC_Fifo1 forwardFifo (.CLK(CLK), .nRST(nRST),
        .in$enq__ENA(forward$enq__ENA),
        .in$enq$v(forward$enq$v),
        .in$enq__RDY(forward$enq__RDY),
        .out$deq__ENA(fifoRule__ENA),
        .out$deq__RDY(forwardFifo$out$deq__RDY),
        .out$first(forwardFifo$out$first),
        .out$first__RDY(forwardFifo$out$first__RDY));
    assign in$enq__RDY = out$enq__RDY ;
    assign out$enq$v = fifoRule__ENA  ? { forwardFifo$out$first[127:96]  , forwardFifo$out$first[95:64]  , forwardFifo$out$first[63:32]  , forwardFifo$out$first[31:0]  } : in$enq$v ;
    assign out$enq__ENA = fifoRule__ENA  || in$enq__ENA ;
endmodule 

module l_top (input CLK, input nRST,
    output indication$enq__ENA,
    output [127:0]indication$enq$v,
    input indication$enq__RDY,
    input request$enq__ENA,
    input [127:0]request$enq$v,
    output request$enq__RDY);
    wire [31:0]DUT__l_module_OC_Echo$indication$heard$v;
    wire [15:0]DUT__l_module_OC_Echo$indication$heard2$a;
    wire [15:0]DUT__l_module_OC_Echo$indication$heard2$b;
    wire DUT__l_module_OC_Echo$indication$heard2__ENA;
    wire [15:0]DUT__l_module_OC_Echo$indication$heard3$a;
    wire [31:0]DUT__l_module_OC_Echo$indication$heard3$b;
    wire [31:0]DUT__l_module_OC_Echo$indication$heard3$c;
    wire [15:0]DUT__l_module_OC_Echo$indication$heard3$d;
    wire DUT__l_module_OC_Echo$indication$heard3__ENA;
    wire DUT__l_module_OC_Echo$indication$heard__ENA;
    wire DUT__l_module_OC_Echo$request$say2__RDY;
    wire DUT__l_module_OC_Echo$request$say__RDY;
    wire DUT__l_module_OC_Echo$request$setLeds__RDY;
    wire DUT__l_module_OC_Echo$request$zsay4__RDY;
    wire M2P__indication$method$heard2__RDY;
    wire M2P__indication$method$heard3__RDY;
    wire M2P__indication$method$heard__RDY;
    wire [31:0]P2M__request$method$say$v;
    wire [15:0]P2M__request$method$say2$a;
    wire [15:0]P2M__request$method$say2$b;
    wire P2M__request$method$say2__ENA;
    wire P2M__request$method$say__ENA;
    wire [7:0]P2M__request$method$setLeds$v;
    wire P2M__request$method$setLeds__ENA;
    wire P2M__request$method$zsay4__ENA;
    l_module_OC_Echo DUT__l_module_OC_Echo (.CLK(CLK), .nRST(nRST),
        .request$say__ENA(P2M__request$method$say__ENA),
        .request$say$v(P2M__request$method$say$v),
        .request$say2__ENA(P2M__request$method$say2__ENA),
        .request$say2$a(P2M__request$method$say2$a),
        .request$say2$b(P2M__request$method$say2$b),
        .request$say2__RDY(DUT__l_module_OC_Echo$request$say2__RDY),
        .request$say__RDY(DUT__l_module_OC_Echo$request$say__RDY),
        .request$setLeds__ENA(P2M__request$method$setLeds__ENA),
        .request$setLeds$v(P2M__request$method$setLeds$v),
        .request$setLeds__RDY(DUT__l_module_OC_Echo$request$setLeds__RDY),
        .request$zsay4__ENA(P2M__request$method$zsay4__ENA),
        .request$zsay4__RDY(DUT__l_module_OC_Echo$request$zsay4__RDY),
        .indication$heard__ENA(DUT__l_module_OC_Echo$indication$heard__ENA),
        .indication$heard$v(DUT__l_module_OC_Echo$indication$heard$v),
        .indication$heard2__ENA(DUT__l_module_OC_Echo$indication$heard2__ENA),
        .indication$heard2$a(DUT__l_module_OC_Echo$indication$heard2$a),
        .indication$heard2$b(DUT__l_module_OC_Echo$indication$heard2$b),
        .indication$heard2__RDY(M2P__indication$method$heard2__RDY),
        .indication$heard3__ENA(DUT__l_module_OC_Echo$indication$heard3__ENA),
        .indication$heard3$a(DUT__l_module_OC_Echo$indication$heard3$a),
        .indication$heard3$b(DUT__l_module_OC_Echo$indication$heard3$b),
        .indication$heard3$c(DUT__l_module_OC_Echo$indication$heard3$c),
        .indication$heard3$d(DUT__l_module_OC_Echo$indication$heard3$d),
        .indication$heard3__RDY(M2P__indication$method$heard3__RDY),
        .indication$heard__RDY(M2P__indication$method$heard__RDY));
    l_module_OC_EchoIndication___M2P M2P__indication (.CLK(CLK), .nRST(nRST),
        .method$heard__ENA(DUT__l_module_OC_Echo$indication$heard__ENA),
        .method$heard$v(DUT__l_module_OC_Echo$indication$heard$v),
        .method$heard2__ENA(DUT__l_module_OC_Echo$indication$heard2__ENA),
        .method$heard2$a(DUT__l_module_OC_Echo$indication$heard2$a),
        .method$heard2$b(DUT__l_module_OC_Echo$indication$heard2$b),
        .method$heard2__RDY(M2P__indication$method$heard2__RDY),
        .method$heard3__ENA(DUT__l_module_OC_Echo$indication$heard3__ENA),
        .method$heard3$a(DUT__l_module_OC_Echo$indication$heard3$a),
        .method$heard3$b(DUT__l_module_OC_Echo$indication$heard3$b),
        .method$heard3$c(DUT__l_module_OC_Echo$indication$heard3$c),
        .method$heard3$d(DUT__l_module_OC_Echo$indication$heard3$d),
        .method$heard3__RDY(M2P__indication$method$heard3__RDY),
        .method$heard__RDY(M2P__indication$method$heard__RDY),
        .pipe$enq__ENA(indication$enq__ENA),
        .pipe$enq$v(indication$enq$v),
        .pipe$enq__RDY(indication$enq__RDY));
    l_module_OC_EchoRequest___P2M P2M__request (.CLK(CLK), .nRST(nRST),
        .method$say__ENA(P2M__request$method$say__ENA),
        .method$say$v(P2M__request$method$say$v),
        .method$say2__ENA(P2M__request$method$say2__ENA),
        .method$say2$a(P2M__request$method$say2$a),
        .method$say2$b(P2M__request$method$say2$b),
        .method$say2__RDY(DUT__l_module_OC_Echo$request$say2__RDY),
        .method$say__RDY(DUT__l_module_OC_Echo$request$say__RDY),
        .method$setLeds__ENA(P2M__request$method$setLeds__ENA),
        .method$setLeds$v(P2M__request$method$setLeds$v),
        .method$setLeds__RDY(DUT__l_module_OC_Echo$request$setLeds__RDY),
        .method$zsay4__ENA(P2M__request$method$zsay4__ENA),
        .method$zsay4__RDY(DUT__l_module_OC_Echo$request$zsay4__RDY),
        .pipe$enq__ENA(request$enq__ENA),
        .pipe$enq$v(request$enq$v),
        .pipe$enq__RDY(request$enq__RDY));
endmodule 

module l_module_OC_EchoIndication___M2P (input CLK, input nRST,
    input method$heard__ENA,
    input [31:0]method$heard$v,
    input method$heard2__ENA,
    input [15:0]method$heard2$a,
    input [15:0]method$heard2$b,
    output method$heard2__RDY,
    input method$heard3__ENA,
    input [15:0]method$heard3$a,
    input [31:0]method$heard3$b,
    input [31:0]method$heard3$c,
    input [15:0]method$heard3$d,
    output method$heard3__RDY,
    output method$heard__RDY,
    output pipe$enq__ENA,
    output [127:0]pipe$enq$v,
    input pipe$enq__RDY);
    assign method$heard2__RDY = pipe$enq__RDY ;
    assign method$heard3__RDY = pipe$enq__RDY ;
    assign method$heard__RDY = pipe$enq__RDY ;
    assign pipe$enq$v = method$heard__ENA  ? ( { 64'd0, method$heard$v, 16'd0, 16'd2}  ) : method$heard2__ENA  ? ( { 64'd0, method$heard2$b, method$heard2$a, 16'd1, 16'd2}  ) : ( { method$heard3$d, method$heard3$c, method$heard3$b, method$heard3$a, 16'd2, 16'd4}  );
    assign pipe$enq__ENA = method$heard__ENA  || method$heard2__ENA  || method$heard3__ENA ;
endmodule 

module l_module_OC_EchoRequest___P2M (input CLK, input nRST,
    output method$say__ENA,
    output [31:0]method$say$v,
    output method$say2__ENA,
    output [15:0]method$say2$a,
    output [15:0]method$say2$b,
    input method$say2__RDY,
    input method$say__RDY,
    output method$setLeds__ENA,
    output [7:0]method$setLeds$v,
    input method$setLeds__RDY,
    output method$zsay4__ENA,
    input method$zsay4__RDY,
    input pipe$enq__ENA,
    input [127:0]pipe$enq$v,
    output pipe$enq__RDY);
    assign method$say$v = pipe$enq$v[63:32] ;
    assign method$say2$a = pipe$enq$v[47:32] ;
    assign method$say2$b = pipe$enq$v[63:48] ;
    assign method$say2__ENA = ( pipe$enq$v[31:16]  == 16'd1 ) & pipe$enq__ENA ;
    assign method$say__ENA = ( pipe$enq$v[31:16]  == 16'd0 ) & pipe$enq__ENA ;
    assign method$setLeds$v = pipe$enq$v[39:32] ;
    assign method$setLeds__ENA = ( pipe$enq$v[31:16]  == 16'd2 ) & pipe$enq__ENA ;
    assign method$zsay4__ENA = ( pipe$enq$v[31:16]  == 16'd3 ) & pipe$enq__ENA ;
    assign pipe$enq__RDY = method$say__RDY  & method$say2__RDY  & method$setLeds__RDY  & method$zsay4__RDY ;
endmodule 

