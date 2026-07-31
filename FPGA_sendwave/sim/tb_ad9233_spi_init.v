`timescale 1ns/1ps
module tb_ad9233_spi_init;
	reg clk = 1'b0;
	reg rstn = 1'b0;
	reg enable = 1'b0;
	wire cs_n, sclk, sdio, cfg_done;
	reg [23:0] observed = 24'd0;
	reg [23:0] expected [0:4];
	integer bit_count = 0;
	integer frame_count = 0;
	integer errors = 0;

	Ad9233SpiInit #(.STARTUP_DELAY_CYCLES(2),
		.SPI_HALF_PERIOD_CYCLES(1), .TEST_MODE(4)) dut(
		.clk(clk), .rstn(rstn), .enable(enable), .spi_cs_n(cs_n),
		.spi_sclk(sclk), .spi_sdio(sdio), .cfg_done(cfg_done));
	always #10 clk = ~clk;

	always @(posedge sclk) begin
		if (!cs_n) begin
			observed = {observed[22:0], sdio};
			bit_count = bit_count + 1;
		end
	end

	always @(posedge cs_n) begin
		if (bit_count == 24) begin
			if (observed !== expected[frame_count]) begin
				$display("FRAME_FAIL index=%0d got=%06h expected=%06h",
					frame_count, observed, expected[frame_count]);
				errors = errors + 1;
			end
			frame_count = frame_count + 1;
			bit_count = 0;
			observed = 24'd0;
		end
	end

	initial begin
		expected[0]=24'h001401; expected[1]=24'h001600;
		expected[2]=24'h0018c0; expected[3]=24'h000d04;
		expected[4]=24'h00ff01;
		repeat (3) @(negedge clk);
		rstn = 1'b1; enable = 1'b1;
		wait(cfg_done);
		repeat (2) @(negedge clk);
		if (frame_count != 5) errors = errors + 1;
		if (errors == 0) $display("TB_PASS adc_spi_init five frames checker mode");
		else $display("TB_FAIL errors=%0d frames=%0d", errors, frame_count);
		$finish;
	end
endmodule
