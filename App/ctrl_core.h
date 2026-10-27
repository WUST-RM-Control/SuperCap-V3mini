/**
  ******************************************************************************
  * @file    ctrl_core.h
  * @brief   三层级联控制核心：电流环 / 功率环 / 缓冲环 + 状态机
  ******************************************************************************
  */
#ifndef __CTRL_CORE_H
#define __CTRL_CORE_H

#include "main.h"
#include "params.h"

typedef enum
{
    ST_INIT = 0,
    ST_IDLE,          /* 未使能 / 通信丢失，PWM 关 */
    ST_SOFT_START,    /* 使能后 300 ms 斜坡 */
    ST_RUN,
    ST_FAULT          /* 锁存故障 */
} CtrlState_Enum;

typedef struct
{
    /* ---- 上位机下发 ---- */
    uint8_t  Enable_Flag;
    uint8_t  Power_Buff_Energy;    /* 缓冲能量 J (0-60)，协议原字段 */
    uint8_t  Max_Referee_P;        /* 功率限制 W */

    /* ---- 状态机 ---- */
    CtrlState_Enum State;
    uint8_t  PwmOn;
    float    Soft;                 /* 0..1 软启动系数 */

    /* ---- 三级控制器状态 ---- */
    float    E_Buff_f;             /* 滤波后的缓冲能量 */
    float    P_Tgt;                /* 输入功率目标 */
    float    P_Cha_f;              /* 底盘功率滤波（前馈通道） */
    float    P_Ref_f;              /* 电管功率滤波（修正通道） */
    float    V_A_f;                /* 母线电压慢滤波，做增益归一化 */
    float    Trim;                 /* 功率环修正积分 */
    float    u;                    /* 电流环输出（占空比增量） */
    float    e_last;               /* 电流环上一次误差 */

    /* ---- 指令与输出 ---- */
    float    I_L_Ref;              /* 电感电流参考 */
    float    D_A, D_B;

    /* ---- 电压管理标志 ---- */
    uint8_t  Regen;                /* 动能回收中 */
    uint8_t  Ov;                   /* 26.0V 超压：禁充 + 1A 泄放 */
    uint8_t  HwOv;                 /* 26.5V 物理极限：锁存 */
} Ctrl_t;

extern Ctrl_t Ctrl;

void  Ctrl_Init(void);
void  Ctrl_Reset(void);                 /* 清空全部积分/滤波状态 */
void  Ctrl_Step(float Ts);              /* 每个控制中断调用一次 */
void  Ctrl_Loop_Energy(void);           /* 缓冲环，166 Hz 调用 */
void  Ctrl_Apply_ControlFrame(const uint8_t *d);   /* CAN/UART 共用的解包（只写数据） */
void  Ctrl_Set_Enable(uint8_t en);

extern volatile uint16_t Ctrl_CommCooldown;   /* 100 Hz 递增，收到帧清零 */

#endif /* __CTRL_CORE_H */
