`include "order.generated.vh"

`default_nettype none
module Order (input wire CLK, input wire nRST,
    input wire request$say__ENA,
    input wire [31:0]request$say$va,
    output wire request$say__RDY);
    reg [31:0]a;
    reg [31:0]offset;
    reg [31:0]outA;
    reg [31:0]outB;
    reg running;
    assign request$say__RDY = !running;

    always @( posedge CLK) begin
      if (!nRST) begin
        a <= 0;
        offset <= 0;
        outA <= 0;
        outB <= 0;
        running <= 0;
      end // nRST
      else begin
        if (request$say__ENA == 0) begin // RULE$A__ENA
            outA <= a + offset;
            if (running != 0)
            a <= a + 1;
        end; // End of RULE$A__ENA
        if (request$say__ENA == 0) begin // RULE$B__ENA
            outB <= a + offset;
            if (running == 0)
            a <= 1;
        end; // End of RULE$B__ENA
        if (request$say__ENA == 0) begin // RULE$C__ENA
            offset <= offset + 1;
        end; // End of RULE$C__ENA
        if (!( running | ( !request$say__ENA ) )) begin // request$say__ENA
            a <= request$say$va;
            offset <= 1;
            running <= 1;
        end; // End of request$say__ENA
      end
    end // always @ (posedge CLK)
endmodule 

`default_nettype wire    // set back to default value
