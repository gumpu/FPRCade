//
//
//

module SmartStack #(parameter WIDTH=16, DEPTH=20)
(
    input                   i_clk,
    input                   i_fetch,
    input                   i_store,
    input [2:0]             i_function,
    input [WIDTH-1:0]       i_write_D,
    output reg [WIDTH-1:0]  o_read_A,
    output reg [WIDTH-1:0]  o_read_B
    // TODO overflow and underflow
);

    reg [WIDTH-1:0] r_stack[DEPTH-1:0];
    reg [$clog2(DEPTH)-1:0] r_stack_top;

    always @ (posedge i_clk)
    begin
        o_read_A <= 16'b0;
        o_read_B <= 16'b0;
    end

endmodule

