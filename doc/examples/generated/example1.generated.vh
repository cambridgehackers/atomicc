`ifndef __example1_GENERATED__VH__
`define __example1_GENERATED__VH__

//METASTART; Example1
//METAEXTERNAL; indication; l_ainterface_OC_UserIndication;
//METAEXCLUSIVE; RULE$delay2_rule__ENA; RULE$delay_rule__ENA; RULE$respond_rule__ENA; request$say__ENA
//METAGUARD; RULE$delay2_rule; busy_delay == 2'd1;
//METAEXCLUSIVE; RULE$delay_rule__ENA; RULE$respond_rule__ENA
//METAGUARD; RULE$delay_rule; busy & ( busy_delay == 2'd0 );
//METAINVOKE; RULE$respond_rule__ENA; :indication$heard__ENA;
//METAEXCLUSIVE; RULE$respond_rule__ENA; request$say__ENA
//METAGUARD; RULE$respond_rule; ( busy_delay == 2'd2 ) & indication$heard__RDY;
//METAGUARD; request$say; !busy;
//METARULES; RULE$delay2_rule; RULE$delay_rule; RULE$respond_rule
`endif
