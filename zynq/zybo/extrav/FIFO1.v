
module FIFO1(CLK,
             RST,
             D_IN,
             ENQ,
             FULL_N,
             D_OUT,
             DEQ,
             EMPTY_N,
             CLR
             );
   parameter width = 1;
   parameter guarded = 1;
   input                  CLK;
   input                  RST;
   input [width - 1 : 0]  D_IN;
   input                  ENQ;
   input                  DEQ;
   input                  CLR ;
   output                 FULL_N;
   output [width - 1 : 0] D_OUT;
   output                 EMPTY_N;
endmodule
