// 50 MHz to 62.5 MHz PLL used as the AD9233 sampling clock.
module Ad9233Pll62m5(
	input  wire areset,
	input  wire inclk0,
	output wire c0,
	output wire locked
	);
	wire [4:0] pll_clocks;
	wire pll_locked;
	wire [1:0] pll_inputs = {1'b0, inclk0};
	assign c0 = pll_clocks[0];
	assign locked = pll_locked;

	altpll altpll_component(
		.areset(areset), .inclk(pll_inputs), .clk(pll_clocks),
		.locked(pll_locked), .activeclock(), .clkbad(),
		.clkena({6{1'b1}}), .clkloss(), .clkswitch(1'b0),
		.configupdate(1'b0), .enable0(), .enable1(), .extclk(),
		.extclkena({4{1'b1}}), .fbin(1'b1), .fbmimicbidir(),
		.fbout(), .fref(), .icdrclk(), .pfdena(1'b1),
		.phasecounterselect(4'b1111), .phasedone(), .phasestep(1'b1),
		.phaseupdown(1'b1), .pllena(1'b1), .scanaclr(1'b0),
		.scanclk(1'b0), .scanclkena(1'b1), .scandata(1'b0),
		.scandataout(), .scandone(), .scanread(1'b0), .scanwrite(1'b0),
		.sclkout0(), .sclkout1(), .vcooverrange(), .vcounderrange());

	defparam
		altpll_component.bandwidth_type = "AUTO",
		altpll_component.clk0_divide_by = 4,
		altpll_component.clk0_duty_cycle = 50,
		altpll_component.clk0_multiply_by = 5,
		altpll_component.clk0_phase_shift = "0",
		altpll_component.compensate_clock = "CLK0",
		altpll_component.inclk0_input_frequency = 20000,
		altpll_component.intended_device_family = "Cyclone IV E",
		altpll_component.lpm_hint = "CBX_MODULE_PREFIX=Ad9233Pll62m5",
		altpll_component.lpm_type = "altpll",
		altpll_component.operation_mode = "NORMAL",
		altpll_component.pll_type = "AUTO",
		altpll_component.port_areset = "PORT_USED",
		altpll_component.port_inclk0 = "PORT_USED",
		altpll_component.port_inclk1 = "PORT_UNUSED",
		altpll_component.port_locked = "PORT_USED",
		altpll_component.port_clk0 = "PORT_USED",
		altpll_component.port_clk1 = "PORT_UNUSED",
		altpll_component.port_clk2 = "PORT_UNUSED",
		altpll_component.port_clk3 = "PORT_UNUSED",
		altpll_component.port_clk4 = "PORT_UNUSED",
		altpll_component.port_clk5 = "PORT_UNUSED",
		altpll_component.port_extclk0 = "PORT_UNUSED",
		altpll_component.port_extclk1 = "PORT_UNUSED",
		altpll_component.port_extclk2 = "PORT_UNUSED",
		altpll_component.port_extclk3 = "PORT_UNUSED",
		altpll_component.width_clock = 5;
endmodule
