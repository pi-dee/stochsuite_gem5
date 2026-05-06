`ifndef __TAUSWORTHE_113_V__
`define __TAUSWORTHE_113_V__

`include "common/register.v"

module taus113 (
    input  wire        clk,
    input  wire        rst_n,

    input  wire [31:0] seed,     // reseed only S1
    input  wire        re_seed,  // active high

    output wire [31:0] rnd       // random number output
);

    // ---- constants ----
    localparam state_bitwidth = 4 * 4 * 8; // 4 * 32-bit = 128
    localparam [31:0] C1 = 32'hFFFFFFFE;
    localparam [31:0] C2 = 32'hFFFFFFF8;
    localparam [31:0] C3 = 32'hFFFFFFF0;
    localparam [31:0] C4 = 32'hFFFFFF80;

    // ---- state wires ----
    wire [31:0] S1_cur, S2_cur, S3_cur, S4_cur;
    wire [31:0] S1_wd,  S2_wd,  S3_wd,  S4_wd;

    // ---- full state register ----
    Register
    #(
        .NUM_BITS (state_bitwidth),
        .RST_VAL  ({(state_bitwidth){1'b0}})
    ) state_regs (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data ({S4_wd, S3_wd, S2_wd, S1_wd}),
        .read_data  ({S4_cur, S3_cur, S2_cur, S1_cur})
    );

    // ---- taus113 next-state logic (direct translation of your C) ----
    wire [31:0] S1_x, S2_x, S3_x, S4_x;
    assign S1_x = (S1_cur <<  6) ^ S1_cur;
    assign S2_x = (S2_cur <<  2) ^ S2_cur;
    assign S3_x = (S3_cur << 13) ^ S3_cur;
    assign S4_x = (S4_cur <<  3) ^ S4_cur;

    wire [31:0] S1_next, S2_next, S3_next, S4_next;
    assign S1_next = ((S1_cur & C1) << 18) ^ (S1_x >> 13);
    assign S2_next = ((S2_cur & C2) <<  2) ^ (S2_x >> 27);
    assign S3_next = ((S3_cur & C3) <<  7) ^ (S3_x >> 21);
    assign S4_next = ((S4_cur & C4) << 13) ^ (S4_x >> 12);

    // ---- reseed mux (matches your C defaults) ----
    assign S1_wd = re_seed ? seed     : S1_next;
    assign S2_wd = re_seed ? 32'd8    : S2_next;
    assign S3_wd = re_seed ? 32'd16   : S3_next;
    assign S4_wd = re_seed ? 32'd128  : S4_next;

    // ---- output ----
    // Keep consistent with your taus88 style: XOR current state
    assign rnd = S1_cur ^ S2_cur ^ S3_cur ^ S4_cur;

endmodule

`endif // __TAUSWORTHE_113_OPT_V__
