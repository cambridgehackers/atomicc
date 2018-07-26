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

module VsimReceive #(parameter width = 32) (input CLK, input nRST,
    output EN_beat, input RDY_beat, output [width-1:0] beat, output last);

    import "DPI-C" function longint dpi_msgReceive_beat();
    always @(posedge CLK) begin
        if (nRST != `BSV_RESET_VALUE) begin
            if (RDY_beat) begin
`ifndef BOARD_cvc
	        automatic longint v = dpi_msgReceive_beat();
	        last = v[33];
	        EN_beat = v[32];
	        beat = v[31:0];
                //if (v[32])
                    //$display("VsimReceive: last %x beat %x", v[33], v[31:0]);
`else
	        { last, EN_beat, beat } = dpi_msgReceive_beat();
`endif
            end
            else begin
	        last = 0;
	        EN_beat = 0;
	        beat = 0;
            end
        end
    end
endmodule
