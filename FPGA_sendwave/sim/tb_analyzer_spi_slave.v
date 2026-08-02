`timescale 1ns/1ps

module tb_analyzer_spi_slave;
	reg rstn = 1'b0;
	reg spi_sck = 1'b1;
	reg spi_cs_n = 1'b1;
	reg spi_mosi = 1'b0;
	wire spi_miso;
	wire arm_toggle;
	wire [11:0] ram_read_address;
	reg [11:0] ram_read_data = 12'h123;
	wire [15:0] crc_error_count;
	reg [15:0] rx_word;
	integer errors = 0;

	AnalyzerSpiSlave dut (
		.rstn(rstn),
		.spi_sck(spi_sck),
		.spi_cs_n(spi_cs_n),
		.spi_mosi(spi_mosi),
		.spi_miso(spi_miso),
		.pll_locked_async(1'b1),
		.cfg_done_async(1'b1),
		.capture_busy_async(1'b0),
		.frame_ready_async(1'b1),
		.frame_id_async(16'h002A),
		.otr_count_async(32'h00000000),
		.arm_toggle(arm_toggle),
		.ram_read_address(ram_read_address),
		.ram_read_data(ram_read_data),
		.crc_error_count(crc_error_count)
	);

	task spi_word;
		input [15:0] tx;
		output [15:0] rx;
		integer bit_index;
		begin
			rx = 16'd0;
			for (bit_index = 15; bit_index >= 0;
				 bit_index = bit_index - 1) begin
				spi_mosi = tx[bit_index];
				#20;
				spi_sck = 1'b0;
				#1;
				rx[bit_index] = spi_miso;
				#19;
				spi_sck = 1'b1;
			end
		end
	endtask

	initial begin
		#100;
		rstn = 1'b1;
		#100;
		spi_cs_n = 1'b0;

		spi_word(16'hA55A, rx_word);
		spi_word(16'h0002, rx_word);
		spi_word(16'h0000, rx_word);
		spi_word(16'h91C5, rx_word);

		spi_word(16'h0000, rx_word);
		$display("response[0]=%04h", rx_word);
		if (rx_word !== 16'h5AA5) errors = errors + 1;

		spi_word(16'h0000, rx_word);
		$display("response[1]=%04h", rx_word);
		if (rx_word !== 16'h0001) errors = errors + 1;

		spi_word(16'h0000, rx_word);
		$display("response[2]=%04h", rx_word);
		if (rx_word[1:0] !== 2'b11) errors = errors + 1;

		spi_word(16'h0000, rx_word);
		$display("response[3]=%04h", rx_word);
		if (rx_word !== 16'h002A) errors = errors + 1;

		spi_cs_n = 1'b1;
		#100;
		if (errors == 0)
			$display("TB_PASS");
		else
			$display("TB_FAIL errors=%0d crc_errors=%0d",
					 errors, crc_error_count);
		$finish;
	end
endmodule
