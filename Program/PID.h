#ifndef __PID__
#define __PID__

#include "main.h"


/*===| PID结构体定义 |===*/
typedef struct
{
    float Kp;
    float Ki;
    float Kd;
    
    float Error;
    float Error_Last;    
    float Error_Last_Last; 
    
    float Output;
    
    float Output_Max;
} PID_Struct_TypeDef;


void PID_Calculate(PID_Struct_TypeDef *PID_Struct, float Target, float Feedback);

void PID_Init(PID_Struct_TypeDef *PID_Struct, float Kp, float Ki, float Kd, float Output_Max);


#endif
