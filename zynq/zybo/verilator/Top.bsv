
import ConnectalConfig::*;
import Vector::*;
import BuildVector::*;
import Portal::*;
import CnocPortal::*;
import Connectable::*;
import HostInterface::*;
import ConnectalMemTypes::*;
import IfcNames::*;
import EchoIndication::*;
import Echo::*;
import EchoRequest::*;

module mkCnocTop(CnocTop#(1,1,PhysAddrWidth,DataBusWidth,`PinType,NumberOfMasters));
   EchoIndicationOutput lEchoIndicationOutput <- mkEchoIndicationOutput;
   EchoRequestInput lEchoRequestInput <- mkEchoRequestInput;
   let lEcho <- mkEcho(lEchoIndicationOutput.ifc);
   mkConnection(lEchoRequestInput.pipes, lEcho.request);
   let lEchoIndicationOutputNoc <- mkPortalMsgIndication(extend(pack(IfcNames_EchoIndicationH2S)),
       lEchoIndicationOutput.portalIfc.indications, lEchoIndicationOutput.portalIfc.messageSize);
   let lEchoRequestInputNoc <- mkPortalMsgRequest(extend(pack(IfcNames_EchoRequestS2H)),
       lEchoRequestInput.portalIfc.requests);
   interface requests = vec(lEchoRequestInputNoc);
   interface indications = vec(lEchoIndicationOutputNoc);
endmodule : mkCnocTop
