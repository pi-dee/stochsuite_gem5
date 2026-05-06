`ifndef __TAUSWORTHE_OPTIMIZED_88_V__
`define __TAUSWORTHE_OPTIMIZED_88_V__

`include "common/register.v"

module taus88_opt (
    input wire        clk,         // clock
    input wire        rst_n,       // active-low synchronous reset

    input wire [31:0] seed,        // seed to reset state register S1
    input wire        re_seed,     // active high to reset S1 with seed (S2 & S3 unchanged)

    output wire [31:0] rnd         // random number output
);

    // Taus88 constants
    localparam state_bitwidth = 3 * 4 * 8;
    localparam [31:0] C1 = 32'hFFFFFFFE;
    localparam [31:0] C2 = 32'hFFFFFFF8;
    localparam [31:0] C3 = 32'hFFFFFFF0;

    wire [31:0] S1_cur, S2_cur, S3_cur;             // current S1, S2, S3 value from RNGState
    wire [31:0] S1_wd, S2_wd, S3_wd;                // S1, S2, S3 value to write into RNGState

    // ---------- Full State Register --------------------
    Register
    #(
        .NUM_BITS (state_bitwidth), // 3 32 byte registers
        .RST_VAL  ({(state_bitwidth){1'b0}})
    ) state_regs (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data ({S3_wd, S2_wd, S1_wd}),
        .read_data  ({S3_cur, S2_cur, S1_cur})
    );
    // ---------------------------------------------------

    // Taus88 next random number algorithm
    wire [31:0] S1_next, S2_next, S3_next;
    assign S1_next = {S1_cur[19:1],  S1_cur[18:6] ^ S1_cur[31:19]};
    assign S2_next = {S2_cur[27:3],  S2_cur[29:23] ^ S2_cur[31:25]};
    assign S3_next = {S3_cur[31:21], S3_cur[28:8] ^  S3_cur[31:1]};

    // Write data mux
    assign S1_wd = re_seed ? seed : S1_next;
    assign S2_wd = re_seed ? 32'd8 :S2_next;
    assign S3_wd = re_seed ? 32'd16 :S3_next;

    // Outputs
    assign rnd       = S1_cur ^ S2_cur ^ S3_cur;

endmodule

`endif // __TAUSWORTHE_OPTIMIZED_88_V__
