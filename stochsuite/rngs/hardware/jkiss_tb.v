`ifndef __JKISS_TESTBENCH_V__
`define __JKISS_TESTBENCH_V__

`timescale 1ns/1ps

module jkiss_tb;

`ifdef GATESIM
    glbl glbl ();
`endif
    reg         clk;
    reg         rst_n;
    reg  [31:0] seed;
    reg         re_seed;

    wire [31:0] rnd;

    // dut
    jkiss dut (
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
        seed    = 32'h1234_5678;

        repeat (5) @(posedge clk);
        rst_n <= 1'b0;
        @(posedge clk);
        rst_n <= 1'b1;
        $display("%t ns : rnd = 0x%08h | %d", $time, rnd, rnd);

        // defualt S1, S2, S3
        repeat (10) begin : init_cycles
            @(posedge clk);
            $display("%t ns : rnd = 0x%08h | %d", $time, rnd, rnd);
        end

        // TODO eventually just read values from software version here

        // Spot check a few iterations after reseed
        reseed_dut(32'hDEAD_BEEF);
        test_value(32'd2778845915, rnd);
        test_value(32'd2959504851, rnd);
        test_value(32'd54303350, rnd);
        test_value(32'd4063145910, rnd);
        test_value(32'd255296776, rnd);
        test_value(32'd2951042692, rnd);
        test_value(32'd2699319170, rnd);
        test_value(32'd3808456000, rnd);
        test_value(32'd1453898306, rnd);
        test_value(32'd1621721552, rnd);

        reseed_dut(32'hCAFEBABE);
        test_value(32'd3310253806, rnd);
        test_value(32'd3676668698, rnd);
        test_value(32'd2091206177, rnd);
        test_value(32'd176484917, rnd);
        test_value(32'd1893641483, rnd);
        test_value(32'd2873217019, rnd);
        test_value(32'd1086969501, rnd);
        test_value(32'd1760357743, rnd);
        test_value(32'd2487419189, rnd);
        test_value(32'd2960594551, rnd);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule
`endif // __JKISS_TESTBENCH_V__
