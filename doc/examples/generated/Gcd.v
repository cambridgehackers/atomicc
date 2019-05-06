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
    reg running;
    wire RULE$flip_rule__ENA;
    wire RULE$flip_rule__RDY;
    wire RULE$mod_rule__ENA;
    wire RULE$mod_rule__RDY;
    wire RULE$respond_rule__ENA;
    wire RULE$respond_rule__RDY;
    assign indication$gcd$v = a;
    assign indication$gcd__ENA = ( running && ( b == 32'd0 ) ) != 0;
    assign request$say__RDY = !running;
    // Extra assigments, not to output wires
    assign RULE$flip_rule__ENA = ( running && ( a < b ) ) != 0;
    assign RULE$flip_rule__RDY = ( running && ( a < b ) ) != 0;
    assign RULE$mod_rule__ENA = ( running && ( a >= b ) && ( b != 32'd0 ) ) != 0;
    assign RULE$mod_rule__RDY = ( running && ( a >= b ) && ( b != 32'd0 ) ) != 0;
    assign RULE$respond_rule__ENA = ( ( running && ( b == 32'd0 ) ) != 0 ) & indication$gcd__RDY;
    assign RULE$respond_rule__RDY = ( ( running && ( b == 32'd0 ) ) != 0 ) & indication$gcd__RDY;

    always @( posedge CLK) begin
      if (!nRST) begin
        a <= 0;
        b <= 0;
        running <= 0;
      end // nRST
      else begin
        if (RULE$flip_rule__ENA & RULE$flip_rule__RDY) begin // RULE$flip_rule__ENA
            b <= a;
            a <= b;
        end; // End of RULE$flip_rule__ENA
        if (RULE$mod_rule__ENA & RULE$mod_rule__RDY) begin // RULE$mod_rule__ENA
            a <= a - b;
        end; // End of RULE$mod_rule__ENA
        if (RULE$respond_rule__ENA & RULE$respond_rule__RDY) begin // RULE$respond_rule__ENA
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