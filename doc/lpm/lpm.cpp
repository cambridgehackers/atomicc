\begin{codeblock}
typedef int LookupItem;

__interface LpmRequest {
    void       enter(LookupItem v);
};

__interface LpmMem {
    void       req(LookupItem v);
    void       resAccept(void);
    LookupItem res(void);
};

__emodule LpmMemory {
    LpmMem     ifc;
};

__module Lpm {
    LpmRequest          request;
    Fifo1<LookupItem>   inQ;
    Fifo1<LookupItem>   fifo;
    PipeIn<LookupItem> *outQ;
    LpmMemory           mem;
    Lpm() {
        __rule recirc {
            auto x = mem.ifc.res();
            auto y = fifo.out.first();
            mem.ifc.resAccept();
	    mem.ifc.req(y);
	    fifo.out.deq();
	    fifo.in.enq(f2(x,y));
        };
        __rule exitr {
            auto x = mem.ifc.res();
            auto y = fifo.out.first();
            mem.ifc.resAccept();
	    fifo.out.deq();
	    outQ->enq(f1(x,y));
        };
        __rule enter {
            auto x = inQ.out.first;
            inQ.out.deq();
	    fifo.in.enq(x);
	    mem.ifc.req(addr(x));
        };
        atomiccSchedulePriority("recirc", "exitr;enter", 0);
    };
    void request.enter(LookupItem x) {
	inQ.in.enq(x);
    }
};
\end{codeblock}
