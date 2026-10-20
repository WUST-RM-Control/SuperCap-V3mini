#include "PWM_Driver.h"


CCMRAM void HRTIM_Output_Start(void)
{
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
}

CCMRAM void HRTIM_Output_Stop(void)
{
    HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
}

CCMRAM void SET_AH_Duty(float Duty)
{  
    if(Duty <= 0.05f) Duty = 0.05f;
    if(Duty >= 0.95f) Duty = 0.95f;
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A , HRTIM_COMPAREUNIT_1, 0);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A , HRTIM_COMPAREUNIT_3, (uint16_t)(Duty * 16000));
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A , HRTIM_COMPAREUNIT_4, (uint16_t)(Duty * 8000) + 8000);
}

CCMRAM void SET_BH_Duty(float Duty)
{  
    if(Duty <= 0.05f) Duty = 0.05f;
    if(Duty >= 0.95f) Duty = 0.95f;
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B , HRTIM_COMPAREUNIT_1, 0);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B , HRTIM_COMPAREUNIT_3, (uint16_t)(Duty * 16000));
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B , HRTIM_COMPAREUNIT_4, (uint16_t)(Duty * 8000) + 8000);
}

CCMRAM void Buck_Boost_Control(float VB_To_VA)
{  
    if(VB_To_VA > 5.0f) VB_To_VA = 5.0f;
    if(VB_To_VA < 0.2f) VB_To_VA = 0.2f;
    
    if(VB_To_VA >= 1.25f)
    {
        CapControl.A_Duty = 0.95f;
        CapControl.B_Duty = 0.95f / VB_To_VA;
    }
    else if(VB_To_VA <= 0.8f)
    {
        CapControl.A_Duty = 0.95f * VB_To_VA;
        CapControl.B_Duty = 0.95f;       
    }
    else
    {
        CapControl.A_Duty = 4.0f / 9.0f * (1.0f + VB_To_VA) * 0.95f;
        CapControl.B_Duty = 4.0f / 9.0f * (1.0f + 1.0f / VB_To_VA) * 0.95f;
    } 
    
    SET_AH_Duty(CapControl.A_Duty);
    SET_BH_Duty(CapControl.B_Duty);
}
