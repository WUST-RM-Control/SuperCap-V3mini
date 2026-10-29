/**
  ******************************************************************************
  * @file    modulator.h
  * @brief   调制器：u -> (D_A, D_B)，保证 V_L = u * V_A 恒成立；含 HRTIM 写入
  ******************************************************************************
  */
#ifndef __MODULATOR_H
#define __MODULATOR_H

#include "main.h"
#include "params.h"

void  Modulator_Compute(float V_A, float V_B, float u, float *D_A, float *D_B);
void  Modulator_Apply(float D_A, float D_B);   /* 写 CMP3/CMP4（预载，周期边界生效） */

void  HRTIM_Output_Enable(void);
void  HRTIM_Output_Disable(void);
void  HRTIM_Preload_Enable(void);              /* 打开 CMP 预载 + 重复更新 */
void  HRTIM_Counter_Start(void);

#endif /* __MODULATOR_H */
