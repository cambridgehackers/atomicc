module MMCME2_ADV #(
`ifdef XIL_TIMING
  parameter LOC = "UNPLACED",
`endif
  parameter BANDWIDTH = "OPTIMIZED",
  parameter real CLKFBOUT_MULT_F = 5.000,
  parameter real CLKFBOUT_PHASE = 0.000,
  parameter CLKFBOUT_USE_FINE_PS = "FALSE",
  parameter real CLKIN1_PERIOD = 0.000,
  parameter real CLKIN2_PERIOD = 0.000,
`ifdef XIL_TIMING
  parameter real CLKIN_FREQ_MAX = 1066.000,
  parameter real CLKIN_FREQ_MIN = 10.000,
`endif
  parameter real CLKOUT0_DIVIDE_F = 1.000,
  parameter real CLKOUT0_DUTY_CYCLE = 0.500,
  parameter real CLKOUT0_PHASE = 0.000,
  parameter CLKOUT0_USE_FINE_PS = "FALSE",
  parameter integer CLKOUT1_DIVIDE = 1,
  parameter real CLKOUT1_DUTY_CYCLE = 0.500,
  parameter real CLKOUT1_PHASE = 0.000,
  parameter CLKOUT1_USE_FINE_PS = "FALSE",
  parameter integer CLKOUT2_DIVIDE = 1,
  parameter real CLKOUT2_DUTY_CYCLE = 0.500,
  parameter real CLKOUT2_PHASE = 0.000,
  parameter CLKOUT2_USE_FINE_PS = "FALSE",
  parameter integer CLKOUT3_DIVIDE = 1,
  parameter real CLKOUT3_DUTY_CYCLE = 0.500,
  parameter real CLKOUT3_PHASE = 0.000,
  parameter CLKOUT3_USE_FINE_PS = "FALSE",
  parameter CLKOUT4_CASCADE = "FALSE",
  parameter integer CLKOUT4_DIVIDE = 1,
  parameter real CLKOUT4_DUTY_CYCLE = 0.500,
  parameter real CLKOUT4_PHASE = 0.000,
  parameter CLKOUT4_USE_FINE_PS = "FALSE",
  parameter integer CLKOUT5_DIVIDE = 1,
  parameter real CLKOUT5_DUTY_CYCLE = 0.500,
  parameter real CLKOUT5_PHASE = 0.000,
  parameter CLKOUT5_USE_FINE_PS = "FALSE",
  parameter integer CLKOUT6_DIVIDE = 1,
  parameter real CLKOUT6_DUTY_CYCLE = 0.500,
  parameter real CLKOUT6_PHASE = 0.000,
  parameter CLKOUT6_USE_FINE_PS = "FALSE",
`ifdef XIL_TIMING
  parameter real CLKPFD_FREQ_MAX = 550.000,
  parameter real CLKPFD_FREQ_MIN = 10.000,
`endif
  parameter COMPENSATION = "ZHOLD",
  parameter integer DIVCLK_DIVIDE = 1,
  parameter [0:0] IS_CLKINSEL_INVERTED = 1'b0,
  parameter [0:0] IS_PSEN_INVERTED = 1'b0,
  parameter [0:0] IS_PSINCDEC_INVERTED = 1'b0,
  parameter [0:0] IS_PWRDWN_INVERTED = 1'b0,
  parameter [0:0] IS_RST_INVERTED = 1'b0,
  parameter real REF_JITTER1 = 0.010,
  parameter real REF_JITTER2 = 0.010,
  parameter SS_EN = "FALSE",
  parameter SS_MODE = "CENTER_HIGH",
  parameter integer SS_MOD_PERIOD = 10000,
`ifdef XIL_TIMING
  parameter STARTUP_WAIT = "FALSE",
  parameter real VCOCLK_FREQ_MAX = 1600.000,
  parameter real VCOCLK_FREQ_MIN = 600.000
`else
  parameter STARTUP_WAIT = "FALSE"
`endif
)(
  output CLKFBOUT,
  output CLKFBOUTB,
  output CLKFBSTOPPED,
  output CLKINSTOPPED,
  output CLKOUT0,
  output CLKOUT0B,
  output CLKOUT1,
  output CLKOUT1B,
  output CLKOUT2,
  output CLKOUT2B,
  output CLKOUT3,
  output CLKOUT3B,
  output CLKOUT4,
  output CLKOUT5,
  output CLKOUT6,
  output [15:0] DO,
  output DRDY,
  output LOCKED,
  output PSDONE,
  input CLKFBIN,
  input CLKIN1,
  input CLKIN2,
  input CLKINSEL,
  input [6:0] DADDR,
  input DCLK,
  input DEN,
  input [15:0] DI,
  input DWE,
  input PSCLK,
  input PSEN,
  input PSINCDEC,
  input PWRDWN,
  input RST
);
endmodule