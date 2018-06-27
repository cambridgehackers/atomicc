#
set -x
set -e
scripts/importbvi.py -o MMCME2.cpp -C MMCME2_ADV -I MMCME2_ADV -P Mmcme2 \
    -f CLKFB -f CLK -f PS -f D \
    ..//Vivado/2015.2/data/parts/xilinx/zynq/zynq.lib
