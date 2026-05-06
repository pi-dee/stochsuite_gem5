`ifndef __JKISS32_TESTBENCH_V__
`define __JKISS32_TESTBENCH_V__

`timescale 1ns/1ps

module jkiss32_tb;

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
    jkiss32 dut (
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
        test_value(32'd2919436919, rnd);
        test_value(32'd4217436833, rnd);
        test_value(32'd3525110570, rnd);
        test_value(32'd1598787050, rnd);
        test_value(32'd3836725891, rnd);
        test_value(32'd3056915558, rnd);
        test_value(32'd1818602301, rnd);
        test_value(32'd3051779868,  rnd);
        test_value(32'd720436133, rnd);
        test_value(32'd1196447762, rnd);

        reseed_dut(32'hCAFEBABE);
        test_value(32'd2589199942, rnd);
        test_value(32'd3887199856, rnd);
        test_value(32'd3194873593,  rnd);
        test_value(32'd1268550073, rnd);
        test_value(32'd3506488914, rnd);
        test_value(32'd2726678581, rnd);
        test_value(32'd1488365324, rnd);
        test_value(32'd2721542891, rnd);
        test_value(32'd390199156, rnd);
        test_value(32'd866210785,  rnd);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule

`endif // __JKISS32_TESTBENCH_V__
