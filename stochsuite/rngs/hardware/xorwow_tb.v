`ifndef __XOR_WOW_TESTBENCH_V__
`define __XOR_WOW_TESTBENCH_V__

`timescale 1ns/1ps

module xorwow_tb;

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
    xorwow dut (
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

        // After reset, xorshiftwow_opt state resets to 0 by design (RST_VAL = 0)
        // So we always reseed before checking.
        repeat (5) begin : init_cycles
            @(posedge clk);
            $display("%t ns : rnd = 0x%08h | %0d", $time, rnd, rnd);
        end


        reseed_dut(32'hDEAD_BEEF);
        test_value(32'd1060845059, rnd);
        test_value(32'd3813551060, rnd);
        test_value(32'd3871134865, rnd);
        test_value(32'd2392425413, rnd);
        test_value(32'd1873523627, rnd);
        test_value(32'd3468528200, rnd);
        test_value(32'd1149685374,  rnd);
        test_value(32'd482616012, rnd);
        test_value(32'd709801491, rnd);
        test_value(32'd814130699, rnd);

        reseed_dut(32'hCAFEBABE);
        test_value(32'd226716488, rnd);
        test_value(32'd3993147665, rnd);
        test_value(32'd506877134,  rnd);
        test_value(32'd3462205364, rnd);
        test_value(32'd330099560, rnd);
        test_value(32'd865099009, rnd);
        test_value(32'd2034192053, rnd);
        test_value(32'd3706326693, rnd);
        test_value(32'd454449992,  rnd);
        test_value(32'd2252890982, rnd);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule

`endif // __XOR_WOW_TESTBENCH_V__
