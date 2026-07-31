// One-shot AD9233 SPI configuration.
//
// SCLK idles high.  SDIO changes while SCLK is low and is sampled by the
// converter on each rising edge.  With a 50 MHz system clock and the default
// half-period of five cycles, SCLK is 5 MHz.
module Ad9233SpiInit #(
	parameter integer STARTUP_DELAY_CYCLES = 5000000,
	parameter integer SPI_HALF_PERIOD_CYCLES = 5,
	parameter integer TEST_MODE = 0
	)(
	input  wire clk,
	input  wire rstn,
	input  wire enable,
	output reg  spi_cs_n,
	output reg  spi_sclk,
	output reg  spi_sdio,
	output reg  cfg_done
	);

	localparam [2:0] STATE_WAIT = 3'd0;
	localparam [2:0] STATE_LOAD = 3'd1;
	localparam [2:0] STATE_LOW  = 3'd2;
	localparam [2:0] STATE_HIGH = 3'd3;
	localparam [2:0] STATE_GAP  = 3'd4;
	localparam [2:0] STATE_DONE = 3'd5;

	reg [2:0] state;
	reg [23:0] startup_count;
	reg [7:0] half_count;
	reg [2:0] frame_index;
	reg [4:0] bit_index;
	reg [23:0] shift_data;

	function [23:0] config_frame;
		input [2:0] index;
		begin
			case (index)
				3'd0: config_frame = 24'h001401;
				3'd1: config_frame = 24'h001600;
				3'd2: config_frame = 24'h0018C0;
				3'd3: config_frame = {16'h000D, TEST_MODE[7:0]};
				3'd4: config_frame = 24'h00FF01;
				default: config_frame = 24'h000000;
			endcase
		end
	endfunction

	wire [23:0] selected_frame;
	assign selected_frame = config_frame(frame_index);

	always @(posedge clk or negedge rstn) begin
		if (!rstn) begin
			state <= STATE_WAIT;
			startup_count <= 24'd0;
			half_count <= 8'd0;
			frame_index <= 3'd0;
			bit_index <= 5'd23;
			shift_data <= 24'd0;
			spi_cs_n <= 1'b1;
			spi_sclk <= 1'b1;
			spi_sdio <= 1'b0;
			cfg_done <= 1'b0;
		end
		else begin
			case (state)
				STATE_WAIT: begin
					spi_cs_n <= 1'b1;
					spi_sclk <= 1'b1;
					spi_sdio <= 1'b0;
					cfg_done <= 1'b0;
					frame_index <= 3'd0;
					if (!enable) begin
						startup_count <= 24'd0;
					end
					else if (startup_count == STARTUP_DELAY_CYCLES - 1) begin
						startup_count <= 24'd0;
						state <= STATE_LOAD;
					end
					else begin
						startup_count <= startup_count + 1'b1;
					end
				end

				STATE_LOAD: begin
					shift_data <= selected_frame;
					bit_index <= 5'd23;
					spi_cs_n <= 1'b0;
					spi_sclk <= 1'b0;
					spi_sdio <= selected_frame[23];
					half_count <= 8'd0;
					state <= STATE_LOW;
				end

				STATE_LOW: begin
					if (half_count == SPI_HALF_PERIOD_CYCLES - 1) begin
						half_count <= 8'd0;
						spi_sclk <= 1'b1;
						state <= STATE_HIGH;
					end
					else begin
						half_count <= half_count + 1'b1;
					end
				end

				STATE_HIGH: begin
					if (half_count == SPI_HALF_PERIOD_CYCLES - 1) begin
						half_count <= 8'd0;
						if (bit_index == 5'd0) begin
							spi_cs_n <= 1'b1;
							spi_sclk <= 1'b1;
							state <= STATE_GAP;
						end
						else begin
							bit_index <= bit_index - 1'b1;
							shift_data <= {shift_data[22:0], 1'b0};
							spi_sclk <= 1'b0;
							spi_sdio <= shift_data[22];
							state <= STATE_LOW;
						end
					end
					else begin
						half_count <= half_count + 1'b1;
					end
				end

				STATE_GAP: begin
					spi_cs_n <= 1'b1;
					spi_sclk <= 1'b1;
					spi_sdio <= 1'b0;
					if (frame_index == 3'd4) begin
						state <= STATE_DONE;
					end
					else begin
						frame_index <= frame_index + 1'b1;
						state <= STATE_LOAD;
					end
				end

				STATE_DONE: begin
					spi_cs_n <= 1'b1;
					spi_sclk <= 1'b1;
					spi_sdio <= 1'b0;
					cfg_done <= 1'b1;
				end

				default: begin
					state <= STATE_WAIT;
				end
			endcase
		end
	end

endmodule
