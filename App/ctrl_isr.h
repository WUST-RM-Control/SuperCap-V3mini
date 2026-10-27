/**
  ******************************************************************************
  * @file    ctrl_isr.h
  * @brief   控制中断：ADC DMA 完成 -> 21.25 kHz 控制节拍；以及 1 kHz 分频任务
  ******************************************************************************
  */
#ifndef __CTRL_ISR_H
#define __CTRL_ISR_H

#include "main.h"
#include "params.h"

void Ctrl_Isr_Init(void);     /* 允许控制中断进入 */
void Ctrl_Tick_1ms(void);     /* 由 TIM7 1 kHz 中断调用 */

#endif /* __CTRL_ISR_H */
