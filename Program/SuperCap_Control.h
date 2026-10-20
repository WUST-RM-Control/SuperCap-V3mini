#ifndef __SUPERCAP_CONTROL__
#define __SUPERCAP_CONTROL__

#include "main.h"

typedef __PACKED_STRUCT
{
    float Referee_I;
    float A_Power_V;
    float A_Power_I;
    float B_Power_V;
    float B_Power_I;
    
    float Referee_P;
    float Chassis_P;
    float A_Power_P;
    float B_Power_P;
} CapData_StructTypedef;

typedef __PACKED_STRUCT
{
    uint8_t PWM_Out_Enable;
    
    uint8_t Enable_Flag;
    uint8_t Power_Buff_Energy;
    uint8_t Max_Referee_P;
    
    uint8_t Cap_Empty_Flag;
    
    uint8_t V_Error_Flag;
    uint8_t I_Error_Flag;
    uint8_t Connect_Error_Flag;
  
    float Target_A_P;
    
    float VB_to_VA_Set;
    float A_Duty;
    float B_Duty;
} CapControl_StructTypedef;

typedef __PACKED_STRUCT
{
    uint8_t State;                       //超电开关状态
    uint16_t SuperCap_V_x100;            //电容组电压x100
    uint16_t Referee_P_x10;              //裁判系统功率x10
    uint16_t Chassis_P_x10;              //底盘功率x10
} SuperCap_FeedBack_Data_StructTypedef; 

typedef __PACKED_STRUCT
{
    uint8_t Enable_Flag;
    uint8_t Power_Buff_Energy;
    uint8_t Max_Referee_P;
} SuperCap_Control_Data_StructTypedef;

extern CapData_StructTypedef CapData;
extern CapControl_StructTypedef CapControl;


void Cap_Init(void);

void Cap_ErrorHandle(void);

void Cap_Calulate_VB_to_VA(void);

void Cap_Unpack_Control_Data(uint8_t *Data);
void Cap_Send_FeedBack_Data(void);

#endif

