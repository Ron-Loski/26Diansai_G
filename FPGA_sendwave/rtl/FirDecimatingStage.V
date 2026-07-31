// Symmetric, time-multiplexed FIR decimator used by the 62.5 MSPS ADC path.
// Samples are signed 17-bit Q4. Coefficients are signed Q1.17. The rounded
// output remains signed 17-bit Q4, so all three stages have the same interface.
module FirDecimatingStage #(
	parameter integer STAGE_ID = 1,
	parameter integer TAP_COUNT = 15,
	parameter integer UNIQUE_TAPS = 8,
	parameter integer DECIMATION = 4,
	parameter integer LANES = 4,
	parameter integer BATCHES = 2
	)(
	input  wire                    rstn,
	input  wire                    clock,
	input  wire                    sample_valid_in,
	input  wire signed [16:0]      sample_in,
	output reg                     sample_valid_out,
	output reg signed [16:0]       sample_out,
	output reg [15:0]              overrun_count,
	output reg [15:0]              saturation_count
	);

	reg signed [16:0] history [0:TAP_COUNT-1];
	reg signed [16:0] snapshot [0:TAP_COUNT-1];
	reg [5:0] warmup_count;
	reg [5:0] decimation_count;
	reg       mac_busy;
	reg [5:0] issue_batch;
	reg signed [41:0] accumulator;

	reg signed [17:0] operand_sample [0:3];
	reg signed [17:0] operand_coefficient [0:3];
	reg operand_valid;
	reg operand_last;
	reg signed [35:0] product_pipe [0:3];
	reg product_valid;
	reg product_last;
	reg signed [41:0] product_sum;
	reg signed [41:0] completed_sum;
	reg signed [41:0] result_pipe;
	reg result_valid;
	reg signed [41:0] rounded_value;
	reg signed [41:0] shifted_value;

	integer i;
	integer lane;
	integer coefficient_index;
	reg signed [17:0] pair_value;
	reg start_now;

	function signed [17:0] coefficient;
		input integer index;
		begin
			coefficient = 18'sd0;
			case (STAGE_ID)
				1: case (index)
					0: coefficient=-18'sd544;  1: coefficient=-18'sd1689;
					2: coefficient=-18'sd2368; 3: coefficient=-18'sd154;
					4: coefficient= 18'sd6975; 5: coefficient= 18'sd18109;
					6: coefficient= 18'sd28679;7: coefficient= 18'sd33053;
					default: coefficient=18'sd0;
				endcase
				2: case (index)
					0: coefficient=18'sd169;    1: coefficient=18'sd412;
					2: coefficient=18'sd534;    3: coefficient=18'sd144;
					4: coefficient=-18'sd1003;  5: coefficient=-18'sd2590;
					6: coefficient=-18'sd3525;  7: coefficient=-18'sd2251;
					8: coefficient=18'sd2399;   9: coefficient=18'sd10250;
					10: coefficient=18'sd19395; 11: coefficient=18'sd26788;
					12: coefficient=18'sd29629;
					default: coefficient=18'sd0;
				endcase
				3: case (index)
					0: coefficient=-18'sd27;    1: coefficient=-18'sd16;
					2: coefficient=18'sd83;     3: coefficient=18'sd131;
					4: coefficient=-18'sd92;    5: coefficient=-18'sd408;
					6: coefficient=-18'sd177;   7: coefficient=18'sd688;
					8: coefficient=18'sd977;    9: coefficient=-18'sd430;
					10: coefficient=-18'sd2166; 11: coefficient=-18'sd1143;
					12: coefficient=18'sd2788;  13: coefficient=18'sd4436;
					14: coefficient=-18'sd937;  15: coefficient=-18'sd8849;
					16: coefficient=-18'sd6625; 17: coefficient=18'sd12742;
					18: coefficient=18'sd38945; 19: coefficient=18'sd51233;
					default: coefficient=18'sd0;
				endcase
			endcase
		end
	endfunction

	// The product registers form the first MAC pipeline stage. Quartus maps
	// the signed 18x18 products to embedded multipliers.
	always @(posedge clock or negedge rstn) begin
		if (!rstn) begin
			warmup_count <= 6'd0;
			decimation_count <= 6'd0;
			mac_busy <= 1'b0;
			issue_batch <= 6'd0;
			accumulator <= 42'sd0;
			operand_valid <= 1'b0;
			operand_last <= 1'b0;
			product_valid <= 1'b0;
			product_last <= 1'b0;
			result_pipe <= 42'sd0;
			result_valid <= 1'b0;
			sample_valid_out <= 1'b0;
			sample_out <= 17'sd0;
			overrun_count <= 16'd0;
			saturation_count <= 16'd0;
			for (lane = 0; lane < 4; lane = lane + 1) begin
				operand_sample[lane] <= 18'sd0;
				operand_coefficient[lane] <= 18'sd0;
				product_pipe[lane] <= 36'sd0;
			end
		end
		else begin
			start_now = 1'b0;
			sample_valid_out <= 1'b0;
			operand_valid <= 1'b0;
			product_valid <= 1'b0;
			result_valid <= 1'b0;

			if (sample_valid_in) begin
				history[0] <= sample_in;
				for (i = 1; i < TAP_COUNT; i = i + 1)
					history[i] <= history[i-1];
				if (warmup_count < TAP_COUNT)
					warmup_count <= warmup_count + 1'b1;

				if (decimation_count == DECIMATION - 1) begin
					decimation_count <= 6'd0;
					if (warmup_count >= TAP_COUNT - 1) begin
						if (mac_busy && !(product_valid && product_last)) begin
							if (overrun_count != 16'hffff)
								overrun_count <= overrun_count + 1'b1;
						end
						else begin
							snapshot[0] <= sample_in;
							for (i = 1; i < TAP_COUNT; i = i + 1)
								snapshot[i] <= history[i-1];
							mac_busy <= 1'b1;
							issue_batch <= 6'd0;
							start_now = 1'b1;
						end
					end
				end
				else begin
					decimation_count <= decimation_count + 1'b1;
				end
			end

			if (mac_busy && (issue_batch < BATCHES)) begin
				for (lane = 0; lane < 4; lane = lane + 1) begin
					coefficient_index = issue_batch * LANES + lane;
					if ((lane < LANES) && (coefficient_index < UNIQUE_TAPS)) begin
						if (coefficient_index == UNIQUE_TAPS - 1)
							pair_value = {snapshot[coefficient_index][16],
								snapshot[coefficient_index]};
						else
							pair_value = snapshot[coefficient_index]
								+ snapshot[TAP_COUNT-1-coefficient_index];
						operand_sample[lane] <= pair_value;
						operand_coefficient[lane] <= coefficient(coefficient_index);
					end
					else begin
						operand_sample[lane] <= 18'sd0;
						operand_coefficient[lane] <= 18'sd0;
					end
				end
				operand_valid <= 1'b1;
				operand_last <= (issue_batch == BATCHES - 1);
				issue_batch <= issue_batch + 1'b1;
			end

			// Dedicated operand registers split the coefficient/history muxes
			// from the embedded multiplier input path.
			if (operand_valid) begin
				for (lane = 0; lane < 4; lane = lane + 1)
					product_pipe[lane] <= operand_sample[lane]
						* operand_coefficient[lane];
				product_valid <= 1'b1;
				product_last <= operand_last;
			end

			if (product_valid) begin
				// Explicit balanced tree prevents the four-lane case from
				// becoming a serial adder chain on older Quartus versions.
				product_sum =
					({{6{product_pipe[0][35]}}, product_pipe[0]} +
					 {{6{product_pipe[1][35]}}, product_pipe[1]}) +
					({{6{product_pipe[2][35]}}, product_pipe[2]} +
					 {{6{product_pipe[3][35]}}, product_pipe[3]});
				completed_sum = accumulator + product_sum;
				if (product_last) begin
					result_pipe <= completed_sum;
					result_valid <= 1'b1;
					if (!start_now)
						mac_busy <= 1'b0;
					accumulator <= 42'sd0;
				end
				else begin
					accumulator <= completed_sum;
				end
			end

			// Rounding, saturation and valid generation are isolated from
			// the MAC adder tree by result_pipe.
			if (result_valid) begin
				if (result_pipe >= 0)
					rounded_value = result_pipe + 42'sd65536;
				else
					rounded_value = result_pipe + 42'sd65535;
				shifted_value = rounded_value >>> 17;
				if (shifted_value > 42'sd65535) begin
					sample_out <= 17'sd65535;
					if (saturation_count != 16'hffff)
						saturation_count <= saturation_count + 1'b1;
				end
				else if (shifted_value < -42'sd65536) begin
					sample_out <= 17'sh10000;
					if (saturation_count != 16'hffff)
						saturation_count <= saturation_count + 1'b1;
				end
				else begin
					sample_out <= shifted_value[16:0];
				end
				sample_valid_out <= 1'b1;
			end
		end
	end
endmodule
