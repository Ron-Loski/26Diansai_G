`timescale 1ns/1ps

module tb_ad9233_decimating_fir;

	reg rstn;
	reg dco;
	reg [11:0] sample_in;
	wire [11:0] sample_out;
	wire sample_valid;

	Ad9233DecimatingFir dut(
		.rstn(rstn),
		.dco(dco),
		.sample_in(sample_in),
		.sample_out(sample_out),
		.sample_valid(sample_valid)
		);

	initial dco = 1'b0;
	always #20 dco = ~dco;

	integer error_count;
	integer driven_value;
	integer sample_index;
	integer output_count;
	integer collected_count;
	integer maximum_sample;
	integer minimum_sample;
	integer signed_output;
	integer last_valid_index;
	real angle;
	real pi;

	task apply_reset;
		begin
			rstn = 1'b0;
			sample_in = 12'd0;
			repeat (5) @(negedge dco);
			rstn = 1'b1;
		end
	endtask

	task run_tone;
		input real frequency_hz;
		input integer amplitude_code;
		input integer minimum_pp;
		input integer maximum_pp;
		begin
			apply_reset;
			sample_index = 0;
			output_count = 0;
			collected_count = 0;
			maximum_sample = -32768;
			minimum_sample = 32767;
			last_valid_index = -1;

			while (collected_count < 256) begin
				@(negedge dco);
				angle = 2.0 * pi * frequency_hz
					* sample_index / 25000000.0;
				driven_value = $rtoi(
					amplitude_code * $sin(angle)
				);
				sample_in = driven_value[11:0];
				sample_index = sample_index + 1;

				if (sample_valid) begin
					if ((last_valid_index >= 0)
						&& ((sample_index-last_valid_index) != 13)) begin
						$display(
							"FAIL valid interval=%0d at f=%f",
							sample_index-last_valid_index,
							frequency_hz
						);
						error_count = error_count + 1;
					end
					last_valid_index = sample_index;
					output_count = output_count + 1;

					// Ignore startup transient before measuring.
					if (output_count > 64) begin
						signed_output = $signed(sample_out);
						if (signed_output > maximum_sample)
							maximum_sample = signed_output;
						if (signed_output < minimum_sample)
							minimum_sample = signed_output;
						collected_count = collected_count + 1;
					end
				end
			end

			$display(
				"TONE f=%f Hz input_amp=%0d output_pp=%0d",
				frequency_hz,
				amplitude_code,
				maximum_sample-minimum_sample
			);
			if (((maximum_sample-minimum_sample) < minimum_pp)
				|| ((maximum_sample-minimum_sample) > maximum_pp)) begin
				$display(
					"FAIL output_pp outside [%0d,%0d]",
					minimum_pp,
					maximum_pp
				);
				error_count = error_count + 1;
			end
		end
	endtask

	initial begin
		pi = 3.14159265358979323846;
		error_count = 0;
		rstn = 1'b0;
		sample_in = 12'd0;

		// Passband edge must preserve a 1000-code peak sine.
		run_tone(500000.0, 1000, 1950, 2020);

		// Stopband checks include the lower edge and the previously
		// observed 4 MHz aliasing case.
		run_tone(1000000.0, 1000, 0, 6);
		run_tone(4000000.0, 1000, 0, 6);
		run_tone(10000000.0, 1000, 0, 6);

		if (error_count == 0)
			$display("PASS tb_ad9233_decimating_fir");
		else
			$display(
				"FAIL tb_ad9233_decimating_fir errors=%0d",
				error_count
			);
		$finish;
	end

endmodule
