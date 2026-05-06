`ifndef __JKISS32_V__
`define __JKISS32_V__

`include "common/register.v"

// JKISS32 (state = x,y,z,w,c) total 17 bytes in C++; here stored as 4*32 + 8 = 136 bits.
module jkiss32 (
    input  wire        clk,
    input  wire        rst_n,

    input  wire [31:0] seed,        // reseed X0 register
    input  wire        re_seed,     // active high

    output wire [31:0] rnd
);

    localparam integer STATE_BITWIDTH = 4*32 + 8; // 136 bits

    // Default seeds (C++ constructor)
    localparam [31:0] X0 = 32'd123456789;
    localparam [31:0] Y0 = 32'd987654321;
    localparam [31:0] Z0 = 32'd43219876;
    localparam [31:0] W0 = 32'd6543217;
    localparam [7:0]  C0 = 8'd0;

    // ---- state wires ----
    wire [31:0] x_cur, y_cur, z_cur, w_cur;
    wire [7:0]  c_cur;

    wire [31:0] x_wd,  y_wd,  z_wd,  w_wd;
    wire [7:0]  c_wd;

    // ---- full state register ----
    // Pack as {c,w,z,y,x} to match byte layout conceptually (c is extra byte).
    Register #(
        .NUM_BITS (STATE_BITWIDTH),
        .RST_VAL  ({C0, W0, Z0, Y0, X0})
    ) state_regs (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data ({c_wd, w_wd, z_wd, y_wd, x_wd}),
        .read_data  ({c_cur, w_cur, z_cur, y_cur, x_cur})
    );

    // ---- next-state logic (match C++) ----
    // y xorshift
    wire [31:0] y_t1, y_t2, y_next;
    assign y_t1   = y_cur ^ (y_cur << 5);
    assign y_t2   = y_t1  ^ (y_t1  >> 7);
    assign y_next = y_t2  ^ (y_t2  << 22);

    // t = z + w + c
    wire [32:0] t_ext;
    assign t_ext = {1'b0, z_cur} + {1'b0, w_cur} + {25'd0, c_cur}; // 33-bit to capture carry

    // z = w
    wire [31:0] z_next;
    assign z_next = w_cur;

    // c = t < 0;   (in C++ t is uint32_t, so this is always false => c becomes 0)
    // If the original intent was signed check, it would be (t[31]==1).
    // Here we implement the C++ as-written: c_next is always 0.
    wire [7:0] c_next;
    assign c_next = 8'd0;

    // w = t & 2147483647 (0x7FFFFFFF)
    wire [31:0] w_next;
    assign w_next = t_ext[31:0] & 32'h7FFF_FFFF;

    // x += 1411392427
    wire [31:0] x_next;
    assign x_next = x_cur + 32'd1411392427;

    // ---- reseed behavior ----
    // Match JKISS style used earlier: reseed sets x=seed and resets the rest to defaults.
    assign x_wd = re_seed ? seed : x_next;
    assign y_wd = re_seed ? Y0   : y_next;
    assign z_wd = re_seed ? Z0   : z_next;
    assign w_wd = re_seed ? W0   : w_next;
    assign c_wd = re_seed ? C0   : c_next;

    // ---- output (match C return: x + y + z of CURRENT state) ----
    assign rnd = x_cur + y_cur + z_cur;

endmodule

`endif // __JKISS32_V__
