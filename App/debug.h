/**
  ******************************************************************************
  * @file    debug.h
  * @brief   工程量串口输出（P1 验收用：与万用表/钳表比对）
  ******************************************************************************
  */
#ifndef __DEBUG_H
#define __DEBUG_H

#include "main.h"
#include "params.h"

/* JustFloat 最大通道数（缓冲按此分配：最大帧 = N*4 + 4 字节）
   14 通道 = 56B + 4B 帧尾 = 60B/帧；1 kHz 下 60 kB/s，USART2 1 Mbps(100 kB/s)
   占 60%，仍在安全范围（Tools/check_params.py 的 A25 会校验带宽）。 */
#define DEBUG_JF_MAX_CH    14u

void Debug_Init(void);
void Debug_Request(void);      /* 100 Hz 任务里置位，主循环消费 */
void Debug_Loop(void);         /* 主循环调用，非阻塞（DMA 发送） */

/* VOFA+ JustFloat 帧：n 个 float(小端) + 帧尾 00 00 80 7F，走 USART2 TX DMA。
   返回 1 = 已启动 DMA；0 = 上一帧未发完 / 参数非法（丢帧，不阻塞）。 */
uint8_t Debug_SendJustFloat(const float *v, uint8_t n);

#endif /* __DEBUG_H */
