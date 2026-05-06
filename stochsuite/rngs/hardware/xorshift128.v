`ifndef __XORSHIFT_128_V__
`define __XORSHIFT_128_V__

`include "common/register.v"

module xorshift128 (
    input  wire        clk,
    input  wire        rst_n,

    input  wire [31:0] seed,     // reseed only x
    input  wire        re_seed,  // active high

    output wire [31:0] rnd
);

    localparam integer STATE_BITWIDTH = 4 * 4 * 8; // 128 bits

    // Default seeds (C++ constructor)
    localparam [31:0] X0 = 32'd123456789;
    localparam [31:0] Y0 = 32'd362436069;
    localparam [31:0] Z0 = 32'd521288629;
    localparam [31:0] W0 = 32'd88675123;

    // ---- state wires ----
    wire [31:0] x_cur, y_cur, z_cur, w_cur;
    wire [31:0] x_wd,  y_wd,  z_wd,  w_wd;

    // ---- full state register ----
    Register #(
        .NUM_BITS (STATE_BITWIDTH),
        .RST_VAL  ({W0, Z0, Y0, X0})
    ) state_regs (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data ({w_wd, z_wd, y_wd, x_wd}),
        .read_data  ({w_cur, z_cur, y_cur, x_cur})
    );

    // ---- xorshift128 next-state logic ----
    wire [31:0] t0, t1;
    assign t0 = x_cur ^ (x_cur << 11);
    assign t1 = t0 ^ (t0 >> 8);

    wire [31:0] x_next, y_next, z_next, w_next;
    assign x_next = y_cur;
    assign y_next = z_cur;
    assign z_next = w_cur;
    assign w_next = t1 ^ w_cur ^ (w_cur >> 19);

    // ---- reseed mux ----
    assign x_wd = re_seed ? seed : x_next;
    assign y_wd = re_seed ? Y0   : y_next;
    assign z_wd = re_seed ? Z0   : z_next;
    assign w_wd = re_seed ? W0   : w_next;

    // ---- output ----
    assign rnd = w_cur;

endmodule

`endif // __XORSHIFT_128_V__
