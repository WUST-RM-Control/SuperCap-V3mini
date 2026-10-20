#include "ADC_Driver.h"

/*===| ADC处理后的数据 |===*/
ADCData_StructTypedef ADC_Data;

/*===| ADC原始数据 |===*/
uint16_t A_Power_V_ADC_RawData;
uint16_t A_Power_I_ADC_RawData;
uint16_t Referee_I_ADC_RawData;
uint16_t B_Power_I_ADC_RawData;
uint16_t B_Power_V_ADC_RawData;

/*===|ADC换算系数  |===*/
#define Power_V_Gain (100 + 1000) / (100)   // 3.3V - 100kR - ADC - 10kR - GND
#define Power_I_Gain 1 / 20 / 0.004f        // I * 0.002R * 20 + 1.65V = ADC_V

/*===| 板ID |===*/
uint32_t Up_Board_ID = 0;
uint32_t Down_Board_ID = 0;

//////////////////////// 实际值/测量值 /////////////////
float Power_A_V_Adjust  = 1;
float Power_B_V_Adjust  = 1;
float Power_A_I_Adjust  = 0;
float Power_B_I_Adjust  = 0;
float Referee_I_Adjust  = 0;


/**
 * @brief ADC初始化
 */
void ADC_Init(void)
{
    /*===| ADC校准 |===*/
    Up_Board_ID = HAL_GetUIDw0();
    ADC_Adjust(Up_Board_ID, 0);
    
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_Delay(10);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_Delay(10);
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_Delay(10);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
    HAL_Delay(10);
    HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED);
    HAL_Delay(10);

    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&A_Power_V_ADC_RawData, 1);
    HAL_ADC_Start_DMA(&hadc2, (uint32_t *)&A_Power_I_ADC_RawData, 1);
    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)&Referee_I_ADC_RawData, 1);
    HAL_ADC_Start_DMA(&hadc4, (uint32_t *)&B_Power_I_ADC_RawData, 1);
    HAL_ADC_Start_DMA(&hadc5, (uint32_t *)&B_Power_V_ADC_RawData, 1);
    
    HAL_Delay(10);
    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_Start(&hopamp2);
    HAL_OPAMP_Start(&hopamp3);
    HAL_OPAMP_Start(&hopamp5);
    HAL_OPAMP_Start(&hopamp6);
}


/**
 * @brief ADC数据更新
 */
CCMRAM void ADC_Update(void)
{
    CapData.A_Power_V = A_Power_V_ADC_RawData * (3.3f / 4096.0f * Power_V_Gain) * Power_A_V_Adjust;
    CapData.B_Power_V = B_Power_V_ADC_RawData * (3.3f / 4096.0f * Power_V_Gain) * Power_B_V_Adjust;
    
    CapData.A_Power_I = (A_Power_I_ADC_RawData * (3.3f / 4096.0f) - 1.65f) * Power_I_Gain + Power_A_I_Adjust;
    CapData.B_Power_I = (B_Power_I_ADC_RawData * (3.3f / 4096.0f) - 1.65f) * Power_I_Gain + Power_B_I_Adjust;
    CapData.Referee_I = (Referee_I_ADC_RawData * (3.3f / 4096.0f) - 1.65f) * Power_I_Gain + Referee_I_Adjust;
    
    
    CapData.Referee_P = CapData.A_Power_V * CapData.Referee_I;
    CapData.A_Power_P = CapData.A_Power_V * CapData.A_Power_I;
    CapData.B_Power_P = CapData.B_Power_V * CapData.B_Power_I;
    CapData.Chassis_P = CapData.Referee_P - CapData.A_Power_P;
} 

