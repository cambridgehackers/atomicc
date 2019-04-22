`include "lpm.generated.vh"

`default_nettype none
module Fifo2 (input wire CLK, input wire nRST,
    input wire in$enq__ENA,
    input wire [95:0]in$enq$v,
    output wire in$enq__RDY,
    input wire out$deq__ENA,
    output wire out$deq__RDY,
    output wire [95:0]out$first,
    output wire out$first__RDY);
    genvar  __inst$Genvar1;
    reg [31:0]element$a [2:0];
    reg [31:0]element$b [2:0];
    reg [31:0]element$c [2:0];
    reg [31:0]rindex;
    reg [31:0]windex;
    wire [95:0]element;
    assign in$enq__RDY = ( ( windex + 1 ) % 2 ) != rindex;
    assign out$deq__RDY = rindex != windex;
    assign out$first = ( rindex == 32'd0 ) ? element[0] : element[1];
    assign out$first__RDY = rindex != windex;
    // Extra assigments, not to output wires
    assign element = { element$c , element$b , element$a };

    always @( posedge CLK) begin
      if (!nRST) begin
        rindex <= 0;
        windex <= 0;
      end // nRST
      else begin
        if (in$enq__ENA & in$enq__RDY) begin // in$enq__ENA
            windex <= ( windex + 1 ) % 2;
            if (windex == 0)
            element[0] <= in$enq$v;
            if (windex == 1)
            element[1] <= in$enq$v;
        end; // End of in$enq__ENA
        if (out$deq__ENA & ( rindex != windex )) begin // out$deq__ENA
            rindex <= ( rindex + 1 ) % 2;
        end; // End of out$deq__ENA
      end
    end // always @ (posedge CLK)
endmodule 

`default_nettype wire    // set back to default value
