// G-problem measurement top level.
//
// The DAC path is intentionally quiescent on this branch to reduce digital
// switching noise. The separate DDS/DAC branch remains available as rollback.
module DAC904_WriteTEST #(
	parameter integer ADC_TEST_MODE = 0
	)(
	input  wire        clk,
	input  wire        rstn,
	input  wire        mcu_uart_rx,
	input  wire        mcu_spi_sck,
	input  wire        mcu_spi_mosi,
	output wire        mcu_spi_miso,
	input  wire        adc_dco_i,
	input  wire [11:0] adc_data_i,
	input  wire        adc_otr_i,

	output wire        clk_driver,
	output wire [13:0] IO_data,
	output wire        dac_gnd,
	output wire        adc_clk_o,
	output wire        adc_oeb_o,
	output wire        adc_pwdn_o,
	output wire        adc_spi_cs_n_o,
	output wire        adc_spi_sclk_o,
	output wire        adc_spi_sdio_o,
	output wire        adc_debug_clk_o,
	output wire [11:0] adc_debug_sample_o,
	output wire [11:0] adc_debug_status_o
	);

	wire areset = ~rstn;
	wire adc_clk_62m5;
	wire adc_pll_locked;
	reg [1:0] adc_pll_lock_sync;
	wire adc_cfg_done;

	Ad9233Pll62m5 U0_adc_pll62m5_inst(
		.areset(areset),
		.inclk0(clk),
		.c0(adc_clk_62m5),
		.locked(adc_pll_locked)
		);

	always @(posedge clk or negedge rstn) begin
		if (!rstn)
			adc_pll_lock_sync <= 2'b00;
		else
			adc_pll_lock_sync <= {
				adc_pll_lock_sync[0], adc_pll_locked
			};
	end

	Ad9233SpiInit #(.TEST_MODE(ADC_TEST_MODE)) U1_ad9233_spi_init(
		.clk(clk),
		.rstn(rstn),
		.enable(adc_pll_lock_sync[1]),
		.spi_cs_n(adc_spi_cs_n_o),
		.spi_sclk(adc_spi_sclk_o),
		.spi_sdio(adc_spi_sdio_o),
		.cfg_done(adc_cfg_done)
		);

	reg [11:0] adc_raw_sample;
	reg adc_raw_otr;
	reg [1:0] adc_cfg_done_dco_sync;
	reg [1:0] adc_pll_locked_dco_sync;
	reg [23:0] checker_sample_count;
	reg [31:0] checker_error_count;
	reg checker_started;
	reg checker_expected_phase;
	reg checker_done;
	wire checker_mode = (ADC_TEST_MODE == 8'h04);

	always @(posedge adc_dco_i or negedge rstn) begin
		if (!rstn) begin
			adc_raw_sample <= 12'd0;
			adc_raw_otr <= 1'b0;
			adc_cfg_done_dco_sync <= 2'b00;
			adc_pll_locked_dco_sync <= 2'b00;
			checker_sample_count <= 24'd0;
			checker_error_count <= 32'd0;
			checker_started <= 1'b0;
			checker_expected_phase <= 1'b0;
			checker_done <= 1'b0;
		end
		else begin
			adc_raw_sample <= adc_data_i;
			adc_raw_otr <= adc_otr_i;
			adc_cfg_done_dco_sync <= {
				adc_cfg_done_dco_sync[0], adc_cfg_done
			};
			adc_pll_locked_dco_sync <= {
				adc_pll_locked_dco_sync[0], adc_pll_locked
			};
			if (checker_mode && adc_cfg_done_dco_sync[1] && !checker_done) begin
				if (!checker_started) begin
					checker_started <= 1'b1;
					checker_expected_phase <= (adc_data_i == 12'hAAA);
				end
				else begin
					checker_expected_phase <= ~checker_expected_phase;
					if (adc_data_i != (checker_expected_phase ? 12'h555 : 12'hAAA))
						checker_error_count <= checker_error_count + 1'b1;
					if (checker_sample_count == 24'd9999999)
						checker_done <= 1'b1;
					else
						checker_sample_count <= checker_sample_count + 1'b1;
				end
			end
		end
	end

	wire arm_toggle;
	wire capture_busy;
	wire frame_ready;
	wire [15:0] frame_id;
	wire [31:0] otr_count;
	wire [11:0] ram_read_address;
	wire [11:0] ram_read_data;
	wire [15:0] spi_crc_error_count;
	wire filter_error;
	wire [15:0] filter_overrun_count;
	wire [15:0] filter_saturation_count;

	Ad9233FrameCapture U2_frame_capture(
		.rstn(rstn),
		.dco(adc_dco_i),
		.cfg_ready(adc_cfg_done_dco_sync[1]),
		.adc_data(adc_raw_sample),
		.adc_otr(adc_raw_otr),
		.arm_toggle_async(arm_toggle),
		.read_clock(mcu_spi_sck),
		.read_address(ram_read_address),
		.read_data(ram_read_data),
		.capture_busy(capture_busy),
		.frame_ready(frame_ready),
		.frame_id(frame_id),
		.otr_count(otr_count),
		.filter_error(filter_error),
		.filter_overrun_count(filter_overrun_count),
		.filter_saturation_count(filter_saturation_count)
		);

	AnalyzerSpiSlave U3_analyzer_spi(
		.rstn(rstn),
		.spi_sck(mcu_spi_sck),
		.spi_cs_n(mcu_uart_rx),
		.spi_mosi(mcu_spi_mosi),
		.spi_miso(mcu_spi_miso),
		.pll_locked_async(adc_pll_locked_dco_sync[1]),
		.cfg_done_async(adc_cfg_done_dco_sync[1]),
		.capture_busy_async(capture_busy),
		.frame_ready_async(frame_ready),
		.filter_error_async(filter_error),
		.frame_id_async(frame_id),
		.otr_count_async(otr_count),
		.arm_toggle(arm_toggle),
		.ram_read_address(ram_read_address),
		.ram_read_data(ram_read_data),
		.crc_error_count(spi_crc_error_count)
		);

	assign adc_clk_o = adc_clk_62m5;
	assign adc_oeb_o = ~adc_cfg_done;
	assign adc_pwdn_o = 1'b0;

	// F0 SignalTap observes every raw 62.5 MSPS conversion.
	assign adc_debug_clk_o = adc_dco_i;
	assign adc_debug_sample_o = adc_raw_sample;
	assign adc_debug_status_o = {
		checker_done,
		(checker_error_count != 0),
		filter_error,
		frame_ready,
		capture_busy,
		(spi_crc_error_count != 0),
		2'd0,
		adc_pll_locked_dco_sync[1],
		adc_cfg_done_dco_sync[1],
		adc_raw_otr,
		1'b1
	};

	// Measurement branch: disable DAC clock/data switching.
	assign clk_driver = 1'b0;
	assign IO_data = 14'd8192;
	assign dac_gnd = 1'b0;

endmodule
