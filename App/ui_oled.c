/**
  ******************************************************************************
  * @file    ui_oled.c
  * @brief   OLED 页面（带热插拔容错）
  *
  *  运行在主循环里，与控制环完全解耦：控制跑在 21.25 kHz 中断里，
  *  屏掉了/卡了最多让显示停住，不影响功率控制。
  *
  *  容错策略：
  *    - 所有 I2C 传输走 I2C_Guard（DMA + 超时 + 失败计数）
  *    - 连续失败 -> 判离线，此后不再碰总线（每次传输 0 开销）
  *    - 离线期间每 2 s 探测一次：9 时钟总线恢复 + 重新初始化屏幕
  *    - 一帧分 8 次 DMA 发起（一页一笔），主循环每次只花几微秒，不阻塞；
  ******************************************************************************
  */
#include "ui_oled.h"
#include "OLED.h"
#include "OLED_Data.h"
#include "sample.h"
#include "ctrl_core.h"
#include "protect.h"
#include "i2c_guard.h"
#include "DWT.h"

uint8_t UI_OLED_Online;

#define OLED_PAGE_NUM   8u

static volatile uint8_t RefreshReq;
static uint16_t ProbeTimer;      /* 离线探测计时（10 Hz 节拍） */
static uint8_t  PageIdx;         /* 当前帧已推送到第几页；OLED_PAGE_NUM = 空闲 */

/* 重新初始化屏幕（热插拔插回来时屏已断电重启，配置全丢） */
static void UI_OLED_Reinit(void)
{
    OLED_Init();
    OLED_Clear();
}

void UI_OLED_Init(void)
{
    I2C_Guard_Init();

    OLED_Init();
    OLED_Clear();

    /* 探测：失败就离线，等主循环里周期性重试，绝不在这里死等 */
    UI_OLED_Online = (uint8_t)(I2C_Guard_Probe(OLED_I2C_ADDR) == 0u);
    if (UI_OLED_Online)
    {
        UI_OLED_Reinit();
    }
    ProbeTimer = 0;
    PageIdx    = OLED_PAGE_NUM;      /* 空闲：等第一次刷新请求 */
}

void UI_OLED_RequestRefresh(void)
{
    RefreshReq = 1;
}

void UI_OLED_Loop(void)
{
    /* DMA 卡死检查（总线被拉低 / 模块半失效），每次主循环都过一遍 */
    I2C_Guard_Tick();

    /* ---------- 离线：周期性探测，期间不碰总线 ---------- */
    if (!UI_OLED_Online)
    {
        PageIdx = OLED_PAGE_NUM;

        if (!RefreshReq) { return; }
        RefreshReq = 0;

        if (++ProbeTimer >= (OLED_PROBE_MS / 100u))
        {
            ProbeTimer = 0;
            if (I2C_Guard_Probe(OLED_I2C_ADDR) == 0u)
            {
                UI_OLED_Reinit();          /* 插回来了：重新配置屏幕 */
                UI_OLED_Online = 1u;
            }
        }
        return;
    }

    /* ---------- 有帧在推：一页一页发 DMA，不阻塞主循环 ---------- */
    if (PageIdx < OLED_PAGE_NUM)
    {
        if (I2C_Guard_Is_Busy()) { return; }        /* 上一页还在传 */

        if (OLED_UpdatePage(PageIdx) == 0u)
        {
            PageIdx++;
            if (PageIdx >= OLED_PAGE_NUM)            /* 一帧推完 */
            {
                if (I2C_Guard_Is_Offline())
                {
                    UI_OLED_Online = 0u;
                    ProbeTimer = 0;
                }
            }
            return;
        }

        /* 发不出去（离线/忙）：放弃本帧，交给探测流程 */
        PageIdx = OLED_PAGE_NUM;
        UI_OLED_Online = 0u;
        ProbeTimer = 0;
        return;
    }

    /* ---------- 空闲：有新帧就画显存，下一轮从第 0 页开始推 ---------- */
    if (!RefreshReq) { return; }
    RefreshReq = 0;

    UI_OLED_Render();                                /* 只刷显存，不碰总线 */
    PageIdx = 0u;
}

/**
  * @brief 显示值：全部走 10 Hz 一阶低通（在 1 kHz 任务里算，代价 ≈ 1/21）
  *
  *        Smp 的瞬时值随开关动作和环路抖动很大（实测 I_Ref 同一秒在 0.18~2.0A
  *        之间跳，而电源读数是稳的），屏上直接显示原始值根本看不清。
  *        低通放在 1 kHz 而不是 21.25 kHz 中断里：12 个变量的开销降到 1/21，
  *        对"人眼看的 10 Hz 数值"完全够用。
  */
typedef struct
{
    float V_A, V_B, I_A, I_B;
    float P_Ref, P_Tgt, P_Cha, P_A, P_B;
    float E, D_A, D_B;
} UI_Show_t;

static UI_Show_t Show;

static void UI_Lp(float *s, float x)
{
    *s += KP_UI * (x - *s);
}

void UI_OLED_Tick(void)
{
    /* 功率/电流取采样层已经算好的 1 ms 块平均（Smp.*_Avg）：
       瞬时值是“开关周期内某一相位”的点采样，逐拍起伏十几瓦，
       直接送 10 Hz 低通后屏上仍会一行一行地跳。 */
    UI_Lp(&Show.V_A,   Smp.V_A);
    UI_Lp(&Show.V_B,   Smp.V_B);
    UI_Lp(&Show.I_A,   Smp.I_A_Avg);
    UI_Lp(&Show.I_B,   Smp.I_B_Avg);
    UI_Lp(&Show.P_Ref, Smp.P_Ref_Avg);
    UI_Lp(&Show.P_Cha, Smp.P_Cha_Avg);
    UI_Lp(&Show.P_A,   Smp.P_A_Avg);
    UI_Lp(&Show.P_B,   Smp.P_B_Avg);
    UI_Lp(&Show.P_Tgt, Ctrl.P_Tgt);
    UI_Lp(&Show.E,     Ctrl.E_Buff_f);
    UI_Lp(&Show.D_A,   Ctrl.D_A);
    UI_Lp(&Show.D_B,   Ctrl.D_B);
}

/* ---- 图形元件 ---------------------------------------------------------- */

/**
  * @brief 小功率仪表：上方填充 = 实际取电，下方刻度 = 目标
  *
  *        两者必须错开行：目标刻度画在底部 3 行，所以即使落在填充区间里
  *        也永远看得见（驱动没有"清点"接口，叠画到一起就分辨不出来了）。
  */
static void UI_MiniGauge(uint8_t X, uint8_t Y, uint8_t W, float act, float tgt)
{
    uint8_t inner = (uint8_t)(W - 2u);
    float   ra = act / UI_PBAR_MAX;
    float   rt = tgt / UI_PBAR_MAX;
    uint8_t fa, ft;

    if (ra < 0.0f) { ra = 0.0f; }
    if (ra > 1.0f) { ra = 1.0f; }
    if (rt < 0.0f) { rt = 0.0f; }
    if (rt > 1.0f) { rt = 1.0f; }

    fa = (uint8_t)(ra * (float)inner);
    ft = (uint8_t)(rt * (float)inner);

    OLED_DrawRectangle(X, Y, W, 8u, OLED_UNFILLED);
    if (fa) { OLED_DrawRectangle((uint8_t)(X + 1u), (uint8_t)(Y + 1u), fa, 4u, OLED_FILLED); }
    OLED_DrawRectangle((uint8_t)(X + 1u + ft), (uint8_t)(Y + 4u), 1u, 3u, OLED_FILLED);
}

/* 通用比例条（ratio 0..1）：占空比、缓冲能量共用 */
static void UI_Bar(uint8_t X, uint8_t Y, uint8_t W, float ratio)
{
    uint8_t inner = (uint8_t)(W - 2u);
    uint8_t f     = 0u;

    if (ratio > 0.0f) { f = (uint8_t)(ratio * (float)inner); }
    if (f > inner)    { f = inner; }

    OLED_DrawRectangle(X, Y, W, 8u, OLED_UNFILLED);
    if (f) { OLED_DrawRectangle((uint8_t)(X + 1u), (uint8_t)(Y + 1u), f, 6u, OLED_FILLED); }
}

/**
  * @brief 简化主电路图：A 母线 -|TA1/TA2 半桥|- 电感 -|TB1/TB2 半桥|- B 母线
  *
  *        画在 x=46..86 / y=18..42，正好夹在 A 侧（左）和 B 侧（右）数据列中间，
  *        "哪个数属于哪一侧"靠位置直读，不需要文字标签。
  *        实心小矩形 = 开关管，三个小圆 = 电感线圈，内外框 = 母线+/-。
  */
static void UI_Schematic(void)
{
    /* 母线框：上边 = 母线+，下边 = 母线-，左右两边 = A/B 母线 */
    OLED_DrawRectangle(46u, 18u, 41u, 25u, OLED_UNFILLED);

    /* 两个桥臂 */
    OLED_DrawLine(56u, 18u, 56u, 42u);
    OLED_DrawLine(76u, 18u, 76u, 42u);

    /* 开关管：每个桥臂上下各一只 */
    OLED_DrawRectangle(52u, 21u, 9u, 5u, OLED_FILLED);
    OLED_DrawRectangle(52u, 34u, 9u, 5u, OLED_FILLED);
    OLED_DrawRectangle(72u, 21u, 9u, 5u, OLED_FILLED);
    OLED_DrawRectangle(72u, 34u, 9u, 5u, OLED_FILLED);

    /* 电感：两桥臂中点之间的线圈 */
    OLED_DrawLine(56u, 29u, 59u, 29u);
    OLED_DrawCircle(62u, 29u, 2u, OLED_UNFILLED);
    OLED_DrawCircle(67u, 29u, 2u, OLED_UNFILLED);
    OLED_DrawCircle(72u, 29u, 2u, OLED_UNFILLED);
    OLED_DrawLine(74u, 29u, 76u, 29u);

    /* 引出线，指向左右两侧数据列 */
    OLED_DrawLine(42u, 24u, 46u, 24u);
    OLED_DrawLine(88u, 24u, 92u, 24u);
}

/* ---- 渲染 -------------------------------------------------------------- */

/**
  * @brief 渲染一帧（只在在线时调用，10 Hz）
  *
  *  128x64 单屏布局，相邻行之间至少留 1 像素空隙：
  *    y0    裁判功率：实际 / 期望（左）+ 小仪表（右）
  *    y9    缓冲能量：满宽进度条 + 数值
  *    y18   简化电路图（x=46..86）
  *    y20   A 侧：电压 / 电流 / 功率（左列 x=6）
  *    y20   B 侧：电压 / 电流 / 功率（右列 x=92）
  *    y47   A / B 占空比条（放在各自数据列正下方，位置即归属）
  *    y56   状态 / 故障字 / 底盘功率
  */
void UI_OLED_Render(void)
{
    uint16_t f = Protect_Get();

    OLED_Clear();                       /* 整屏重画，避免短字符串留下残字 */

    /* ---- 顶部：裁判功率 实际 / 期望 ---- */
    OLED_Printf(0, 0, OLED_6X8, "%4.1fW / %4.1fW", Show.P_Ref, Show.P_Tgt);
    UI_MiniGauge(82u, 0u, 46u, Show.P_Ref, Show.P_Tgt);

    /* ---- 缓冲能量：进度条 + 数值 ---- */
    UI_Bar(0u, 9u, 104u, Show.E / E_MAX);
    OLED_Printf(104, 9, OLED_6X8, "%3.0fJ", Show.E);

    /* ---- 中间：简化电路图（两侧就是 A/B 数据列） ---- */
    UI_Schematic();

    OLED_Printf(6,  20, OLED_6X8, "%5.2fV", Show.V_A);
    OLED_Printf(6,  29, OLED_6X8, "%+5.2fA", Show.I_A);
    OLED_Printf(6,  38, OLED_6X8, "%+5.1fW", Show.P_A);

    OLED_Printf(92, 20, OLED_6X8, "%5.2fV", Show.V_B);
    OLED_Printf(92, 29, OLED_6X8, "%+5.2fA", Show.I_B);
    OLED_Printf(92, 38, OLED_6X8, "%+5.1fW", Show.P_B);

    /* ---- 占空比条：左右各一，正好在各侧数据列下方 ---- */
    UI_Bar(6u,  47u, 36u, Show.D_A);
    UI_Bar(92u, 47u, 36u, Show.D_B);

    /* ---- 底部：状态 / 故障字 / 底盘功率 ---- */
    OLED_Printf(0, 56, OLED_6X8, "S%d  F%02X  Pc%+5.1fW",
                (int)Ctrl.State, (unsigned)f, Show.P_Cha);
}
