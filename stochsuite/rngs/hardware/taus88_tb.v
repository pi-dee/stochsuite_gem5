`ifndef __TAUSWORTHE_88_TESTBENCH_V__
`define __TAUSWORTHE_88_TESTBENCH_V__

`timescale 1ns/1ps

module taus88_tb;

`ifdef GATESIM
    glbl glbl ();
`endif

    reg         clk;
    reg         rst_n;
    reg  [31:0] seed;
    reg         re_seed;

    wire [31:0] rnd;

    // dut
    taus88 dut (
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
            @(posedge clk); // Wait for a positive clock edge
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

        repeat (5) @(posedge clk);
        rst_n <= 1'b0;
        @(posedge clk);
        rst_n <= 1'b1;
        $display("%t ns : rnd = 0x%08h | %d", $time, rnd, rnd);

        reseed_dut(32'h1234_5678);
        // defualt S1, S2, S3
        repeat (10) begin : init_cycles
            @(posedge clk);
            $display("%t ns : rnd = 0x%08h | %d", $time, rnd, rnd);
        end

        // TODO eventually just read values from software version here

        // Spot check a few iterations after reseed
        reseed_dut(32'hDEAD_BEEF);
        test_value(32'd3687771566, rnd);
        test_value(32'd4006792393, rnd);
        test_value(32'd1712068217, rnd);
        test_value(32'd3375142808, rnd);
        test_value(32'd1509541458, rnd);
        test_value(32'd45559123, rnd);
        test_value(32'd4065404862, rnd);
        test_value(32'd49953709, rnd);
        test_value(32'd2327600246, rnd);
        test_value(32'd3851033654, rnd);

        reseed_dut(32'hCAFEBABE);
        test_value(32'd3951813429, rnd);
        test_value(32'd3191570171, rnd);
        test_value(32'd4247730860, rnd);
        test_value(32'd4225878095, rnd);
        test_value(32'd2349118836, rnd);
        test_value(32'd3582280224, rnd);
        test_value(32'd3567654117, rnd);
        test_value(32'd1909425086, rnd);
        test_value(32'd3510461680, rnd);
        test_value(32'd4127355812, rnd);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule
`endif // __TAUSWORTHE_88_TESTBENCH_V__
