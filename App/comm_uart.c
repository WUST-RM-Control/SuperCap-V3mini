/**
  ******************************************************************************
  * @file    comm_uart.c
  * @brief   USART2 通信：空闲中断收 8 字节帧，尾部校验 0x55 0xAA
  ******************************************************************************
  */
#include "comm_uart.h"
#include "ctrl_core.h"
#include "sample.h"
#include "protect.h"

#include <stdio.h>

static uint8_t Uart_RxData[8];

/* ---- 发送侧：DMA 单槽 ----------------------------------------------------
   控制帧 RX 占 DMA1_Channel6，TX 是 DMA1_Channel7（已在 HAL_UART_MspInit 里
   配好，VERY_HIGH）。只留一帧的在途空间：调试口宁可丢帧也绝不在主循环里
   阻塞（1 Mbaud 下 28 字节≈0.3 ms，但 DMA 启动 + 等待完成仍会被 RTOS/中断
   抖动放大）。忙标志由 TxCplt 中断清除。 */
static volatile uint8_t Uart_TxBusy;

void Comm_Uart_Init(void)
{
    Uart_TxBusy = 0u;
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, Uart_RxData, sizeof(Uart_RxData));
}

uint8_t Comm_Uart_TxIdle(void)
{
    return (uint8_t)(Uart_TxBusy == 0u);
}

uint8_t Comm_Uart_Send_DMA(const uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0u)  { return 0u; }
    if (Uart_TxBusy != 0u)         { return 0u; }   /* 上一帧未发完：丢帧 */

    /* 注意：buf 必须在 DMA 完成前保持有效且不被改写（调用方用 static 缓冲）。
       先置忙再启动，避免 TxCplt 先到把忙标志提前清掉。 */
    Uart_TxBusy = 1u;
    if (HAL_UART_Transmit_DMA(&huart2, buf, len) != HAL_OK)
    {
        Uart_TxBusy = 0u;
        return 0u;
    }
    return 1u;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart2) { return; }
    Uart_TxBusy = 0u;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != &huart2) { return; }

    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, Uart_RxData, sizeof(Uart_RxData));

    if (Size >= 8u && Uart_RxData[6] == 0x55u && Uart_RxData[7] == 0xAAu)
    {
        Ctrl_Apply_ControlFrame(Uart_RxData);
    }
}

/**
  * @brief 串口错误恢复：只中止并重启接收，不重建句柄
  *        （旧固件 memset 整个 huart2 会丢 DMA 句柄状态）
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart2) { return; }

    Uart_TxBusy = 0u;      /* 出错时 HAL 会回到 READY，忙标志必须同步清 */
    (void)HAL_UART_AbortReceive(&huart2);
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, Uart_RxData, sizeof(Uart_RxData));
}

/* printf 重定向（保留接口）：行缓冲，遇 '\n' 或写满才起一次 DMA。
   上一行未发完时本轮到字符直接丢弃 —— 调试输出不得阻塞主循环。 */
#define TX_LINE_MAX     96u
static uint8_t  TxLine[TX_LINE_MAX];
static uint16_t TxLineLen;

int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;
    (void)f;

    if (Comm_Uart_TxIdle() == 0u) { TxLineLen = 0u; return ch; }

    if (c == (uint8_t)'\n' || TxLineLen >= (uint16_t)TX_LINE_MAX)
    {
        if (TxLineLen > 0u) { (void)Comm_Uart_Send_DMA(TxLine, TxLineLen); }
        TxLineLen = 0u;
        if (c != (uint8_t)'\n') { TxLine[TxLineLen++] = c; }
        return ch;
    }

    TxLine[TxLineLen++] = c;
    return ch;
}
