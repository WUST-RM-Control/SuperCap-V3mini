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

#endif /* __COMM_UART_H */
