`ifndef __TAUSWORTHE_113_TESTBENCH_V__
`define __TAUSWORTHE_113_TESTBENCH_V__

`timescale 1ns/1ps

module taus113_tb;

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
    taus113 dut (
        .clk      (clk),
        .rst_n    (rst_n),
        .seed     (seed),
        .re_seed  (re_seed),
        .rnd      (rnd)
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
            $display("%t Reseeding S1 register with 0x%08h", $time, new_seed);
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

        $display("%t ns : rnd = 0x%08h | %d", $time, rnd, rnd);
        reseed_dut(seed);
        // default S1, S2, S3, S4
        repeat (10) begin : init_cycles
            @(posedge clk);
            $display("%t ns : rnd = 0x%08h | %d", $time, rnd, rnd);
        end

        // Spot check a few iterations after reseed

        reseed_dut(32'hDEAD_BEEF);
        test_value(32'd4222330416, rnd);
        test_value(32'd3091505929, rnd);
        test_value(32'd2837792084,  rnd);
        test_value(32'd222548152, rnd);
        test_value(32'd2079507190,  rnd);
        test_value(32'd586323012, rnd);
        test_value(32'd3877301905, rnd);
        test_value(32'd4006392071, rnd);
        test_value(32'd3844192471, rnd);
        test_value(32'd3234492883, rnd);

        reseed_dut(32'hCAFEBABE);
        test_value(32'd3941311136, rnd);
        test_value(32'd2852563200, rnd);
        test_value(32'd2164347728, rnd);
        test_value(32'd1431493044, rnd);
        test_value(32'd1606426732, rnd);
        test_value(32'd2828783638, rnd);
        test_value(32'd4132597587, rnd);
        test_value(32'd3922111040, rnd);
        test_value(32'd2285868209, rnd);
        test_value(32'd1901274490, rnd);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule

`endif // __TAUSWORTHE_113_TESTBENCH_V__
