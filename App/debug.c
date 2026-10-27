/**
  ******************************************************************************
  * @file    debug.c
  * @brief   工程量串口输出（10 Hz，CSV 一行）
  *
  *  P1 验收方法：
  *    1. 串口 115200 打开，观察 V_A / V_B 与万用表比对（误差应 < 1%）
  *    2. 钳表卡在 R77 上量 I_Ref，与打印值比对
  *    3. 静置时 I_A / I_B / I_Ref 的零点应接近 0（标定表命中时 < 50 mA）
  *    4. I_Ch 应等于 I_Ref - I_A
  *    5. I_L 与 I_B/D_B、I_A/D_A 应基本一致
  *
  *  注意：printf 是阻塞的（115200 下每行约 8 ms），只能放在主循环里。
  *
  *  **CSV 打的是 1 ms 块平均值（Smp.*_Avg）**，与控制环用的瞬时值不同：
  *  瞬时值是“开关周期内某个相位”的点采样，逐拍起伏十几瓦；直接用它会
  *  把开关纹波当成功率脉动，误导台架分析。
  ******************************************************************************
  */
#include "debug.h"
#include "sample.h"
#include "ctrl_core.h"
#include "protect.h"

static volatile uint8_t PrintReq;
static uint8_t           HeaderDone;

void Debug_Init(void)
{
    PrintReq   = 0;
    HeaderDone = 0;
}

void Debug_Request(void)
{
    PrintReq = 1;
}

void Debug_Loop(void)
{
#if (CFG_DEBUG_UART == 1)
    if (!PrintReq) { return; }
    PrintReq = 0;

    if (!HeaderDone)
    {
        HeaderDone = 1;
        printf("VA,VB,IRef,IA,ICha,IB,IL,PRef,PA,PB,PCha,PTgt,DA,DB,u,VL,uVA,St,Fl,Lim\r\n");
    }

    /* VL   = 实际电感电压 D_A*V_A − D_B*V_B
       uVA  = 调制器承诺的 u*V_A
       P2 验收就看这两列是否相等（差 < 1% 说明调制器线性度达标） */
    {
        float vl  = Ctrl.D_A * Smp.V_A - Ctrl.D_B * Smp.V_B;
        float uva = Ctrl.u * Smp.V_A;

        printf("%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"
               "%.1f,%.1f,%.1f,%.1f,%.1f,"
               "%.3f,%.3f,%.4f,%.4f,%.4f,%d,%02X,%d\r\n",
               Smp.V_A, Smp.V_B, Smp.I_Ref_Avg,
               Smp.I_A_Avg, (Smp.I_Ref_Avg - Smp.I_A_Avg), Smp.I_B_Avg, Smp.I_L_Avg,
               Smp.P_Ref_Avg, Smp.P_A_Avg, Smp.P_B_Avg, Smp.P_Cha_Avg, Ctrl.P_Tgt,
               Ctrl.D_A, Ctrl.D_B, Ctrl.u, vl, uva,
               (int)Ctrl.State, (unsigned)Protect_Get(), (int)Protect_CurLimit());
    }
#endif
}
