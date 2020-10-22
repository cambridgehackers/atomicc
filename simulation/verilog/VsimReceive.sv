// Copyright (c) 2015,2018 The Connectal Project

// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

`ifndef __PipeInLast_DEF__
`define __PipeInLast_DEF__
interface PipeInLast#(width = 32);
    logic enq__ENA;
    logic [width - 1:0] enq$v;
    logic  enq$last;
    logic enq__RDY;
    modport server (input  enq__ENA, enq$v, enq$last,
                    output enq__RDY);
    modport client (output enq__ENA, enq$v, enq$last,
                    input  enq__RDY);
endinterface
`endif
module VsimReceive #(parameter width = 32) (input CLK, input nRST, PipeInLast.client port);
    //output reg enq__ENA, input enq__RDY, output reg [width-1:0] enq$v, output reg enq$last);

    import "DPI-C" function longint dpi_msgReceive_enq();
    always @(posedge CLK) begin
        if (nRST != 0) begin
            if (port.enq__RDY) begin
`ifndef BOARD_cvc
	        automatic longint v = dpi_msgReceive_enq();
	        port.enq$last = v[33];
	        port.enq__ENA = v[32];
	        port.enq$v = v[31:0];
                //if (v[32])
                    //$display("VsimReceive: last %x enq %x", v[33], v[31:0]);
`else
	        { port.enq$last, port.enq__ENA, port.enq$v } = dpi_msgReceive_enq();
`endif
            end
            else begin
	        port.enq$last = 0;
	        port.enq__ENA = 0;
	        port.enq$v = 0;
            end
        end
    end
endmodule
