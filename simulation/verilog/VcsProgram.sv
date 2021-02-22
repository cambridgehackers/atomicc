// Copyright (c) 2020 The Connectal Project

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

module VcsProgram;
    reg CLK;
    reg nRST;
    reg CLK_derivedClock;
    reg nRST_derivedReset;
    reg CLK_sys_clk;

    VsimTop top(CLK, nRST, CLK_derivedClock, nRST_derivedReset, CLK_sys_clk);

    always #1 CLK = ~CLK;

    initial begin
        CLK = 1'b0;
        nRST = 1'b0;
        CLK_derivedClock = 1'b0;
        nRST_derivedReset = 1'b0;
        CLK_sys_clk = 1'b0;
        #10
        nRST = 1'b1;
        #20
    	$display("VCSTOP starting");
        #9999999999
    	$display("VCSTOP end");
        $finish;
    end
endmodule
