/**
  ******************************************************************************
  * @file    sample.h
  * @brief   ADC/DMA 采样层：16 点块平均、量纲换算、电感电流重构
  ******************************************************************************
  */
#ifndef __SAMPLE_H
#define __SAMPLE_H

#include "main.h"
#include "params.h"

typedef struct
{
    /* 工程量 */
    float V_A;      /* A 侧母线电压 (V) */
    float V_B;      /* B 侧母线（电容侧）电压 (V) */
    float I_Ref;    /* 电管输出电流 (A)，R77 */
    float I_A;      /* 流入 BuckBoost A 侧电流 (A)，R6 */
    float I_B;      /* 电容组电流 (A)，R79 */
    float I_Cha;    /* 底盘电流 = I_Ref - I_A (A) */

    float P_Ref;    /* 电管功率 (W) —— 被控量 */
    float P_A;      /* BuckBoost A 侧功率 (W) */
    float P_B;      /* 电容功率 (W)，正=充电 */
    float P_Cha;    /* 底盘功率 (W) */

    float I_L;      /* 重构电感电流 (A) */

    /* ---- 1 ms 块平均（只给显示/上报，控制环不用）----
       上面那些量是“开关周期里某个相位”的点采样，逐拍起伏很大
       （实测同一秒内 P_Ref 在 3.6~47 W 之间跳，而电源读数稳定）。
       每拍累加、每 SAMPLE_AVG_N 拍取一次均值，窗口 ≈1 ms：既把 1 kHz 脉动
       整周期抵消，又只花“每拍一次加法”，不加重 21.25 kHz 中断负担。 */
    float I_A_Avg, I_B_Avg;
    float I_Ref_Avg, I_L_Avg;
    float P_Ref_Avg, P_A_Avg, P_B_Avg, P_Cha_Avg;

    uint16_t sat;   /* 位掩码：某通道 ADC 码饱和 */
} Sample_t;

extern Sample_t Smp;

/* DMA 目标缓冲：[通道][N_AVG] */
extern volatile uint16_t Sample_Raw[CH_NUM][N_AVG];

void Sample_Init(void);                       /* 自校准 + 启动 5 路 DMA */
void Sample_Update(const float D_A, const float D_B);  /* 均值 + 换算（控制中断内） */
uint16_t Sample_BlockMean(uint8_t ch);

/* 重新武装 ADC1 -> DMA1_Channel1 控制节拍（节拍丢失时由 1 kHz 守卫调用） */
void Sample_Restart_Tick(void);

/* 节拍重启次数，供 EmberProbe / 诊断查看：一直在涨说明触发源没回来 */
extern uint16_t Sample_TickRestartCnt;

#endif /* __SAMPLE_H */
