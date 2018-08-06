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

module VsimSend #(parameter width = 32) (input CLK, input nRST,
    input beat__ENA, output beat__RDY, input [width-1:0] beat$v, input beat$last);

    import "DPI-C" function void dpi_msgSend_beat(input int beat, input int last);
    assign beat__RDY = 1;
    always @(posedge CLK) begin
        if (nRST != 0) begin
            if (beat__ENA) begin
                //$display("VSOURCE: outgoing data %x last %x", beat$v, beat$last);
                dpi_msgSend_beat(beat$v, {31'b0, beat$last});
            end
        end
    end
endmodule
