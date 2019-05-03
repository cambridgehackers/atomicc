`ifndef __gcd_GENERATED__VH__
`define __gcd_GENERATED__VH__

//METASTART; Gcd
//METAEXTERNAL; indication; l_ainterface_OC_UserIndication;
//METAEXCLUSIVE; RULE$flip_rule__ENA; RULE$mod_rule__ENA; request$say__ENA
//METAGUARD; RULE$flip_rule; a < b;
//METAEXCLUSIVE; RULE$mod_rule__ENA; request$say__ENA
//METAGUARD; RULE$mod_rule; ( a >= b ) & ( b != 32'd0 );
//METAINVOKE; RULE$respond_rule__ENA; :indication$gcd__ENA;
//METAEXCLUSIVE; RULE$respond_rule__ENA; request$say__ENA
//METAGUARD; RULE$respond_rule; ( b == 32'd0 ) & indication$gcd__RDY;
//METAGUARD; request$say; !busy;
//METARULES; RULE$flip_rule; RULE$mod_rule; RULE$respond_rule
`endif
