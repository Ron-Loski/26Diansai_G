// 62.5 MSPS, three-stage FIR decimator: 4 x 4 x 2 = 32.
// The final sample rate is 1.953125 MSPS.
module Ad9233MultistageDecimator(
	input  wire               rstn,
	input  wire               dco,
	input  wire               sample_valid_in,
	input  wire signed [11:0] sample_in,
	output reg  signed [11:0] sample_out,
	output reg                sample_valid,
	output wire               filter_error,
	output wire [15:0]        overrun_count,
	output wire [15:0]        saturation_count
	);

	wire signed [16:0] input_q4 = {sample_in[11], sample_in, 4'b0000};
	wire signed [16:0] stage1_sample;
	wire signed [16:0] stage2_sample;
	wire signed [16:0] stage3_sample;
	wire stage1_valid, stage2_valid, stage3_valid;
	wire [15:0] overrun1, overrun2, overrun3;
	wire [15:0] saturate1, saturate2, saturate3;

	FirDecimatingStage #(.STAGE_ID(1), .TAP_COUNT(15), .UNIQUE_TAPS(8),
		.DECIMATION(4), .LANES(4), .BATCHES(2)) stage1(
		.rstn(rstn), .clock(dco), .sample_valid_in(sample_valid_in),
		.sample_in(input_q4), .sample_valid_out(stage1_valid),
		.sample_out(stage1_sample), .overrun_count(overrun1),
		.saturation_count(saturate1));

	FirDecimatingStage #(.STAGE_ID(2), .TAP_COUNT(25), .UNIQUE_TAPS(13),
		.DECIMATION(4), .LANES(2), .BATCHES(7)) stage2(
		.rstn(rstn), .clock(dco), .sample_valid_in(stage1_valid),
		.sample_in(stage1_sample), .sample_valid_out(stage2_valid),
		.sample_out(stage2_sample), .overrun_count(overrun2),
		.saturation_count(saturate2));

	FirDecimatingStage #(.STAGE_ID(3), .TAP_COUNT(39), .UNIQUE_TAPS(20),
		.DECIMATION(2), .LANES(1), .BATCHES(20)) stage3(
		.rstn(rstn), .clock(dco), .sample_valid_in(stage2_valid),
		.sample_in(stage2_sample), .sample_valid_out(stage3_valid),
		.sample_out(stage3_sample), .overrun_count(overrun3),
		.saturation_count(saturate3));

	assign overrun_count = overrun1 + overrun2 + overrun3;
	assign saturation_count = saturate1 + saturate2 + saturate3;
	assign filter_error = (overrun_count != 0) || (saturation_count != 0);

	reg signed [17:0] rounded_q4;
	reg signed [17:0] integer_sample;
	always @(posedge dco or negedge rstn) begin
		if (!rstn) begin
			sample_out <= 12'sd0;
			sample_valid <= 1'b0;
		end
		else begin
			sample_valid <= 1'b0;
			if (stage3_valid) begin
				if (stage3_sample >= 0)
					rounded_q4 = {stage3_sample[16], stage3_sample} + 18'sd8;
				else
					rounded_q4 = {stage3_sample[16], stage3_sample} + 18'sd7;
				integer_sample = rounded_q4 >>> 4;
				if (integer_sample > 18'sd2047)
					sample_out <= 12'sd2047;
				else if (integer_sample < -18'sd2048)
					sample_out <= 12'h800;
				else
					sample_out <= integer_sample[11:0];
				sample_valid <= 1'b1;
			end
		end
	end
endmodule
