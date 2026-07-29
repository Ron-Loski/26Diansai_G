# STM32H743VI 与 FPGA 采集链路

完整状态、测试结果和下一步计划请先阅读根目录的`AI_HANDOFF.md`。

## 接线

| STM32H743VI | 功能 | FPGA |
|---|---|---|
| PC10 | SPI3_SCK | E6 |
| PB2 | SPI3_MOSI | D5 |
| PC11 | SPI3_MISO | M7 |
| PC6 | GPIO片选，低有效 | A8 |
| GND | 共地 | GND |

SPI3固定为Mode 2、16位、MSB优先、12 MHz。USART3使用PD8/PD9、
230400、8N1输出结果。

## 首次上板

1. 下载`FPGA_sendwave/prj/DAC904_HighSpeedDAC.sof`；
2. 下载`MDK-ARM/Project/Project.hex`；
3. 按上表连接SPI和共地；
4. 打开USART3串口；
5. 正常状态应为`pll=1,cfg=1,crc=0,protocol=0,dma=0,otr=0`；
6. 输入100 kHz正弦，确认连续输出`ANALYZER_RESULT`。

`MISO_LOW_TEST_M7.sof`只用于把M7拉低检查线路，不是正式功能文件。
