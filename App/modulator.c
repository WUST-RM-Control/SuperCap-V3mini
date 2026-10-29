/**
  ******************************************************************************
  * @file    modulator.c
  * @brief   四开关 Buck-Boost 调制器
  *
  *  核心恒等式（见开发文档 §5）：
  *      buck  : D_B = c,        D_A = c*r + u      ->  V_L = u*V_A
  *      boost : D_A = c,        D_B = (c - u)/r    ->  V_L = u*V_A
  *  两式都精确成立，故任意凸组合也精确成立 -> 平权混合实现无扰切换。
  *
  *  c(r) 在 r≈1 处下压到 DMAX-DH，为充电/放电两个方向各留 DH 的占空比裕量
  *  （否则 r→1 时 D_B 顶在 0.95，充电方向裕量为 0）。
  ******************************************************************************
  */
#include "modulator.h"

/* HRTIM 周期计数值（与 hrtim.c 中 Period 一致） */
#define HRTIM_PERIOD    16000u
#define HRTIM_HALF      8000u

/* 比较值合法窗口：RM 硬性要求 96 ≤ CMP ≤ 65503，否则比较事件**不产生**。
   低于 96 时不是“提早一点”，而是那一个事件根本不存在（实测：
   CMP4 写成 16，ADC 触发 TRG1 直接消失、ADC1/2/3 静默停转）。 */
#define HRTIM_CMP_MIN   96u

static uint32_t Modulator_Cmp_Clamp(uint32_t v)
{
    if (v < HRTIM_CMP_MIN)          { return HRTIM_CMP_MIN; }
    if (v > (HRTIM_PERIOD - 1u))    { return HRTIM_PERIOD - 1u; }
    return v;
}

/**
  * @brief 算 CMP4（兼作 ADC 触发点）：**始终取导通窗口中点**，并且落在合法窗口内。
  *
  *        两个硬约束：
  *        1) CMP4 < 96 计数时比较事件根本不产生 -> TRG1(ADC1/2/3) 或
  *           TRG2(ADC4/5) 永久停摆，而 ADC 不会报任何错误（实测：ADC1/2/3
  *           的原始缓冲全冻住、ADC4/5 还在跑、hadc1.ErrorCode 恒为 0）。
  *           控制节拍就靠这个触发源，丢了 PWM 立刻关断、屏幕数字全部冻住。
  *           NaN 强转成 uint32 也得 0，一并兑住。
  *        2) **不能**在 D 跨 0.5 时切到“关断窗口中点”。旧版是
  *           `D<0.5 ? D·8000 : 8000+D·8000`，而电流通道测的是**斩波后**的
  *           支路电流（关断窗口里≈ 0）。本板 r = V_B/V_A ≈ 0.5，
  *           D_A = c·r + u ≈ 0.475 + u 正好骑在 0.5 上：u 一抖 CMP4 就在
  *           两个窗口之间跳 —— 实测 I_A 在 0.2~2 A 之间来回漂、P_Ref 跟着乱跳。
  */
static uint32_t Modulator_Cmp4(uint32_t cmp3)
{
    return Modulator_Cmp_Clamp(cmp3 / 2u);
}

void Modulator_Compute(float V_A, float V_B, float u, float *D_A, float *D_B)
{
    float r, c, w, da_b, db_b, da_q, db_q;

    if (V_A < 1.0f) { *D_A = DMAX; *D_B = DMIN; return; }

    r = V_B / V_A;

    /* 过渡区共同占空比下压，制造双向裕量 */
    {
        float d = 1.0f - ((r > 1.0f) ? (r - 1.0f) : (1.0f - r)) / MOD_RB;
        if (d < 0.0f) { d = 0.0f; }
        c = DMAX - MOD_DH * d;
    }

    /* buck / boost 两个调制器 */
    da_b = c * r + u;
    db_b = c;
    da_q = c;
    db_q = (r > 1e-3f) ? ((c - u) / r) : DMAX;

    /* 混合权重：r<0.95 -> 0，r>1.05 -> 1 */
    w = (r - (1.0f - MOD_BLEND * 0.5f)) / MOD_BLEND;
    if (w < 0.0f) { w = 0.0f; }
    if (w > 1.0f) { w = 1.0f; }

    *D_A = (1.0f - w) * da_b + w * da_q;
    *D_B = (1.0f - w) * db_b + w * db_q;

    /* D_A 的下限不能用固定的 DMIN：V_B≈0 时前馈 c*r 也≈0，抬到 0.05 会凭空
       产生 1.3V 的电感电压，而且钳位后 u 再也调不回来（V_L=u*V_A 不成立），
       电流环直接失去调节权限 -> 几百 us 内冲到过流。这里只防止占空比落到 0。 */
    *D_A = CLAMP(*D_A, DMIN_ABS, DMAX);
    *D_B = CLAMP(*D_B, DMIN, DMAX);
}

/**
  * @brief 写比较值。CMP3 决定占空比；CMP4 同时是 ADC 触发点。
  *
  * CMP4 取"较宽导通窗口的中点"：D<0.5 取中点=导通中点，否则取关断中点。
  * 三角波纹路在中点等于周期平均值，与 D 无关。
  */
void Modulator_Apply(float D_A, float D_B)
{
    uint32_t cmp3_a = Modulator_Cmp_Clamp((uint32_t)(D_A * (float)HRTIM_PERIOD));
    uint32_t cmp3_b = Modulator_Cmp_Clamp((uint32_t)(D_B * (float)HRTIM_PERIOD));

    /* A 桥 */
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_1, 0);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_3, cmp3_a);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_4,
                           Modulator_Cmp4(cmp3_a));

    /* B 桥 */
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_1, 0);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_3, cmp3_b);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_4,
                           Modulator_Cmp4(cmp3_b));
}

void HRTIM_Output_Enable(void)
{
    HAL_HRTIM_WaveformOutputStart(&hhrtim1,
        HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
}

void HRTIM_Output_Disable(void)
{
    HAL_HRTIM_WaveformOutputStop(&hhrtim1,
        HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
}

void HRTIM_Counter_Start(void)
{
    HAL_HRTIM_WaveformCounterStart(&hhrtim1,
        HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B);
}

/**
  * @brief 打开 CMP 预载 + 重复事件更新
  *
  * 由 CubeMX 生成的 hrtim.c 里 PreloadEnable = DISABLED，意味着在主循环任意时刻
  * 写 CMP3：若计数器已越过新值，本周期不会复位 -> 偶发整周期 100% 占空比脉冲。
  * .ioc 里没有对应选项，故在 USER CODE 之外用寄存器补上（重新生成代码不会丢）。
  */
void HRTIM_Preload_Enable(void)
{
    /* PREEN: 比较值预载；TREPU: 在重复事件（RCR=0 即每个周期）更新 */
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].TIMxCR |= (HRTIM_TIMCR_PREEN | HRTIM_TIMCR_TREPU);
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].TIMxCR |= (HRTIM_TIMCR_PREEN | HRTIM_TIMCR_TREPU);
}
