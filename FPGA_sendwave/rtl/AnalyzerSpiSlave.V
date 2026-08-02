// AD9233 analyzer SPI slave.
//
// SPI mode 2, 16-bit, MSB first. A transaction starts with:
//   A55A, command, argument, CRC16-CCITT.
// STATUS returns 8 header words plus CRC. READ returns the same header,
// 4096 sign-extended samples, then CRC.
module AnalyzerSpiSlave(
	input  wire        rstn,
	input  wire        spi_sck,
	input  wire        spi_cs_n,
	input  wire        spi_mosi,
	output reg         spi_miso,

	input  wire        pll_locked_async,
	input  wire        cfg_done_async,
	input  wire        capture_busy_async,
	input  wire        frame_ready_async,
	input  wire [15:0] frame_id_async,
	input  wire [31:0] otr_count_async,

	output reg         arm_toggle,
	output reg  [11:0] ram_read_address,
	input  wire [11:0] ram_read_data,
	output reg  [15:0] crc_error_count
	);

	localparam [15:0] REQUEST_SYNC = 16'hA55A;
	localparam [15:0] RESPONSE_SYNC = 16'h5AA5;
	localparam [15:0] CMD_ARM = 16'h0001;
	localparam [15:0] CMD_STATUS = 16'h0002;
	localparam [15:0] CMD_READ = 16'h0003;
	localparam [15:0] PROTOCOL_VERSION = 16'h0001;
	localparam [15:0] SAMPLE_RATE_KHZ = 16'd1923;
	localparam [15:0] SAMPLE_COUNT = 16'd4096;

	function [15:0] crc16_word;
		input [15:0] crc_in;
		input [15:0] data_in;
		integer i;
		reg [15:0] crc;
		begin
			crc = crc_in;
			for (i = 15; i >= 0; i = i - 1) begin
				if (crc[15] ^ data_in[i])
					crc = {crc[14:0], 1'b0} ^ 16'h1021;
				else
					crc = {crc[14:0], 1'b0};
			end
			crc16_word = crc;
		end
	endfunction

	reg [2:0] status_sync_0;
	reg [2:0] status_sync_1;
	reg [2:0] status_sync_2;
	reg [1:0] ready_sync;
	wire pll_locked = status_sync_2[0];
	wire cfg_done = status_sync_2[1];
	wire capture_busy = status_sync_2[2];
	wire frame_ready = ready_sync[1];

	always @(negedge spi_sck or negedge rstn) begin
		if (!rstn) begin
			status_sync_0 <= 3'b000;
			status_sync_1 <= 3'b000;
			status_sync_2 <= 3'b000;
			ready_sync <= 2'b00;
		end
		else begin
			status_sync_0 <= {
				capture_busy_async, cfg_done_async, pll_locked_async
			};
			status_sync_1 <= status_sync_0;
			status_sync_2 <= status_sync_1;
			ready_sync <= {ready_sync[0], frame_ready_async};
		end
	end

	wire [15:0] live_status_flags = {
		9'd0,
		(crc_error_count != 0),
		1'b0,
		(otr_count_async != 0),
		frame_ready,
		capture_busy,
		cfg_done,
		pll_locked
	};

	// Falling-edge request receiver.
	reg [15:0] rx_shift;
	reg [3:0] rx_bit_count;
	reg [1:0] request_word_index;
	reg [15:0] request_command;
	reg [15:0] request_crc;
	reg request_sync_valid;
	reg rx_ignore;
	reg response_toggle;
	reg response_read_latched;
	reg [15:0] frame_id_latched;
	reg [31:0] otr_count_latched;
	reg [15:0] flags_latched;
	wire [15:0] assembled_rx_word = {rx_shift[14:0], spi_mosi};

	always @(negedge spi_sck or posedge spi_cs_n or negedge rstn) begin
		if (!rstn) begin
			rx_shift <= 16'd0;
			rx_bit_count <= 4'd0;
			request_word_index <= 2'd0;
			request_command <= 16'd0;
			request_crc <= 16'hFFFF;
			request_sync_valid <= 1'b0;
			rx_ignore <= 1'b0;
			response_toggle <= 1'b0;
			response_read_latched <= 1'b0;
			frame_id_latched <= 16'd0;
			otr_count_latched <= 32'd0;
			flags_latched <= 16'd0;
			arm_toggle <= 1'b0;
			crc_error_count <= 16'd0;
		end
		else if (spi_cs_n) begin
			rx_shift <= 16'd0;
			rx_bit_count <= 4'd0;
			request_word_index <= 2'd0;
			request_crc <= 16'hFFFF;
			request_sync_valid <= 1'b0;
			rx_ignore <= 1'b0;
		end
		else if (!rx_ignore) begin
			rx_shift <= assembled_rx_word;
			if (rx_bit_count == 4'd15) begin
				rx_bit_count <= 4'd0;
				case (request_word_index)
					2'd0: begin
						request_sync_valid <=
							(assembled_rx_word == REQUEST_SYNC);
						request_crc <= crc16_word(
							16'hFFFF, assembled_rx_word
						);
						request_word_index <= 2'd1;
					end
					2'd1: begin
						request_command <= assembled_rx_word;
						request_crc <= crc16_word(
							request_crc, assembled_rx_word
						);
						request_word_index <= 2'd2;
					end
					2'd2: begin
						request_crc <= crc16_word(
							request_crc, assembled_rx_word
						);
						request_word_index <= 2'd3;
					end
					2'd3: begin
						request_word_index <= 2'd0;
						rx_ignore <= 1'b1;
						if ((request_crc == assembled_rx_word)
							&& request_sync_valid) begin
							if (request_command == CMD_ARM) begin
								arm_toggle <= ~arm_toggle;
							end
							else if ((request_command == CMD_STATUS)
								|| (request_command == CMD_READ)) begin
								frame_id_latched <= frame_id_async;
								otr_count_latched <= otr_count_async;
								flags_latched <= live_status_flags;
								response_read_latched <=
									(request_command == CMD_READ)
									&& frame_ready;
								response_toggle <= ~response_toggle;
							end
						end
						else begin
							crc_error_count <= crc_error_count + 1'b1;
						end
					end
				endcase
			end
			else begin
				rx_bit_count <= rx_bit_count + 1'b1;
			end
		end
	end

	function [15:0] header_word;
		input [3:0] index;
		input [15:0] flags;
		input [15:0] frame_id;
		input [31:0] otr_count;
		begin
			case (index)
				4'd0: header_word = RESPONSE_SYNC;
				4'd1: header_word = PROTOCOL_VERSION;
				4'd2: header_word = flags;
				4'd3: header_word = frame_id;
				4'd4: header_word = SAMPLE_RATE_KHZ;
				4'd5: header_word = SAMPLE_COUNT;
				4'd6: header_word = otr_count[15:0];
				4'd7: header_word = otr_count[31:16];
				default: header_word = 16'd0;
			endcase
		end
	endfunction

	// Rising-edge response transmitter. MISO is stable at each following
	// falling edge, which is the sampling edge used by the MCU.
	reg response_seen;
	reg tx_active;
	reg tx_read;
	reg [12:0] response_word_index;
	reg [15:0] response_crc;
	reg [15:0] tx_shift;
	reg [4:0] tx_bits_remaining;
	reg [15:0] next_word;
	reg next_is_crc;

	always @(posedge spi_sck or posedge spi_cs_n or negedge rstn) begin
		if (!rstn) begin
			spi_miso <= 1'b0;
			response_seen <= 1'b0;
			tx_active <= 1'b0;
			tx_read <= 1'b0;
			response_word_index <= 13'd0;
			response_crc <= 16'hFFFF;
			tx_shift <= 16'd0;
			tx_bits_remaining <= 5'd0;
			ram_read_address <= 12'hFFF;
		end
		else if (spi_cs_n) begin
			spi_miso <= 1'b0;
			tx_active <= 1'b0;
			tx_bits_remaining <= 5'd0;
			ram_read_address <= 12'hFFF;
		end
		else if (response_seen != response_toggle) begin
			response_seen <= response_toggle;
			tx_active <= 1'b1;
			tx_read <= response_read_latched;
			response_word_index <= 13'd0;
			response_crc <= crc16_word(16'hFFFF, RESPONSE_SYNC);
			spi_miso <= RESPONSE_SYNC[15];
			tx_shift <= {RESPONSE_SYNC[14:0], 1'b0};
			tx_bits_remaining <= 5'd15;
			ram_read_address <= 12'd0;
		end
		else if (tx_active) begin
			spi_miso <= tx_shift[15];
			if (tx_bits_remaining > 5'd1) begin
				tx_shift <= {tx_shift[14:0], 1'b0};
				tx_bits_remaining <= tx_bits_remaining - 1'b1;
			end
			else begin
				next_is_crc = 1'b0;
				if (response_word_index < 13'd7) begin
					next_word = header_word(
						response_word_index[3:0] + 1'b1,
						flags_latched,
						frame_id_latched,
						otr_count_latched
					);
				end
				else if (tx_read
					&& (response_word_index < 13'd4103)) begin
					next_word = {
						{4{ram_read_data[11]}}, ram_read_data
					};
					ram_read_address <= ram_read_address + 1'b1;
				end
				else begin
					next_word = response_crc;
					next_is_crc = 1'b1;
				end

				if ((response_word_index == 13'd8 && !tx_read)
					|| (response_word_index == 13'd4104 && tx_read)) begin
					tx_active <= 1'b0;
					tx_bits_remaining <= 5'd0;
				end
				else begin
					response_word_index <=
						response_word_index + 1'b1;
					tx_shift <= next_word;
					tx_bits_remaining <= 5'd16;
					if (!next_is_crc)
						response_crc <= crc16_word(
							response_crc, next_word
						);
				end
			end
		end
		else begin
			spi_miso <= 1'b0;
		end
	end

endmodule
