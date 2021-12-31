// Generated using findDep.cpp 
module example (v_1, v_2, v_5, v_3, v_4, o_1);
input v_1;
input v_2;
input v_5;
input v_3;
input v_4;
output o_1;
wire x_1;
wire x_2;
wire x_3;
wire x_4;
wire x_5;
wire x_6;
wire x_7;
assign x_1 = v_1 | v_3 | ~v_5;
assign x_2 = ~v_3 | ~v_4;
assign x_3 = v_4 | ~v_1 | v_2 | v_5;
assign x_4 = v_2 | ~v_3 | v_1;
assign x_5 = x_1 & x_2;
assign x_6 = x_3 & x_4;
assign x_7 = x_5 & x_6;
assign o_1 = x_7;
endmodule
