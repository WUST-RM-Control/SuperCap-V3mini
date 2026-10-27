/**
  ******************************************************************************
  * @file    protect.c
  * @brief   保护层
  *
  *  L1  每拍（47us）：软件限流（I_LIM_SOFT 压制）/ 持续越限跳闸（OC_TRIP_A）/ 母线过压
  *  L2  1 kHz      ：底盘离线（18/19V 回差）、母线过压恢复、传感器合理性（带宽限+防抖）
  *  心跳           ：1 kHz 检查 21.25 kHz 是否还在跑（活故障，恢复即清）
  *
  *  **不再使用 ADC 模拟看门狗**：它需要在“无转换进行中”时配置，写入时机稍错
  *  就会静默失效（实测回读 AWD1EN=0），且阀值只能落在寄存器里、不能回差。
  *  现在最快一级是 21.25 kHz 的软件限流：47us 发现越限并压制占空比，
  *  持续 OC_TRIP_TICKS 拍才判故障关断，阀值/回差全部在 params.h 里可调。
  *
  *  立即关断类：过流、母线过压、离线、传感器、节拍丢失
  *  仅上报类  ：FLT_HW_OV（26.5V 物理极限）—— PWM 必须保持以继续 1A 泄放
  *
  *  L1 / L2 分别持有自己的故障字，读取时合并，避免两个中断互相覆盖。
  ******************************************************************************
  */
#include "protect.h"
#include "sample.h"
#include "ctrl_core.h"
#include "modulator.h"

#define OC_SAT_TICKS    43u        /* 2 ms @21.25 kHz */
#define RETRY_WIN_MS    10000u

volatile uint16_t Protect_Latched;

/* 跳闸事件记录（见 protect.h）：定位 HardLock 到底被什么故障数出来的。
   **用标量而非数组**：EmberProbe 对本工程的 volatile 数组没有 DWARF 布局，读不出来。 */
volatile uint16_t Protect_Ev0Bits, Protect_Ev1Bits, Protect_Ev2Bits, Protect_Ev3Bits;
volatile uint32_t Protect_Ev0Ms,   Protect_Ev1Ms,   Protect_Ev2Ms,   Protect_Ev3Ms;
volatile float    Protect_Ev0VB,   Protect_Ev1VB,   Protect_Ev2VB,   Protect_Ev3VB;
volatile float    Protect_MaxVA,   Protect_MaxVB;
volatile uint8_t  Protect_EvIdx;
volatile uint32_t Protect_Ms;

static volatile uint16_t FaultFast;   /* L1 独占 */
static volatile uint16_t FaultSlow;   /* L2 独占 */
static volatile uint8_t  CurLimit;    /* L1：限流压制中（不是故障） */
static volatile uint8_t  TickLost;    /* 1 kHz：控制节拍丢失（活故障） */
static volatile uint8_t  HardLock;    /* 10 s 内 3 次过流 => 硬故障，只能断电 */
static uint16_t SatTicks;
static uint16_t OcTicks;
static uint8_t  TripWas;              /* 上一次跳闸状态（用于数跳闸“边沿”） */
static uint8_t  AnyTripWas;           /* 同上，但覆盖所有跳闸位（含 OFFLINE/TICK），只用于记录 */
static uint8_t  OfflineFlag;
static uint8_t  OvFlag;
static uint16_t OvMs;                 /* L2 过压防抖计数（1 kHz） */
static uint16_t OvFastTicks;          /* L1 过压防抖计数（21.25 kHz） */
static uint16_t RetryCnt;
static uint16_t RetryWinMs;

void Protect_Init(void)
{
    Protect_Latched = 0;
    FaultFast = 0;
    FaultSlow = 0;
    CurLimit  = 0;
    TickLost  = 0;
    HardLock  = 0;
    SatTicks  = 0;
    OcTicks   = 0;
    TripWas   = 0;
    AnyTripWas = 0;
    OfflineFlag = 0;
    OvFlag = 0;
    OvMs = 0;
    OvFastTicks = 0;
    RetryCnt = 0;
    RetryWinMs = 0;
    Protect_EvIdx = 0;
    Protect_Ms = 0;
}

uint16_t Protect_Get(void)
{
    uint16_t f = (uint16_t)(FaultFast | FaultSlow | Protect_Latched);
    if (TickLost) { f |= FLT_TICK; }
    if (HardLock) { f |= FLT_LOCK; }   /* 硬故障：Enable 上升沿也清不掉 */
    return f;
}

uint8_t Protect_CurLimit(void)
{
    return CurLimit;
}

void Protect_Set_TickLost(uint8_t lost)
{
    TickLost = lost ? 1u : 0u;
}

void Protect_Set_Latched(uint16_t bits)
{
    Protect_Latched |= bits;
}

uint8_t Protect_IsTripped(void)
{
    return (uint8_t)((Protect_Get() & FLT_TRIP_MASK) != 0u);
}

void Protect_Clear_Latched(void)
{
    /* Enable 重新上升沿：清掉所有"会持续重新评估"的锁存。
       过流/传感器故障若真存在，几毫秒内会重新锁上，所以清掉是安全的；
       反过来，如果不清，一个上电瞬间的误判就能把整块板永久锁死。 */
    Protect_Latched &= (uint16_t)~(FLT_OC | FLT_SENSOR);
    OcTicks    = 0;
    TripWas    = 0;
    RetryCnt   = 0;
    RetryWinMs = 0;
}

/**
  * @brief L1，每个控制中断调用（21.25 kHz，47us）
  *
  *  两级：
  *    限流：变换器侧最大电流到 I_LIM_SOFT 就置 CurLimit，控制环只允许
  *          往减小 |I| 的方向动；回差 I_LIM_RELEASE 退出——只压制、不跳闸。
  *    跳闸：持续 OC_TRIP_TICKS 拍越过 OC_TRIP_A（或 ADC 码值饱和 2ms）才锁存
  *          FLT_OC 并关 PWM。单点噪声不会把板子打掉。
  *
  *  注意限流只看变换器侧（I_A / I_B / I_L）：I_Ref 是电管→底盘的母线总电流，
  *  开环时底盘电流也会流过 R77，拿它限流会误压制。跳闸仍然保留 I_Ref 判据。
  */
void Protect_Fast(void)
{
    uint16_t f = 0;
    float ia, ib, il, im;
    uint8_t over = 0;

    ia = fabsf(Smp.I_A);
    ib = fabsf(Smp.I_B);
    il = fabsf(Smp.I_L);
    im = (ia > ib) ? ia : ib;
    if (il > im) { im = il; }

    /* ---- 软件限流（取代原 ADC 模拟看门狗） ---- */
    if (im >= I_LIM_SOFT)        { CurLimit = 1; }
    else if (im < I_LIM_RELEASE) { CurLimit = 0; }

    /* ---- 过流跳闸 ---- */
    if (im >= OC_TRIP_A || fabsf(Smp.I_Ref) >= OC_TRIP_A) { over = 1; }

    /* ADC 码值饱和（含 INA240 摆幅上限）单独算 2 ms 防抖 */
    if (Smp.sat & (uint16_t)((1u << CH_IA) | (1u << CH_IREF) | (1u << CH_IB)))
    {
        if (SatTicks < 0xFFFFu) { SatTicks++; }
        if (SatTicks > OC_SAT_TICKS) { over = 1; }
    }
    else { SatTicks = 0; }

    if (over)
    {
        if (OcTicks < 0xFFFFu) { OcTicks++; }
        if (OcTicks >= (uint16_t)OC_TRIP_TICKS) { f |= FLT_OC; }
    }
    else
    {
        OcTicks = 0;
    }

    /* ---- 过压：分两级，避免"关断导致泄放停摆"的死锁 ----
       26.5 V：只置 FLT_HW_OV（不参与跳闸），PWM 必须保持才能继续 1 A 泄放；
       26.8 V：最后防线，才真正关断 PWM。
       如果 26.5 V 就直接关断，泄放随之停止，电压永远降不回 25.5 V，故障永远解不开。

       **必须防抖**：母线电容不可能在 1 拍(47us) 内跳几十伏，单拍越限只能是采样
       毛刺（实测上电头一秒 V_B 通道读到 26.8V 以上四次，间隔~300ms）。 */
    if (Smp.V_A > SAFE_V_REF_MAX || Smp.V_B > V_HW_CUTOFF)
    {
        if (OvFastTicks < 0xFFFFu) { OvFastTicks++; }
        if (OvFastTicks >= (uint16_t)OV_FAST_TICKS) { f |= FLT_OV; }
    }
    else { OvFastTicks = 0; }

    if (Smp.V_B >= V_HW_MAX) { f |= FLT_HW_OV; }

    /* 峰值追踪：判断越限是“单点毛刺”还是“持续高压”最直接的证据 */
    if (Smp.V_A > Protect_MaxVA) { Protect_MaxVA = Smp.V_A; }
    if (Smp.V_B > Protect_MaxVB) { Protect_MaxVB = Smp.V_B; }

    FaultFast = f;

    /* 只锁存过流（需 Enable 重上升沿）；离线/过压按回差自动恢复，不锁存。
       跳闸计数不在这里做：这里是 21.25 kHz，会在一次跳闸里数上百次。
       统一在 Protect_Slow() 里按“跳闸上升沿”计数（见 FAULT_RETRY_MAX）。 */
    if (f & FLT_OC) { Protect_Latched |= FLT_OC; }
}

/**
  * @brief L2，1 kHz
  */
void Protect_Slow(void)
{
    uint16_t fs = 0;

    /* ---- 统一跳闸计数（只数真故障）----
       任何一次“从不跳闸到跳闸”都计数：窗口内超过 FAULT_RETRY_MAX 次就硬锁，
       只能断电复位。**只统计 FLT_HARDLOCK_MASK**（OC / OV / SENSOR）：
         - FLT_OFFLINE（V_A < 18V）是正常工况，没接电池时持续为 1，且在
           18/19V 回差附近来回抖，计入的话上电就永久 HardLock（实测 F80）；
         - FLT_TICK 是活故障（PWM 已关、恢复即清），硬锁只会把一次 ADC 抖动
           升级成“必须断电”。
       每次跳闸边沿的故障字/电压/电流存进 Protect_Ev* 槽位，便于事后定位。 */
    {
        uint16_t now = Protect_Get();
        uint8_t hardTrip = (uint8_t)(((now & FLT_HARDLOCK_MASK) != 0u) ? 1u : 0u);
        uint8_t anyTrip  = (uint8_t)(((now & FLT_TRIP_MASK) != 0u) ? 1u : 0u);
        /* 只在“真的在变换”时计数：ST_IDLE（没使能、母线摆着）下的跳闸只是
           总线状态噪声（例如 V_A 采样抖一下越过 32V），不该累成永久硬锁。 */
        uint8_t armed = (uint8_t)((Ctrl.State == ST_RUN || Ctrl.State == ST_SOFT_START) ? 1u : 0u);

        Protect_Ms++;

        /* 记录：任意跳闸上升沿（含 OFFLINE / TICK），便于事后分辨是谁在抖 */
        if (anyTrip && !AnyTripWas)
        {
            uint8_t i = Protect_EvIdx;
            if (i > 3u) { i = 0u; }
            if      (i == 0u) { Protect_Ev0Bits = now; Protect_Ev0Ms = Protect_Ms; Protect_Ev0VB = Smp.V_B; }
            else if (i == 1u) { Protect_Ev1Bits = now; Protect_Ev1Ms = Protect_Ms; Protect_Ev1VB = Smp.V_B; }
            else if (i == 2u) { Protect_Ev2Bits = now; Protect_Ev2Ms = Protect_Ms; Protect_Ev2VB = Smp.V_B; }
            else              { Protect_Ev3Bits = now; Protect_Ev3Ms = Protect_Ms; Protect_Ev3VB = Smp.V_B; }
            Protect_EvIdx = (uint8_t)((i + 1u) % 4u);
        }
        AnyTripWas = anyTrip;

        /* 计数：只数“使能状态下、真正会烧板”的故障 */
        if (armed && hardTrip && !TripWas)
        {
            if (RetryWinMs == 0u) { RetryWinMs = (uint16_t)FAULT_RETRY_WIN_MS; RetryCnt = 0; }
            if (RetryCnt < 0xFFFFu) { RetryCnt++; }
            if (RetryCnt >= (uint16_t)FAULT_RETRY_MAX) { HardLock = 1; }
        }
        TripWas = hardTrip;
    }

    /* ---- 底盘离线：18V 触发 / 19V 恢复 ---- */
    if (Smp.V_A < CHASSIS_OFF_V)      { OfflineFlag = 1; }
    else if (Smp.V_A > CHASSIS_ON_V)  { OfflineFlag = 0; }
    if (OfflineFlag) { fs |= FLT_OFFLINE; }

    /* ---- 母线过压：32V 触发，V_A<31V 且 V_B<26V 恢复（OV_DEB_MS 防抖）----
       越限本拍只加计数，连续 OV_DEB_MS 毫秒才认；中途回落就清零。 */
    if (Smp.V_A > SAFE_V_REF_MAX || Smp.V_B > V_HW_CUTOFF)
    {
        if (OvMs < 0xFFFFu) { OvMs++; }
        if (OvMs >= (uint16_t)OV_DEB_MS) { OvFlag = 1; }
    }
    else
    {
        OvMs = 0;
        if (Smp.V_A < SAFE_V_REF_CLR && Smp.V_B < 26.0f) { OvFlag = 0; }
    }
    if (OvFlag)
    {
        fs |= FLT_OV;
        Protect_Latched |= FLT_OV;
    }
    else
    {
        Protect_Latched &= (uint16_t)~FLT_OV;
    }

    /* ---- 传感器合理性检查已删除 ----
       原来的两条判据（“两侧功率一致”与“电压码值卡死”）在真实硬件上会持续误跳：
         - I_A / I_B 分别在各自桥的导通窗口采样、相位差 180°，逐拍功率本就不等
           （实测同一时刻 P_A=52W 而 P_B=20W）；
         - 深放电时 V_B 本来就是 0V，码值贴 0 是正常状态。
       与其带一个整天误跳闸的判据，不如直接去掉——过流/过压/离线/节拍已经把
       真正会烧板的情况盖住了。
       注意：FLT_SENSOR 位保留但不再置位（debug CSV / OLED 的 F 字段兼容）。 */

    FaultSlow = fs;

    /* ---- 跳闸计数窗口衰减 ---- */
    if (RetryWinMs)
    {
        RetryWinMs--;
        if (RetryWinMs == 0u) { RetryCnt = 0; }
    }
}
