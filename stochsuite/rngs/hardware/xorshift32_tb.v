`ifndef __XORSHIFT_32_TESTBENCH_V__
`define __XORSHIFT_32_TESTBENCH_V__

`timescale 1ns/1ps

module xorshift32_tb;

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
    xorshift32 dut (
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
        test_value(32'd1199382711, rnd);
        test_value(32'd2384302402, rnd);
        test_value(32'd3129746520, rnd);
        test_value(32'd4276113467, rnd);
        test_value(32'd1745748808, rnd);
        test_value(32'd2760751131, rnd);
        test_value(32'd1649732188, rnd);
        test_value(32'd486387635,  rnd);
        test_value(32'd2289630710, rnd);
        test_value(32'd1862841525, rnd);

        reseed_dut(32'hCAFEBABE);
        test_value(32'd2827483434, rnd);
        test_value(32'd2750467483, rnd);
        test_value(32'd4064143354,  rnd);
        test_value(32'd2526188539, rnd);
        test_value(32'd1499439149, rnd);
        test_value(32'd101746304, rnd);
        test_value(32'd3469816288, rnd);
        test_value(32'd4115003222, rnd);
        test_value(32'd1045250721,  rnd);
        test_value(32'd1430002701, rnd);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule

`endif // __XORSHIFT_32_TESTBENCH_V__
