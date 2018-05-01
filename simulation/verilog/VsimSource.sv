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

module VsimSource(CLK, RST_N, EN_data, data_v, data_length, RDY_data);
   parameter width = 64;
   input CLK;
   input RST_N;
   input EN_data;
   input [width-1:0] data_v;
   input [15:0] data_length;
   output RDY_data;
   wire sourceLast;
   reg [width-1 : 0] outgoingData;
   reg indicationState;
   reg [15 : 0] indicationWords;

   assign sourceLast = indicationWords == 16'd1;
   assign RDY_data = !indicationState;

   import "DPI-C" function void dpi_msgSource_beat(input int beat, input int last);

   always @(posedge CLK) begin
    if (RST_N == `BSV_RESET_VALUE) begin
      indicationState <= 0;
      indicationWords <= 16'd0;
    end
    else begin
      if (EN_data) begin
          indicationState <= 1;
          outgoingData <= data_v;
          indicationWords <= data_length + 1;
          //$display("VSOURCE: start data %x", data);
      end
      if (indicationState) begin
          //$display("VSOURCE: outgoing data %x last %x", outgoingData, sourceLast);
          dpi_msgSource_beat(outgoingData[31:0], {31'b0, sourceLast});
          outgoingData <= {32'b0, outgoingData[width-1:32]};
          if (sourceLast)
              indicationState <= 0;
          indicationWords <= indicationWords - 16'd1;
      end
    end
  end
endmodule
