#ifndef __PWM_DRIVER__
#define __PWM_DRIVER__

#include "main.h"


void HRTIM_Output_Start(void);
void HRTIM_Output_Stop(void);
    
void SET_AH_Duty(float Duty);
void SET_BH_Duty(float Duty);

void Buck_Boost_Control(float VB_To_VA);

#endif
