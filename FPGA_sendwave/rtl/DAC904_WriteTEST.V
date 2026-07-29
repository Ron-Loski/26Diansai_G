// G-problem measurement top level.
//
// The DAC path is intentionally quiescent on this branch to reduce digital
// switching noise. The separate DDS/DAC branch remains available as rollback.
module DAC904_WriteTEST(
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
	wire adc_clk_25m;
	wire adc_pll_locked;
	reg [1:0] adc_pll_lock_sync;
	wire adc_cfg_done;

	Ad9233Pll25 U0_adc_pll25_inst(
		.areset(areset),
		.inclk0(clk),
		.c0(adc_clk_25m),
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

	Ad9233SpiInit U1_ad9233_spi_init(
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

	always @(posedge adc_dco_i or negedge rstn) begin
		if (!rstn) begin
			adc_raw_sample <= 12'd0;
			adc_raw_otr <= 1'b0;
			adc_cfg_done_dco_sync <= 2'b00;
			adc_pll_locked_dco_sync <= 2'b00;
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

	Ad9233FrameCapture U2_frame_capture(
		.rstn(rstn),
		.dco(adc_dco_i),
		.cfg_ready(adc_cfg_done_dco_sync[1]),
		.adc_data(adc_data_i),
		.adc_otr(adc_otr_i),
		.arm_toggle_async(arm_toggle),
		.read_clock(mcu_spi_sck),
		.read_address(ram_read_address),
		.read_data(ram_read_data),
		.capture_busy(capture_busy),
		.frame_ready(frame_ready),
		.frame_id(frame_id),
		.otr_count(otr_count)
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
		.frame_id_async(frame_id),
		.otr_count_async(otr_count),
		.arm_toggle(arm_toggle),
		.ram_read_address(ram_read_address),
		.ram_read_data(ram_read_data),
		.crc_error_count(spi_crc_error_count)
		);

	assign adc_clk_o = adc_clk_25m;
	assign adc_oeb_o = ~adc_cfg_done;
	assign adc_pwdn_o = 1'b0;

	// F0 SignalTap observes every raw 25 MSPS conversion.
	assign adc_debug_clk_o = adc_dco_i;
	assign adc_debug_sample_o = adc_raw_sample;
	assign adc_debug_status_o = {
		frame_ready,
		capture_busy,
		(spi_crc_error_count != 0),
		5'd0,
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
