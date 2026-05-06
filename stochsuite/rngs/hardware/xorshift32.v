`ifndef __XORSHIFT_32_OPT_V__
`define __XORSHIFT_32_OPT_V__

`include "common/register.v"

module xorshift32 (
    input  wire        clk,
    input  wire        rst_n,

    input  wire [31:0] seed,     // reseed state s
    input  wire        re_seed,  // active high

    output wire [31:0] rnd
);

    // Default XorShift32 configuration from your C:
    // a = 13 (left), b = 17 (right), c = 5 (left)
    localparam [4:0] A = 5'd13;
    localparam [4:0] B = 5'd17;
    localparam [4:0] C = 5'd5;

    localparam LEFT_A = 1'b1;
    localparam LEFT_B = 1'b0;
    localparam LEFT_C = 1'b1;

    // ---- state register (32-bit) ----
    wire [31:0] s_cur;
    wire [31:0] s_wd;

    Register #(
        .NUM_BITS (32),
        .RST_VAL  (32'd0)
    ) state_reg (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data (s_wd),
        .read_data  (s_cur)
    );

    // ---- xorshift combinational next-state ----
    wire [31:0] s1, s2, s3;

    assign s1 = LEFT_A ? (s_cur ^ (s_cur << A)) : (s_cur ^ (s_cur >> A));
    assign s2 = LEFT_B ? (s1    ^ (s1    << B)) : (s1    ^ (s1    >> B));
    assign s3 = LEFT_C ? (s2    ^ (s2    << C)) : (s2    ^ (s2    >> C));

    wire [31:0] s_next;
    assign s_next = s3;

    // ---- reseed mux ----
    // C code forbids seed==0. Here we pass it through as-is; avoid reseeding with 0 in tests.
    assign s_wd = re_seed ? seed : s_next;

    // ---- output ----
    // This generator returns the updated state in software. In this RTL style, output current state,
    // consistent with your taus modules' convention.
    assign rnd = s_cur;

endmodule

`endif // __XORSHIFT_32_OPT_V__
