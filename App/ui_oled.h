/**
  ******************************************************************************
  * @file    ui_oled.h
  * @brief   OLED 显示（沿用旧版字段，主循环里刷新，不再占用中断）
  ******************************************************************************
  */
#ifndef __UI_OLED_H
#define __UI_OLED_H

#include "main.h"

void UI_OLED_Init(void);
void UI_OLED_RequestRefresh(void);   /* 由 1 kHz 任务置位，主循环执行 */
void UI_OLED_Tick(void);             /* 1 kHz 任务里调用：刷新显示用的低通值 */
void UI_OLED_Loop(void);             /* 主循环调用：有请求则刷一屏 */
void UI_OLED_Render(void);           /* 渲染一帧（在线时） */

extern uint8_t UI_OLED_Online;

#endif /* __UI_OLED_H */
