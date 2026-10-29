/**
  ******************************************************************************
  * @file    comm_uart.h
  * @brief   USART2：8 字节帧（前 3 字节同 CAN，尾部 0x55 0xAA）
  ******************************************************************************
  */
#ifndef __COMM_UART_H
#define __COMM_UART_H

#include "main.h"
#include "params.h"

void Comm_Uart_Init(void);

/* 发送（printf 与 JustFloat 共用 DMA1_Channel7）
   Comm_Uart_TxIdle()   1 = 上一帧已发完，可以准备新数据
   Comm_Uart_Send_DMA() 返回 1 = 已启动 DMA；0 = 忙/参数错（丢帧，不阻塞）
   缓冲在 DMA 完成前不得改写，调用方用 static 缓冲。 */
uint8_t Comm_Uart_TxIdle(void);
uint8_t Comm_Uart_Send_DMA(const uint8_t *buf, uint16_t len);

#endif /* __COMM_UART_H */
