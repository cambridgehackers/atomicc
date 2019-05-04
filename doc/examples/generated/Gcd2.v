`include "gcd2.generated.vh"

`default_nettype none
module Gcd2 (input wire CLK, input wire nRST,
    input wire request$say__ENA,
    input wire [31:0]request$say$va,
    input wire [31:0]request$say$vb,
    output wire request$say__RDY,
    output wire indication$gcd__ENA,
    output wire [31:0]indication$gcd$v,
    input wire indication$gcd__RDY);
    reg [31:0]a;
    reg [31:0]b;
    reg running;
    wire RULE$respond_rule__ENA;
    wire RULE$respond_rule__RDY;
    assign indication$gcd$v = a;
    assign indication$gcd__ENA = ( b == 32'd0 ) & running;
    assign request$say__RDY = !running;
    // Extra assigments, not to output wires
    assign RULE$respond_rule__ENA = ( b != 32'd0 ) | ( !running ) | indication$gcd__RDY;
    assign RULE$respond_rule__RDY = ( b != 32'd0 ) | ( !running ) | indication$gcd__RDY;

    always @( posedge CLK) begin
      if (!nRST) begin
        a <= 0;
        b <= 0;
        running <= 0;
      end // nRST
      else begin
        // RULE$flip_rule__ENA
            if (( a < b ) & ( running != 0 )) begin
            b <= a;
            a <= b;
            end;
        // End of RULE$flip_rule__ENA
        // RULE$mod_rule__ENA
            if (( b != 0 ) & ( a >= b ) & ( running != 0 ))
            a <= a - b;
        // End of RULE$mod_rule__ENA
        if (RULE$respond_rule__ENA & RULE$respond_rule__RDY) begin // RULE$respond_rule__ENA
            if (( b == 0 ) & ( running != 0 ))
            running <= 0;
        end; // End of RULE$respond_rule__ENA
        if (request$say__ENA & ( !running )) begin // request$say__ENA
            a <= request$say$va;
            b <= request$say$vb;
            running <= 1;
        end; // End of request$say__ENA
      end
    end // always @ (posedge CLK)
endmodule 

`default_nettype wire    // set back to default value
