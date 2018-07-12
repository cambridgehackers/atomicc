///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 1995/2004 Xilinx, Inc.
// All Right Reserved.
///////////////////////////////////////////////////////////////////////////////
//   ____  ____
//  /   /\/   /
// /___/  \  /    Vendor : Xilinx
// \   \   \/     Version : 10.1
//  \   \         Description : Xilinx Formal Library Component
//  /   /                  Buffer
// /___/   /\     Filename : BUFG.v
// \   \  /  \    Timestamp : Tue Feb 26 11:11:42 PST 2008
//  \___\/\___\
//
// Revision:
//    04/01/08 - Initial version.
// End Revision

`celldefine
`timescale  1 ps / 1 ps

module BUFG (O, I);

`ifdef XIL_TIMING //Simprim
 
  parameter LOC = "UNPLACED";

`endif

    output O;

    input  I;

	buf B1 (O, I);

endmodule
`endcelldefine
