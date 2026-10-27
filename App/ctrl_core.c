/**
  ******************************************************************************
  * @file    ctrl_core.c
  * @brief   三层级联控制核心
  *
  *  缓冲环(166Hz)   P_Tgt = clamp(P_Limit - KP_E*(E_TGT - E_Buff), 0, P_Limit+P_OVER)
  *  功率环(21.25k)  P_A_ref = P_Tgt - P_Cha_f + Trim            <- 死拍前馈
  *                  Trim'   = 2π*5*(P_Tgt - P_Ref_f)             <- 只修残差
  *  电流环(21.25k)  du = Kp*(e-e_last) + Ki*e*Ts; u += du       <- 增量式，自带抗饱和
  *
  *  电管接电池，允许轻微倒灌：P_Ref = P_Cha + P_A < 0 才算倒灌，限 -P_REV_MAX。
  *  注意限幅用瞬时 P_Cha，用滤波值会因 80us 滞后在卸载瞬间放过倒灌。
  ******************************************************************************
  */
#include "ctrl_core.h"
#include "sample.h"
#include "modulator.h"
#include "protect.h"

Ctrl_t Ctrl;

volatile uint16_t Ctrl_CommCooldown;

/**
  * @brief 控制帧解包（CAN 0x30 与 UART 前 3 字节共用）
  *        帧格式与旧固件逐字节一致：Enable / Power_Buff_Energy / Max_Referee_P
  *
  * 注意：这里只做"写数据"，不碰状态机/积分器。使能边沿、清锁存全部由
  *       1 kHz 任务处理，避免与 21.25 kHz 控制中断抢占时把控制器状态写坏。
  */
void Ctrl_Apply_ControlFrame(const uint8_t *d)
{
    uint8_t e    = d[1];
    uint8_t plim = d[2];

    if (e > (uint8_t)E_MAX) { e = (uint8_t)E_MAX; }

    Ctrl.Power_Buff_Energy = e;
    Ctrl.Max_Referee_P     = plim;
    Ctrl.Enable_Flag       = d[0];

    Ctrl_CommCooldown = 0;
}

void Ctrl_Set_Enable(uint8_t en)
{
    Ctrl.Enable_Flag = en;
}

/* 一阶低通系数 */
static float lpf_a(float fc, float Ts)
{
    float a = 6.2831853f * fc * Ts;
    if (a > 1.0f) { a = 1.0f; }
    return a;
}

void Ctrl_Init(void)
{
    Ctrl.Enable_Flag       = 0;
    Ctrl.Power_Buff_Energy = (uint8_t)E_TGT;
    Ctrl.Max_Referee_P     = 20;          /* 与旧版一致的初值，等待上位机下发 */
    Ctrl.State             = ST_IDLE;
    Ctrl.PwmOn             = 0;
    Ctrl.D_A               = 0.90f;       /* 占位：I_L 重构不放大噪声 */
    Ctrl.D_B               = 0.90f;
    Ctrl_Reset();
}

void Ctrl_Reset(void)
{
    Ctrl.Soft      = 0.0f;
    Ctrl.E_Buff_f  = (float)Ctrl.Power_Buff_Energy;
    Ctrl.P_Tgt     = 0.0f;
    Ctrl.P_Cha_f   = 0.0f;
    Ctrl.P_Ref_f   = 0.0f;
    Ctrl.V_A_f     = V_NOM;
    Ctrl.Trim      = 0.0f;
    Ctrl.u         = 0.0f;
    Ctrl.e_last    = 0.0f;
    Ctrl.I_L_Ref   = 0.0f;
    Ctrl.Regen     = 0;
    Ctrl.Ov        = 0;
    Ctrl.HwOv      = 0;
}

/**
  * @brief 缓冲环：166 Hz 调用。被控对象是积分器，P 控即无稳态误差、无需抗饱和。
  */
void Ctrl_Loop_Energy(void)
{
    float e, p_tgt;
    float p_limit = (float)Ctrl.Max_Referee_P;

    e = E_TGT - Ctrl.E_Buff_f;
    p_tgt = p_limit - KP_E * e;

    /* 缓冲告急：优先回充 */
    if (Ctrl.E_Buff_f < E_CRIT) { p_tgt = 0.0f; }

    Ctrl.P_Tgt = CLAMP(p_tgt, 0.0f, p_limit + P_OVER);
}

/**
  * @brief 主控制步：21.25 kHz 调用一次
  */
void Ctrl_Step(float Ts)
{
    float a_cha = lpf_a(LPF_CHA_HZ, Ts);
    float a_ref = lpf_a(LPF_REF_HZ, Ts);
    float a_v   = lpf_a(20.0f, Ts);
    float a_e   = lpf_a(LPF_E_HZ, Ts);

    /* ---------- 1. 测量滤波 ---------- */
    Ctrl.P_Cha_f  += a_cha * (Smp.P_Cha - Ctrl.P_Cha_f);
    Ctrl.P_Ref_f  += a_ref * (Smp.P_Ref - Ctrl.P_Ref_f);
    Ctrl.V_A_f    += a_v   * (Smp.V_A   - Ctrl.V_A_f);
    Ctrl.E_Buff_f += a_e   * ((float)Ctrl.Power_Buff_Energy - Ctrl.E_Buff_f);
    Ctrl.V_A_f     = CLAMP(Ctrl.V_A_f, 10.0f, 40.0f);

    /* ---------- 2. 软启动斜坡 ---------- */
    if (Ctrl.State == ST_SOFT_START)
    {
        Ctrl.Soft += Ts / (SOFT_START_MS * 1e-3f);
        if (Ctrl.Soft >= 1.0f) { Ctrl.Soft = 1.0f; Ctrl.State = ST_RUN; }
    }
    else if (Ctrl.State == ST_RUN)
    {
        Ctrl.Soft = 1.0f;
    }
    else
    {
        Ctrl.Soft = 0.0f;
    }

    /* ---------- 3. 功率环：死拍前馈 + 慢积分修正 ---------- */
    {
        float p_a_ref = (Ctrl.P_Tgt - Ctrl.P_Cha_f) + Ctrl.Trim;

        /* 倒灌限幅：P_Ref = P_Cha + P_A >= -P_REV_MAX（用瞬时 P_Cha） */
        float lim_low = -(Smp.P_Cha + P_REV_MAX);
        if (p_a_ref < lim_low) { p_a_ref = lim_low; }

        /* 修正积分：只补损耗/标定残差；软启动期间禁止（否则会上电冲高预充电流） */
        if (Ctrl.Soft >= 1.0f && Ctrl.State == ST_RUN)
        {
            Ctrl.Trim += KI_PWR * (Ctrl.P_Tgt - Ctrl.P_Ref_f) * Ts;
            Ctrl.Trim  = CLAMP(Ctrl.Trim, -TRIM_MAX, TRIM_MAX);
        }

        /* ---------- 4. 电压管理 ---------- */
        {
            float i_b_ref, lim, k_norm, k_regen, k_dis;
            float v_b = Smp.V_B;

            /* 动能回收判定（±2 W 回差） */
            if      (Ctrl.P_Cha_f < -2.0f) { Ctrl.Regen = 1; }
            else if (Ctrl.P_Cha_f >  2.0f) { Ctrl.Regen = 0; }

            k_norm  = CLAMP((V_CHG_MAX   - v_b) / V_TAP_CHG,   0.0f, 1.0f);
            k_regen = CLAMP((V_CHG_REGEN - v_b) / V_TAP_REGEN, 0.0f, 1.0f);
            k_dis   = CLAMP((v_b - V_DIS_MIN) / V_TAP_DIS,     0.0f, 1.0f);

            i_b_ref = p_a_ref / (v_b > 3.0f ? v_b : 3.0f);

            if (i_b_ref > 0.0f) { lim = I_B_MAX * (Ctrl.Regen ? k_regen : k_norm); }
            else                { lim = I_B_MAX * k_dis; }

            /* 超压：26.0 触发 / 25.8 释放 */
            if      (v_b >= V_OV_TRIP)              { Ctrl.Ov = 1; }
            else if (v_b <  V_OV_TRIP - V_OV_HYST)  { Ctrl.Ov = 0; }

            /* 物理极限：26.5 锁存 / 25.5 解锁 */
            if      (v_b >= V_HW_MAX)               { Ctrl.HwOv = 1; }
            else if (v_b <= V_OV_UNLOCK)            { Ctrl.HwOv = 0; }

            /* 主动泄放：ov 与 hw_ov 共同驱动（hw_ov 必须灌放到 25.5 才停） */
            if (Ctrl.Ov || Ctrl.HwOv)
            {
                lim      = I_DUMP;
                i_b_ref  = -I_DUMP;
            }

            i_b_ref = CLAMP(i_b_ref, -lim, lim);

            /* 深放电恒流预充 */
            if (v_b < V_DIS_MIN && i_b_ref > I_PRE) { i_b_ref = I_PRE; }

            /* 换算到电感电流：I_B = D_B * I_L（用上一拍的 D_B） */
            Ctrl.I_L_Ref = i_b_ref / (Ctrl.D_B > DMIN ? Ctrl.D_B : DMIN);
            Ctrl.I_L_Ref = CLAMP(Ctrl.I_L_Ref, -I_L_MAX, I_L_MAX) * Ctrl.Soft;
        }
    }

    /* ---------- 5. 电流环（增量式，限幅作用在状态上 => 自带抗饱和） ---------- */
    {
        float e_n = (Ctrl.I_L_Ref - Smp.I_L) * (V_NOM / Ctrl.V_A_f);
        float du  = KP_CUR * (e_n - Ctrl.e_last) + KI_CUR * e_n * Ts;

        Ctrl.e_last = e_n;

        /* 软件限流（取代 ADC 看门狗的那一级）：一旦 protect.c 判定越限，
           本拍 du 至少要按 U_LIM_BACK 往"减小 |I_L|"的方向走，
           哪怕误差项还在要求继续加流。到位后 u 自然回退，47us 一拍。 */
        if (Protect_CurLimit())
        {
            float back = (Smp.I_L >= 0.0f) ? -U_LIM_BACK : U_LIM_BACK;
            if ((back < 0.0f) ? (du > back) : (du < back)) { du = back; }
        }

        Ctrl.u = CLAMP(Ctrl.u + du, -U_LIM, U_LIM);
    }

    /* ---------- 6. 调制器 ---------- */
    Modulator_Compute(Smp.V_A, Smp.V_B, Ctrl.u, &Ctrl.D_A, &Ctrl.D_B);
}
