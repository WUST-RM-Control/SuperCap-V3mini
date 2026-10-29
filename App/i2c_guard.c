/**
  ******************************************************************************
  * @file    i2c_guard.c
  * @brief   I2C1 总线守卫
  ******************************************************************************
  */
#include "i2c_guard.h"

#define I2C_TX_TIMEOUT_MS   I2C_TIMEOUT_MS      /* 单次传输基础超时（params.h） */
#define I2C_FAIL_LIMIT      I2C_FAIL_MAX        /* 连续失败多少次判离线 */

static volatile uint8_t  Offline;
static volatile uint8_t  Busy;      /* DMA 发送进行中 */
static volatile uint8_t  TxOk;      /* 最近一次发送是否成功 */
static uint16_t          FailCnt;
static uint16_t          FailTotal;
static uint32_t          TxStart;   /* 本次发送的起始 tick */
static uint32_t          TxTmo;     /* 本次发送的毫秒预算 */

/**
  * @brief 按字节数算超时。
  *
  *        固定 1ms 是不够的：OLED 整页写入一次是 **129 字节**，400kHz 下要 ≈2.9ms，
  *        HAL 会在超时点直接中止传输并返回错误——现象就是“每行前几个字正常、
  *        后面是乱码”（GDDRAM 里剩下的还是上电随机值），连续 3 次失败后整屏判离线。
  *        短命令（2~3 字节）仍然只用 I2C_TIMEOUT_MS，热插拔容错不受影响。
  */
static uint32_t I2C_Tx_Timeout(uint16_t len)
{
    return (uint32_t)I2C_TX_TIMEOUT_MS + ((uint32_t)len * (uint32_t)I2C_BYTE_US) / 1000u;
}

/* 约 2~3 µs 的粗延时，用于手工打 SCL 时钟（约 150~250 kHz） */
static void I2C_ShortDelay(void)
{
    volatile uint32_t d = 200u;
    while (d--) { }
}

/* ------------------------------------------------------------------ */
/* 失败计数 / 中断注册                                                  */
/* ------------------------------------------------------------------ */

static void I2C_Fail(void)
{
    TxOk = 0u;
    if (FailTotal < 0xFFFFu) { FailTotal++; }
    if (FailCnt < 0xFFFFu)   { FailCnt++; }
    if (FailCnt >= I2C_FAIL_LIMIT) { Offline = 1u; }
}

/**
  * @brief 把 SCL/SDA 配成带内部上拉的 AF 开漏
  *        上拉在 OLED 模块上，模块一拔总线就悬空，I2C 会一直认为 SCL 被拉低。
  */
static void I2C_Pins_Pullup(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);
}

void I2C_Guard_Init(void)
{
    /* DMA1_Channel8 / I2C1_EV / I2C1_ER 的 NVIC 由 CubeMX 根据 .ioc 生成在
       HAL_I2C_MspInit 里，这里不用再管。 */
    I2C_Pins_Pullup();

    Busy    = 0u;
    TxOk    = 1u;
    FailCnt = 0;
}

/**
  * @brief 总线恢复：9 个 SCL 时钟 + 手动 STOP + 外设复位
  *
  * 从机卡在半个字节里时，SDA 可能被它一直拉着；打 9 个时钟能让它把剩下的位移出来。
  */
static void I2C_Bus_Recover(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t i;

    Busy = 0u;
    TxOk = 0u;

    /* 1. 关外设 + 强制复位，清掉卡死的 BUSY 位。
       刻意不用 HAL_I2C_DeInit：它会走 MspDeInit 把 **DMA1 时钟**关掉，
       而 DMA1 还挂着 ADC（21.25 kHz 控制节拍）和串口，关一下就可能丢拍。 */
    CLEAR_BIT(hi2c1.Instance->CR1, I2C_CR1_PE);
    (void)HAL_DMA_Abort(&hdma_i2c1_tx);
    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();

    /* 2. 引脚切成普通开漏输出，手工打时钟 */
    g.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);      /* SDA 释放 */

    for (i = 0u; i < 9u; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET); /* SCL 低 */
        I2C_ShortDelay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);   /* SCL 高 */
        I2C_ShortDelay();
    }

    /* 3. 手动 STOP：SCL 高电平期间 SDA 由低到高 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    I2C_ShortDelay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    I2C_ShortDelay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    I2C_ShortDelay();

    /* 4. 重新初始化外设（MspInit 会把引脚恢复成 AF 开漏、重新链接 DMA、使能 NVIC） */
    MX_I2C1_Init();
    I2C_Pins_Pullup();
}

/**
  * @brief 发起一次 DMA 发送（非阻塞）。0 = 已发起；1 = 忙 / 离线 / 失败
  *
  *        要求 buf 在传输完成前保持有效（不能是栈上临时数组）。
  */
uint8_t I2C_Guard_Send(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    if (Offline || Busy || len == 0u) { return 1u; }

    TxTmo   = I2C_Tx_Timeout(len);
    TxStart = HAL_GetTick();
    TxOk    = 0u;
    Busy    = 1u;

    if (HAL_I2C_Master_Transmit_DMA(&hi2c1, addr, (uint8_t *)buf, len) != HAL_OK)
    {
        Busy = 0u;
        I2C_Fail();
        return 1u;
    }

    return 0u;
}

uint8_t I2C_Guard_Is_Busy(void)
{
    return Busy;
}

/**
  * @brief 主循环里反复调用：检查 DMA 是否卡死（总线被拉低 / 模块半失效）
  *        超时就收尾 + 复位总线，避免整个显示卡在一个永远等不到的完成中断上。
  */
void I2C_Guard_Tick(void)
{
    if (!Busy) { return; }
    if ((uint32_t)(HAL_GetTick() - TxStart) <= TxTmo) { return; }

    Busy = 0u;
    I2C_Fail();

    if (!Offline) { I2C_Bus_Recover(); }
}

/**
  * @brief 阻塞发送（只给小命令 / 探测用，几十 µs 量级）
  */
uint8_t I2C_Guard_Tx(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    uint32_t t0;

    if (I2C_Guard_Send(addr, buf, len) != 0u) { return 1u; }

    t0 = HAL_GetTick();
    while (Busy)
    {
        I2C_Guard_Tick();
        if ((uint32_t)(HAL_GetTick() - t0) > (TxTmo + 2u)) { return (uint8_t)(TxOk ? 0u : 1u); }
    }

    return (uint8_t)(TxOk ? 0u : 1u);
}

/* ---- HAL 回调：DMA 链在发送结束时收尾 ---- */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c1)
    {
        if (Busy) { Busy = 0u; TxOk = 1u; FailCnt = 0; }
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c1)
    {
        Busy = 0u;
        I2C_Fail();
    }
}

/**
  * @brief 探测（仅在离线时调用）：先恢复总线，再发一条安全命令
  */
uint8_t I2C_Guard_Probe(uint16_t addr)
{
    uint8_t cmd[2] = {0x00u, 0xAEu};   /* SSD1306 关显示，安全 */

    I2C_Bus_Recover();

    Offline = 0u;                      /* 先放行这一次发送 */
    if (I2C_Guard_Tx(addr, cmd, 2u) == 0u)
    {
        FailCnt = 0;
        return 0u;
    }

    Offline = 1u;                      /* 还是不行，保持离线 */
    return 1u;
}

uint8_t I2C_Guard_Is_Offline(void)
{
    return Offline;
}

uint16_t I2C_Guard_FailCount(void)
{
    return FailTotal;
}
