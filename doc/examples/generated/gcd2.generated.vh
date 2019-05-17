`ifndef __gcd2_GENERATED__VH__
`define __gcd2_GENERATED__VH__

//METASTART; Gcd2
//METAEXTERNAL; indication; l_ainterface_OC_UserIndication;
//METAEXCLUSIVE; RULE$flip_rule__ENA; RULE$mod_rule__ENA; request$say__ENA
//METABEFORE; RULE$flip_rule__ENA; :RULE$mod_rule__ENA; :request$say__ENA
//METAGUARD; RULE$flip_rule; 1;
//METAEXCLUSIVE; RULE$mod_rule__ENA; request$say__ENA
//METABEFORE; RULE$mod_rule__ENA; :RULE$flip_rule__ENA; :request$say__ENA
//METAGUARD; RULE$mod_rule; 1;
//METAINVOKE; RULE$respond_rule__ENA; ( b == 0 ) & ( running != 0 ):indication$gcd__ENA;
//METAEXCLUSIVE; RULE$respond_rule__ENA; request$say__ENA
//METABEFORE; RULE$respond_rule__ENA; :RULE$flip_rule__ENA; :RULE$mod_rule__ENA; :request$say__ENA
//METAGUARD; RULE$respond_rule; ( b != 32'd0 ) | indication$gcd__RDY | ( !running );
//METAGUARD; request$say; !running;
//METARULES; RULE$flip_rule; RULE$mod_rule; RULE$respond_rule
`endif
