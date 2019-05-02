\begin{codeblock}
__interface LpmRequest {
    void       enter(IPA x);
};

__module Lpm {
    LpmRequest          request;
    BufTicket    compBuf;
    Fifo1<IPA>   inQ;
    FifoB1<ProcessData>   fifo;
    PipeIn<IPA> *outQ;
    LpmMemory           mem;
    Lpm() {
        __rule recirc if (!p(mem.ifc.resValue())) {
            auto x = mem.ifc.resValue();
            auto y = fifo.out.first();
            mem.ifc.resAccept();
	    mem.ifc.req(compute_addr(x, y.state, y.IPA));
	    fifo.out.deq();
	    fifo.in.enq(ProcessData{y.ticket, y.IPA, y.state + 1});
        };
        __rule exitr if (p(mem.ifc.resValue()) & !__valid(RULE$recirc)) {
            auto x = mem.ifc.resValue();
            auto y = fifo.out.first();
            mem.ifc.resAccept();
	    fifo.out.deq();
	    outQ->enq(f1(x,y));
        };
        __rule enter if (!__valid(RULE$recirc)) {
            auto x = inQ.out.first();
            auto ticket = compBuf.tickIfc.getTicket();
            compBuf.tickIfc.allocateTicket();
            inQ.out.deq();
	    fifo.in.enq(ProcessData{ticket, static_cast<__uint(16)>(__bitsubstr(x, 15, 0)), 0});
	    mem.ifc.req(addr(x));
        };
    };
    void request.enter(IPA x) {
	inQ.in.enq(x);
    }
};
\end{codeblock}
