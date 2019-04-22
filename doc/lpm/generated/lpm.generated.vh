`ifndef __lpm_GENERATED__VH__
`define __lpm_GENERATED__VH__

//METASTART; Fifo2
//METAGUARD; in$enq; ( ( windex + 1 ) % 2 ) != rindex;
//METAGUARD; out$deq; rindex != windex;
//METAGUARD; out$first; rindex != windex;
//METASTART; Lpm
//METAEXTERNAL; ind; l_ainterface_OC_LpmIndication;
//METAINTERNAL; inQ; Fifo1Base$__PARAM__$width$96;
//METAINTERNAL; fifo; Fifo2;
//METAINTERNAL; outQ; Fifo1Base$__PARAM__$width$96;
//METAINTERNAL; mem; LpmMemory;
//METAINVOKE; RULE$enter__ENA; :fifo$in$enq__ENA;:inQ$out$deq__ENA;:inQ$out$first;:mem$ifc$req__ENA;
//METAEXCLUSIVE; RULE$enter__ENA; RULE$recirc__ENA
//METAGUARD; RULE$enter; inQ$out$first__RDY & inQ$out$deq__RDY & fifo$in$enq__RDY & mem$ifc$req__RDY;
//METAINVOKE; RULE$exit_rule__ENA; :fifo$out$deq__ENA;:fifo$out$first;:mem$ifc$resAccept__ENA;:mem$ifc$resValue;:outQ$in$enq__ENA;
//METAEXCLUSIVE; RULE$exit_rule__ENA; RULE$recirc__ENA
//METAGUARD; RULE$exit_rule; fifo$out$first__RDY & mem$ifc$resValue__RDY & mem$ifc$resAccept__RDY & fifo$out$deq__RDY & outQ$in$enq__RDY;
//METAINVOKE; RULE$recirc__ENA; :fifo$in$enq__ENA;:fifo$out$deq__ENA;:fifo$out$first;:mem$ifc$req__ENA;:mem$ifc$resAccept__ENA;:mem$ifc$resValue;
//METAGUARD; RULE$recirc; fifo$out$first__RDY & mem$ifc$resValue__RDY & mem$ifc$resAccept__RDY & fifo$out$deq__RDY & fifo$in$enq__RDY & mem$ifc$req__RDY;
//METAINVOKE; RULE$respond__ENA; :ind$heard__ENA;:outQ$out$deq__ENA;:outQ$out$first;
//METAGUARD; RULE$respond; outQ$out$first__RDY & outQ$out$deq__RDY & ind$heard__RDY;
//METAINVOKE; request$say__ENA; :inQ$in$enq__ENA;
//METAGUARD; request$say; inQ$in$enq__RDY;
//METARULES; RULE$enter; RULE$exit_rule; RULE$recirc; RULE$respond
//METAPRIORITY; recirc; enter;exit
//METASTART; LpmMemory
//METAEXCLUSIVE; RULE$memdelay_rule__ENA; ifc$req__ENA; ifc$resAccept__ENA
//METAGUARD; RULE$memdelay_rule; delayCount > 1;
//METAEXCLUSIVE; ifc$req__ENA; ifc$resAccept__ENA
//METAGUARD; ifc$req; delayCount == 32'd0;
//METAGUARD; ifc$resAccept; delayCount == 32'd1;
//METAGUARD; ifc$resValue; delayCount == 32'd1;
//METARULES; RULE$memdelay_rule
`endif
