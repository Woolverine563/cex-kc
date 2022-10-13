module exampleUnate ( x1, x2, y1, y2, res );
input x1;
input x2;
input y1;
input y2;
output res;
wire v1;
assign v1 = (x1 | x2);
assign res = (y1 & ~y2 & v1);

endmodule