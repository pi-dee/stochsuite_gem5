`ifndef __LCG_TESTBENCH_V__
`define __LCG_TESTBENCH_V__

`timescale 1ns/1ps

module lcg_tb;

    // only for gatesim
    `ifdef GATESIM
        glbl glbl ();
    `endif

    reg         clk;
    reg         rst_n;
    reg  [31:0] seed;
    reg         re_seed;

    wire [31:0] rnd_cong;
    wire [31:0] rnd_glibc;
    wire [31:0] rnd_drand48;

    // ----------------------------
    // DUTs
    // ----------------------------

    // CONG
    lcg #(
        .SEED0(32'd123132),
        .MUL  (64'd69069),
        .ADD  (32'd1234567),
        .MOD  (32'hFFFF_FFFF)
    ) dut_cong (
        .clk     (clk),
        .rst_n   (rst_n),
        .seed    (seed),
        .re_seed (re_seed),
        .rnd     (rnd_cong)
    );

    // GLIBC_CRAND
    lcg #(
        .SEED0(32'd123132),
        .MUL  (64'd1103515245),
        .ADD  (32'd12345),
        .MOD  (32'h7FFF_FFFF)
    ) dut_glibc (
        .clk     (clk),
        .rst_n   (rst_n),
        .seed    (seed),
        .re_seed (re_seed),
        .rnd     (rnd_glibc)
    );

    // DRAND48 variant in your C++ code
    lcg #(
        .SEED0(32'd1),
        .MUL  (64'h0000_0005_DEEC_E66D),
        .ADD  (32'h0000_000B),
        .MOD  (32'hFFFF_FFFF)
    ) dut_drand48 (
        .clk     (clk),
        .rst_n   (rst_n),
        .seed    (seed),
        .re_seed (re_seed),
        .rnd     (rnd_drand48)
    );

    // ----------------------------
    // clock
    // ----------------------------
    initial begin
        clk = 1'b0;
    end
    always #5 clk = ~clk;

    // ----------------------------
    // tasks
    // ----------------------------
    task test_value;
        input [255:0] rng_name;
        input integer expected;
        input [31:0] produced;
        begin
            if (produced != expected) begin
                $fatal(1, "%0s: expected rnd %0d, got %0d", rng_name, expected, produced);
            end
            @(posedge clk);
        end
    endtask

    task reseed_all;
        input [31:0] new_seed;
        begin
            seed    <= new_seed;
            re_seed <= 1'b1;
            $display("%t Reseeding all LCG DUTs with 0x%08h", $time, new_seed);
            @(posedge clk);
            re_seed <= 1'b0;
            @(posedge clk);
            @(posedge clk);
        end
    endtask

    task show_all_outputs;
        begin
            $display("%t ns : CONG     rnd = 0x%08h | %0d", $time, rnd_cong, rnd_cong);
            $display("%t ns : GLIBC    rnd = 0x%08h | %0d", $time, rnd_glibc, rnd_glibc);
            $display("%t ns : DRAND48  rnd = 0x%08h | %0d", $time, rnd_drand48, rnd_drand48);
        end
    endtask

    // ----------------------------
    // test sequence
    // ----------------------------
    initial begin
        rst_n   = 1'b1;
        re_seed = 1'b0;
        seed    = 32'h1234_5678;

        repeat (5) @(posedge clk);
        rst_n <= 1'b0;
        @(posedge clk);
        rst_n <= 1'b1;

        show_all_outputs();
        reseed_all(seed);

        repeat (5) begin : init_cycles
            @(posedge clk);
            show_all_outputs();
        end

        // =========================================================
        // Seed = 0xDEAD_BEEF
        // =========================================================
        reseed_all(32'hDEAD_BEEF);

        // CONG placeholders
        test_value("CONG",    32'd3805667050, rnd_cong);
        test_value("CONG",    32'd1620195817, rnd_cong);
        test_value("CONG",    32'd4228188956, rnd_cong);
        test_value("CONG",    32'd482945011, rnd_cong);
        test_value("CONG",    32'd1814178590, rnd_cong);
        test_value("CONG",    32'd2126373773, rnd_cong);
        test_value("CONG",    32'd104675184, rnd_cong);
        test_value("CONG",    32'd1381559095, rnd_cong);
        test_value("CONG",    32'd1617951890, rnd_cong);
        test_value("CONG",    32'd3861217649, rnd_cong);

        reseed_all(32'hDEAD_BEEF);

        // GLIBC_CRAND placeholders
        test_value("GLIBC",   32'd469847548, rnd_glibc);
        test_value("GLIBC",   32'd523840645, rnd_glibc);
        test_value("GLIBC",   32'd1791404762, rnd_glibc);
        test_value("GLIBC",   32'd2101435147, rnd_glibc);
        test_value("GLIBC",   32'd1947422184, rnd_glibc);
        test_value("GLIBC",   32'd2003110913, rnd_glibc);
        test_value("GLIBC",   32'd695507622, rnd_glibc);
        test_value("GLIBC",   32'd1881709799, rnd_glibc);
        test_value("GLIBC",   32'd583229588, rnd_glibc);
        test_value("GLIBC",   32'd454387517, rnd_glibc);

        reseed_all(32'hDEAD_BEEF);

        // // DRAND48 placeholders
        test_value("DRAND48", 32'd802751950, rnd_drand48);
        test_value("DRAND48", 32'd1485212865, rnd_drand48);
        test_value("DRAND48", 32'd3014349880, rnd_drand48);
        test_value("DRAND48", 32'd2705140707, rnd_drand48);
        test_value("DRAND48", 32'd3955073458, rnd_drand48);
        test_value("DRAND48", 32'd723240149, rnd_drand48);
        test_value("DRAND48", 32'd2013670588, rnd_drand48);
        test_value("DRAND48", 32'd3961648151, rnd_drand48);
        test_value("DRAND48", 32'd49649622, rnd_drand48);
        test_value("DRAND48", 32'd1984162345, rnd_drand48);

        // // =========================================================
        // // Seed = 0xCAFE_BABE
        // // =========================================================
        reseed_all(32'hCAFE_BABE);

        // // CONG placeholders
        test_value("CONG",    32'd944244397, rnd_cong);
        test_value("CONG",    32'd3234068496, rnd_cong);
        test_value("CONG",    32'd1219054423, rnd_cong);
        test_value("CONG",    32'd332305970, rnd_cong);
        test_value("CONG",    32'd4032013969, rnd_cong);
        test_value("CONG",    32'd1494586788, rnd_cong);
        test_value("CONG",    32'd77135579, rnd_cong);
        test_value("CONG",    32'd1919093478, rnd_cong);
        test_value("CONG",    32'd2882944693, rnd_cong);
        test_value("CONG",    32'd3129425528, rnd_cong);

        reseed_all(32'hCAFE_BABE);

        // // GLIBC_CRAND placeholders
        test_value("GLIBC",   32'd944740127, rnd_glibc);
        test_value("GLIBC",   32'd2062088812, rnd_glibc);
        test_value("GLIBC",   32'd1829222453, rnd_glibc);
        test_value("GLIBC",   32'd879215818, rnd_glibc);
        test_value("GLIBC",   32'd1818265147, rnd_glibc);
        test_value("GLIBC",   32'd1520521560, rnd_glibc);
        test_value("GLIBC",   32'd2139660977, rnd_glibc);
        test_value("GLIBC",   32'd883254166, rnd_glibc);
        test_value("GLIBC",   32'd1791445783, rnd_glibc);
        test_value("GLIBC",   32'd445000452, rnd_glibc);

        reseed_all(32'hCAFE_BABE);

        // // DRAND48 placeholders
        test_value("DRAND48", 32'd895760113, rnd_drand48);
        test_value("DRAND48", 32'd1765010088, rnd_drand48);
        test_value("DRAND48", 32'd1926093203, rnd_drand48);
        test_value("DRAND48", 32'd3768825250, rnd_drand48);
        test_value("DRAND48", 32'd2822496773, rnd_drand48);
        test_value("DRAND48", 32'd556513836, rnd_drand48);
        test_value("DRAND48", 32'd829541575, rnd_drand48);
        test_value("DRAND48", 32'd2766338758, rnd_drand48);
        test_value("DRAND48", 32'd75147865, rnd_drand48);
        test_value("DRAND48", 32'd1431469552, rnd_drand48);

        $display("%t: TEST PASSED", $time);
        $finish;
    end

endmodule

`endif // __LCG_TESTBENCH_V__
