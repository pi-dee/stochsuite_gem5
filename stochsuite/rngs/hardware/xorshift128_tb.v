`ifndef __XORSHIFT_128_TESTBENCH_V__
`define __XORSHIFT_128_TESTBENCH_V__

`timescale 1ns/1ps

module xorshift128_tb;

    // only for gatesim
    `ifdef GATESIM
        glbl glbl ();
    `endif

    reg         clk;
    reg         rst_n;
    reg  [31:0] seed;
    reg         re_seed;

    wire [31:0] rnd;

    // dut
    xorshift128 dut (
        .clk     (clk),
        .rst_n   (rst_n),
        .seed    (seed),
        .re_seed (re_seed),
        .rnd     (rnd)
    );

    // clock
    initial begin
        clk = 1'b0;
    end
    always #5 clk = ~clk;

    task test_value;
        input integer expected;
        input [31:0] produced;
        begin
            if (produced != expected) begin
                $fatal(1, "Expected rnd %d after reseed, got %d", expected, produced);
            end
            @(posedge clk);
        end
    endtask

    task reseed_dut;
        input [31:0] new_seed;
        begin
            seed    <= new_seed;
            re_seed <= 1'b1;
            $display("%t Reseeding state register with 0x%08h", $time, new_seed);
            @(posedge clk);
            re_seed <= 1'b0;
            @(posedge clk);
            @(posedge clk);
        end
    endtask

    initial begin
        rst_n   = 1'b1;
        re_seed = 1'b0;
        seed    = 32'h1234_5678;

        repeat (5) @(posedge clk);
        rst_n <= 1'b0;
        @(posedge clk);
        rst_n <= 1'b1;
        $display("%t ns : rnd = 0x%08h | %0d", $time, rnd, rnd);
        reseed_dut(seed);

        // After reset, xorshift32_opt state resets to 0 by design (RST_VAL = 0)
        // So we always reseed before checking.
        repeat (5) begin : init_cycles
            @(posedge clk);
            $display("%t ns : rnd = 0x%08h | %0d", $time, rnd, rnd);
        end


        reseed_dut(32'hDEAD_BEEF);
        test_value(32'd3063975859, rnd);
        test_value(32'd1901253119, rnd);
        test_value(32'd4279469043, rnd);
        test_value(32'd2995701673, rnd);
        test_value(32'd14752604, rnd);
        test_value(32'd3889950614, rnd);
        test_value(32'd2226447493, rnd);
        test_value(32'd1086572597,  rnd);
        test_value(32'd1223688330, rnd);
        test_value(32'd1886384477, rnd);

        reseed_dut(32'hCAFEBABE);
        test_value(32'd979202670, rnd);
        test_value(32'd4256120253, rnd);
        test_value(32'd1945009710,  rnd);
        test_value(32'd1047735275, rnd);
        test_value(32'd4016889929, rnd);
        test_value(32'd1806330572, rnd);
        test_value(32'd1807054489, rnd);
        test_value(32'd3426624631, rnd);
        test_value(32'd1155070737,  rnd);
        test_value(32'd2082211148, rnd);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule

`endif // __XORSHIFT_32_TESTBENCH_V__
