/**
  ******************************************************************************
  * @file    protect.h
  * @brief   保护层：L0(ADC 看门狗) / L1(每拍) / L2(1 kHz) + 故障锁存
  ******************************************************************************
  */
#ifndef __PROTECT_H
#define __PROTECT_H

#include "main.h"
#include "params.h"

/* 故障位 */
#define FLT_OC       0x0001u   /* 过流：>=18A 或 ADC 饱和 */
#define FLT_OV       0x0002u   /* 母线过压 V_A > 32V */
#define FLT_HW_OV    0x0004u   /* 电容物理极限 26.5V（仅上报，PWM 保持以继续泄放） */
#define FLT_OFFLINE  0x0008u   /* 底盘离线 V_A < 18V */
#define FLT_SENSOR   0x0010u   /* 保留位：传感器合理性检查已删除，不再置位 */
#define FLT_COMM     0x0020u   /* 通信超时 */
#define FLT_TICK     0x0040u   /* 控制节拍丢失（ADC DMA 停了） */
#define FLT_LOCK     0x0080u   /* 硬锁：窗口内跳闸太多次，只能断电复位 */

/* 需要 PWM 立即关断的故障 */
#define FLT_TRIP_MASK  (FLT_OC | FLT_OV | FLT_OFFLINE | FLT_SENSOR | FLT_TICK | FLT_LOCK)

/* 计入“窗口内跳闸 N 次 -> 硬锁”的故障。只有**功率级真故障**才配得上硬锁：
     - 不含 FLT_OFFLINE：V_A<18V 是正常工况（母线未上电/没接电池），
       且 18/19V 回差会来回抖 —— 计入会让板上电就永久 HardLock（实测 F80）。
     - 不含 FLT_TICK：节拍丢失是活故障，PWM 已经关断，恢复后自动清除；
       把它硬锁只会把一次 ADC 抖动升级成“必须断电”。 */
#define FLT_HARDLOCK_MASK  (FLT_OC | FLT_OV | FLT_SENSOR)

/* ---- 跳闸事件记录（诊断）----
   HardLock 一旦置位就只能断电复位，现场无法判断是哪一类故障连续触发了 3 次。
   这里保留最近 4 次“跳闸上升沿”的故障字与当时的采样值。
   **刻意用标量而不是数组**：EmberProbe 对本工程里的 volatile 数组拿不到 DWARF
   布局，只能一个标量一个标量地读。 */
extern volatile uint16_t Protect_Ev0Bits, Protect_Ev1Bits, Protect_Ev2Bits, Protect_Ev3Bits;
extern volatile uint32_t Protect_Ev0Ms,   Protect_Ev1Ms,   Protect_Ev2Ms,   Protect_Ev3Ms;
extern volatile float    Protect_Ev0VB,   Protect_Ev1VB,   Protect_Ev2VB,   Protect_Ev3VB;
extern volatile float    Protect_MaxVA,   Protect_MaxVB;   /* 上电以来的电压峰值 */
extern volatile uint8_t  Protect_EvIdx;      /* 下一个写入槽 */
extern volatile uint32_t Protect_Ms;         /* 上电后经过的毫秒数 */

extern volatile uint16_t Protect_Latched;   /* 锁存故障（需 Enable 重上升沿或上电） */

uint16_t Protect_Get(void);                 /* 当前故障位（L1/L2/锁存 合并） */
void     Protect_Set_Latched(uint16_t bits);
void     Protect_Init(void);
void     Protect_Fast(void);                /* 21.25 kHz 中断里调用 */
void     Protect_Slow(void);                /* 1 kHz 调用 */
uint8_t  Protect_IsTripped(void);
void     Protect_Clear_Latched(void);       /* Enable 重新上升沿时清可恢复锁存 */

/* 软件限流：1 = 电流已越 I_LIM_SOFT，电流环只允许往减小 |I| 的方向动。
   它只限制、不跳闸，所以不是故障位。 */
uint8_t  Protect_CurLimit(void);

/* 控制节拍心跳：1 = 21.25 kHz 中断丢了 5 ms 以上。活故障，恢复即自动消失。 */
void     Protect_Set_TickLost(uint8_t lost);

#endif /* __PROTECT_H */
