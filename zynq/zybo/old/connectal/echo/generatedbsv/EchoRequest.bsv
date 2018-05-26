package EchoRequest;

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
} Say_Message deriving (Bits);

typedef struct {
    Bit#(16) a;
    Bit#(16) b;
} Say2_Message deriving (Bits);

typedef struct {
    Bit#(8) v;
} SetLeds_Message deriving (Bits);

// exposed wrapper portal interface
interface EchoRequestInputPipes;
    interface PipeOut#(Say_Message) say_PipeOut;
    interface PipeOut#(Say2_Message) say2_PipeOut;
    interface PipeOut#(SetLeds_Message) setLeds_PipeOut;

endinterface
typedef PipePortal#(3, 0, SlaveDataBusWidth) EchoRequestPortalInput;
interface EchoRequestInput;
    interface EchoRequestPortalInput portalIfc;
    interface EchoRequestInputPipes pipes;
endinterface
interface EchoRequestWrapperPortal;
    interface EchoRequestPortalInput portalIfc;
endinterface
// exposed wrapper MemPortal interface
interface EchoRequestWrapper;
    interface StdPortal portalIfc;
endinterface

instance Connectable#(EchoRequestInputPipes,EchoRequest);
   module mkConnection#(EchoRequestInputPipes pipes, EchoRequest ifc)(Empty);

    rule handle_say_request;
        let request <- toGet(pipes.say_PipeOut).get();
        ifc.say(request.v);
    endrule

    rule handle_say2_request;
        let request <- toGet(pipes.say2_PipeOut).get();
        ifc.say2(request.a, request.b);
    endrule

    rule handle_setLeds_request;
        let request <- toGet(pipes.setLeds_PipeOut).get();
        ifc.setLeds(request.v);
    endrule

   endmodule
endinstance

// exposed wrapper Portal implementation
(* synthesize *)
module mkEchoRequestInput(EchoRequestInput);
    Vector#(3, PipeIn#(Bit#(SlaveDataBusWidth))) requestPipeIn;

    AdapterFromBus#(SlaveDataBusWidth,Say_Message) say_requestAdapter <- mkAdapterFromBus();
    requestPipeIn[0] = say_requestAdapter.in;

    AdapterFromBus#(SlaveDataBusWidth,Say2_Message) say2_requestAdapter <- mkAdapterFromBus();
    requestPipeIn[1] = say2_requestAdapter.in;

    AdapterFromBus#(SlaveDataBusWidth,SetLeds_Message) setLeds_requestAdapter <- mkAdapterFromBus();
    requestPipeIn[2] = setLeds_requestAdapter.in;

    interface PipePortal portalIfc;
        interface PortalSize messageSize;
        method Bit#(16) size(Bit#(16) methodNumber);
            case (methodNumber)
            0: return fromInteger(valueOf(SizeOf#(Say_Message)));
            1: return fromInteger(valueOf(SizeOf#(Say2_Message)));
            2: return fromInteger(valueOf(SizeOf#(SetLeds_Message)));
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
    interface EchoRequestInputPipes pipes;
        interface say_PipeOut = say_requestAdapter.out;
        interface say2_PipeOut = say2_requestAdapter.out;
        interface setLeds_PipeOut = setLeds_requestAdapter.out;
    endinterface
endmodule

module mkEchoRequestWrapperPortal#(EchoRequest ifc)(EchoRequestWrapperPortal);
    let dut <- mkEchoRequestInput;
    mkConnection(dut.pipes, ifc);
    interface PipePortal portalIfc = dut.portalIfc;
endmodule

interface EchoRequestWrapperMemPortalPipes;
    interface EchoRequestInputPipes pipes;
    interface MemPortal#(12,32) portalIfc;
endinterface

(* synthesize *)
module mkEchoRequestWrapperMemPortalPipes#(Bit#(SlaveDataBusWidth) id)(EchoRequestWrapperMemPortalPipes);

  let dut <- mkEchoRequestInput;
  PortalCtrlMemSlave#(SlaveControlAddrWidth,SlaveDataBusWidth) ctrlPort <- mkPortalCtrlMemSlave(id, dut.portalIfc.intr);
  let memslave  <- mkMemMethodMuxIn(ctrlPort.memSlave,dut.portalIfc.requests);
  interface EchoRequestInputPipes pipes = dut.pipes;
  interface MemPortal portalIfc = (interface MemPortal;
      interface PhysMemSlave slave = memslave;
      interface ReadOnly interrupt = ctrlPort.interrupt;
      interface WriteOnly num_portals = ctrlPort.num_portals;
    endinterface);
endmodule

// exposed wrapper MemPortal implementation
module mkEchoRequestWrapper#(idType id, EchoRequest ifc)(EchoRequestWrapper)
   provisos (Bits#(idType, a__),
	     Add#(b__, a__, SlaveDataBusWidth));
  let dut <- mkEchoRequestWrapperMemPortalPipes(zeroExtend(pack(id)));
  mkConnection(dut.pipes, ifc);
  interface MemPortal portalIfc = dut.portalIfc;
endmodule

// exposed proxy interface
typedef PipePortal#(0, 3, SlaveDataBusWidth) EchoRequestPortalOutput;
interface EchoRequestOutput;
    interface EchoRequestPortalOutput portalIfc;
    interface Echo::EchoRequest ifc;
endinterface
interface EchoRequestProxy;
    interface StdPortal portalIfc;
    interface Echo::EchoRequest ifc;
endinterface

interface EchoRequestOutputPipeMethods;
    interface PipeIn#(Say_Message) say;
    interface PipeIn#(Say2_Message) say2;
    interface PipeIn#(SetLeds_Message) setLeds;

endinterface

interface EchoRequestOutputPipes;
    interface EchoRequestOutputPipeMethods methods;
    interface EchoRequestPortalOutput portalIfc;
endinterface

function Bit#(16) getEchoRequestMessageSize(Bit#(16) methodNumber);
    case (methodNumber)
            0: return fromInteger(valueOf(SizeOf#(Say_Message)));
            1: return fromInteger(valueOf(SizeOf#(Say2_Message)));
            2: return fromInteger(valueOf(SizeOf#(SetLeds_Message)));
    endcase
endfunction

(* synthesize *)
module mkEchoRequestOutputPipes(EchoRequestOutputPipes);
    Vector#(3, PipeOut#(Bit#(SlaveDataBusWidth))) indicationPipes;

    AdapterToBus#(SlaveDataBusWidth,Say_Message) say_responseAdapter <- mkAdapterToBus();
    indicationPipes[0] = say_responseAdapter.out;

    AdapterToBus#(SlaveDataBusWidth,Say2_Message) say2_responseAdapter <- mkAdapterToBus();
    indicationPipes[1] = say2_responseAdapter.out;

    AdapterToBus#(SlaveDataBusWidth,SetLeds_Message) setLeds_responseAdapter <- mkAdapterToBus();
    indicationPipes[2] = setLeds_responseAdapter.out;

    PortalInterrupt#(SlaveDataBusWidth) intrInst <- mkPortalInterrupt(indicationPipes);
    interface EchoRequestOutputPipeMethods methods;
    interface say = say_responseAdapter.in;
    interface say2 = say2_responseAdapter.in;
    interface setLeds = setLeds_responseAdapter.in;

    endinterface
    interface PipePortal portalIfc;
        interface PortalSize messageSize;
            method size = getEchoRequestMessageSize;
        endinterface
        interface Vector requests = nil;
        interface Vector indications = indicationPipes;
        interface PortalInterrupt intr = intrInst;
    endinterface
endmodule

(* synthesize *)
module mkEchoRequestOutput(EchoRequestOutput);
    let indicationPipes <- mkEchoRequestOutputPipes;
    interface Echo::EchoRequest ifc;

    method Action say(Bit#(32) v);
        indicationPipes.methods.say.enq(Say_Message {v: v});
        //$display("indicationMethod 'say' invoked");
    endmethod
    method Action say2(Bit#(16) a, Bit#(16) b);
        indicationPipes.methods.say2.enq(Say2_Message {a: a, b: b});
        //$display("indicationMethod 'say2' invoked");
    endmethod
    method Action setLeds(Bit#(8) v);
        indicationPipes.methods.setLeds.enq(SetLeds_Message {v: v});
        //$display("indicationMethod 'setLeds' invoked");
    endmethod
    endinterface
    interface PipePortal portalIfc = indicationPipes.portalIfc;
endmodule
instance PortalMessageSize#(EchoRequestOutput);
   function Bit#(16) portalMessageSize(EchoRequestOutput p, Bit#(16) methodNumber);
      return getEchoRequestMessageSize(methodNumber);
   endfunction
endinstance


interface EchoRequestInverse;
    method ActionValue#(Say_Message) say;
    method ActionValue#(Say2_Message) say2;
    method ActionValue#(SetLeds_Message) setLeds;

endinterface

interface EchoRequestInverter;
    interface Echo::EchoRequest ifc;
    interface EchoRequestInverse inverseIfc;
endinterface

instance Connectable#(EchoRequestInverse, EchoRequestOutputPipeMethods);
   module mkConnection#(EchoRequestInverse in, EchoRequestOutputPipeMethods out)(Empty);
    mkConnection(in.say, out.say);
    mkConnection(in.say2, out.say2);
    mkConnection(in.setLeds, out.setLeds);

   endmodule
endinstance

(* synthesize *)
module mkEchoRequestInverter(EchoRequestInverter);
    FIFOF#(Say_Message) fifo_say <- mkFIFOF();
    FIFOF#(Say2_Message) fifo_say2 <- mkFIFOF();
    FIFOF#(SetLeds_Message) fifo_setLeds <- mkFIFOF();

    interface Echo::EchoRequest ifc;

    method Action say(Bit#(32) v);
        fifo_say.enq(Say_Message {v: v});
    endmethod
    method Action say2(Bit#(16) a, Bit#(16) b);
        fifo_say2.enq(Say2_Message {a: a, b: b});
    endmethod
    method Action setLeds(Bit#(8) v);
        fifo_setLeds.enq(SetLeds_Message {v: v});
    endmethod
    endinterface
    interface EchoRequestInverse inverseIfc;

    method ActionValue#(Say_Message) say;
        fifo_say.deq;
        return fifo_say.first;
    endmethod
    method ActionValue#(Say2_Message) say2;
        fifo_say2.deq;
        return fifo_say2.first;
    endmethod
    method ActionValue#(SetLeds_Message) setLeds;
        fifo_setLeds.deq;
        return fifo_setLeds.first;
    endmethod
    endinterface
endmodule

(* synthesize *)
module mkEchoRequestInverterV(EchoRequestInverter);
    PutInverter#(Say_Message) inv_say <- mkPutInverter();
    PutInverter#(Say2_Message) inv_say2 <- mkPutInverter();
    PutInverter#(SetLeds_Message) inv_setLeds <- mkPutInverter();

    interface Echo::EchoRequest ifc;

    method Action say(Bit#(32) v);
        inv_say.mod.put(Say_Message {v: v});
    endmethod
    method Action say2(Bit#(16) a, Bit#(16) b);
        inv_say2.mod.put(Say2_Message {a: a, b: b});
    endmethod
    method Action setLeds(Bit#(8) v);
        inv_setLeds.mod.put(SetLeds_Message {v: v});
    endmethod
    endinterface
    interface EchoRequestInverse inverseIfc;

    method ActionValue#(Say_Message) say;
        let v <- inv_say.inverse.get;
        return v;
    endmethod
    method ActionValue#(Say2_Message) say2;
        let v <- inv_say2.inverse.get;
        return v;
    endmethod
    method ActionValue#(SetLeds_Message) setLeds;
        let v <- inv_setLeds.inverse.get;
        return v;
    endmethod
    endinterface
endmodule

// synthesizeable proxy MemPortal
(* synthesize *)
module mkEchoRequestProxySynth#(Bit#(SlaveDataBusWidth) id)(EchoRequestProxy);
  let dut <- mkEchoRequestOutput();
  PortalCtrlMemSlave#(SlaveControlAddrWidth,SlaveDataBusWidth) ctrlPort <- mkPortalCtrlMemSlave(id, dut.portalIfc.intr);
  let memslave  <- mkMemMethodMuxOut(ctrlPort.memSlave,dut.portalIfc.indications);
  interface MemPortal portalIfc = (interface MemPortal;
      interface PhysMemSlave slave = memslave;
      interface ReadOnly interrupt = ctrlPort.interrupt;
      interface WriteOnly num_portals = ctrlPort.num_portals;
    endinterface);
  interface Echo::EchoRequest ifc = dut.ifc;
endmodule

// exposed proxy MemPortal
module mkEchoRequestProxy#(idType id)(EchoRequestProxy)
   provisos (Bits#(idType, a__),
	     Add#(b__, a__, SlaveDataBusWidth));
   let rv <- mkEchoRequestProxySynth(extend(pack(id)));
   return rv;
endmodule
endpackage: EchoRequest
