/**
  ******************************************************************************
  * @file    app_main.c
  * @brief   应用层入口：初始化顺序、1 kHz 节拍、后台循环
  ******************************************************************************
  */
#include "app_main.h"
#include "params.h"
#include "sample.h"
#include "ctrl_core.h"
#include "ctrl_isr.h"
#include "modulator.h"
#include "protect.h"
#include "comm_can.h"
#include "comm_uart.h"
#include "ui_oled.h"
#include "debug.h"
#include "LED.h"
#include "DWT.h"

#if (CFG_RESET_ON_COMM_LOSS == 1)
static uint16_t DisabledMs;
#endif

static void App_IWDG_Init(void);

/**
  * @brief 中断优先级
  *
  *  权威来源是 SuperCapV3.ioc 的 NVIC 段（重新生成代码时原样保留）：
  *      DMA1_Channel1(控制节拍) = 0
  *      TIM7 / FDCAN2 / USART2  = 1
  *      DMA1_Channel6,7 / ADC1_2 / ADC3 / ADC4 = 2
  *  这里再设一遍是幂等的保险：即使有人用旧版 CubeMX 重新生成、丢了某些键，
  *  控制节拍依旧是最高的，不会被背景任务顶歪。
  */
static void App_NVIC_Priority(void)
{
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);   /* 21.25 kHz 控制节拍 */
    HAL_NVIC_SetPriority(TIM7_DAC_IRQn, 1, 0);        /* 1 kHz 任务 */
    HAL_NVIC_SetPriority(FDCAN2_IT0_IRQn, 1, 0);
    HAL_NVIC_SetPriority(FDCAN2_IT1_IRQn, 1, 0);
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 2, 0);
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 2, 0);
    HAL_NVIC_SetPriority(ADC1_2_IRQn, 2, 0);          /* 过流看门狗 */
    HAL_NVIC_SetPriority(ADC3_IRQn, 2, 0);
    HAL_NVIC_SetPriority(ADC4_IRQn, 2, 0);
}

void App_Init(void)
{
    /* 0. 周期计数器（UI 用它测主循环频率） */
    DWT_Init(170);

    /* 0.5 中断优先级（必须在各外设启动前设置） */
    App_NVIC_Priority();

    /* 1. 采样（含 UID 标定、ADC 自校准、DMA 启动） */
    Sample_Init();

    /* 2. 人机接口 */
    LED_OFF();
    UI_OLED_Init();

    /* 3. HRTIM：先跑计数器（输出关闭），再打开比较值预载 */
    HRTIM_Counter_Start();
    HRTIM_Preload_Enable();

    /* 4. 控制核心（先于通信初始化，避免首帧打进来时状态还是空的） */
    Ctrl_Init();
    Protect_Init();

    /* 5. 通信 */
    Comm_Can_Init();
    Comm_Uart_Init();

    /* 6. 1 kHz 节拍（TIM7） */
    HAL_TIM_Base_Start_IT(&htim7);

    /* 7. 打开控制中断（ADC DMA 完成 = 21.25 kHz） */
    Ctrl_Isr_Init();

    /* 8. 看门狗（放最后，避免初始化耗时触发） */
    App_IWDG_Init();

    /* 9. 调试输出 */
    Debug_Init();
}

void App_Loop(void)
{
    /* 控制全部在中断里跑，主循环只做显示等背景任务。
       看门狗在 1 kHz 中断里喂（见 ctrl_isr.c），所以这里即使被 OLED 拖住
       甚至卡死，也不会导致整板复位——这正是 OLED 热插拔的要求。 */
    UI_OLED_Loop();
    Debug_Loop();

    /* CAN 总线自愈：低电压上电（收发器 5V 还没起来）时总线无 ACK，
       FDCAN 会 bus-off / TX 队列堵死，这里自动 Stop+Start 拉回来。 */
    Comm_Can_Monitor();
}

/**
  * @brief 独立看门狗（直接操作寄存器，不依赖 HAL IWDG 驱动）
  *        IWDG 启动时自动使能 LSI，超时 ≈1 s；调试器 halt 时冻结。
  */
static void App_IWDG_Init(void)
{
#if (CFG_IWDG_ENABLE == 1)
    IWDG->KR  = 0xCCCCu;                 /* 启动 */
    IWDG->KR  = 0x5555u;                 /* 解锁 PR/RLR */
    IWDG->PR  = 3u;                      /* LSI/32 ≈ 1 kHz */
    IWDG->RLR = 1000u;                   /* ≈1 s */
    while (IWDG->SR & 0x00000002u) { }   /* 等 RVU 更新完成 */
    IWDG->KR  = 0xAAAAu;                 /* 首次重装 */

    /* 调试器暂停内核时冻结看门狗，避免打断点被反复复位 */
    DBGMCU->APB1FZR1 |= DBGMCU_APB1FZR1_DBG_IWDG_STOP;
#endif
}

/**
  * @brief 1 kHz 节拍（TIM7）
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim7)
    {
        Ctrl_Tick_1ms();

#if (CFG_RESET_ON_COMM_LOSS == 1)
        /* 仅调试用：还原旧版"禁用 1 s 自复位"的行为 */
        if (Ctrl.Enable_Flag == 0)
        {
            if (++DisabledMs >= 1000u) { NVIC_SystemReset(); }
        }
        else { DisabledMs = 0; }
#endif
    }
}
