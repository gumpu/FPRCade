//
// Function
// 000   push   (X X X -> X X X D)
// 001   d1push (X X A -> X X D)
// 010   d2push (X B A -> X D)
// 011   swap   (X B A -> X A B)
// 100   drop   (X X A -> X X)
// 101   drop2  (X B A -> X)
// 110   roll   (C B A -> A C B)
// 111   peek   (C B A -> C B A)
//

module SmartStack #(parameter WIDTH=16, DEPTH=8)
(
    input                   i_rst,
    input                   i_clk,
    input                   i_fetch,
    input                   i_store,
    input [2:0]             i_function,
    input [WIDTH-1:0]       i_write_D,
    output reg [WIDTH-1:0]  o_read_A,
    output reg [WIDTH-1:0]  o_read_B
    // TODO overflow and underflow
);
    // Local variables
    // The stack
    reg [WIDTH-1:0] r_stack[DEPTH-1:0];
    // Pointer first free cell at the
    // top of the stack
    reg [$clog2(DEPTH)-1:0] r_stack_top;
    // Temporary values
    reg [WIDTH-1:0] r_temp_A;
    reg [WIDTH-1:0] r_temp_B;
    reg [WIDTH-1:0] r_temp_C;

    // Function enumeration
    localparam PUSH   = 3'b000;
    localparam D1PUSH = 3'b001;
    localparam D2PUSH = 3'b010;
    localparam SWAP   = 3'b011;
    localparam DROP   = 3'b100;
    localparam DROP2  = 3'b101;
    localparam ROLL   = 3'b110;
    localparam PEEK   = 3'b111;

    always @ (posedge i_rst)
    begin
        r_stack_top <= 0;
        r_temp_A <= 16'h0000;
        r_temp_B <= 16'h0000;
        r_temp_C <= 16'h0000;
    end

    always @ (posedge i_clk)
    begin
        if (i_fetch)
        begin
            if (r_stack_top == 0)
            begin
                // There are no values on the stack
                r_temp_A <= 16'hDEAD;
                r_temp_B <= 16'hDEAD;
                r_temp_C <= 16'hDEAD;
            end else
            begin
                if (r_stack_top == 1)
                begin
                    // There is one value on the stack
                    r_temp_A <= r_stack[r_stack_top-1];
                    r_temp_B <= 16'hDEAD;
                    r_temp_C <= 16'hDEAD;
                end else
                begin
                    if (r_stack_top == 2)
                    begin
                        // There are two values on the stack
                        r_temp_A <= r_stack[r_stack_top-1];
                        r_temp_B <= r_stack[r_stack_top-2];
                        r_temp_C <= 16'hDEAD;
                    end else
                    begin
                        // There are two or more values on the stack
                        r_temp_A <= r_stack[r_stack_top-1];
                        r_temp_B <= r_stack[r_stack_top-2];
                        r_temp_C <= r_stack[r_stack_top-3];
                   end
               end
           end
        end

        if(i_store) begin
            case(i_function)
                PUSH   : begin
                    r_stack[r_stack_top] <= i_write_D;
                    r_stack_top <= r_stack_top + 1;
                    // TODO overflow
                end
                D1PUSH : begin
                    r_stack[r_stack_top-1] <= i_write_D;
                    // TODO underflow
                end
                D2PUSH : begin
                    r_stack[r_stack_top-2] <= i_write_D;
                    r_stack_top <= r_stack_top - 1;
                    // TODO underflow
                end
                SWAP   : begin
                    r_stack[r_stack_top-1] <= r_temp_B;
                    r_stack[r_stack_top-2] <= r_temp_A;
                    // TODO underflow
                end
                DROP   : begin
                    r_stack_top <= r_stack_top - 1;
                    // TODO underflow
                end
                DROP2  : begin
                    r_stack_top <= r_stack_top - 2;
                    // TODO underflow
                end
                ROLL   : begin
                    r_stack[r_stack_top-1] <= r_temp_C;
                    r_stack[r_stack_top-3] <= r_temp_A;
                    // TODO underflow
                end
                PEEK   : begin
                    o_read_A <= r_temp_A;
                    o_read_B <= r_temp_B;
                end
                default: begin
                    // code for any other case
                end
            endcase
        end
    end

endmodule

// ------------------------ end of file -------------------------------
