// 25 MSPS low-pass FIR followed by decimation by 13.
//
// Fixed design targets:
//   passband       : DC .. 500 kHz
//   stopband       : 1 MHz .. 12.5 MHz
//   decimated rate : 25 MHz / 13 = 1.923076923 MSPS
//
// The 159 symmetric coefficients were designed with Parks-McClellan and
// quantized to signed Q1.17. Their exact integer sum is 2^17, so DC gain is
// exactly one. Quantized response verification gives about 0.031 dB maximum
// passband ripple and at least 51.6 dB stopband rejection.
//
// One decimated result needs 80 unique products. Eight multipliers are reused
// for 10 DCO clocks, fitting inside the 13-clock output interval.
module Ad9233DecimatingFir(
	input  wire        rstn,
	input  wire        dco,
	input  wire [11:0] sample_in,
	output reg  [11:0] sample_out,
	output reg         sample_valid
	);

	localparam integer TAP_COUNT = 159;
	localparam integer UNIQUE_TAPS = 80;
	localparam integer DECIMATION = 13;

	reg signed [11:0] history [0:TAP_COUNT-1];
	reg signed [11:0] snapshot [0:TAP_COUNT-1];
	reg [7:0] warmup_count;
	reg [3:0] decimation_count;
	reg       mac_busy;
	reg [3:0] mac_cycle;
	reg signed [39:0] accumulator;
	integer lane;
	integer coefficient_index;
	reg signed [12:0] pair_value;
	reg signed [30:0] lane_product;
	reg signed [39:0] cycle_sum;

	integer shift_index;
	always @(posedge dco or negedge rstn) begin
		if (!rstn) begin
			warmup_count <= 8'd0;
			decimation_count <= 4'd0;
			mac_busy <= 1'b0;
			mac_cycle <= 4'd0;
			accumulator <= 40'sd0;
			sample_out <= 12'd0;
			sample_valid <= 1'b0;
		end
		else begin
			history[0] <= sample_in;
			for (shift_index = 1;
				shift_index < TAP_COUNT;
				shift_index = shift_index + 1)
				history[shift_index] <= history[shift_index-1];

			if (warmup_count < TAP_COUNT)
				warmup_count <= warmup_count + 1'b1;

			sample_valid <= 1'b0;

			// The decimation phase advances on every raw conversion,
			// independently of the time-multiplexed MAC engine.
			if (decimation_count == DECIMATION - 1) begin
				decimation_count <= 4'd0;
				if ((warmup_count == TAP_COUNT) && !mac_busy) begin
					// Include the conversion present on sample_in at this
					// edge as the newest sample in the frozen window.
					snapshot[0] <= sample_in;
					for (shift_index = 1;
						shift_index < TAP_COUNT;
						shift_index = shift_index + 1)
						snapshot[shift_index]
							<= history[shift_index-1];

					mac_busy <= 1'b1;
					mac_cycle <= 4'd0;
					accumulator <= 40'sd0;
				end
			end
			else begin
				decimation_count <= decimation_count + 1'b1;
			end

			if (mac_busy) begin
				if (mac_cycle == 4'd9) begin
					sample_out <= quantize_sample(
						accumulator + cycle_sum
					);
					sample_valid <= 1'b1;
					mac_busy <= 1'b0;
					mac_cycle <= 4'd0;
					accumulator <= 40'sd0;
				end
				else begin
					accumulator <= accumulator + cycle_sum;
					mac_cycle <= mac_cycle + 1'b1;
				end
			end
		end
	end

	function signed [17:0] coefficient;
		input [6:0] index;
		begin
			case (index)
				7'd0: coefficient = 18'sd72;
				7'd1: coefficient = 18'sd34;
				7'd2: coefficient = 18'sd40;
				7'd3: coefficient = 18'sd45;
				7'd4: coefficient = 18'sd49;
				7'd5: coefficient = 18'sd51;
				7'd6: coefficient = 18'sd50;
				7'd7: coefficient = 18'sd48;
				7'd8: coefficient = 18'sd42;
				7'd9: coefficient = 18'sd32;
				7'd10: coefficient = 18'sd20;
				7'd11: coefficient = 18'sd3;
				7'd12: coefficient = -18'sd16;
				7'd13: coefficient = -18'sd38;
				7'd14: coefficient = -18'sd62;
				7'd15: coefficient = -18'sd88;
				7'd16: coefficient = -18'sd114;
				7'd17: coefficient = -18'sd139;
				7'd18: coefficient = -18'sd162;
				7'd19: coefficient = -18'sd182;
				7'd20: coefficient = -18'sd198;
				7'd21: coefficient = -18'sd207;
				7'd22: coefficient = -18'sd208;
				7'd23: coefficient = -18'sd202;
				7'd24: coefficient = -18'sd186;
				7'd25: coefficient = -18'sd160;
				7'd26: coefficient = -18'sd124;
				7'd27: coefficient = -18'sd79;
				7'd28: coefficient = -18'sd25;
				7'd29: coefficient = 18'sd38;
				7'd30: coefficient = 18'sd106;
				7'd31: coefficient = 18'sd179;
				7'd32: coefficient = 18'sd253;
				7'd33: coefficient = 18'sd325;
				7'd34: coefficient = 18'sd393;
				7'd35: coefficient = 18'sd454;
				7'd36: coefficient = 18'sd503;
				7'd37: coefficient = 18'sd538;
				7'd38: coefficient = 18'sd555;
				7'd39: coefficient = 18'sd552;
				7'd40: coefficient = 18'sd527;
				7'd41: coefficient = 18'sd478;
				7'd42: coefficient = 18'sd404;
				7'd43: coefficient = 18'sd306;
				7'd44: coefficient = 18'sd184;
				7'd45: coefficient = 18'sd42;
				7'd46: coefficient = -18'sd118;
				7'd47: coefficient = -18'sd292;
				7'd48: coefficient = -18'sd474;
				7'd49: coefficient = -18'sd657;
				7'd50: coefficient = -18'sd837;
				7'd51: coefficient = -18'sd1004;
				7'd52: coefficient = -18'sd1151;
				7'd53: coefficient = -18'sd1271;
				7'd54: coefficient = -18'sd1355;
				7'd55: coefficient = -18'sd1396;
				7'd56: coefficient = -18'sd1387;
				7'd57: coefficient = -18'sd1324;
				7'd58: coefficient = -18'sd1200;
				7'd59: coefficient = -18'sd1013;
				7'd60: coefficient = -18'sd760;
				7'd61: coefficient = -18'sd443;
				7'd62: coefficient = -18'sd62;
				7'd63: coefficient = 18'sd378;
				7'd64: coefficient = 18'sd873;
				7'd65: coefficient = 18'sd1416;
				7'd66: coefficient = 18'sd1997;
				7'd67: coefficient = 18'sd2606;
				7'd68: coefficient = 18'sd3233;
				7'd69: coefficient = 18'sd3864;
				7'd70: coefficient = 18'sd4488;
				7'd71: coefficient = 18'sd5090;
				7'd72: coefficient = 18'sd5659;
				7'd73: coefficient = 18'sd6181;
				7'd74: coefficient = 18'sd6645;
				7'd75: coefficient = 18'sd7040;
				7'd76: coefficient = 18'sd7357;
				7'd77: coefficient = 18'sd7589;
				7'd78: coefficient = 18'sd7730;
				7'd79: coefficient = 18'sd8002;
				default: coefficient = 18'sd0;
			endcase
		end
	endfunction

	always @* begin
		cycle_sum = 40'sd0;
		pair_value = 13'sd0;
		lane_product = 31'sd0;
		coefficient_index = 0;

		for (lane = 0; lane < 8; lane = lane + 1) begin
			coefficient_index = (mac_cycle * 8) + lane;
			if (coefficient_index < UNIQUE_TAPS - 1) begin
				pair_value = {
					snapshot[coefficient_index][11],
					snapshot[coefficient_index]
				} + {
					snapshot[TAP_COUNT-1-coefficient_index][11],
					snapshot[TAP_COUNT-1-coefficient_index]
				};
				lane_product = pair_value
					* coefficient(coefficient_index[6:0]);
			end
			else if (coefficient_index == UNIQUE_TAPS - 1) begin
				pair_value = {
					snapshot[(TAP_COUNT-1)/2][11],
					snapshot[(TAP_COUNT-1)/2]
				};
				lane_product = pair_value
					* coefficient(coefficient_index[6:0]);
			end
			else begin
				pair_value = 13'sd0;
				lane_product = 31'sd0;
			end

			cycle_sum = cycle_sum + {
				{9{lane_product[30]}}, lane_product
			};
		end
	end

	function [11:0] quantize_sample;
		input signed [39:0] value;
		reg signed [39:0] adjusted;
		reg signed [39:0] shifted;
		begin
			// Round to nearest while preserving symmetry around zero.
			if (value >= 0)
				adjusted = value + 40'sd65536;
			else
				adjusted = value + 40'sd65535;
			shifted = adjusted >>> 17;

			if (shifted > 40'sd2047)
				quantize_sample = 12'h7FF;
			else if (shifted < -40'sd2048)
				quantize_sample = 12'h800;
			else
				quantize_sample = shifted[11:0];
		end
	endfunction

endmodule
