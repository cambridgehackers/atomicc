`ifndef __order_GENERATED__VH__
`define __order_GENERATED__VH__

//METASTART; Order
//METAEXCLUSIVE; RULE$A__ENA; RULE$B__ENA; request$say__ENA
//METABEFORE; RULE$A__ENA; :RULE$B__ENA; :request$say__ENA
//METAGUARD; RULE$A; request$say__ENA == 0;
//METAEXCLUSIVE; RULE$B__ENA; request$say__ENA
//METAGUARD; RULE$B; request$say__ENA == 0;
//METAEXCLUSIVE; RULE$C__ENA; request$say__ENA
//METAGUARD; RULE$C; request$say__ENA == 0;
//METAGUARD; request$say; !running;
//METARULES; RULE$A; RULE$B; RULE$C
`endif
