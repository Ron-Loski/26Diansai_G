`timescale 1ns/1ps
`include "vectors/vector_counts.vh"

module tb_ad9233_multistage_decimator;
	reg rstn = 1'b0;
	reg dco = 1'b0;
	reg signed [11:0] sample_in = 12'sd0;
	reg sample_valid_in = 1'b0;
	wire signed [11:0] sample_out;
	wire sample_valid;
	wire filter_error;
	wire [15:0] overrun_count;
	wire [15:0] saturation_count;
	reg [11:0] raw_vectors [0:`RAW_VECTOR_COUNT-1];
	reg [11:0] expected_vectors [0:`EXPECTED_VECTOR_COUNT-1];
	integer input_index;
	integer output_index = 0;
	integer errors = 0;
	integer raw_at_last_output = -1;

	Ad9233MultistageDecimator dut(
		.rstn(rstn), .dco(dco), .sample_valid_in(sample_valid_in),
		.sample_in(sample_in), .sample_out(sample_out),
		.sample_valid(sample_valid), .filter_error(filter_error),
		.overrun_count(overrun_count), .saturation_count(saturation_count));

	always #8 dco = ~dco;

	initial begin
		$readmemh("vectors/raw_q12.mem", raw_vectors);
		$readmemh("vectors/expected_q12.mem", expected_vectors);
		repeat (5) @(negedge dco);
		rstn = 1'b1;
		for (input_index = 0; input_index < `RAW_VECTOR_COUNT;
			 input_index = input_index + 1) begin
			@(negedge dco);
			if (sample_valid) begin
				if (sample_out !== expected_vectors[output_index]) begin
					$display("MISMATCH output=%0d got=%0d expected=%0d",
						output_index, $signed(sample_out),
						$signed(expected_vectors[output_index]));
					errors = errors + 1;
				end
				if ((raw_at_last_output >= 0) &&
					(input_index - raw_at_last_output != 32)) begin
					$display("BAD_INTERVAL raw=%0d interval=%0d",
						input_index, input_index - raw_at_last_output);
					errors = errors + 1;
				end
				raw_at_last_output = input_index;
				output_index = output_index + 1;
			end
			sample_in = raw_vectors[input_index];
			sample_valid_in = 1'b1;
		end
		sample_valid_in = 1'b0;
		repeat (100) begin
			@(negedge dco);
			if (sample_valid) begin
				if (sample_out !== expected_vectors[output_index])
					errors = errors + 1;
				output_index = output_index + 1;
			end
		end
		if (output_index != `EXPECTED_VECTOR_COUNT) begin
			$display("BAD_COUNT got=%0d expected=%0d", output_index,
				`EXPECTED_VECTOR_COUNT);
			errors = errors + 1;
		end
		if (filter_error || overrun_count || saturation_count)
			errors = errors + 1;
		if (errors == 0)
			$display("TB_PASS multistage exact fixed-point outputs=%0d", output_index);
		else
			$display("TB_FAIL errors=%0d", errors);
		$finish;
	end
endmodule
