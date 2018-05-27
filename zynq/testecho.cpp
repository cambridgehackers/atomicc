/* Copyright (c) 2014 Quanta Research Cambridge, Inc
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

#include <errno.h>
#include <stdio.h>
#include "EchoIndication.h"
#include "EchoRequest.h"
#include "GeneratedTypes.h"

static EchoRequestProxy *echoRequestProxy = 0;
static sem_t sem_heard2;

class EchoIndication : public EchoIndicationWrapper
{
public:
    virtual void heard(uint32_t v) {
        printf("heard an echo: %d\n", v);
	echoRequestProxy->say2(v, 2*v);
    }
    virtual void heard2(uint16_t a, uint16_t b) {
        sem_post(&sem_heard2);
        printf("heard an echo2: %d %d\n", a, b);
    }
    EchoIndication(unsigned int id) : EchoIndicationWrapper(id) {}
};
EchoIndication *echoIndication;
volatile unsigned int *getPtr(void)
{
int channel = 0;
    PortalInternal *p = &echoIndication->pint;
    return p->transport->mapchannelInd(p, channel);
}
unsigned int readReg(void)
{
    volatile unsigned int* temp_working_addr = getPtr();
    unsigned int rc = temp_working_addr[5];
    temp_working_addr[5] = 0;
    return rc;
}
void writeReg(int ind, unsigned int val)
{
    //printf("[%s:%d] [%x] = %x\n", __FUNCTION__, __LINE__, ind, val);
    getPtr()[ind] = val;
}
void readRegisters(const char *tag)
{
    volatile unsigned int* temp_working_addr = getPtr();
    printf("[%s:%d]   %s:", __FUNCTION__, __LINE__, tag);
    //printf("0x%x,   ", temp_working_addr[5]);
    printf("0x%x,   ", temp_working_addr[7]);
    printf("\n");
    //RDY_requests_0_message_enq              8
    //requests_0_message_notFull              4
    //RDY_indications_0_message_first/deq     2
    //indications_0_message_notEmpty          1
}
void writeReq(int ind, unsigned int val)
{
printf("[%s:%d] val %x\n", __FUNCTION__, __LINE__, val);
readRegisters("REQ        val");
     writeReg(4, ind);
///readRegisters("REQchannel/len");
     writeReg(4, val);
}
void getInd()
{
readRegisters("IND        val");
     unsigned int len = readReg();
     unsigned int val = readReg();
printf("getInd: len %x val %x\n", len, val);
}

static void call_say(int v)
{
    printf("[%s:%d] %d\n", __FUNCTION__, __LINE__, v);
    echoRequestProxy->say(v);
    sem_wait(&sem_heard2);
}

static void call_say2(int v, int v2)
{
    printf("[%s:%d] %d\n", __FUNCTION__, __LINE__, v);
    echoRequestProxy->say2(v, v2);
    sem_wait(&sem_heard2);
}

int main(int argc, const char **argv)
{
    long actualFrequency = 0;
    long requestedFrequency = 1e9 / MainClockPeriod;

    echoIndication = new EchoIndication(IfcNames_EchoIndicationH2S);
    echoRequestProxy = new EchoRequestProxy(IfcNames_EchoRequestS2H);

    int status = setClockFrequency(0, requestedFrequency, &actualFrequency);
    fprintf(stderr, "Requested main clock frequency %5.2f, actual clock frequency %5.2f MHz status=%d errno=%d\n",
	    (double)requestedFrequency * 1.0e-6,
	    (double)actualFrequency * 1.0e-6,
	    status, (status != 0) ? errno : 0);

    int v = 42;
    printf("Saying %d\n", v);
    call_say(v);
    call_say(v*5);
    call_say(v*17);
for (int i = 0; i < 14; i++)
writeReq(0x00002, 0x33000+i); // req
for (int i = 0; i < 14; i++)
getInd();
    call_say(v*93);
    call_say2(v, v*3);
    printf("TEST TYPE: SEM\n");
    echoRequestProxy->setLeds(9);
    return 0;
}
