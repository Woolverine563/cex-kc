// Verilog file written by procedure Aig_ManDumpVerilog()
module exampleUnate ( n01, n02, n03, n04, n05, n06, n07, n08, n09, n10, n11, n12, n13 );
input n01;
input n02;
input n03;
input n04;
input n05;
input n06;
input n07;
input n08;
input n09;
input n10;
input n11;
input n12;
output n13;
wire n14;
wire n15;
wire n16;
wire n17;
wire n18;
wire n19;
assign n14 =  n01 &  n11;
assign n15 =  n12 &  n14;
assign n16 =  n02 &  n05;
assign n17 =  n03 &  n06;
assign n18 = ~n16 & ~n17;
assign n19 =  n15 &  n18;
assign n13 =  n19;
endmodule

