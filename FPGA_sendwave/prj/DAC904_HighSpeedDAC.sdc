# 50 MHz board oscillator and derived 62.5 MHz ADC output clock.
create_clock -name clk_50m -period 20.000 [get_ports clk]
derive_pll_clocks
derive_clock_uncertainty

# AD9233-80 conservative source-synchronous constraints at 62.5 MSPS.
# Data is launched for the adjacent rising edge; values below include the
# specified 4.9 ns setup, 5.9 ns hold and 0.5 ns interconnect skew budget.
create_clock -name adc_dco -period 16.000 [get_ports adc_dco_i]
set_input_delay -clock adc_dco -clock_fall -max 3.600 \
	[get_ports {adc_data_i[*] adc_otr_i}]
set_input_delay -clock adc_dco -clock_fall -min -2.600 \
	[get_ports {adc_data_i[*] adc_otr_i}]

# STM32H743 SPI3: 192 MHz kernel clock / 16 = 12 MHz, mode 2.
create_clock -name mcu_spi_sck -period 83.333 [get_ports mcu_spi_sck]
set_input_delay -clock mcu_spi_sck -clock_fall -max 3.000 \
	[get_ports mcu_spi_mosi]
set_input_delay -clock mcu_spi_sck -clock_fall -min 0.000 \
	[get_ports mcu_spi_mosi]
set_output_delay -clock mcu_spi_sck -clock_fall -max 12.000 \
	[get_ports mcu_spi_miso]
set_output_delay -clock mcu_spi_sck -clock_fall -min 1.000 \
	[get_ports mcu_spi_miso]

# A8 is the active-low analyzer SPI chip select.
set_false_path -from [get_ports mcu_uart_rx]
set_false_path -from [get_ports rstn]

# ARM crosses from SPI SCK to DCO through a three-flop toggle synchronizer.
set_false_path -from [get_registers {*U3_analyzer_spi|arm_toggle*}] \
	-to [get_registers {*U2_frame_capture|arm_sync[0]*}]

# The frozen frame metadata and status flags cross into the SPI clock domain.
# Status bits have explicit synchronizers; frame_id/OTR remain stable from
# frame-ready until the next ARM.
set_clock_groups -asynchronous \
	-group [get_clocks mcu_spi_sck] \
	-group [remove_from_collection [all_clocks] [get_clocks mcu_spi_sck]]
set_clock_groups -asynchronous \
	-group [get_clocks adc_dco] \
	-group [remove_from_collection [all_clocks] [get_clocks adc_dco]]
