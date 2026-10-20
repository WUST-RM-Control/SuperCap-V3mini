//=====================================================================================
//                    PID[PID计算]
// PID_Init(PID_Struct_TypeDef *PID_Struct, float Kp, float Ki, float Kd, float Kf, float I_Output_Max, float Output_Max)
// PID_Calculate(PID_Struct_TypeDef *PID_Struct, float Target, float Feedback)
//=====================================================================================

#include "PID.h"

/**
 * @brief PID前馈位置式计算
 *
 * @param PID数据结构体
 */
CCMRAM void PID_Calculate(PID_Struct_TypeDef *PID_Struct, float Target, float Feedback)
{ 
	/*===| 计算误差 |===*/
    PID_Struct->Error_Last_Last = PID_Struct->Error_Last;
    PID_Struct->Error_Last = PID_Struct->Error;
	PID_Struct->Error = Target - Feedback;
    
    /*===| 计算各部分输出 |===*/
    PID_Struct->Output += PID_Struct->Kp * (PID_Struct->Error - PID_Struct->Error_Last)
                        + PID_Struct->Ki * PID_Struct->Error
                        + PID_Struct->Kd * (PID_Struct->Error - 2 * PID_Struct->Error_Last + PID_Struct->Error_Last_Last);
    
    /*===| 输出限幅 |===*/
	if(PID_Struct->Output >  PID_Struct->Output_Max && PID_Struct->Output_Max != 0)	PID_Struct->Output =  PID_Struct->Output_Max;
	if(PID_Struct->Output < -PID_Struct->Output_Max && PID_Struct->Output_Max != 0)	PID_Struct->Output = -PID_Struct->Output_Max;
}


/**
 * @brief PID参数初始化
 *
 * @param PID结构体
 * @param Kp, Ki, Kd, Output_Max
 */
void PID_Init(PID_Struct_TypeDef *PID_Struct, float Kp, float Ki, float Kd, float Output_Max)
{
    PID_Struct->Kp = Kp;
    PID_Struct->Ki = Ki;
    PID_Struct->Kd = Kd;
    PID_Struct->Output_Max = Output_Max;
}

