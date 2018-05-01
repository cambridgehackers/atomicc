/* Copyright (c) 2018 The Connectal Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>      // FIONBIO

#include "vlsim.h"
#include "verilated.h"
#include <ConnectalProjectConfig.h>
#if VM_TRACE
# include <verilated_vcd_c.h>   // Trace file format header
#endif
#include "sock_utils.h"

#ifdef BSV_POSITIVE_RESET
  #define BSV_RESET_VALUE 1
  #define BSV_RESET_EDGE 0 //posedge
#else
  #define BSV_RESET_VALUE 0
  #define BSV_RESET_EDGE 1 //negedge
#endif

#define MAX_REQUEST_LENGTH 1000
void memdump(unsigned char *p, int len, const char *title);

bool dump_vcd = false;
const char *vcd_file_name = "dump.vcd";
vluint64_t main_time = 0;
vluint64_t derived_time = 0;

static int trace_xsimtop = 1;
static int masterfpga_fd = -1, clientfd = -1, masterfpga_number = 5;
static uint32_t rxBuffer[MAX_REQUEST_LENGTH], txBuffer[MAX_REQUEST_LENGTH];
static int txIndex = 1, rxLength;

static int finish = 0;
long cycleCount;

extern "C" int dpi_cycle()
{
  cycleCount++;
  return finish;
}

double sc_time_stamp()
{
  return (double)cycleCount;
}

extern "C" void dpi_init()
{
    char buff[100];
    sprintf(buff, "SWSOCK%d", masterfpga_number);
    masterfpga_fd = init_listening(buff, NULL);
    int on = 1;
    ioctl(masterfpga_fd, FIONBIO, &on);
    if (trace_xsimtop) fprintf(stdout, "%s: end\n", __FUNCTION__);
}

#define VALID_FLAG (1ll << 32)
#define END_FLAG   (2ll << 32)
extern "C" long long dpi_msgSink_beat(void)
{
top:
    if (rxLength > 1) {
        long long ret = VALID_FLAG | rxBuffer[--rxLength];
        if (rxLength == 1)
            ret |= END_FLAG;
        return ret;
    }
    if (clientfd != -1) {
        int sendFd;
        int len = portalRecvFd(clientfd, (void *)rxBuffer, sizeof(uint32_t), &sendFd);
        if (len == 0) { /* EOF */
            printf("VerilatorTop.disconnect called %d\n", clientfd);
            close(clientfd);
            clientfd = -1;
            exit(-1);
        }
        else if (len == -1) {
            if (errno != EAGAIN) {
                printf( "%s[%d]: read error %d\n",__FUNCTION__, clientfd, errno);
                exit(1);
            }
        }
        else if (len == sizeof(uint32_t)) {
            rxLength = rxBuffer[0] & 0xffff;
            int rc = portalRecvFd(clientfd, (void *)&rxBuffer[1], (rxLength-1) * sizeof(uint32_t), &sendFd);
            if (rc > 0) {
                char bname[100];
                sprintf(bname,"RECV%d.%d", getpid(), clientfd);
                memdump((uint8_t*)rxBuffer, rxLength * sizeof(uint32_t), bname);
                rxBuffer[1] = ((rxBuffer[1] & 0xffff0000) | (rxBuffer[0] >> 16)); //combine methodNumber and portalNumber
                goto top;
            }
        }
    }
    else if (masterfpga_fd != -1) {
        int sockfd = accept_socket(masterfpga_fd);
        if (sockfd != -1) {
            printf("[%s:%d]afteracc accfd %d fd %d\n", __FUNCTION__, __LINE__, masterfpga_fd, sockfd);
            clientfd = sockfd;
        }
    }
  return 0xbadad7a;
}

extern "C" void dpi_msgSource_beat(int beat, int last)
{
    //if (trace_xsimtop)
        //fprintf(stdout, "dpi_msgSource_beat: beat=%08x\n", beat);
//printf("[%s:%d] index %x txBuffer[0] %x beat %x last %x\n", __FUNCTION__, __LINE__, txIndex, txBuffer[0], beat, last);
    txBuffer[txIndex++] = beat;
    if (last) {
        char bname[100];
        sprintf(bname,"SEND%d.%d", getpid(), masterfpga_fd);
        txBuffer[0] = (txBuffer[1] << 16) | txIndex;
        txBuffer[1] = (txBuffer[1] & 0xffff0000) | (txIndex - 1);
        memdump((uint8_t*)txBuffer, txIndex * sizeof(uint32_t), bname);
        portalSendFd(clientfd, (void *)txBuffer, (txBuffer[0] & 0xffff) * sizeof(uint32_t), -1);
        txIndex = 1;
    }
}

static void parseArgs(int argc, char **argv)
{
    signed char opt;
    while ((opt = getopt(argc, argv, "ht:")) != -1) {
      switch (opt) {
      case 't':
	dump_vcd = true;
	vcd_file_name = optarg;
	break;
      }
    }
}

int main(int argc, char **argv, char **env)
{
  fprintf(stderr, "vlsim::main\n");
  Verilated::commandArgs(argc, argv);
  parseArgs(argc, argv);

  vlsim* top = new vlsim;

  fprintf(stderr, "vlsim calling dpi_init\n");
  dpi_init();

#if VM_TRACE                    // If verilator was invoked with --trace
  VerilatedVcdC* tfp = 0;
  if (dump_vcd) {
        Verilated::traceEverOn(true);       // Verilator must compute traced signals
	VL_PRINTF("Enabling vcd waves to %s\n", vcd_file_name);
	tfp = new VerilatedVcdC;
	top->trace (tfp, 4);       // Trace 4 levels of hierarchy
	tfp->open (vcd_file_name); // Open the dump file
  }
#endif

  fprintf(stderr, "starting simulation\n");
  top->CLK = 0;
  top->RST_N = BSV_RESET_VALUE;
  top->CLK_derivedClock = 0;
  top->CLK_sys_clk = 0;
  top->RST_N_derivedReset = BSV_RESET_VALUE;
  while (!Verilated::gotFinish()) {
    if (main_time >= 10) {
      if ((top->CLK == BSV_RESET_EDGE) && (top->RST_N == BSV_RESET_VALUE)) {
	fprintf(stderr, "time=%ld leaving reset new value %d\n", (long)main_time, !BSV_RESET_VALUE);
	top->RST_N = !BSV_RESET_VALUE;
      }
    }
    if (derived_time >= 10) {
      if ((top->CLK_derivedClock == BSV_RESET_EDGE) && (top->RST_N_derivedReset == BSV_RESET_VALUE)) {
	fprintf(stderr, "time=%ld deasserting derivedReset new value %d\n", (long)main_time, !BSV_RESET_VALUE);
	top->RST_N_derivedReset = !BSV_RESET_VALUE;
      }
    }

    if (dpi_cycle())
      vl_finish(__FILE__, __LINE__, "vlsim");

    top->CLK = main_time % 2;
    top->CLK_derivedClock = derived_time % 2;
    top->CLK_sys_clk = main_time % 2;
    top->eval();

#if VM_TRACE
    if (tfp) tfp->dump (main_time); // Create waveform trace for this timestamp                                                                
#endif

    main_time++;
    if (main_time & 1)
      derived_time++;
  }
  top->final();
#if VM_TRACE
  if (tfp) tfp->close();
#endif                                                                                                                                             

  delete top;
  exit(0);
}
