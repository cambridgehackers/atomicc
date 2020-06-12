///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 1995/2010 Xilinx, Inc.
// All Right Reserved.
///////////////////////////////////////////////////////////////////////////////
//   ____  ____
//  /   /\/   /
// /___/  \  /    Vendor : Xilinx
// \   \   \/     Version : 13.1
//  \   \         Description : Xilinx Formal Library Component
//  /   /                  Boundary Scan Logic Control Circuit for VIRTEX7
// /___/   /\     Filename : BSCANE2.v
// \   \  /  \    Timestamp : Mon Feb  8 21:47:02 PST 2010
//  \___\/\___\
//
// Revision:
//    02/08/10 - Initial version.
// End Revision


`timescale 1 ps / 1 ps 

module BSCANE2 (
  CAPTURE,
  DRCK,
  RESET,
  RUNTEST,
  SEL,
  SHIFT,
  TCK,
  TDI,
  TMS,
  UPDATE,

  TDO
);

  parameter DISABLE_JTAG = "FALSE";
  parameter integer JTAG_CHAIN = 1;


  output CAPTURE;
  output DRCK;
  output RESET;
  output RUNTEST;
  output SEL;
  output SHIFT;
  output TCK;
  output TDI;
  output TMS;
  output UPDATE;

  input TDO;
  reg SEL_zd;

 
endmodule
