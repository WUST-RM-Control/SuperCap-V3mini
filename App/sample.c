/**
  ******************************************************************************
  * @file    sample.c
  * @brief   ADC/DMA 采样层
  *
  *  通道映射（网表核对）：
  *    ADC1 PA3  A_POWER_V_ADC   -> V_A   (OPAMP1)
  *    ADC2 PA7  A_POWER_I_ADC   -> I_A   (OPAMP2, R6)
  *    ADC3 PB0  REFEREE_I_ADC   -> I_Ref (OPAMP3, R77)
  *    ADC4 PB13 B_POWER_I_ADC   -> I_B   (OPAMP6, R79)
  *    ADC5 PB14 B_POWER_V_ADC   -> V_B   (OPAMP5)
  *
  *  过流保护不在这里：ADC 模拟看门狗(AWD)已取消，改由 protect.c 在
  *  21.25 kHz 控制中断里做软件限流 + 持续越限跳闸（OC_TRIP_TICKS 拍）。
  *  这样阈值/回差全部可在 params.h 里改，不依赖写入时机。
  ******************************************************************************
  */
#include "sample.h"
#include "calib.h"

Sample_t Smp;

volatile uint16_t Sample_Raw[CH_NUM][N_AVG];

/* adc.c 中为全局句柄，可直接引用 */
extern DMA_HandleTypeDef hdma_adc1;

/* 电感电流重构时占空比的下限，避免小占空比把噪声放大 */
#define D_MIN_EST       0.25f

/* 显示用块平均的窗口（拍）：21 拍 ≈ 1 ms，
   正好是台架上“恒压负载与变换器对打”产生的 1 kHz 脉动的一个整周期。 */
#define SAMPLE_AVG_N    21u

void Sample_Init(void)
{
    uint8_t i;

    for (i = 0; i < CH_NUM; i++)
    {
        uint8_t k;
        for (k = 0; k < N_AVG; k++) { Sample_Raw[i][k] = 0; }
    }

    /* 按芯片 UID 载入板级标定 */
    Calib_Load(HAL_GetUIDw0());

    /* ADC 自校准 */
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED);

    /* OPAMP 跟随器（内部输出接 ADC 通道）。必须先起并等前端稳定，
       否则运放输出悬空时采样值是假的。 */
    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_Start(&hopamp2);
    HAL_OPAMP_Start(&hopamp3);
    HAL_OPAMP_Start(&hopamp5);
    HAL_OPAMP_Start(&hopamp6);
    HAL_Delay(5);

    /* ADC1 的 DMA 传输完成中断 = 21.25 kHz 控制节拍。
       NVIC 使能由 CubeMX 按 .ioc 生成在 HAL_DMA_MspInit 里，这里不再重复。 */

    /* 16 点循环 DMA，由 HRTIM TRG1/TRG2 触发 */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)Sample_Raw[CH_VA],   N_AVG);
    HAL_ADC_Start_DMA(&hadc2, (uint32_t *)Sample_Raw[CH_IA],   N_AVG);
    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)Sample_Raw[CH_IREF], N_AVG);
    HAL_ADC_Start_DMA(&hadc4, (uint32_t *)Sample_Raw[CH_IB],   N_AVG);
    HAL_ADC_Start_DMA(&hadc5, (uint32_t *)Sample_Raw[CH_VB],   N_AVG);
}


uint16_t Sample_TickRestartCnt;

/**
  * @brief 重新武装 ADC1 -> DMA1_Channel1（即控制节拍源）
  *
  *        HAL 没有任何自愈逻辑，而下面三种情况都会让节拍**永久**消失、且 ADC
  *        自己一个错误都不报（已在硬件上实测到：ADC1/2/3 的原始缓冲全冻住，
  *        ADC4/5 照常更新，hadc1.ErrorCode 一直是 0）：
  *          1. 过采样(OVR)：DATA_PRESERVED 模式下 ADC 停在那里等 OVR 被清，
  *             OVR 中断没被处理就再也不会清 -> 永久停转；
  *          2. DMA1_Channel1 停在异常状态后再也没被重新挂起；
  *          3. 触发源（HRTIM TRG1 = TIMER_A CMP4）停摆后 ADC 空等。
  *        所以这里做一次完整的“停机 + 清状态 + 重挂”，三类都能救回来。
  */
void Sample_Restart_Tick(void)
{
    (void)HAL_ADC_Stop_DMA(&hadc1);
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_OVR | ADC_FLAG_EOC | ADC_FLAG_EOS);

    /* Stop_DMA 只在 DMA 处于 BUSY 时才 Abort，异常状态要手动归位 */
    hdma_adc1.State = HAL_DMA_STATE_READY;
    hadc1.State     = HAL_ADC_STATE_READY;
    hadc1.ErrorCode = HAL_ADC_ERROR_NONE;

    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);      /* 兜底：中断使能位被意外清掉 */
    (void)HAL_ADC_Start_DMA(&hadc1, (uint32_t *)Sample_Raw[CH_VA], N_AVG);

    if (Sample_TickRestartCnt < 0xFFFFu) { Sample_TickRestartCnt++; }
}


/**
  * @brief 16 点块平均（返回 0..4095 的码值）
  */
uint16_t Sample_BlockMean(uint8_t ch)
{
    uint32_t sum = 0;
    uint8_t k;
    for (k = 0; k < N_AVG; k++) { sum += Sample_Raw[ch][k]; }
    return (uint16_t)(sum / N_AVG);
}

/**
  * @brief 均值 + 量纲换算（在 21.25 kHz 控制中断里调用）
  */
void Sample_Update(const float D_A, const float D_B)
{
    uint16_t code[CH_NUM];
    uint8_t i;
    float da, db;

    for (i = 0; i < CH_NUM; i++)
    {
        code[i] = Sample_BlockMean(i);
        if (code[i] >= OC_ADC_SAT) { Smp.sat |= (uint16_t)(1u << i); }
        else                       { Smp.sat &= (uint16_t)~(1u << i); }
    }

    /* 电压：raw * (3.3/4096 * 11) * kV */
    Smp.V_A = (float)code[CH_VA] * (ADC_VREF / ADC_FULL * ADC_V_GAIN) * Power_A_V_Adjust;
    Smp.V_B = (float)code[CH_VB] * (ADC_VREF / ADC_FULL * ADC_V_GAIN) * Power_B_V_Adjust;

    /* 电流：(raw*3.3/4096 - 1.65) * 12.5 + kI */
    Smp.I_A   = ((float)code[CH_IA]   * (ADC_VREF / ADC_FULL) - I_SENS_ZERO) * I_SENS_GAIN
                + Power_A_I_Adjust;
    Smp.I_B   = ((float)code[CH_IB]   * (ADC_VREF / ADC_FULL) - I_SENS_ZERO) * I_SENS_GAIN
                + Power_B_I_Adjust;
    Smp.I_Ref = ((float)code[CH_IREF] * (ADC_VREF / ADC_FULL) - I_SENS_ZERO) * I_SENS_GAIN
                + Referee_I_Adjust;

    Smp.I_Cha = Smp.I_Ref - Smp.I_A;

    Smp.P_Ref = Smp.V_A * Smp.I_Ref;
    Smp.P_A   = Smp.V_A * Smp.I_A;
    Smp.P_Cha = Smp.V_A * Smp.I_Cha;
    Smp.P_B   = Smp.V_B * Smp.I_B;

    /* 电感电流重构：I_A = D_A*I_L, I_B = D_B*I_L -> 双路平均 */
    da = (D_A > D_MIN_EST) ? D_A : D_MIN_EST;
    db = (D_B > D_MIN_EST) ? D_B : D_MIN_EST;
    Smp.I_L = 0.5f * (Smp.I_A / da + Smp.I_B / db);

    /* ---- 显示用块平均（≈1 ms 窗口）----
       只在“每拍累加 + 每 N 拍一次除法”，代价可忽略。 */
    {
        static float aPRef, aPA, aPB, aPCha, aIA, aIB, aIRef, aIL;
        static uint8_t n;

        aPRef += Smp.P_Ref;
        aPA   += Smp.P_A;
        aPB   += Smp.P_B;
        aPCha += Smp.P_Cha;
        aIA   += Smp.I_A;
        aIB   += Smp.I_B;
        aIRef += Smp.I_Ref;
        aIL   += Smp.I_L;

        if (++n >= (uint8_t)SAMPLE_AVG_N)
        {
            const float k = 1.0f / (float)SAMPLE_AVG_N;
            Smp.P_Ref_Avg = aPRef * k;
            Smp.P_A_Avg   = aPA   * k;
            Smp.P_B_Avg   = aPB   * k;
            Smp.P_Cha_Avg = aPCha * k;
            Smp.I_A_Avg   = aIA   * k;
            Smp.I_B_Avg   = aIB   * k;
            Smp.I_Ref_Avg = aIRef * k;
            Smp.I_L_Avg   = aIL   * k;
            aPRef = 0.0f; aPA = 0.0f; aPB = 0.0f; aPCha = 0.0f;
            aIA = 0.0f; aIB = 0.0f; aIRef = 0.0f; aIL = 0.0f;
            n = 0u;
        }
    }
}
