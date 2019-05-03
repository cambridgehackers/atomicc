`include "gcd.generated.vh"

`default_nettype none
module Gcd (input wire CLK, input wire nRST,
    input wire request$say__ENA,
    input wire [31:0]request$say$va,
    input wire [31:0]request$say$vb,
    output wire request$say__RDY,
    output wire indication$gcd__ENA,
    output wire [31:0]indication$gcd$v,
    input wire indication$gcd__RDY);
    reg [31:0]a;
    reg [31:0]b;
    reg busy;
    reg [1:0]busy_delay;
    wire RULE$mod_rule__ENA;
    wire RULE$mod_rule__RDY;
    assign indication$gcd$v = a;
    assign indication$gcd__ENA = b == 32'd0;
    assign request$say__RDY = !busy;
    // Extra assigments, not to output wires
    assign RULE$mod_rule__ENA = ( a >= b ) & ( b != 32'd0 );
    assign RULE$mod_rule__RDY = ( a >= b ) & ( b != 32'd0 );

    always @( posedge CLK) begin
      if (!nRST) begin
        a <= 0;
        b <= 0;
        busy <= 0;
        busy_delay <= 0;
      end // nRST
      else begin
        if (a < b) begin // RULE$flip_rule__ENA
            b <= a;
            a <= b;
        end; // End of RULE$flip_rule__ENA
        if (RULE$mod_rule__ENA & RULE$mod_rule__RDY) begin // RULE$mod_rule__ENA
            a <= a - b;
        end; // End of RULE$mod_rule__ENA
        if (( b == 32'd0 ) & indication$gcd__RDY) begin // RULE$respond_rule__ENA
            busy <= 0;
        end; // End of RULE$respond_rule__ENA
        if (request$say__ENA & ( !busy )) begin // request$say__ENA
            a <= request$say$va;
            b <= request$say$vb;
            busy <= 1;
        end; // End of request$say__ENA
      end
    end // always @ (posedge CLK)
endmodule 

`default_nettype wire    // set back to default value
