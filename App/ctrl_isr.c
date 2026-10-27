/**
  ******************************************************************************
  * @file    ctrl_isr.c
  * @brief   控制节拍
  *
  *  21.25 kHz（ADC1 DMA 传输完成中断，与 PWM 严格同步）：
  *      采样 -> L1 保护 -> 三环 -> 写占空比
  *      分频：缓冲环 1/128（166 Hz）
  *
  *  1 kHz（TIM7）：
  *      L2 保护、状态机、通信看门狗、100 Hz 回帧、灯语、OLED 触发
  ******************************************************************************
  */
#include "ctrl_isr.h"
#include "sample.h"
#include "ctrl_core.h"
#include "modulator.h"
#include "protect.h"
#include "comm_can.h"
#include "debug.h"
#include "ui_oled.h"
#include "LED.h"

static volatile uint8_t Ctrl_Isr_Enabled;

/* 分频计数 */
#define DIV_ENERGY   128u     /* 21.25 kHz / 128 = 166 Hz */

static uint16_t DivEnergy;
static uint8_t  PwmApplied;   /* 上一次的 PWM 输出状态 */

/* 控制节拍心跳：ADC DMA 停了（ADC 出错/DMA 断）会让控制中断彻底消失，
   此时 PWM 会保持最后一拍的占空比一直跑 —— 必须检测并强制关断。 */
static volatile uint16_t IsrTick;

void Ctrl_Isr_Init(void)
{
    DivEnergy   = 0;
    PwmApplied  = 0;
    Ctrl_Isr_Enabled = 1;
}

/**
  * @brief 控制节拍心跳检查：1 kHz 调用，若 5 ms 内控制中断没跑过就判定节拍丢失。
  *        ADC/DMA 一旦停摆，控制中断消失、PWM 会卡在最后一拍的占空比上继续跑，
  *        这是本板最危险的单点故障，必须兜住。
  *
  *        故障是"活"的：节拍恢复后自动清除，不需要重新上电——否则一次
  *        ADC 抖动就能把整块板永久锁死。
  *
  *        自愈（实测必需）：只关输出是不够的。ADC 一旦停转，HAL 不会自己把它
  *        拉起来（实测 ADC1/2/3 冻死而 ADC4/5 正常，ErrorCode 始终为 0），
  *        所以每 TICK_RECOVER_MS 重新武装一次 ADC1，并把 ADC 触发点重写一遍
  *        ——触发点落在 0/越界时 TRG1 会永久停摆，重写即可恢复。
  */
static void Ctrl_Tick_Watchdog(void)
{
    static uint16_t last;
    static uint8_t  lost;
    static uint8_t  retry;      /* 恢复重试计时（单位：5 ms） */

    if (IsrTick != last)
    {
        last = IsrTick;
        lost = 0;
        retry = 0;
        Protect_Set_TickLost(0);
        return;
    }

    if (++lost >= 5u)   /* 连续 5 ms 没有控制中断 */
    {
        lost = 0;
        if (PwmApplied) { HRTIM_Output_Disable(); PwmApplied = 0; }
        Ctrl.PwmOn = 0;
        Ctrl.State = ST_FAULT;
        Protect_Set_TickLost(1);

        if (++retry >= (uint8_t)(TICK_RECOVER_MS / 5u))
        {
            retry = 0;
            Modulator_Apply(Ctrl.D_A, Ctrl.D_B);   /* 重写 ADC 触发点 CMP4 */
            Sample_Restart_Tick();                 /* 重新武装 ADC1 节拍源 */
        }
    }
}

/**
  * @brief ADC 规则转换完成回调（每个 ADC 各一次，只用 ADC1 作为节拍源）
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    uint8_t pwm_ok;

    if (hadc != &hadc1) { return; }

    /* 允许自然回绕：心跳只看"有没有变化"。
       千万不要写成 if (IsrTick < 0xFFFF) IsrTick++ —— 饱和后值不再变化，
       1 kHz 心跳会把它当成"节拍丢失"，上电 3 秒后必锁死（已实测）。 */
    IsrTick++;

    if (!Ctrl_Isr_Enabled) { return; }

    /* ---- 采样（I_L 重构用上一拍的占空比） ---- */
    Sample_Update(Ctrl.D_A, Ctrl.D_B);

    /* ---- L1 保护 ---- */
    Protect_Fast();

    /* ---- 状态机：是否允许输出 ---- */
    pwm_ok = (uint8_t)((Ctrl.State == ST_RUN || Ctrl.State == ST_SOFT_START) &&
                       !Protect_IsTripped());

    if (pwm_ok)
    {
        Ctrl.PwmOn = 1;
        Ctrl_Step(TS_CTRL);
        Modulator_Apply(Ctrl.D_A, Ctrl.D_B);

        if (!PwmApplied) { HRTIM_Output_Enable(); PwmApplied = 1; }

        /* ---- 缓冲环分频 ---- */
        DivEnergy++;
        if (DivEnergy >= DIV_ENERGY)
        {
            DivEnergy = 0;
            Ctrl_Loop_Energy();
        }
    }
    else
    {
        /* 关输出并把控制器状态清干净，保证下次使能无扰 */
        if (PwmApplied) { HRTIM_Output_Disable(); PwmApplied = 0; }
        Ctrl.PwmOn = 0;
        Ctrl.u     = 0.0f;
        Ctrl.Trim  = 0.0f;
        Ctrl.e_last = 0.0f;
        Ctrl.I_L_Ref = 0.0f;
    }
}

/**
  * @brief 灯语（与旧版一致）
  *        红常亮 = 过流/过压/传感器锁存；蓝闪 = 底盘离线；绿闪 = 未使能；同步 1 Hz = 正常
  */
static void Ctrl_LED_Update(void)
{
    static uint8_t s_last = 0xFFu;
    uint16_t f = Protect_Get();
    uint8_t  s;

    if      (f & (FLT_OC | FLT_OV | FLT_SENSOR)) { s = 3u; }
    else if (f & FLT_OFFLINE)                    { s = 2u; }
    else if (!Ctrl.Enable_Flag)                  { s = 1u; }
    else                                         { s = 0u; }

    if (s == s_last) { return; }
    s_last = s;

    LED_ALL_1Hz_Flash_Set(0);            /* 先全灭，顺便清各通道 flash 标志 */
    LED_Set(R, OFF);
    LED_Set(G, OFF);
    LED_Set(B, OFF);

    switch (s)
    {
        case 0u:  LED_ALL_1Hz_Flash_Set(1); break;
        case 1u:  LED_Set(G, Flash);        break;
        case 2u:  LED_Set(B, Flash);        break;
        default:  LED_Set(R, ON);           break;
    }
}

/**
  * @brief 1 kHz 任务（TIM7 中断内）
  *
  *  使能边沿、清锁存、状态迁移全部集中在这里做（不在通信中断里做），
  *  避免与 21.25 kHz 控制中断抢占时把控制器状态写坏。
  */
void Ctrl_Tick_1ms(void)
{
    static uint16_t T_10ms, T_100ms;
    static uint16_t CoolMs;      /* 故障消失后的冷却计时，防止跳闸-重启无限循环 */
    static uint8_t  EnLast;

    /* ---- 控制节拍心跳 ---- */
    Ctrl_Tick_Watchdog();

    /* ---- 显示值低通（10 Hz，1 kHz 算，代价极小） ---- */
    UI_OLED_Tick();

    /* ---- 喂狗放在这里，而不是主循环 ----
       主循环只做 OLED/打印这类非关键任务；屏掉线把主循环拖慢甚至卡住时，
       绝不能因此复位整块板子。控制节拍是否正常由上面的心跳单独判定。 */
#if (CFG_IWDG_ENABLE == 1)
    IWDG->KR = 0xAAAAu;
#endif

    /* ---- L2 保护 ---- */
    Protect_Slow();

    /* ---- 使能上升沿：清可恢复的锁存故障 ---- */
    if (Ctrl.Enable_Flag && !EnLast) { Protect_Clear_Latched(); }
    EnLast = Ctrl.Enable_Flag;

    /* ---- 灯语 ---- */
    Ctrl_LED_Update();

    /* ---- 状态机 ---- */
    switch (Ctrl.State)
    {
        case ST_SOFT_START:
        case ST_RUN:
            if (Protect_IsTripped())
            {
                Ctrl.State = ST_FAULT;
                Ctrl.PwmOn = 0;
            }
            else if (!Ctrl.Enable_Flag)
            {
                Ctrl.State = ST_IDLE;
                Ctrl.PwmOn = 0;
            }
            break;

        case ST_FAULT:
            /* 先冷却再重试：故障还在就不断刷新冷却计时，故障消失后还要再静默
               FAULT_COOL_MS 才允许重进 SOFT_START。
               以前是“一不 tripped 立刻重启”，一旦故障条件反复出现就是
               “跳闸-重启-跳闸”的死循环，每次重启都往功率级灌一次冲击（实测）。
               跳闸次数超限则由 HardLock(FLT_LOCK) 彻底锁死，只能断电复位。 */
            if (Protect_IsTripped())
            {
                CoolMs = (uint16_t)FAULT_COOL_MS;
            }
            else if (CoolMs)
            {
                CoolMs--;
            }
            else
            {
                Ctrl.State = Ctrl.Enable_Flag ? ST_SOFT_START : ST_IDLE;
                if (Ctrl.State == ST_SOFT_START) { Ctrl_Reset(); }
            }
            break;

        case ST_IDLE:
        default:
            if (Ctrl.Enable_Flag && !Protect_IsTripped())
            {
                Ctrl_Reset();
                Ctrl.State = ST_SOFT_START;
            }
            break;
    }

    /* ---- 100 Hz ---- */
    T_10ms++;
    if (T_10ms >= 10u)
    {
        T_10ms = 0;

        /* 通信看门狗：500 ms 无合法帧则清使能 */
        if (Ctrl_CommCooldown < 0xFFFFu) { Ctrl_CommCooldown++; }
        if (Ctrl_CommCooldown >= (uint16_t)(COMM_TIMEOUT_MS / 10u))
        {
            Ctrl_Set_Enable(0);
        }

        Comm_Can_Send_Feedback();
        Debug_Request();
    }

    /* ---- 10 Hz ---- */
    T_100ms++;
    if (T_100ms >= 100u)
    {
        T_100ms = 0;
        LED_Flash_100ms_Loop();
        UI_OLED_RequestRefresh();
    }
}
