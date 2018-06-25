#
set -x
set -e
scripts/importbvi.py -o PPS7LIB.cpp -C PS7 -I PPS7LIB -P Pps7 \
    -f DDR -f FTMT -f FTMD -f IRQ \
    -f EMIOGPIO -f EMIOPJTAG -f EMIOTRACE -f EMIOWDT -f EVENT -f PS -f SAXIACP \
    -f SAXIHP -f SAXIGP -f MAXIHP -f MAXIGP \
    -f DMA -f EMIOENET -f EMIOI2C -f EMIOSDIO -f EMIOTTC -f EMIOUSB -f EMIOSDIO \
    -f EMIOCAN -f EMIOSPI -f EMIOUART \
    -f FPGAID -f EMIOSRAMINT -f FCLK -f M \
    ..//Vivado/2015.2/data/parts/xilinx/zynq/zynq.lib
