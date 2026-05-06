// jkiss.v
`ifndef __JKISS_V__
`define __JKISS_V__

`include "common/register.v"

module jkiss (
    input  wire        clk,
    input  wire        rst_n,

    input  wire [31:0] seed,        // reseed x0 register
    input  wire        re_seed,      // active high

    output wire [31:0] rnd
);

    localparam integer STATE_BITWIDTH = 4 * 32; // 128 bits

    // Default seeds (C++ constructor)
    localparam [31:0] X0 = 32'd123456789;
    localparam [31:0] Y0 = 32'd987654321;
    localparam [31:0] Z0 = 32'd43219876;
    localparam [31:0] C0 = 32'd6543217;

    // ---- state wires ----
    wire [31:0] x_cur, y_cur, z_cur, c_cur;
    wire [31:0] x_wd,  y_wd,  z_wd,  c_wd;

    // ---- full state register ----
    Register #(
        .NUM_BITS (STATE_BITWIDTH),
        .RST_VAL  ({C0, Z0, Y0, X0})
    ) state_regs (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data ({c_wd, z_wd, y_wd, x_wd}),
        .read_data  ({c_cur, z_cur, y_cur, x_cur})
    );

    // ---- JKISS next-state logic (match C++) ----
    // x = 314527869 * x + 1234567;
    wire [31:0] x_next;
    assign x_next = (32'd314527869 * x_cur) + 32'd1234567;

    // y ^= y << 5; y ^= y >> 7; y ^= y << 22;
    wire [31:0] y_t1, y_t2, y_next;
    assign y_t1  = y_cur ^ (y_cur << 5);
    assign y_t2  = y_t1  ^ (y_t1  >> 7);
    assign y_next = y_t2 ^ (y_t2  << 22);

    // t = 4294584393*z + c; c = t >> 32; z = t;
    wire [63:0] t;
    assign t = (64'd4294584393 * {32'd0, z_cur}) + {32'd0, c_cur};

    wire [31:0] z_next, c_next;
    assign z_next = t[31:0];
    assign c_next = t[63:32];

    // ---- reseed behavior (match correctness.o -seed) ----
    // When re_seed=1: x=seed, y/z/c reset to defaults.
    assign x_wd = re_seed ? seed : x_next;
    assign y_wd = re_seed ? Y0   : y_next;
    assign z_wd = re_seed ? Z0   : z_next;
    assign c_wd = re_seed ? C0   : c_next;

    // ---- output (match C return: x + y + z of CURRENT state) ----
    assign rnd = x_cur + y_cur + z_cur;

endmodule

`endif // __JKISS_V__
