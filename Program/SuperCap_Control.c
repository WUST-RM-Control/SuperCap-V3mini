#include "SuperCap_Control.h"

#define IF_USED_AS_BAT_BUFFER 0     //是否作为电池补偿电容

#define Safe_RefMax_V 32.0f       //底盘安全电压上限
#define Safe_CupMax_V 27.0f       //电容安全电压上限
#define CAP_MAX_V 25.5f        //超电最大充电电压
#define CAP_MIN_V 8.0f         //超电最小放电电压
#define Power_Buff_Target_Energy 55     //期望缓冲区剩余能量

#if(IF_USED_AS_BAT_BUFFER)
    #define MAX_B_Current 20.0f   //电容最大充放电电流
    #define MAX_Power 450.0f       //电容最大充放电功率
#else
    #define MAX_B_Current 14.0f   //电容最大充放电电流
    #define MAX_Power 360.0f       //电容最大充放电功率
#endif

CapData_StructTypedef CapData = {0};

CapControl_StructTypedef CapControl = {0};

PID_Struct_TypeDef PowerBuffer_PID_Struct;   //缓冲区能量闭环
PID_Struct_TypeDef Referee_P_PID_Struct;     //裁判系统功率闭环
PID_Struct_TypeDef Cap_Vmax_PID_Struct;      //电容最大电压限制
PID_Struct_TypeDef Cap_Vmin_PID_Struct;      //电容最小电压限制
PID_Struct_TypeDef CAP_P_PID_Struct;         //电容充放电功率闭环

void Cap_Init(void)
{
    ADC_Init();
    OLED_Init();  
    LED_OFF();
    HAL_HRTIM_WaveformCounterStart(&hhrtim1, HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B);
    FDCAN_Init();
    USART_Init();
    
    PID_Init(&PowerBuffer_PID_Struct,   0.0f,   0.005f,   0.0f,     10.0f);
    PID_Init(&Referee_P_PID_Struct,     4.0f,   0.05f,    0.0f,     MAX_Power);
    PID_Init(&Cap_Vmax_PID_Struct,      0.0f,   0.05f,     0.0f,     MAX_Power);
    PID_Init(&Cap_Vmin_PID_Struct,      0.0f,   0.05f,     0.0f,     MAX_Power);
    PID_Init(&CAP_P_PID_Struct,         0.001f, 0.0001f,  0.0f,     23.0f);
   
    
    CapControl.Enable_Flag = 0;
    CapControl.Power_Buff_Energy = Power_Buff_Target_Energy;
    CapControl.Max_Referee_P = 20.0f;
    
    HAL_TIM_Base_Start_IT(&htim6);
    HAL_TIM_Base_Start_IT(&htim7);
}

CCMRAM void Cap_ErrorHandle(void)
{
    /*===| 软件开关电容 |===*/
        if(CapControl.Enable_Flag == 0)
        {
            HRTIM_Output_Stop();
            LED_Set(G, Flash);
        }
        else
        {
            if(LED_G_Flash_Flag == 1) LED_Set(G, OFF);
        }
    
    /*===| 底盘断电 |===*/
        if(CapData.A_Power_V <= 19.0f)
        {
            HRTIM_Output_Stop();
            CapControl.Connect_Error_Flag = 1; 
            LED_Set(B, Flash);
        }
        else if(CapData.A_Power_V >= 20.0f)
        {
            CapControl.Connect_Error_Flag = 0; 
            if(LED_B_Flash_Flag == 1) LED_Set(B, OFF);
        }
            
    /*===| 过压 |===*/
        if(CapData.A_Power_V >= Safe_RefMax_V || CapData.B_Power_V >= Safe_CupMax_V)
        {
            CapControl.V_Error_Flag = 1;
            HRTIM_Output_Stop();
            LED_Set(R, ON);
        }
        else if(CapData.A_Power_V <= Safe_RefMax_V - 1 && CapData.B_Power_V <= Safe_CupMax_V - 1)
        {
            CapControl.V_Error_Flag = 0;
            if(LED_R_Flash_Flag == 1) LED_Set(R, OFF);
        }
        
//    /*===| 过流 |===*/
//        if(fabs(CapData.A_Power_I) >= 40.0f || fabs(CapData.B_Power_I) >= 40.0f || fabs(CapData.Referee_I) >= 40.0f)
//        {
//            CapControl.I_Error_Flag = 1;
//            HRTIM_Output_Stop();
//            LED_Set(R, Flash);
//        }
//        else 
//        {
////            CapControl.I_Error_Flag = 0;
////            LED_Set(G, OFF);
//        }
        
    /*===| 重启 |===*/
        if(CapControl.V_Error_Flag == 0 && CapControl.I_Error_Flag == 0 && CapControl.Connect_Error_Flag == 0 && CapControl.Enable_Flag)
        {
            if(CapControl.PWM_Out_Enable == 0) 
            {
                /*===| 首次解决问题 |===*/
                HRTIM_Output_Start();
                LED_ALL_1Hz_Flash_Set(1);
                PowerBuffer_PID_Struct.Output = 0;
                Referee_P_PID_Struct  .Output = 0;
                Cap_Vmax_PID_Struct   .Output = 0;
                Cap_Vmin_PID_Struct   .Output = 0;
                CAP_P_PID_Struct      .Output = 0;
            }
            
            CapControl.PWM_Out_Enable = 1; 
        }
        else
        {
            if(CapControl.PWM_Out_Enable == 1) 
            {
                /*===| 首次出问题 |===*/
                LED_ALL_1Hz_Flash_Set(0);
            }
            
            PowerBuffer_PID_Struct.Output = 0;
            Referee_P_PID_Struct  .Output = 0;
            Cap_Vmax_PID_Struct   .Output = 0;
            Cap_Vmin_PID_Struct   .Output = 0;
            CAP_P_PID_Struct      .Output = 0;
            
            CapControl.PWM_Out_Enable = 0;
        }
}

CCMRAM void Cap_Calulate_VB_to_VA(void)
{
#if(IF_USED_AS_BAT_BUFFER)
    CapControl.Max_Referee_P = 200.0f;
    CapControl.Enable_Flag = 1;
    Capcontrol.Power_Buff_Energy = Power_Buff_Target_Energy;
#endif

    /*===| 计算充放电功率 |===*/
    if(CapControl.Connect_Error_Flag == 0 && CapControl.V_Error_Flag == 0 && CapControl.I_Error_Flag == 0 && CapControl.Enable_Flag)   
    {       
        /*===| 正常情况 |===*/  
        /*===| 计算期望充放电功率 |===*/
        //根据规则限制电容电流(<16A)
        float A_P_Max = MAX_B_Current * CapData.B_Power_V;
        if(A_P_Max > MAX_Power) A_P_Max = MAX_Power;
        Referee_P_PID_Struct.Output_Max = A_P_Max;
        Cap_Vmax_PID_Struct.Output_Max = A_P_Max;
        Cap_Vmin_PID_Struct.Output_Max = A_P_Max;
        
        PID_Calculate(&Referee_P_PID_Struct, CapControl.Max_Referee_P - PowerBuffer_PID_Struct.Output, CapData.Referee_P); 
        PID_Calculate(&Cap_Vmax_PID_Struct, CAP_MAX_V, CapData.B_Power_V); 
        PID_Calculate(&Cap_Vmin_PID_Struct, CAP_MIN_V, CapData.B_Power_V); 
        
        /*===| 根据超电电压限制充放电功率 |===*/
        if      (Cap_Vmax_PID_Struct.Output < Referee_P_PID_Struct.Output)
        {
            CapControl.Target_A_P = Cap_Vmax_PID_Struct.Output;
        }
        else if (Cap_Vmin_PID_Struct.Output > Referee_P_PID_Struct.Output)
        {
            CapControl.Target_A_P = Cap_Vmin_PID_Struct.Output;
        }
        else
        {
            CapControl.Target_A_P = Referee_P_PID_Struct.Output;
        }
        
        /*===| 如果超电电压过低，先30w充电到最小工作电压再闭环 |===*/
        if(CapData.B_Power_V < CAP_MIN_V - 2.0f) CapControl.Cap_Empty_Flag = 1;
        else if(CapData.B_Power_V > CAP_MIN_V + 1.0f) CapControl.Cap_Empty_Flag = 0;
        
        if(CapControl.Cap_Empty_Flag) 
        {
            CapControl.Target_A_P = 30.0f; 
            Referee_P_PID_Struct.Output = 0;
        }
    }
    else
    {   
        /*===| 如果有错误情况，保持无电流 |===*/
        CapControl.Target_A_P = 0;
    }   
        
    /*===| 根据期望充放电功率计算变压比，从而控制电流 |===*/
    PID_Calculate(&CAP_P_PID_Struct, CapControl.Target_A_P, CapData.A_Power_P);
    
    
    /*===| 计算变压比 |===*/
//    CapControl.VB_to_VA_Set = (24.0f + CAP_P_PID_Struct.Output) / 24.0f;
    CapControl.VB_to_VA_Set = (CapData.B_Power_V + CAP_P_PID_Struct.Output) / CapData.A_Power_V;
    
    if(CapControl.VB_to_VA_Set > 5.0f) CapControl.VB_to_VA_Set = 5.0f;
    if(CapControl.VB_to_VA_Set < 0.2f) CapControl.VB_to_VA_Set = 0.2f;
}

SuperCap_Control_Data_StructTypedef SuperCap_Control_Data;
CCMRAM void Cap_Unpack_Control_Data(uint8_t *Data)
{
#if(IF_USED_AS_BAT_BUFFER != 1)
    memcpy((uint8_t*)&SuperCap_Control_Data, Data, sizeof(SuperCap_Control_Data_StructTypedef));
    
    CapControl.Enable_Flag = SuperCap_Control_Data.Enable_Flag;
    CapControl.Max_Referee_P = SuperCap_Control_Data.Max_Referee_P;
//    CapControl.Max_Referee_P = 15;
    CapControl.Power_Buff_Energy = SuperCap_Control_Data.Power_Buff_Energy;
    
    /*===| 通过缓冲区能量修正误差 |===*/
    PID_Calculate(&PowerBuffer_PID_Struct, Power_Buff_Target_Energy, CapControl.Power_Buff_Energy); 
    if(CapData.B_Power_V >= CAP_MAX_V) PowerBuffer_PID_Struct.Output = 0;
#endif
}

CCMRAM void Cap_Send_FeedBack_Data(void)
{
#if(IF_USED_AS_BAT_BUFFER != 1)
    SuperCap_FeedBack_Data_StructTypedef SuperCap_FeedBack_Dat;
    SuperCap_FeedBack_Dat.State = CapControl.Enable_Flag;
    SuperCap_FeedBack_Dat.SuperCap_V_x100 = CapData.B_Power_V * 100;
    if(CapData.Referee_P > 0)
    {
        SuperCap_FeedBack_Dat.Referee_P_x10 = (uint16_t)(CapData.Referee_P * 10);
    }
    else
    {
        SuperCap_FeedBack_Dat.Referee_P_x10 = 0;
    }
    if(CapData.Chassis_P > 0)
    {
        SuperCap_FeedBack_Dat.Chassis_P_x10 = (uint16_t)(CapData.Chassis_P * 10);
    }
    else
    {
        SuperCap_FeedBack_Dat.Chassis_P_x10 = 0;
    }
    FDCAN_Send_Data(&hfdcan2, CAP_CAN_TX_ID, (uint8_t*)&SuperCap_FeedBack_Dat);
#endif
}
