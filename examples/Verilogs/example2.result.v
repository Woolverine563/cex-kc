// Verilog file written by procedure Aig_ManDumpVerilog()
module test ( n01, n02, n03, n04, n05, n06, n07, n08, n09 );
input n01;
input n02;
input n03;
input n04;
input n05;
input n06;
input n07;
input n08;
output n09;
wire n10;
wire n11;
wire n12;
assign n10 = ~n01 & ~n02;
assign n11 =  n03 &  n08;
assign n12 = ~n10 &  n11;
assign n09 =  n12;
endmodule

