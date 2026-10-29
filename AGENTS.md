# AGENTS.md — 本仓库的固定环境与操作约定

> 给 AI 助手 / 新同事的速查表。**改代码或回答构建问题前先读这里**，不要重新摸索。

## 1. Keil 工具链位置（本机固定）

| 项 | 路径 / 值 |
| --- | --- |
| uVision | `E:\Embeded\Keil_v5\UV4\UV4.exe`（**不在 PATH 中**，必须用绝对路径） |
| 版本 | uVision 5.42.0.0 |
| 编译器 | ARMCLANG V6.23（`E:\Embeded\Keil_v5\ARM\ARMCLANG\Bin`） |
| 器件包 | Keil.STM32G4xx_DFP |
| 工程文件 | `MDK-ARM\SuperCapV3.uvprojx` |
| 输出 | `MDK-ARM\SuperCapV3\SuperCapV3.axf` / `.hex`（目录被 `.gitignore` 忽略） |

其他常见安装位置（`C:\Keil_v5`、`D:\Keil_v5`）在本机**不存在**；`arm-none-eabi-gcc`、`armclang` 都不在 PATH 中。

## 2. 命令行编译（唯一可靠写法）

`UV4.exe` 是 GUI 子系统程序，PowerShell 用 `&` 调用**不会等待**，`$LASTEXITCODE` 为空。必须用 `Start-Process -Wait`：

```powershell
Start-Process -FilePath 'E:\Embeded\Keil_v5\UV4\UV4.exe' `
  -ArgumentList '-b','MDK-ARM\SuperCapV3.uvprojx','-j0','-o','build_check.log' `
  -Wait -PassThru | Select-Object ExitCode
```

- `-b` = batch build（Rebuild），`-j0` = 不弹窗，`-o` = 日志文件名；
- 日志落在**工程文件所在目录**，即 `MDK-ARM\build_check.log`（`*.log` 已被 `.gitignore` 忽略，不会污染仓库）；
- 构建约 7 s。

## 3. 判定构建成功

读 `MDK-ARM\build_check.log`，末尾应出现：

```
Program Size: Code=xxxxx RO-data=xxxx RW-data=xx ZI-data=xxxx
"SuperCapV3\SuperCapV3.axf" - 0 Error(s), 0 Warning(s).
```

> 基线（2026-10-28，修完 `HardLock` 误触发系列问题 + 显示改 1 ms 块平均后）：
> `Code=56252 RO-data=2716 RW-data=24 ZI-data=5144`，0 Error 0 Warning。
> 注：这是 `CFG_DEBUG_UART=0`（正常使用）的数值；调成 1 会多约 880 B Flash
> （`57132 / 2888 / 28 / 5140`）。
> 注：2026-10-27 用 CubeMX 重新生成后 HAL 升到 **1.2.7**，链接脚本换成
> `MDK-ARM/stm32g474xx_flash.sct`（与原 `SuperCapV3/SuperCapV3.sct` 内存布局一致）。
> Code 大幅变化时先确认是不是误改了 `App/params.h` 的常量或漏登记 `uvprojx` 里的文件。
> 参数改完建议跑 `python Tools/check_params.py`（24 条静态约束 + `.ioc`/生成代码一致性），
> 环参数验证跑 `python Tools/sim_ctrl.py`。
>
> **约定：能在 CubeMX (.ioc) 里配的，一律落 `.ioc`**，不要只改生成代码——
> 否则重新生成会被静默改回默认值。当前 `.ioc` 已覆盖：HRTIM 死区 10 计数、
> I2C1 400 kHz（`Timing=0x20824349`）、NVIC（含控制节拍 DMA1_Channel1、
> OLED 用的 DMA1_Channel8 / I2C1_EV / I2C1_ER）及各中断优先级。
> 进不了 `.ioc` 的只有：I2C 引脚内部上拉（CubeMX 强制 NOPULL）、HRTIM 比较值预载、
> ADC DMA 长度（运行时参数）。
> 过流保护**不再用 ADC 模拟看门狗**（写入时机容易被静默跳过），改在 `protect.c`
> 用 21.25 kHz 软件限流 + 持续越限跳闸，阈值全在 `App/params.h`。

退出码：`0` = 成功；非 0 通常是错误数/警告数，或 1 = 警告、2 = 错误（以日志为准）。

## 4. 烧录（可选，需要接 ST-Link）

```powershell
Start-Process -FilePath 'E:\Embeded\Keil_v5\UV4\UV4.exe' `
  -ArgumentList '-f','MDK-ARM\SuperCapV3.uvprojx','-j0','-o','flash.log' -Wait -PassThru
```

## 5. 代码修改约定

- 应用层在 [`App/`](App/)（`Program/` 已在控制软件重写中删除）：
  - `params.h` —— **所有可调参数的唯一来源**，改阈值只改这里；
  - `ctrl_core.c` —— 三环级联（缓冲环 / 功率环 / 电流环）+ 电压管理；
  - `modulator.c` —— `u → D_A/D_B`，保证 `V_L = u·V_A`；
  - `sample.c` —— ADC/DMA 16 点均值、换算、`I_L` 重构、ADC/DMA 中断入口；
  - `ctrl_isr.c` —— 21.25 kHz 控制中断 + 1 kHz 任务 + 节拍心跳；
  - `protect.c` —— 软件限流（取代 AWD）/ L1·L2 保护 / 故障锁存；
  - `comm_can.c` / `comm_uart.c` / `ui_oled.c` / `calib.c` / `debug.c`；
  - 设计依据见 [`Doc/开发文档-控制软件重写.md`](Doc/开发文档-控制软件重写.md)。
- `App/` 不参与 CubeMX 代码生成，重新生成 `.ioc` 不会覆盖；
- `Core/` 是 CubeMX 生成代码，只改 `USER CODE BEGIN/END` 之间的内容；
  **能配在`.ioc` 里的一律不改生成代码**——死区、I2C 时序、NVIC 全部由 `.ioc` 决定，
  生成代码里的对应数值只是 `.ioc` 的投影，一致性由 `Tools/check_params.py` 的 A21~A23 校验；
- 改完必须跑第 2 节的命令确认 0 Error / 0 Warning；
- 禁止把 `MDK-ARM/*/`（编译产物）、`*.uvoptx`、`*.uvguix.*` 提交进仓库。

## 6. 当前固件状态

- 控制软件重写 **P0 已完成**（2026-10-27）：`App/` 层落地，`Program/` 删除，编译 0E0W；
- 通信看门狗：500 ms 无控制帧关 PWM（**不再自复位**，见 `CFG_RESET_ON_COMM_LOSS`）；
- **CAN 总线自愈**：收发器（TJA1044）VCC 是 5 V，低压/上电顺序不当时总线无 ACK，
  FDCAN 会 bus-off + TX 队列堵死。`Comm_Can_Monitor()`（主循环）监视 `PSR.BO/EP` 与
  TX 队列，异常持续 100 ms 就 `Stop+Start` 一次（≤5 次/s），异常期间不同队列里塞帧；
- 独立看门狗：1 s，**在 1 kHz 中断里喂**（主循环卡住不复位），调试器 halt 时冻结；
- 过流保护：**ADC 模拟看门狗已移除**，改 21.25 kHz 软件限流
  （`I_LIM_SOFT`=15 A 压制占空比 / `OC_TRIP_A`=16.5 A 持续 `OC_TRIP_TICKS` 拍才跳闸），
  调试 CSV 的 `Lim` 列就是限流状态；
- 控制节拍心跳 `IsrTick` **允许自然回绕**（饱和会误判丢拍，上电 3 s 锁死，已修）；
  丢拍现在是**活故障**，节拍恢复即自动清除；
- 传感器合理性检查有 `SEN_GRACE_MS` 宽限 + `SEN_TRIP_MS` 防抖，Enable 上升沿可清；
- 硬锁 `FLT_LOCK` 只统计 `FLT_OC | FLT_OV | FLT_SENSOR`，且**只在 `Ctrl.State` 为
  `ST_RUN`/`ST_SOFT_START` 时计数**：离线（`V_A<18V`）、节拍丢失、空载状态下的跳闸
  都不再累成永久锁（2026-10-28 实测 F80 误锁的根因）；跳闸现场留在
  `Protect_Ev0..3{Bits,Ms,VA,IL}`，可用 EmberProbe 直接读；
- **台架坑（2026-10-28）**：CAP 侧**不要用电子负载的 CV 模式**。它的调节环会和变换器
  对打，表现为 ~1 kHz、脉宽 275 µs 的电流脉冲串（`I_L` 0.3~4.5 A 跳、`V_A/V_B` 却很稳），
  而控制器的 `u / D_A / D_B / I_L_Ref / Trim` 全程不动 —— 极容易被误当成环路振荡。
  判断方法：看控制器输出是否跟着动；断开负载脉动即消失。台架请用 CC/CR 或电池。
- OLED 热插拔容错：`i2c_guard.c` 负责内部上拉 / **DMA 发送（整页 129 字节不阻塞主循环）** /
  超时判离线 / 9 时钟总线恢复；
  屏掉线只影响显示，不影响功率控制（控制全在 21.25 kHz 中断里）；
- 调试串口**默认关闭**（`CFG_DEBUG_UART=0`）；需要 P1 台架验收时改成 1。
  CSV 打的是 **1 ms 块平均值**（`Smp.*_Avg`），与控制环用的瞬时值不同；
- P1~P9 为硬件在环验收，步骤见 `Doc/开发文档-控制软件重写.md` §13 / §15.3。

## 7. 改动展示约定（用户偏好）

用户**在 VS Code / Copilot 里 review 改动**，不在对话里看整段 diff。因此：

1. 改完文件**不自动提交**，改动留在工作区——用户在编辑器的源代码管理 / Copilot 改动视图里直接看 diff；
2. 回复里默认**不要**整段贴 `git diff`，只在用户明确要求时贴；平时用 1–3 行说明改了哪个文件、改了什么、影响什么；
3. 只有破坏性变更（改接口、删文件、切工作模式、改保护阈值）才贴关键片段让用户确认；
4. 需要用户提交时，给出命令即可，不要代跑 `git commit`。
