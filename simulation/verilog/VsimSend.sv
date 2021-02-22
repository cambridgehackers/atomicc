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

module VsimSend #(parameter width = 32) (input CLK, input nRST, PipeInLast.server port);
`ifndef YOSYS
    import "DPI-C" function void dpi_msgSend_enq(input int enq, input int last);
    assign port.enq__RDY = 1'b1;
    always @(posedge CLK) begin
        if (nRST != 0) begin
            if (port.enq__ENA) begin
                //$display("VSOURCE: outgoing data %x last %x", port.enq$v, port.enq$last);
                dpi_msgSend_enq(port.enq$v, {31'b0, port.enq$last});
            end
        end
    end
`endif // YOSYS
endmodule
