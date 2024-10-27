//
//
// Test bench for the Smart Stack
//
//

module SmartStack_TB #(parameter WIDTH=16, DEPTH=8) ();

    reg r_rst;
    reg r_clk;
    wire [WIDTH-1:0] w_A;
    wire [WIDTH-1:0] w_B;
    reg  [WIDTH-1:0] r_D;
    reg  [2:0]       r_func;
    reg r_fetch;
    reg r_store;

    SmartStack uut(
        .i_rst (r_rst),
        .i_clk (r_clk),
        .i_fetch (r_fetch),
        .i_store (r_store),
        .i_function (r_func),
        .i_write_D (r_D),
        .o_read_A (w_A),
        .o_read_B (w_B)
    );


    initial begin
        $dumpfile("smartstack_tb.vcd");
        // Specify what variables to trace, 0 == all of them,
        // and from which module
        $dumpvars(0, SmartStack_TB);

        r_clk = 1'b0;
        forever #10 r_clk = ~r_clk; // generate a clock
    end


    initial begin
        r_D = 16'b11110000;
        r_rst = 1'b0;
        #3
        r_rst = 1'b1;
        #3
        r_rst = 1'b0;
        #7
        r_func = 3'b000;
        r_fetch = 1'b1;
        r_store = 1'b0;
        #17
        r_func = 3'b000;
        r_fetch = 1'b0;
        r_store = 1'b1;
        #27
        r_func = 3'b111;
        r_fetch = 1'b1;
        r_store = 1'b0;
        #37
        r_func = 3'b111;
        r_fetch = 1'b0;
        r_store = 1'b1;
        #100
        $finish;
    end

endmodule

