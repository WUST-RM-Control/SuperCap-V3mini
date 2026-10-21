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

> 基线（2026-10-21，受控模式，`IF_USED_AS_BAT_BUFFER = 0`）：
> `Code=38828 RO-data=2736 RW-data=20 ZI-data=4756`，0 Error 0 Warning。
> Code 大幅变化时先确认是不是误改了 `#if(IF_USED_AS_BAT_BUFFER)` 分支。

退出码：`0` = 成功；非 0 通常是错误数/警告数，或 1 = 警告、2 = 错误（以日志为准）。

## 4. 烧录（可选，需要接 ST-Link）

```powershell
Start-Process -FilePath 'E:\Embeded\Keil_v5\UV4\UV4.exe' `
  -ArgumentList '-f','MDK-ARM\SuperCapV3.uvprojx','-j0','-o','flash.log' -Wait -PassThru
```

## 5. 代码修改约定

- 工作模式开关只有一处：[`Program/SuperCap_Control.c:3`](Program/SuperCap_Control.c#L3) 的 `IF_USED_AS_BAT_BUFFER`
  - `0` = 受控模式（CAN `0x30` / UART 帧生效，`0x31` 反馈使能）——**当前**
  - `1` = 电池缓冲模式（100 W 恒功率，无需上位机）
  - 两种模式的私有参数：`MAX_B_Current` 14 A / 20 A，`Chassis_Offline_V` 19 V / 14 V，`Bat_Buffer_P` 仅 1 时定义。
- `Program/` 不参与 CubeMX 代码生成，重新生成 `.ioc` 不会覆盖；
- `Core/` 是 CubeMX 生成代码，只改 `USER CODE BEGIN/END` 之间的内容；
- 改完必须跑第 2 节的命令确认 0 Error / 0 Warning；
- 禁止把 `MDK-ARM/*/`（编译产物）、`*.uvoptx`、`*.uvguix.*` 提交进仓库。

## 6. 当前固件状态

- 工作模式：受控模式（宏 = 0），提交 `041b3da`（2026-10-21）；
- 通信看门狗：500 ms 无控制帧关 PWM，`Enable_Flag=0` 持续 1 s → 芯片复位（受控模式下无上位机会周期复位，属预期）；
- 已知问题清单见 [README.md 第 13.2 节](README.md)。
