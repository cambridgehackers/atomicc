`ifndef __gcd_GENERATED__VH__
`define __gcd_GENERATED__VH__

//METASTART; Gcd
//METAEXTERNAL; indication; l_ainterface_OC_UserIndication;
//METAEXCLUSIVE; RULE$flip_rule__ENA; RULE$mod_rule__ENA; request$say__ENA
//METAGUARD; RULE$flip_rule; ( running && ( a < b ) ) != 0;
//METAEXCLUSIVE; RULE$mod_rule__ENA; request$say__ENA
//METAGUARD; RULE$mod_rule; ( running && ( a >= b ) && ( b != 32'd0 ) ) != 0;
//METAINVOKE; RULE$respond_rule__ENA; :indication$gcd__ENA;
//METAEXCLUSIVE; RULE$respond_rule__ENA; request$say__ENA
//METAGUARD; RULE$respond_rule; ( ( running && ( b == 32'd0 ) ) != 0 ) & indication$gcd__RDY;
//METAGUARD; request$say; !running;
//METARULES; RULE$flip_rule; RULE$mod_rule; RULE$respond_rule
`endif
