`ifndef __XORWOW_V__
`define __XORWOW_V__

`include "common/register.v"

module xorwow (
    input  wire        clk,
    input  wire        rst_n,

    input  wire [31:0] seed,     // reseed only x
    input  wire        re_seed,  // active high

    output wire [31:0] rnd
);

    localparam integer STATE_BITWIDTH = 6 * 32; // 192 bits

    // Default seeds (C++ constructor)
    localparam [31:0] X0 = 32'd123456789;
    localparam [31:0] Y0 = 32'd362436069;
    localparam [31:0] Z0 = 32'd521288629;
    localparam [31:0] W0 = 32'd88675123;
    localparam [31:0] V0 = 32'd5783321;
    localparam [31:0] D0 = 32'd6615241;

    // ---- state wires ----
    wire [31:0] x_cur, y_cur, z_cur, w_cur, v_cur, d_cur;
    wire [31:0] x_wd,  y_wd,  z_wd,  w_wd,  v_wd,  d_wd;

    // ---- full state register ----
    Register #(
        .NUM_BITS (STATE_BITWIDTH),
        .RST_VAL  ({D0, V0, W0, Z0, Y0, X0})
    ) state_regs (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data ({d_wd, v_wd, w_wd, z_wd, y_wd, x_wd}),
        .read_data  ({d_cur, v_cur, w_cur, z_cur, y_cur, x_cur})
    );

    // ---- xorwow next-state logic (match C exactly) ----
    wire [31:0] t;
    assign t = x_cur ^ (x_cur >> 2);

    wire [31:0] x_next, y_next, z_next, w_next, v_next, d_next;
    assign x_next = y_cur;
    assign y_next = z_cur;
    assign z_next = w_cur;
    assign w_next = v_cur;
    assign v_next = (v_cur ^ (v_cur << 4)) ^ (t ^ (t << 1));
    assign d_next = d_cur + 32'd362437;

    // ---- reseed behavior  ----
    // reseed sets full state to defaults, but x gets overridden by seed
    assign x_wd = re_seed ? seed : x_next;
    assign y_wd = re_seed ? Y0   : y_next;
    assign z_wd = re_seed ? Z0   : z_next;
    assign w_wd = re_seed ? W0   : w_next;
    assign v_wd = re_seed ? V0   : v_next;
    assign d_wd = re_seed ? D0   : d_next;

    // ---- output  ----
    assign rnd = d_cur + v_cur;

endmodule

`endif // __XORWOW_V__
