#ifndef __ADC_DRIVER__
#define __ADC_DRIVER__

#include "main.h"

extern float Power_A_V_Adjust;
extern float Power_B_V_Adjust;
extern float Power_A_I_Adjust;
extern float Power_B_I_Adjust;
extern float Referee_I_Adjust;

typedef __PACKED_STRUCT
{
    uint16_t A_Power_V_ADC;
    uint16_t A_Power_I_ADC;
    uint16_t B_Power_V_ADC;
    uint16_t B_Power_I_ADC;
    uint16_t Referee_I_ADC_and_Tempture[2];
} ADCData_StructTypedef;


void ADC_Init(void);

void ADC_Update(void);

void ADC_Update_Tempture(void);

void ADC_Filter(void);


#endif
