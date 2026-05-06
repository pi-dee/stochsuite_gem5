`ifndef __LCG_V__
`define __LCG_V__

`include "common/register.v"

// LCG: s <- (mul*s + add) & mod
module lcg #(
    parameter [31:0] SEED0 = 32'd1634404289,

    // C++ defaults: _mul=69069, _add=1234567, _mod=0xFFFFFFFF
    // Note: mul is uint64_t in C++; keep 64-bit here.
    parameter [63:0] MUL  = 64'd69069,
    parameter [31:0] ADD  = 32'd1234567,
    parameter [31:0] MOD  = 32'hFFFF_FFFF
)(
    input  wire        clk,
    input  wire        rst_n,

    input  wire [31:0] seed,
    input  wire        re_seed,   // active high

    output wire [31:0] rnd
);

    localparam integer STATE_BITWIDTH = 32;

    wire [31:0] s_cur;
    wire [31:0] s_wd;

    // state register
    Register #(
        .NUM_BITS (STATE_BITWIDTH),
        .RST_VAL  (SEED0)
    ) state_reg (
        .clk        (clk),
        .rst_n      (rst_n),
        .write_data (s_wd),
        .read_data  (s_cur)
    );

    // next-state logic (match C++)
    wire [63:0] internal;
    assign internal = (MUL * s_cur) + {{32{1'b0}}, ADD};

    wire [31:0] s_next;
    assign s_next = internal[31:0] & MOD;

    // reseed mux
    assign s_wd = re_seed ? seed : s_next;

    // output: same as C++ return value (new state)
    assign rnd = s_cur;

endmodule

`endif // __LCG_V__
