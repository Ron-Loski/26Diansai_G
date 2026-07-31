// One-shot AD9233 acquisition buffer.
//
// The write port runs from DCO. A low-pass FIR first removes content above
// 1 MHz, then every 13th filtered conversion is kept. The frozen frame is
// read through a second, SPI-clocked RAM port.
module Ad9233FrameCapture #(
	parameter integer SAMPLE_COUNT = 4096
	)(
	input  wire        rstn,
	input  wire        dco,
	input  wire        cfg_ready,
	input  wire [11:0] adc_data,
	input  wire        adc_otr,
	input  wire        arm_toggle_async,

	input  wire        read_clock,
	input  wire [11:0] read_address,
	output reg  [11:0] read_data,

	output reg         capture_busy,
	output reg         frame_ready,
	output reg  [15:0] frame_id,
	output reg  [31:0] otr_count,
	output wire        filter_error,
	output wire [15:0] filter_overrun_count,
	output wire [15:0] filter_saturation_count
	);

	reg [2:0] arm_sync;
	reg [11:0] write_address;
	wire signed [11:0] filtered_sample;
	wire filtered_valid;
	wire ram_write_enable = capture_busy && filtered_valid;

	Ad9233MultistageDecimator anti_alias_fir(
		.rstn(rstn),
		.dco(dco),
		.sample_valid_in(cfg_ready),
		.sample_in(adc_data),
		.sample_out(filtered_sample),
		.sample_valid(filtered_valid),
		.filter_error(filter_error),
		.overrun_count(filter_overrun_count),
		.saturation_count(filter_saturation_count)
		);

	always @(posedge dco or negedge rstn) begin
		if (!rstn) begin
			arm_sync <= 3'b000;
			write_address <= 12'd0;
			capture_busy <= 1'b0;
			frame_ready <= 1'b0;
			frame_id <= 16'd0;
			otr_count <= 32'd0;
		end
		else begin
			arm_sync <= {arm_sync[1:0], arm_toggle_async};
			if ((arm_sync[2] ^ arm_sync[1]) && cfg_ready) begin
				write_address <= 12'd0;
				capture_busy <= 1'b1;
				frame_ready <= 1'b0;
				otr_count <= 32'd0;
			end
			else if (capture_busy) begin
				if (adc_otr)
					otr_count <= otr_count + 1'b1;

				if (filtered_valid) begin
					if (write_address == SAMPLE_COUNT - 1) begin
						capture_busy <= 1'b0;
						frame_ready <= 1'b1;
						frame_id <= frame_id + 1'b1;
					end
					else begin
						write_address <= write_address + 1'b1;
					end
				end
			end
		end
	end

	wire [11:0] ram_q;
	always @(posedge read_clock or negedge rstn) begin
		if (!rstn)
			read_data <= 12'd0;
		else
			read_data <= ram_q;
	end

	altsyncram capture_ram(
		.clock0        (dco),
		.wren_a        (ram_write_enable),
		.address_a     (write_address),
		.data_a        (filtered_sample),
		.clock1        (read_clock),
		.address_b     (read_address),
		.q_a           (),
		.q_b           (ram_q),
		.eccstatus     (),
		.aclr0         (1'b0),
		.aclr1         (1'b0),
		.addressstall_a(1'b0),
		.addressstall_b(1'b0),
		.byteena_a     (1'b1),
		.byteena_b     (1'b1),
		.clocken0      (1'b1),
		.clocken1      (1'b1),
		.clocken2      (1'b1),
		.clocken3      (1'b1),
		.data_b        (12'd0),
		.rden_a        (1'b1),
		.rden_b        (1'b1),
		.wren_b        (1'b0)
		);

	defparam
		capture_ram.operation_mode = "DUAL_PORT",
		capture_ram.width_a = 12,
		capture_ram.widthad_a = 12,
		capture_ram.numwords_a = SAMPLE_COUNT,
		capture_ram.width_b = 12,
		capture_ram.widthad_b = 12,
		capture_ram.numwords_b = SAMPLE_COUNT,
		capture_ram.outdata_reg_b = "UNREGISTERED",
		capture_ram.address_reg_b = "CLOCK1",
		capture_ram.intended_device_family = "Cyclone IV E",
		capture_ram.ram_block_type = "M9K",
		capture_ram.lpm_type = "altsyncram";

endmodule
