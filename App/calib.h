#ifndef __CALIB__
#define __CALIB__

#include "main.h"

/* 标定系数（旧 ADC_Driver.c 里的全局量，此处归口到 calib） */
extern float Power_A_V_Adjust;    /* V_A 增益修正 */
extern float Power_B_V_Adjust;    /* V_B 增益修正 */
extern float Power_A_I_Adjust;    /* I_A 零点修正 (A) */
extern float Power_B_I_Adjust;    /* I_B 零点修正 (A) */
extern float Referee_I_Adjust;    /* I_Ref 零点修正 (A) */

/* 按芯片 UID 加载 V/I 标定系数（表内容与旧 ADC_Adjust.c 完全一致） */
void Calib_Load(uint32_t UP_Board_ID);

#endif
