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

void Comm_Uart_Init(void)
{
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, Uart_RxData, sizeof(Uart_RxData));
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

    (void)HAL_UART_AbortReceive(&huart2);
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, Uart_RxData, sizeof(Uart_RxData));
}

/* printf 重定向（调试用） */
int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;
    (void)f;
    (void)HAL_UART_Transmit(&huart2, &c, 1, 100);
    return ch;
}
