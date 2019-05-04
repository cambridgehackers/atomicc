`include "example1.generated.vh"

`default_nettype none
module Example1 (input wire CLK, input wire nRST,
    input wire request$say__ENA,
    input wire [31:0]request$say$v,
    output wire request$say__RDY,
    output wire indication$heard__ENA,
    output wire [31:0]indication$heard$v,
    input wire indication$heard__RDY);
    reg busy;
    reg [1:0]busy_delay;
    reg [31:0]v_delay;
    reg [31:0]v_temp;
    wire RULE$delay_rule__ENA;
    wire RULE$delay_rule__RDY;
    assign indication$heard$v = v_delay;
    assign indication$heard__ENA = busy_delay == 2'd2;
    assign request$say__RDY = !busy;
    // Extra assigments, not to output wires
    assign RULE$delay_rule__ENA = ( busy && ( busy_delay == 2'd0 ) ) != 0;
    assign RULE$delay_rule__RDY = ( busy && ( busy_delay == 2'd0 ) ) != 0;

    always @( posedge CLK) begin
      if (!nRST) begin
        busy <= 0;
        busy_delay <= 0;
        v_delay <= 0;
        v_temp <= 0;
      end // nRST
      else begin
        if (busy_delay == 2'd1) begin // RULE$delay2_rule__ENA
            busy_delay <= 2;
            if (v_delay == 1)
            busy <= 0;
        end; // End of RULE$delay2_rule__ENA
        if (RULE$delay_rule__ENA & RULE$delay_rule__RDY) begin // RULE$delay_rule__ENA
            busy_delay <= 1;
            v_delay <= v_temp;
        end; // End of RULE$delay_rule__ENA
        if (( busy_delay == 2'd2 ) & indication$heard__RDY) begin // RULE$respond_rule__ENA
            busy_delay <= 0;
            if (v_delay != 1)
            busy <= 0;
        end; // End of RULE$respond_rule__ENA
        if (request$say__ENA & ( !busy )) begin // request$say__ENA
            v_temp <= request$say$v;
            busy <= 1;
        end; // End of request$say__ENA
      end
    end // always @ (posedge CLK)
endmodule 

`default_nettype wire    // set back to default value
