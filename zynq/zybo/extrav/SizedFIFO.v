module SizedFIFO(CLK, RST, D_IN, ENQ, FULL_N, D_OUT, DEQ, EMPTY_N, CLR);
   parameter               p1width = 1;
   parameter               p2depth = 3;
   parameter               p3cntr_width = 1;
   parameter               guarded = 1;
   input                   CLK;
   input                   RST;
   input                   CLR;
   input [p1width - 1 : 0] D_IN;
   input                   ENQ;
   input                   DEQ;
   output                  FULL_N;
   output                  EMPTY_N;
   output [p1width - 1 : 0] D_OUT;
endmodule
