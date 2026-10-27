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

void Debug_Init(void);
void Debug_Request(void);      /* 1 kHz 任务里每 100 ms 置一次 */
void Debug_Loop(void);         /* 主循环调用，printf 阻塞不影响控制中断 */

#endif /* __DEBUG_H */
