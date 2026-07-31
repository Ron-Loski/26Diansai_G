# 62.5 MSPS 三级抽取版本 AI 交接文档

更新日期：2026-07-31  
工作分支：`codex/62m5-multistage-decimator`  
基线提交：`a132c5c83669a6bb02e473f30b5a6477c1b1e846`  
当前提交：以本分支远程 HEAD 为准，拉取后运行 `git rev-parse HEAD`。提交哈希不能可靠地自写入它自身所包含的文件。

## 1. 项目目标与结论

本版本用于验证把 50 Ω AD9233 的原始采样率由 25 MSPS 提高到 62.5 MSPS 后，弱谐波重复性是否优于比赛稳定版。题目关键指标为：10～500 kHz 有效信号、50～250 mVpp、最多三个谐波分量、频率分辨率 500 Hz、频率误差不超过 1 kHz、Upp/Urms/各分量幅值误差不超过 5 mV，并抑制 1 MHz 以上 200 mVpp 单频干扰。

当前数据链：

```text
50 Ω AD9233，62.5 MSPS
→ 15 taps FIR / 4
→ 15.625 MSPS
→ 25 taps FIR / 4
→ 3.90625 MSPS
→ 39 taps FIR / 2
→ 1.953125 MSPS
→ FPGA 4096 点冻结缓存
→ SPI3 12 MHz、16 bit、Mode 2
→ STM32H743 FFT、谐波识别、最小二乘、HMI
```

最终分析采样率为 1,953,125 Hz，4096 点 FFT 的频率间隔为 476.837158 Hz，采集窗口为 2.097152 ms，满足 500 Hz 分辨率要求。

重要结论：本版本已经通过软件黄金模型、ModelSim、Quartus 和 Keil 构建，但尚未完成 62.5 MSPS 上板实测。因此它目前是“冲刺候选版”，不能替代 25 MSPS 比赛稳定版。

## 2. 工程、Git 与回退版本

| 项目 | 值 |
|---|---|
| 62.5 MSPS 本地目录 | `D:\diansai2025\26Diansai_G_62m5` |
| 远程仓库 | `https://github.com/Ron-Loski/26Diansai_G.git` |
| 推送 remote | `publish` |
| 分支 | `codex/62m5-multistage-decimator` |
| 创建基线 | `codex/weak-harmonic-stability` / `a132c5c` |
| 25 MSPS 回退目录 | `D:\diansai2025\26Diansai_G_handoff` |
| 25 MSPS 回退分支 | `codex/weak-harmonic-stability` |
| 25 MSPS 回退提交 | `a132c5c83669a6bb02e473f30b5a6477c1b1e846` |

不要在回退目录直接合并本分支，也不要覆盖其 25 MSPS SOF/HEX。62.5 版本未通过 F0/F2 物理测试前，比赛主版本仍是 25 MSPS。

## 3. 完整引脚表

### 3.1 AD9233 与 FPGA

| 功能 | FPGA 管脚 | RTL 信号 | 说明 |
|---|---|---|---|
| FPGA 50 MHz 输入 | M2 | `clk` | 板载时钟 |
| FPGA 复位 | M1 | `rstn` | 低有效 |
| ADC CLK | T13 | `adc_clk_o` | PLL_1 输出 62.5 MHz |
| ADC DCO | G2 | `adc_dco_i` | 全局时钟输入 |
| ADC D0～D11 | R10,T11,R9,T10,R8,T9,R7,T8,R6,T7,R5,T6 | `adc_data_i[0:11]` | 12 位补码 |
| ADC OTR | R4 | `adc_otr_i` | 超量程输入 |
| ADC OEB | R11 | `adc_oeb_o` | 初始化后使能输出 |
| ADC PWDN | R12 | `adc_pwdn_o` | 正常工作保持低 |
| ADC CSB | T4 | `adc_spi_cs_n_o` | ADC 配置 SPI |
| ADC SCLK | R3 | `adc_spi_sclk_o` | 约 5 MHz |
| ADC SDIO | T5 | `adc_spi_sdio_o` | 配置输出 |

AD9233 模块单独接 +5 V 和 GND，并与 FPGA、MCU 共地。模块是 50 Ω 输入版，SMA 口不能再并联一个 50 Ω 电阻。T14、R13、N6、R14 不用于 ADC 供电或信号。

### 3.2 MCU 与 FPGA SPI

| 方向 | STM32H743VIT6 | FPGA 管脚 | FPGA 信号 |
|---|---|---|---|
| MCU→FPGA SCK | PC10 / SPI3_SCK | E6 | `mcu_spi_sck` |
| MCU→FPGA MOSI | PB2 / SPI3_MOSI | D5 | `mcu_spi_mosi` |
| FPGA→MCU MISO | PC11 / SPI3_MISO | M7 | `mcu_spi_miso` |
| MCU→FPGA CS_N | PC6 / GPIO | A8 | `mcu_uart_rx`，保留的旧端口名 |
| 地 | GND | GND | 必须共地 |

SPI3：Mode 2、16 位、MSB first、12 MHz。A8 目前实际用作低有效片选，不能再同时接旧 UART 控制线。

### 3.3 串口与 HMI

| 接口 | MCU 管脚 | 波特率 | 用途 |
|---|---|---:|---|
| USART2 | PA2 TX / PA3 RX | 115200 | 串口屏 HMI |
| USART3 | PD8 TX / PD9 RX | 230400 | 调试日志和文本命令 |

正式烧录时继续使用现有 HMI 工程，本次没有改变屏幕控件布局。

## 4. 时钟、三级 FIR 与定点格式

- ADC PLL：50 MHz × 5 / 4 = 62.5 MHz，位于 PLL_1，输出管脚 T13。
- DCO：G2 输入，周期约束 16 ns。
- D0～D11 和 OTR 先进入 DCO 域输入寄存器，13 个寄存器已经由 Fitter 确认进入 I/O 单元。
- SDC 输入约束：相邻下降沿 `set_input_delay -max 3.6 ns`、`-min -2.6 ns`，对应最慢 AD9233-80 时序和 0.5 ns 外部偏斜预算。
- 内部样本：17 位有符号 Q4；12 位 ADC 补码左移 4 位。
- 系数：Parks–McClellan，权重 `[1, 0.2]`，直流增益归一化，Q1.17。
- 对称样本和：18 位；累加器：42 位；正负对称舍入；最后一级才变为 12 位并饱和。
- DSP 调度：Stage 1 使用 4 个 DSP/2 批，Stage 2 使用 2 个 DSP/7 批，Stage 3 使用 1 个 DSP/20 批，总计 7 个 18×18 DSP。
- `sample_valid` 必须每 32 个 DCO 周期出现一次。
- 过载、调度冲突或内部饱和会累计计数并置位 SPI 状态 bit5。

### 4.1 FIR 系数与计算响应

完整可复现结果见 `FPGA_sendwave/debug/fir62m5_summary.json` 和 `fir62m5_response.csv`。

| 级 | Fs | 通带边缘 | 阻带边缘 | taps | 抽取 | Q1.17 唯一对称系数 |
|---|---:|---:|---:|---:|---:|---|
| 1 | 62.5 MHz | 500 kHz | 14.625 MHz | 15 | 4 | -544,-1689,-2368,-154,6975,18109,28679,33053 |
| 2 | 15.625 MHz | 500 kHz | 2.90625 MHz | 25 | 4 | 169,412,534,144,-1003,-2590,-3525,-2251,2399,10250,19395,26788,29629 |
| 3 | 3.90625 MHz | 500 kHz | 1 MHz | 39 | 2 | -27,-16,83,131,-92,-408,-177,688,977,-430,-2166,-1143,2788,4436,-937,-8849,-6625,12742,38945,51233 |

SciPy 1.15.3 计算结果：总通带波动 0.003357371 dB，1～31.25 MHz 最差阻带衰减 62.263639 dB，最差点在 31.25 MHz。以上是脚本计算值，不是实测模拟前端响应。

## 5. SPI 协议版本 2

4096 点返回帧长度和 CRC16-CCITT 结构保持不变。协议字段更新为：

```text
PROTOCOL_VERSION = 2
SAMPLE_RATE_KHZ  = 1953
SAMPLE_COUNT     = 4096
status bit5      = FIR/filter error
status bit6      = FPGA SPI/CRC error
```

MCU 只接受版本 2、采样率字段 1953 和点数 4096。任一不匹配均增加协议错误并停止分析。FIR 错误置位时 MCU 也不会读取该帧。

MCU 常量：

```c
FPGA_CAPTURE_SAMPLE_COUNT   4096
FPGA_CAPTURE_SAMPLE_RATE_HZ (62500000.0f / 32.0f)
```

启动日志应为：

```text
ANALYZER_START,raw_fs=62500000,decimation=32,analysis_fs=1953125.000,n=4096,spi=SPI3_12MHz,protocol=2
```

状态日志应出现 `filter=0`。校准表版本已升级为 4，旧校准自动失效。2.5 mV 峰值门限、0.8 捕获余量、FFT/拟合/HMI 逻辑保持不变。

## 6. 修改文件及职责

### FPGA

- `FPGA_sendwave/rtl/Ad9233Pll62m5.V`：62.5 MHz ADC PLL。
- `FPGA_sendwave/rtl/FirDecimatingStage.V`：可参数化对称 FIR、DSP 调度、舍入和错误计数。
- `FPGA_sendwave/rtl/Ad9233MultistageDecimator.V`：15/25/39 taps 三级链和最终 12 位饱和。
- `FPGA_sendwave/rtl/Ad9233FrameCapture.V`：接入 32 倍抽取输出和 FIR 错误状态。
- `FPGA_sendwave/rtl/Ad9233SpiInit.V`：加入 0x0D 正常/Checker Board 配置。
- `FPGA_sendwave/rtl/AnalyzerSpiSlave.V`：协议 2、1953 kHz、状态 bit5。
- `FPGA_sendwave/rtl/DAC904_WriteTEST.V`：ADC 输入寄存、Checker Board 检查、状态汇总。
- `FPGA_sendwave/prj/DAC904_HighSpeedDAC.qsf`：新 RTL、PLL、Fast Input Register；正式版关闭 SignalTap。
- `FPGA_sendwave/prj/DAC904_HighSpeedDAC.sdc`：16 ns DCO 和 ADC/SPI 时序约束。
- 已删除 `Ad9233Pll25.V` 和 `Ad9233DecimatingFir.V`，避免误用旧架构。

### 脚本与仿真

- `FPGA_sendwave/tools/design_fir62m5.py`：生成/检查系数和频响。
- `FPGA_sendwave/tools/set_build_mode.ps1`：切换 formal/checker/signaltap 构建。
- `FPGA_sendwave/sim/generate_multistage_vectors.py`：Python 定点黄金向量。
- `FPGA_sendwave/sim/tb_ad9233_multistage_decimator.v`：逐点比对、32 周期和错误检查。
- `FPGA_sendwave/sim/tb_ad9233_spi_init.v`：五个 ADC 配置帧检查。
- `FPGA_sendwave/sim/tb_analyzer_spi_slave.v`：协议版本、采样率、bit5、CRC 检查。
- `FPGA_sendwave/sim/run_all.ps1`：一键运行三组 ModelSim。

### STM32H743

- `Uitls/Inc/fpga_capture.h`：62.5/32 常量和 `filter_error`。
- `Uitls/Src/fpga_capture.c`：协议 2 校验、1953 字段、bit5 和错误帧阻断。
- `Uitls/Src/signal_analyzer.c`：校准版本 4。
- `Core/Src/main.c`：新固件标识、采样率和状态日志。
- `Uitls/tests/verify_weak_harmonic_gate.py`：新分析采样率下的弱谐波门限回归。

## 7. 构建与验证步骤

### 7.1 系数

```powershell
cd D:\diansai2025\26Diansai_G_62m5
python FPGA_sendwave\tools\design_fir62m5.py --check
```

要求输出 ripple 不超过 0.01 dB、stopband 不低于 60 dB。

### 7.2 ModelSim 13.1

```powershell
powershell -ExecutionPolicy Bypass -File FPGA_sendwave\sim\run_all.ps1
python Uitls\tests\verify_weak_harmonic_gate.py
```

三组日志位于 `FPGA_sendwave/reports/62m5/modelsim/`。

### 7.3 Quartus II 13.1 正式版

```powershell
powershell -ExecutionPolicy Bypass -File FPGA_sendwave\tools\set_build_mode.ps1 formal
cd FPGA_sendwave\prj
D:\quartus\quartus\bin64\quartus_sh.exe --flow compile DAC904_HighSpeedDAC
```

正式报告位于 `FPGA_sendwave/reports/62m5/quartus/`。

### 7.4 Checker Board 调试版

```powershell
cd D:\diansai2025\26Diansai_G_62m5
powershell -ExecutionPolicy Bypass -File FPGA_sendwave\tools\set_build_mode.ps1 checker
cd FPGA_sendwave\prj
D:\quartus\quartus\bin64\quartus_sh.exe --flow compile DAC904_HighSpeedDAC
```

复制 SOF 后必须重新运行 `set_build_mode.ps1 formal`，避免下一次误编译 Checker 版。仓库已经交付构建好的 `release/62m5/AD9233_62M5_CHECKER_SIGNALTAP.sof`。

### 7.5 Keil μVision

```powershell
cd D:\diansai2025\26Diansai_G_62m5
E:\keil\UV4\UV4.exe -b MDK-ARM\Project.uvprojx -o release\62m5\keil_build_62m5.log
```

当前构建为 0 error、0 warning。生成物已复制到 `release/62m5/`。

## 8. 正确烧录顺序

1. 先烧 `release/62m5/AD9233_62M5_CHECKER_SIGNALTAP.sof`，ADC 接线和电源保持与 25 MSPS 版一致。
2. 示波器确认 T13 到 ADC CLK、ADC DCO 均约 62.5 MHz；SignalTap 运行 Checker Board 连续检查。
3. 达到至少 1000 万点、checker error=0 后，烧 `release/62m5/AD9233_62M5_FORMAL.sof`。
4. 再烧 MCU：`release/62m5/STM32H743_62M5_PROTOCOL_V2.hex`。
5. HMI 使用当前比赛屏工程；本分支没有生成新的 HMI 工程文件。

禁止把 Checker SOF 当正式比赛版本使用；它会让 AD9233 输出测试码而不是输入信号。

## 9. 已完成证据

### 9.1 已计算

- 三级 FIR 频响、量化系数、频率分辨率：`FPGA_sendwave/debug/fir62m5_summary.json`、`fir62m5_response.csv`。
- 计算通带波动 0.003357371 dB，阻带最差 62.263639 dB。

### 9.2 已仿真

- 20,000 个确定性随机原始样本，得到 602 个有效输出，RTL 与 Python 固定点模型逐点一致。
- `sample_valid` 间隔 32 个 DCO，冲激、随机、满量程/饱和路径通过。
- ADC SPI 顺序包括 `0x0D=0x04`；Analyzer SPI 协议 2/1953/bit5/CRC 通过。
- 日志：`FPGA_sendwave/reports/62m5/modelsim/*.log`。

### 9.3 已编译

- Quartus 正式版：0 errors、29 warnings；报告在 `FPGA_sendwave/reports/62m5/quartus/`。
- Quartus Checker/SignalTap 版：0 errors、31 warnings；报告在 `FPGA_sendwave/reports/62m5/quartus_checker/`。
- Keil：0 errors、0 warnings；日志为 `release/62m5/keil_build_62m5.log`。

### 9.4 数据手册/约束支持

- ADC DCO 16 ns；输入 delay 采用 AD9233-80 最慢时序的 4.9 ns setup、5.9 ns hold，并加入 0.5 ns 外部偏斜预算。
- 约束没有为了通过编译而删除或放宽。

### 9.5 尚未实测

- 62.5 MHz ADC CLK/DCO 示波器截图。
- Checker Board 连续 1000 万点零错误截图/日志。
- 100 kHz/500 kHz 正弦、100 kHz 三角波、0.5/1/2 Vpp 线性。
- 正式协议连续 60 秒零错误。
- 10～500 kHz 校准版本 4。
- 25 与 62.5 MSPS 各 1000 帧精度对比 CSV。
- 实际标准差是否下降 20%。

上述项目没有证据，因此不能写成“已通过”。仓库目前也没有 62.5 MSPS SignalTap 实测截图或物理测试 CSV。

## 10. Quartus 资源与时序

正式版 EP4CE10F17C8：

| 指标 | 结果 | 门槛 |
|---|---:|---:|
| Logic elements | 5684 / 10320 = 55% | ≤75% |
| RAM bits | 49152 / 423936 = 12% | ≤50% |
| 9-bit multiplier elements | 14 / 46 = 30% | ≤40% |
| 18×18 signed DSP | 7 | 7 个计划值 |
| PLL | 1 / 2 | PLL_1 |
| I/O 输入寄存器 | 13 | D0～D11 + OTR |
| DCO Fmax，slow 85°C | 88.35 MHz | ≥75 MHz |
| 最差 Setup slack | +4.081 ns | TNS=0 |
| 最差 Hold slack | +0.165 ns | TNS=0 |

所有已约束时钟在三个工艺角下 Setup/Hold TNS 均为 0。最差 Hold 为正，但没有达到原计划的 0.25 ns“目标值”，上板必须用 Checker Board 验证。TimeQuest 仍报告 design not fully constrained，主要与保留的虚拟调试输出/非功能路径有关；不能据此声称每个虚拟端口都已约束。

Checker/SignalTap 版只用于时序观察，不使用其资源数字作为正式版资源结论。

## 11. 25 与 62.5 MSPS 精度对比

目前没有 62.5 MSPS 物理采样数据，因此不能给出真实标准差改善值。理论独立噪声平均改善约为 `sqrt(2.5)=1.581`，即标准差可能下降约 36.8%，但这只是理论上限，会受 ADC 时钟抖动、数字串扰、滤波器量化、信号源和校准误差影响。

只有同一信号源、线缆、50 Ω设置和输入端实测幅值下，25/62.5 两版各记录至少 1000 帧，并满足标准差下降至少 20%、平均偏差不增大，才可在报告中写“62.5 MSPS 提升了实际精度”。

## 12. 已知问题、禁止修改项与边界

- 最差 Hold slack 仅 +0.165 ns；F0 Checker 不通过就停止，不允许放宽 SDC。
- 正式 TimeQuest 仍提示并非所有虚拟端口完全约束，应保留这一事实。
- ADC 输入仍是 50 Ω直采，不含模拟放大器和模拟低通；数字 FIR 不能防止 ADC 前端发生模拟混叠或过载。
- 校准版本 4 尚无实测表，绝对幅值结果不能直接继承 25 MSPS 校准。
- 2.5 mV 峰值门限、0.8 捕获余量和 HMI 布局为公平比较而保持不变，比较阶段不要同时改算法门限。
- 不允许改回旧协议后继续分析；旧 FPGA/新 MCU 或新 FPGA/旧 MCU 必须明确失败。
- 不允许删除 Fast Input Register、ADC 输入寄存器或 SDC 输入 delay 来换资源/时序。
- 不允许把理论 37% 噪声改善写成实测结果。

## 13. 常见故障诊断

| 现象 | 优先检查 |
|---|---|
| `pll=0` | M2 50 MHz、复位、PLL_1、T13 是否有 62.5 MHz |
| `cfg=0` | ADC +5 V/地、T4/R3/T5、OEB/PWDN、ADC SPI 五帧 |
| Checker error 增长 | DCO G2、D0～D11 位序、排线长度、地回流、62.5 MHz 时序；不要先放宽约束 |
| `crc>0` | PC10/PB2/PC11/PC6、M7 MISO、SPI Mode 2、12 MHz、共地 |
| `protocol>0` | FPGA/MCU 是否都是 protocol 2，状态 rate 是否 1953，点数是否 4096 |
| `dma>0` | SPI3 DMA 中断、缓冲区、D-Cache 状态和 CS 时序 |
| `filter=1` | FIR overrun、内部 saturation、valid 间隔；MCU 会停止读取该帧 |
| `otr>0` | ADC 输入过量程、合成信号总峰值、50 Ω幅值设置 |
| 幅值整体异常 | 是否仍使用旧校准表、信号源 50 Ω/High-Z 设置、输入端真实 Vpp |
| 5 mVpp 谐波跳变 | 先记录 1000 帧噪声/峰值分布；不要一边比较采样率一边改 2.5 mV 门限 |

## 14. 下一台电脑 AI 的第一项任务

第一项软件任务：拉取分支后先运行系数检查、三组 ModelSim 和 Keil构建，核对生成结果与本文一致；不要先改 FIR、门限或 HMI。

第一项物理实验：只烧 Checker Board SOF，用示波器确认 ADC CLK/DCO 约 62.5 MHz，然后在 SignalTap 连续检查至少 1000 万个 Checker Board 样本，错误计数必须为 0。F0 未通过时不得进入正式采样或精度比较。

F0 通过后的下一实验：烧正式 SOF 和 protocol 2 MCU HEX，先测 100 kHz、1 Vpp 正弦和 100 kHz 三角波的位序/线性，再做 500 kHz、50 mVpp，最后才做 25/62.5 MSPS 各 1000 帧对比。

## 15. 拉取、建立 worktree 与回退命令

已有仓库时：

```powershell
cd D:\diansai2025\26Diansai_G_handoff
git fetch publish codex/62m5-multistage-decimator
git worktree add D:\diansai2025\26Diansai_G_62m5 publish/codex/62m5-multistage-decimator
cd D:\diansai2025\26Diansai_G_62m5
git switch -c codex/62m5-multistage-decimator --track publish/codex/62m5-multistage-decimator
```

全新电脑：

```powershell
cd D:\diansai2025
git clone https://github.com/Ron-Loski/26Diansai_G.git 26Diansai_G_repo
cd 26Diansai_G_repo
git fetch origin codex/62m5-multistage-decimator
git switch --track origin/codex/62m5-multistage-decimator
```

回退到 25 MSPS 稳定版：

```powershell
cd D:\diansai2025\26Diansai_G_handoff
git switch codex/weak-harmonic-stability
git rev-parse HEAD
# 应为 a132c5c83669a6bb02e473f30b5a6477c1b1e846
```

比赛现场若 62.5 版 Checker、时序、协议或精度任一硬门槛失败，直接烧回 25 MSPS 稳定版 SOF/HEX，不在现场放宽约束或临时改滤波器。
