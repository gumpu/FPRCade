
//
//
//
//

module SmartStack_TB #(parameter WIDTH=16, DEPTH=20) ();

    reg r_clk;
    wire [WIDTH-1:0] r_A;
    wire [WIDTH-1:0] r_B;
    reg  [WIDTH-1:0] r_D;
    reg  [2:0]       r_func;
    reg fetch;
    reg store;

    SmartStack uut(
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
        forever #5 r_clk = ~r_clk; // generate a clock
    end


    initial begin
        #100
        $finish;
    end

endmodule

