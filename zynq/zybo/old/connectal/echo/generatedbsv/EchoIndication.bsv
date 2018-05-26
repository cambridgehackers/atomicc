package EchoIndication;

import FIFO::*;
import FIFOF::*;
import GetPut::*;
import Connectable::*;
import Clocks::*;
import FloatingPoint::*;
import Adapter::*;
import Leds::*;
import Vector::*;
import SpecialFIFOs::*;
import ConnectalConfig::*;
import ConnectalMemory::*;
import Portal::*;
import CtrlMux::*;
import ConnectalMemTypes::*;
import Pipe::*;
import HostInterface::*;
import LinkerLib::*;
import Echo::*;
import FIFO::*;
import Vector::*;
import GetPut::*;




typedef struct {
    Bit#(32) v;
} Heard_Message deriving (Bits);

typedef struct {
    Bit#(16) a;
    Bit#(16) b;
} Heard2_Message deriving (Bits);

// exposed wrapper portal interface
interface EchoIndicationInputPipes;
    interface PipeOut#(Heard_Message) heard_PipeOut;
    interface PipeOut#(Heard2_Message) heard2_PipeOut;

endinterface
typedef PipePortal#(2, 0, SlaveDataBusWidth) EchoIndicationPortalInput;
interface EchoIndicationInput;
    interface EchoIndicationPortalInput portalIfc;
    interface EchoIndicationInputPipes pipes;
endinterface
interface EchoIndicationWrapperPortal;
    interface EchoIndicationPortalInput portalIfc;
endinterface
// exposed wrapper MemPortal interface
interface EchoIndicationWrapper;
    interface StdPortal portalIfc;
endinterface

instance Connectable#(EchoIndicationInputPipes,EchoIndication);
   module mkConnection#(EchoIndicationInputPipes pipes, EchoIndication ifc)(Empty);

    rule handle_heard_request;
        let request <- toGet(pipes.heard_PipeOut).get();
        ifc.heard(request.v);
    endrule

    rule handle_heard2_request;
        let request <- toGet(pipes.heard2_PipeOut).get();
        ifc.heard2(request.a, request.b);
    endrule

   endmodule
endinstance

// exposed wrapper Portal implementation
(* synthesize *)
module mkEchoIndicationInput(EchoIndicationInput);
    Vector#(2, PipeIn#(Bit#(SlaveDataBusWidth))) requestPipeIn;

    AdapterFromBus#(SlaveDataBusWidth,Heard_Message) heard_requestAdapter <- mkAdapterFromBus();
    requestPipeIn[0] = heard_requestAdapter.in;

    AdapterFromBus#(SlaveDataBusWidth,Heard2_Message) heard2_requestAdapter <- mkAdapterFromBus();
    requestPipeIn[1] = heard2_requestAdapter.in;

    interface PipePortal portalIfc;
        interface PortalSize messageSize;
        method Bit#(16) size(Bit#(16) methodNumber);
            case (methodNumber)
            0: return fromInteger(valueOf(SizeOf#(Heard_Message)));
            1: return fromInteger(valueOf(SizeOf#(Heard2_Message)));
            endcase
        endmethod
        endinterface
        interface Vector requests = requestPipeIn;
        interface Vector indications = nil;
        interface PortalInterrupt intr;
           method Bool status();
              return False;
           endmethod
           method Bit#(dataWidth) channel();
              return -1;
           endmethod
        endinterface
    endinterface
    interface EchoIndicationInputPipes pipes;
        interface heard_PipeOut = heard_requestAdapter.out;
        interface heard2_PipeOut = heard2_requestAdapter.out;
    endinterface
endmodule

module mkEchoIndicationWrapperPortal#(EchoIndication ifc)(EchoIndicationWrapperPortal);
    let dut <- mkEchoIndicationInput;
    mkConnection(dut.pipes, ifc);
    interface PipePortal portalIfc = dut.portalIfc;
endmodule

interface EchoIndicationWrapperMemPortalPipes;
    interface EchoIndicationInputPipes pipes;
    interface MemPortal#(12,32) portalIfc;
endinterface

(* synthesize *)
module mkEchoIndicationWrapperMemPortalPipes#(Bit#(SlaveDataBusWidth) id)(EchoIndicationWrapperMemPortalPipes);

  let dut <- mkEchoIndicationInput;
  PortalCtrlMemSlave#(SlaveControlAddrWidth,SlaveDataBusWidth) ctrlPort <- mkPortalCtrlMemSlave(id, dut.portalIfc.intr);
  let memslave  <- mkMemMethodMuxIn(ctrlPort.memSlave,dut.portalIfc.requests);
  interface EchoIndicationInputPipes pipes = dut.pipes;
  interface MemPortal portalIfc = (interface MemPortal;
      interface PhysMemSlave slave = memslave;
      interface ReadOnly interrupt = ctrlPort.interrupt;
      interface WriteOnly num_portals = ctrlPort.num_portals;
    endinterface);
endmodule

// exposed wrapper MemPortal implementation
module mkEchoIndicationWrapper#(idType id, EchoIndication ifc)(EchoIndicationWrapper)
   provisos (Bits#(idType, a__),
	     Add#(b__, a__, SlaveDataBusWidth));
  let dut <- mkEchoIndicationWrapperMemPortalPipes(zeroExtend(pack(id)));
  mkConnection(dut.pipes, ifc);
  interface MemPortal portalIfc = dut.portalIfc;
endmodule

// exposed proxy interface
typedef PipePortal#(0, 2, SlaveDataBusWidth) EchoIndicationPortalOutput;
interface EchoIndicationOutput;
    interface EchoIndicationPortalOutput portalIfc;
    interface Echo::EchoIndication ifc;
endinterface
interface EchoIndicationProxy;
    interface StdPortal portalIfc;
    interface Echo::EchoIndication ifc;
endinterface

interface EchoIndicationOutputPipeMethods;
    interface PipeIn#(Heard_Message) heard;
    interface PipeIn#(Heard2_Message) heard2;

endinterface

interface EchoIndicationOutputPipes;
    interface EchoIndicationOutputPipeMethods methods;
    interface EchoIndicationPortalOutput portalIfc;
endinterface

function Bit#(16) getEchoIndicationMessageSize(Bit#(16) methodNumber);
    case (methodNumber)
            0: return fromInteger(valueOf(SizeOf#(Heard_Message)));
            1: return fromInteger(valueOf(SizeOf#(Heard2_Message)));
    endcase
endfunction

(* synthesize *)
module mkEchoIndicationOutputPipes(EchoIndicationOutputPipes);
    Vector#(2, PipeOut#(Bit#(SlaveDataBusWidth))) indicationPipes;

    AdapterToBus#(SlaveDataBusWidth,Heard_Message) heard_responseAdapter <- mkAdapterToBus();
    indicationPipes[0] = heard_responseAdapter.out;

    AdapterToBus#(SlaveDataBusWidth,Heard2_Message) heard2_responseAdapter <- mkAdapterToBus();
    indicationPipes[1] = heard2_responseAdapter.out;

    PortalInterrupt#(SlaveDataBusWidth) intrInst <- mkPortalInterrupt(indicationPipes);
    interface EchoIndicationOutputPipeMethods methods;
    interface heard = heard_responseAdapter.in;
    interface heard2 = heard2_responseAdapter.in;

    endinterface
    interface PipePortal portalIfc;
        interface PortalSize messageSize;
            method size = getEchoIndicationMessageSize;
        endinterface
        interface Vector requests = nil;
        interface Vector indications = indicationPipes;
        interface PortalInterrupt intr = intrInst;
    endinterface
endmodule

(* synthesize *)
module mkEchoIndicationOutput(EchoIndicationOutput);
    let indicationPipes <- mkEchoIndicationOutputPipes;
    interface Echo::EchoIndication ifc;

    method Action heard(Bit#(32) v);
        indicationPipes.methods.heard.enq(Heard_Message {v: v});
        //$display("indicationMethod 'heard' invoked");
    endmethod
    method Action heard2(Bit#(16) a, Bit#(16) b);
        indicationPipes.methods.heard2.enq(Heard2_Message {a: a, b: b});
        //$display("indicationMethod 'heard2' invoked");
    endmethod
    endinterface
    interface PipePortal portalIfc = indicationPipes.portalIfc;
endmodule
instance PortalMessageSize#(EchoIndicationOutput);
   function Bit#(16) portalMessageSize(EchoIndicationOutput p, Bit#(16) methodNumber);
      return getEchoIndicationMessageSize(methodNumber);
   endfunction
endinstance


interface EchoIndicationInverse;
    method ActionValue#(Heard_Message) heard;
    method ActionValue#(Heard2_Message) heard2;

endinterface

interface EchoIndicationInverter;
    interface Echo::EchoIndication ifc;
    interface EchoIndicationInverse inverseIfc;
endinterface

instance Connectable#(EchoIndicationInverse, EchoIndicationOutputPipeMethods);
   module mkConnection#(EchoIndicationInverse in, EchoIndicationOutputPipeMethods out)(Empty);
    mkConnection(in.heard, out.heard);
    mkConnection(in.heard2, out.heard2);

   endmodule
endinstance

(* synthesize *)
module mkEchoIndicationInverter(EchoIndicationInverter);
    FIFOF#(Heard_Message) fifo_heard <- mkFIFOF();
    FIFOF#(Heard2_Message) fifo_heard2 <- mkFIFOF();

    interface Echo::EchoIndication ifc;

    method Action heard(Bit#(32) v);
        fifo_heard.enq(Heard_Message {v: v});
    endmethod
    method Action heard2(Bit#(16) a, Bit#(16) b);
        fifo_heard2.enq(Heard2_Message {a: a, b: b});
    endmethod
    endinterface
    interface EchoIndicationInverse inverseIfc;

    method ActionValue#(Heard_Message) heard;
        fifo_heard.deq;
        return fifo_heard.first;
    endmethod
    method ActionValue#(Heard2_Message) heard2;
        fifo_heard2.deq;
        return fifo_heard2.first;
    endmethod
    endinterface
endmodule

(* synthesize *)
module mkEchoIndicationInverterV(EchoIndicationInverter);
    PutInverter#(Heard_Message) inv_heard <- mkPutInverter();
    PutInverter#(Heard2_Message) inv_heard2 <- mkPutInverter();

    interface Echo::EchoIndication ifc;

    method Action heard(Bit#(32) v);
        inv_heard.mod.put(Heard_Message {v: v});
    endmethod
    method Action heard2(Bit#(16) a, Bit#(16) b);
        inv_heard2.mod.put(Heard2_Message {a: a, b: b});
    endmethod
    endinterface
    interface EchoIndicationInverse inverseIfc;

    method ActionValue#(Heard_Message) heard;
        let v <- inv_heard.inverse.get;
        return v;
    endmethod
    method ActionValue#(Heard2_Message) heard2;
        let v <- inv_heard2.inverse.get;
        return v;
    endmethod
    endinterface
endmodule

// synthesizeable proxy MemPortal
(* synthesize *)
module mkEchoIndicationProxySynth#(Bit#(SlaveDataBusWidth) id)(EchoIndicationProxy);
  let dut <- mkEchoIndicationOutput();
  PortalCtrlMemSlave#(SlaveControlAddrWidth,SlaveDataBusWidth) ctrlPort <- mkPortalCtrlMemSlave(id, dut.portalIfc.intr);
  let memslave  <- mkMemMethodMuxOut(ctrlPort.memSlave,dut.portalIfc.indications);
  interface MemPortal portalIfc = (interface MemPortal;
      interface PhysMemSlave slave = memslave;
      interface ReadOnly interrupt = ctrlPort.interrupt;
      interface WriteOnly num_portals = ctrlPort.num_portals;
    endinterface);
  interface Echo::EchoIndication ifc = dut.ifc;
endmodule

// exposed proxy MemPortal
module mkEchoIndicationProxy#(idType id)(EchoIndicationProxy)
   provisos (Bits#(idType, a__),
	     Add#(b__, a__, SlaveDataBusWidth));
   let rv <- mkEchoIndicationProxySynth(extend(pack(id)));
   return rv;
endmodule
endpackage: EchoIndication
