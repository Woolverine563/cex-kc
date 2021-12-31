// Verilog file written by procedure writeCombinationalCircuitInVerilog
//Skolem functions to be generated for i_ variables
module factorization4 ( i1[1], i1[0], i2[1], i2[0], o[3], o[2], o[1], o[0], res );
input i1[1];
input i1[0];
input i2[1];
input i2[0];
input o[3];
input o[2];
input o[1];
input o[0];
output res;
wire x_1;
wire x_2;
wire x_3;
wire x_4;
wire x_5;
wire x_6;
wire x_7;
wire x_8;
assign x_1 = i1[0] & i2[0];
assign x_2 = i1[1] & i2[0];
assign x_3 = i1[0] & i2[1];
assign x_4 = (x_2 & ~x_3) | (~x_2 & x_3);
assign x_5 = i1[1] & i2[1] & x_2 & x_3;
assign x_6 = (i1[1] & i2[1]);
assign x_7 = (x_2 & x_3);
assign x_8 = (x_6 & ~x_7) | (~x_6 & x_7);
assign res = (x_1 | ~o[0]) & (~x_1 | o[0]) & (x_4 | ~o[1]) & (~x_4 | o[1]) & (x_5 | ~o[2]) & (~x_5 | o[2]) & (x_8 | ~o[3]) & (~x_8 | o[3]);



endmodule

