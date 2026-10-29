/**
  ******************************************************************************
  * @file    debug.c
  * @brief   工程量输出：VOFA+ JustFloat（10 Hz，二进制帧，DMA 发送）
  *
  *  上位机：VOFA+ -> 串口 1000000 8N1 -> 控件选 **JustFloat**。
  *  帧格式：N 个 float（小端 4 字节）+ 帧尾 `00 00 80 7F`（+Inf 小端）。
  *  当前 6 通道（改通道数只能改 `JF_CH_N` + 下面注释）：
  *    CH0 P_Tgt      缓冲环给出的功率目标 (W)
  *    CH1 P_Ref_Avg  电管功率 1 ms 块平均 (W) —— 稳态抖动看这列
  *    CH2 P_Ref      电管功率瞬时（47 us 块平均）(W) —— 瞬态看这列
  *    CH3 P_Cha_Avg  底盘功率 1 ms 块平均 (W)
  *    CH4 I_L_Avg    电感电流 1 ms 块平均 (A)
  *    CH5 V_CAP      电容侧电压 (V)，与 CAN 0x31 的 V_Cap*100 同源
  *
  *  P1 验收方法：
  *    1. V_CAP 与万用表比对（误差应 < 1%）
  *    2. 钳表卡在 R77 上量 I_Ref，与 CH1/CH2 比对
  *    3. 静置时 I_A / I_B / I_Ref 零点应接近 0（标定表命中时 < 50 mA）
  *    4. I_L 与 I_B/D_B、I_A/D_A 应基本一致
  *
  *  注意：本文件不再调 `printf`。`printf` 重定向保留在 `comm_uart.c`，
  *        已改成「行缓冲 + DMA」，但**与 JustFloat 共用同一条 USART2/TX DMA**，
  *        两者不能同时用（文本会插进二进制帧里，VOFA 会解析失败）。
  ******************************************************************************
  */
#include <string.h>

#include "debug.h"
#include "comm_uart.h"
#include "sample.h"
#include "ctrl_core.h"
#include "protect.h"

/* VOFA+ JustFloat 帧尾：+Inf (0x7F800000) 的小端字节序 */
#define JF_TAIL_0   0x00u
#define JF_TAIL_1   0x00u
#define JF_TAIL_2   0x80u
#define JF_TAIL_3   0x7Fu

#define JF_CH_N     (DEBUG_JF_MAX_CH)       /* 上报通道数 = 6 */
#define JF_BUF_SZ   (DEBUG_JF_MAX_CH * 4u + 4u)

static uint8_t          JfBuf[JF_BUF_SZ];
static float             Snap[2][DEBUG_JF_MAX_CH];
static volatile uint8_t  SnapIdx;
static volatile uint8_t  PrintReq;
static volatile uint8_t DropCnt;            /* 上一帧未发完导致的丢帧计数 */

void Debug_Init(void)
{
    PrintReq = 0;
    SnapIdx  = 0;
    DropCnt  = 0;
}

/* 1 kHz 任务里调用：**在本拍把要上报的值锁存下来**（双缓冲）。
   若放到主循环里读，1 kHz 时主循环的调度抖动会直接变成曲线上的噪声。 */
void Debug_Request(void)
{
#if (CFG_DEBUG_UART == 1)
    uint8_t k = (uint8_t)(SnapIdx ^ 1u);

    Snap[k][0] = Ctrl.P_Tgt;          /* CH0 功率目标 (W) */
    Snap[k][1] = Smp.P_Ref_Avg;       /* CH1 电管功率 1 ms 块平均 (W) */
    Snap[k][2] = Smp.P_Ref;           /* CH2 电管功率 47 us 瞬时 (W) */
    Snap[k][3] = Smp.P_Cha_Avg;       /* CH3 底盘功率 1 ms 块平均 (W) */
    Snap[k][4] = Smp.I_L_Avg;         /* CH4 电感电流 1 ms 块平均 (A) */
    Snap[k][5] = Smp.V_B;             /* CH5 电容侧电压 (V) */
    Snap[k][6] = Ctrl.I_L_Ref;        /* CH6 电感电流指令 (A) */
    Snap[k][7] = Ctrl.Trim;           /* CH7 功率环积分修正 (W) */
    Snap[k][8] = Ctrl.u;              /* CH8 占空比增量 */

    /* ---- A1：电流重构拆开看 ----
       怀疑 Smp.I_L = 0.5*(I_A/D_A + I_B/D_B) 在动态时两路会分叉：
       A 侧 D_A 在 0.35~0.8 之间变（放大 2.3 倍），B 侧 D_B≈0.9 几乎不变。
       阶跃时若 CH9 与 CH10 明显分叉，就是重构而不是被控对象的问题。 */
    Snap[k][9]  = Smp.I_A_Avg;        /* CH9  A 侧 R6 的 1ms 平均 (A) = D_A*I_L */
    Snap[k][10] = Smp.I_B_Avg;        /* CH10 B 侧 R79 的 1ms 平均 (A) = D_B*I_L */
    Snap[k][11] = Ctrl.D_A;           /* CH11 A 桥占空比 */
    Snap[k][12] = Ctrl.D_B;           /* CH12 B 桥占空比 */

    /* CH13 实测 V_A。必须用它（而不是从 D_A= c*r+u 反推）来验证调制器：
         V_L = D_A*V_A - D_B*V_B  应等于  L*dI/dt + I*R
       若 V_A 标定有 x% 误差，实际电感电压会多出 c*V_B*(k-1) 这一项
       （k = V_A_real/V_A_meas），1% 就对应 0.12V，足以解释实测的 0.3V 缺口。 */
    Snap[k][13] = Smp.V_A;            /* CH13 A 侧母线电压 (V) */

    SnapIdx  = k;
    PrintReq = 1;
#endif
}

uint8_t Debug_SendJustFloat(const float *v, uint8_t n)
{
    uint8_t *p = JfBuf;
    uint8_t  i;
    uint32_t bits;

    if (v == NULL || n == 0u || n > (uint8_t)DEBUG_JF_MAX_CH) { return 0u; }

    /* 必须先确认上一帧发完再写缓冲：DMA 可能还在读 JfBuf */
    if (Comm_Uart_TxIdle() == 0u) { DropCnt++; return 0u; }

    for (i = 0; i < n; i++)
    {
        memcpy(&bits, &v[i], sizeof(bits));     /* STM32 小端，低字节先发 */
        *p++ = (uint8_t)(bits);
        *p++ = (uint8_t)(bits >> 8);
        *p++ = (uint8_t)(bits >> 16);
        *p++ = (uint8_t)(bits >> 24);
    }
    *p++ = JF_TAIL_0;
    *p++ = JF_TAIL_1;
    *p++ = JF_TAIL_2;
    *p++ = JF_TAIL_3;

    if (Comm_Uart_Send_DMA(JfBuf, (uint16_t)((uint16_t)n * 4u + 4u)) == 0u)
    {
        DropCnt++;
        return 0u;
    }
    return 1u;
}

void Debug_Loop(void)
{
#if (CFG_DEBUG_UART == 1)
    if (!PrintReq) { return; }
    PrintReq = 0;

    /* Snap[SnapIdx] 已由 1 kHz 中断填好；中断只会写另一块缓冲，
       所以这里直接发不用担心被改写。 */
    (void)Debug_SendJustFloat(Snap[SnapIdx], (uint8_t)JF_CH_N);
#endif
}
