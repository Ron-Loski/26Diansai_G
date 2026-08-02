# 周期信号测量分析装置：AI 交接说明

更新时间：2026-07-30
远程仓库：`https://github.com/Ron-Loski/26Diansai_G.git`
交接分支：`codex/ad9233-analyzer-verified-stage1`

## 1. 接手后先做什么

不要立即修改采样率、FFT点数或SPI协议。当前数字链路已经在实物上通过
10 kHz～500 kHz单音、双音和三音测试。下一阶段的主要风险在模拟低通、
放大器和整机幅频校准。

新电脑建议执行：

```powershell
git clone https://github.com/Ron-Loski/26Diansai_G.git
cd 26Diansai_G
git switch codex/ad9233-analyzer-verified-stage1
```

工程入口：

- MCU Keil工程：`MDK-ARM/Project.uvprojx`
- MCU CubeMX工程：`Project.ioc`
- MCU可直接烧录：`MDK-ARM/Project/Project.hex`
- FPGA Quartus工程：`FPGA_sendwave/prj/DAC904_HighSpeedDAC.qpf`
- FPGA可直接下载：`FPGA_sendwave/prj/DAC904_HighSpeedDAC.sof`
- FPGA SignalTap：`FPGA_sendwave/debug/ad9233_capture.stp`

## 2. 题目目标

装置输入周期信号，信号源输出阻抗为50 Ω：

- 有效信号`ub`峰峰值50～250 mV；
- `ub`所有频率分量位于10～500 kHz；
- `ub`由基波和1个或2个整数次谐波组合，即最多3个有效分量；
- 叠加单频干扰`uJ`，其峰峰值为200 mV、频率不低于1 MHz；
- 显示1个或3个完整周期、Upp、真Urms和基频；
- 显示定性频谱及各有效分量幅值；
- 幅值、Upp、Urms绝对误差不超过5 mV；
- 基频绝对误差不超过1 kHz；
- 频率分辨率500 Hz；
- 每项启动后2秒内完成。

当前内部FFT频格为469.501 Hz，显示时可以量化为500 Hz网格，满足题意。

## 3. 当前架构

```text
50 Ω输入
→ [下一阶段：模拟低通与约7.9倍放大]
→ 50 Ω版AD9233
→ FPGA 25 MSPS原始采样
→ 每13点保留1点
→ 4096点冻结缓存
→ SPI3/CRC16传给STM32H743VI
→ Hann窗FFT找峰
→ 整数谐波阶数判断
→ 未加窗样本联合最小二乘拟合
→ Upp、真Urms、基频、1～3个分量
→ 当前USART3输出，HMI待接入
```

固定参数：

| 参数 | 当前值 |
|---|---:|
| AD9233原始采样率 | 25.000 MSPS |
| FPGA抽取倍数 | 13 |
| MCU分析采样率 | 1.923076923 MSPS |
| 缓存/FFT长度 | 4096点 |
| FFT频格 | 469.501 Hz |
| 原始采集时间 | 2.12992 ms |
| SPI3时钟 | 12 MHz |
| 单音计算时间 | 约47～48 ms |
| 双音计算时间 | 约74 ms |
| 三音计算时间 | 约106 ms |

注意：这里的“13倍抽取”只是每13个原始ADC样本保留1点，目前FPGA没有数字
抗混叠FIR。抽取前必须依靠模拟低通压制1 MHz以上强干扰，否则高频干扰会
混叠到10～500 kHz有效带内。

## 4. 硬件和接线

### 4.1 MCU与FPGA

MCU为STM32H743VIT6，SPI3固定Mode 2、16位、MSB优先、12 MHz：

| 功能 | STM32H743VI | FPGA管脚 | FPGA端口 |
|---|---|---|---|
| SCK | PC10/SPI3_SCK | E6 | `mcu_spi_sck` |
| MOSI | PB2/SPI3_MOSI | D5 | `mcu_spi_mosi` |
| MISO | PC11/SPI3_MISO | M7 | `mcu_spi_miso` |
| CS_N | PC6/GPIO，低有效 | A8 | 顶层仍名为`mcu_uart_rx` |
| 地 | GND | GND | 必须共地 |

M7属于3.3 V I/O Bank，已解决之前D6所在2.5 V Bank不兼容问题。

USART3用于日志和文本命令：

| 功能 | MCU管脚 | 参数 |
|---|---|---|
| TX | PD8 | 230400，8N1 |
| RX | PD9 | 230400，8N1 |

可发送：

- `WaveRaw`：下一帧打印4096个原始抽取样本；
- `WaveDisplay`：下一帧打印480个拟合重构显示点。

### 4.2 AD9233与FPGA

| AD9233信号 | FPGA管脚 |
|---|---|
| ADC CLK | T13 |
| DCO | G2 |
| D0～D11 | R10、T11、R9、T10、R8、T9、R7、T8、R6、T7、R5、T6 |
| OTR | R4 |
| OEB | R11 |
| PWDN | R12 |
| CSB | T4 |
| SCLK | R3 |
| SDIO | T5 |

AD9233模块单独接+5 V和GND，并与FPGA/MCU共地。T14、R13、N6、R14没有
分配ADC功能，不依赖扩展口上的这些电源行。模块SMA为50 Ω版本，不能再在
SMA处并联一个50 Ω终端。

## 5. FPGA实现

关键文件：

- `FPGA_sendwave/rtl/Ad9233Pll25.V`
- `FPGA_sendwave/rtl/Ad9233SpiInit.V`
- `FPGA_sendwave/rtl/Ad9233FrameCapture.V`
- `FPGA_sendwave/rtl/AnalyzerSpiSlave.V`
- `FPGA_sendwave/rtl/DAC904_WriteTEST.V`
- `FPGA_sendwave/prj/DAC904_HighSpeedDAC.qsf`
- `FPGA_sendwave/prj/DAC904_HighSpeedDAC.sdc`

工作过程：

1. 50 MHz板载时钟经PLL产生25 MHz ADC时钟；
2. 上电延时后配置AD9233寄存器；
3. DCO上升沿采集12位二进制补码和OTR；
4. 收到ARM后每13点保留1点；
5. 保存4096点后冻结RAM并置`frame_ready`；
6. MCU通过SPI读取状态和冻结帧；
7. 下一次ARM才覆盖RAM。

最终Quartus II 13.1实测构建结果：

- Fitter成功，器件EP4CE10F17C8；
- 逻辑单元1515/10320；
- RAM 147456/423936 bit；
- PLL 1/2；
- 最差setup约+0.902 ns；
- 最差hold约+0.107 ns；
- 无负时序裕量。

SDC中的SPI时钟仍按18.75 MHz做保守约束，而实机运行12 MHz，因此约束比
实际更严格，不需要为了注释差异改变通信参数。

## 6. SPI协议

每条请求先发送4个16位字：

```text
0xA55A, command, argument, CRC16-CCITT
```

命令：

- `0x0001`：ARM_CAPTURE；
- `0x0002`：GET_STATUS；
- `0x0003`：READ_BLOCK。

响应头：

```text
0x5AA5
protocol_version = 1
flags
frame_id
sample_rate_kHz = 1923
sample_count = 4096
otr_count_low
otr_count_high
```

READ响应随后包含4096个符号扩展到16位的ADC样本和整帧CRC16。MCU只有在
同步字、版本、长度和CRC全部正确时才把样本交给分析算法。

正常串口状态应当类似：

```text
ANALYZER_STATUS,pll=1,cfg=1,busy=0,ready=1,
frame=...,otr=0,good=...,crc=0,protocol=0,dma=0,...
```

如果读取全为`FFFF`，优先检查MISO；全为`0000`则检查FPGA是否装载正式SOF。
`FPGA_sendwave/prj/MISO_LOW_TEST_M7.sof`只用于把M7拉低的接线诊断，不是正式
功能SOF。

## 7. MCU算法

关键文件：

- `Uitls/Src/fpga_capture.c`：SPI状态机、DMA、CRC和错误计数；
- `Uitls/Src/signal_analyzer.c`：FFT、谐波识别、联合拟合和参数计算；
- `Uitls/Src/Command.c`：串口命令；
- `Core/Src/main.c`：主循环和结果打印。

分析流程：

1. ADC样本去直流；
2. Hann窗后做4096点FFT，只搜索10～500 kHz；
3. 合并相邻旁瓣，保留最多3个显著峰；
4. 判断1～50次整数谐波关系并优化基频；
5. 在未加窗样本上联合拟合`DC + Σ(a cos + b sin)`；
6. 分量峰值为`sqrt(a²+b²)`；
7. 真有效值为`sqrt(ΣApeak²/2)`；
8. 用校准后的分量幅相重构一个周期并求`max-min`得到Upp；
9. 生成480点的1周期或3周期显示波形。

Upp不能用各分量Vpp直接相加，因为它与各谐波相位有关。

当前固件启动标识：

```text
ANALYZER_FW,H743VI_ADC_DIRECT_CAL,build=20260729_2110
ANALYZER_CAL,profile=adc_direct,scale=0.968936,mv_per_code=0.299439
```

`0.968936`和`0.299439 mV/code`是当前“信号源直接接50 Ω ADC模块”的临时
台架换算。模拟低通与放大器接入后必须重新标定，不能继续把它作为整机精度
依据。

Keil当前构建：0 Error、0 Warning；输出HEX已经随分支保存。

## 8. 已完成的实物验证

以下均是实际板上串口结果，不是仿真数据。

### 8.1 单音边界

- 10、150、500 kHz均可稳定识别；
- 输入折算约50 mVpp时，测得约49.6～49.8 mVpp；
- 输入折算约250 mVpp时：
  - 10 kHz约250.04 mVpp；
  - 150 kHz约249.75 mVpp；
  - 500 kHz约249.56 mVpp；
- 频率误差远小于1 kHz；
- `otr=0`。

### 8.2 双音最高频边界

输入目标：

```text
250 kHz基波：100 mVpp
500 kHz二次谐波：50 mVpp
```

实测：

```text
f0 ≈ 249999.7 Hz
n=1 peak ≈ 49.978 mV
n=2 peak ≈ 25.013 mV
Upp ≈ 129.885 mV
Urms ≈ 39.517 mV
components=2
processing_us ≈ 74400
otr=0
```

与理论值相比，幅值、Upp和Urms误差均远小于1 mV。

### 8.3 三音

输入目标：

```text
40 kHz基波：100 mVpp
120 kHz三次：50 mVpp
200 kHz五次：25 mVpp
```

实测：

```text
f0 ≈ 40000.8 Hz
n=1 peak ≈ 49.92 mV
n=3 peak ≈ 24.94 mV
n=5 peak ≈ 12.46 mV
Upp ≈ 112.67 mV
Urms ≈ 40.43 mV
components=3
processing_us ≈ 106000
otr=0
```

### 8.4 链路稳定性

连续运行约1分钟时：

```text
pll=1,cfg=1,otr=0,crc=0,protocol=0,dma=0
```

`good`持续增长，未出现CRC、协议或DMA错误。

原始SignalTap导出数据保存在：

- `FPGA_sendwave/prj/ad9233_capture.csv`
- `FPGA_sendwave/prj/ad9233_capture_500KHZ.csv`
- `FPGA_sendwave/prj/ad9233_capture_500khz_0.5VPP.csv`
- `FPGA_sendwave/prj/ad9233_capture_sangjiao.csv`

## 9. 目前已经完成与尚未完成

已完成：

- 50 Ω版AD9233在25 MSPS下稳定采集；
- FPGA 13倍抽取、4096点冻结缓存；
- SPI3双向通信、DMA和CRC；
- 10～500 kHz单音频率/幅值计算；
- 双音和三音任意整数谐波识别；
- Upp、真Urms和每个分量峰值计算；
- 500 kHz最高有效分量边界；
- 230400串口日志和波形导出；
- Quartus/Keil完整构建和可直接下载文件。

尚未完成：

- 模拟抗混叠低通的实物焊接与扫频；
- 约7.9倍低噪声放大器实物验证；
- 接入模拟前端后的整机复频响校准；
- `ub + uJ`抗干扰测试；
- HMI页面和按键1/3周期切换的最终集成；
- 正式比赛证据表和屏幕照片。

## 10. 下一步方案

### 阶段A：模拟低通单独验收

不要先接ADC。按50 Ω源/负载约定测：

- 通带：10、100、250、500 kHz；
- 阻带：1、1.1、1.5、2、5、10 MHz；
- 记录每点输入、输出、增益和相位；
- 重点确认500 kHz损耗稳定；
- 1 MHz目标抑制约40 dB；
- 检查5～10 MHz没有寄生抬升。

### 阶段B：放大器单独验收

- 输入50、100、250 mVpp；
- 测10、100、250、500 kHz；
- 检查实际增益、噪声、削顶和振荡；
- 接50 Ω负载验证；
- 确认全范围`otr=0`。

### 阶段C：整机重新校准

接成：

```text
装置BNC → 低通 → 放大器 → AD9233 → FPGA → MCU
```

使用10、20、…、500 kHz共50个单音频点建立：

- `mv_per_code[50]`；
- `phase_correction_rad[50]`；
- 版本号与CRC。

以装置BNC输入端的实际电压为基准，不以信号源屏幕显示值直接代替。校准后
重新检查50 mVpp和250 mVpp边界。

### 阶段D：抗干扰验收

先只输入`ub`并记录基准，再加入200 mVpp干扰，依次测试：

```text
1、1.1、1.5、2、5、10 MHz
```

最坏边界优先使用：

```text
ub = 500 kHz、50 mVpp
uJ = 1 MHz、200 mVpp
```

加干扰前后要求：

- `|ΔUpp| ≤ 5 mV`；
- `|ΔUrms| ≤ 5 mV`；
- 每个`|Δpeak_mv| ≤ 5 mV`；
- `|Δf0| ≤ 1 kHz`；
- 分量数量和谐波阶数不变；
- 不得把混叠干扰识别成新的有效分量；
- `otr/crc/protocol/dma = 0`；
- 总时间小于2秒。

之后再用已通过的40/120/200 kHz三音，分别叠加1 MHz和2 MHz干扰，确认低通
不会破坏有效谐波。

### 阶段E：显示

算法会生成480点拟合重构波形，适合显示500 kHz时的平滑1周期或3周期。不要
直接把1.923 MSPS抽取样本折线当最终屏幕波形。完成HMI时显示：

- 1/3周期波形；
- Upp、Urms、f0；
- 1～3个分量的阶数、频率和幅值；
- 0～500 kHz正频率谱；
- PLL、ADC配置、OTR、CRC、处理时间诊断。

## 11. 不要做的事情

- 不要把分支合入`main`，当前阶段仍在整机模拟前端前；
- 不要把MISO改回D6，D6所在Bank电压冲突；
- 不要把串口波特率误设为115200，结果口USART3是230400；
- 不要在AD9233 SMA口再并联50 Ω；
- 不要在没有模拟低通时宣称已经通过`uJ≥1 MHz`抗干扰；
- 不要把当前`0.968936`当成最终整机校准；
- 不要仅为了波形“看起来更平滑”改成8192点；当前参数已经满足500 Hz分辨率
  和2秒时限；
- 不要在没有保存当前SOF/HEX和分支的情况下大改协议。

## 12. 首次在新电脑恢复验证

1. Quartus Programmer下载正式`DAC904_HighSpeedDAC.sof`；
2. Keil打开`MDK-ARM/Project.uvprojx`，下载现成HEX或重新Build；
3. 按接线表连接SPI和共地；
4. USART3打开230400、8N1；
5. 上电应看到build 2110启动标识；
6. 确认`pll=1,cfg=1`；
7. 确认`crc=0,protocol=0,dma=0,otr=0`；
8. 输入100 kHz单音，确认`ANALYZER_RESULT`连续输出；
9. 发送`WaveDisplay`，确认收到480点重构波形；
10. 上述全部正常后再接模拟前端。
