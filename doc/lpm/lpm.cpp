\begin{codeblock}
typedef struct {
    int a;
    int b;
} ValuePair;

__interface LpmIndication {
    void heard(int meth, int v);
};

__interface LpmRequest {
    void say(int meth, int v);
};

__interface LpmMem {
    void req(ValuePair v);
    void resAccept(void);
    ValuePair resValue(void);
};

__module Lpm {
    Fifo1<ValuePair> inQ;
    Fifo1<ValuePair> fifo;
    Fifo1<ValuePair> outQ;
    LpmMemory        mem;
    int doneCount;
    void request.say(int meth, int v) {
        ValuePair temp;
        temp.a = meth;
        temp.b = v;
        inQ.in.enq(temp);
    }
    bool done() {
        doneCount++;
        return !(doneCount % 5);
    }
    LpmIndication *ind;
    LpmRequest request;
public:
    Lpm() {
            __rule recirc {
                ValuePair temp = fifo.out.first();
                ValuePair mtemp = mem.ifc.resValue();
                mem.ifc.resAccept();
	        fifo.out.deq();
	        fifo.in.enq(mtemp);
	        mem.ifc.req(temp);
                };
            __rule exit_rule {
                ValuePair temp = fifo.out.first();
                ValuePair mtemp = mem.ifc.resValue();
                mem.ifc.resAccept();
	        fifo.out.deq();
	        outQ.in.enq(temp);
                };
            __rule enter {
                ValuePair temp = inQ.out.first();
	        inQ.out.deq();
	        fifo.in.enq(temp);
	        mem.ifc.req(temp);
                };
            __rule respond {
                ValuePair temp = outQ.out.first();
	        outQ.out.deq();
	        ind->heard(temp.a, temp.b);
                };
            atomiccSchedulePriority("recirc", "enter;exit", 0);
    };
};

Lpm lpmbase;
\end{codeblock}
