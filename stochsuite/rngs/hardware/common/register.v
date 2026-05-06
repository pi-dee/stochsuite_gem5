`ifndef __REGISTER_V__
`define __REGISTER_V__

module Register
#(
    parameter NUM_BITS = 8,
    parameter RST_VAL = {NUM_BITS{1'b0}}  // default init value
)
(
    input  wire                  clk,
    input  wire                  rst_n,
    input  wire [NUM_BITS-1:0]   write_data,

    output wire [NUM_BITS-1:0]   read_data
);
    reg [NUM_BITS-1:0] mem;

    genvar i;
    generate
        for (i = 0; i < NUM_BITS; i = i + 1) begin : gen_bits
            always @(posedge clk) begin
                if (!rst_n) begin
                    mem[i] <= RST_VAL[i];
                end else begin
                    mem[i] <= write_data[i];
                end
            end
        end
    endgenerate

    assign read_data = mem;

endmodule

`endif // __REGISTER_V__
