#include "calib.h"

/* 标定系数定义（旧 ADC_Driver.c 的全局量） */
float Power_A_V_Adjust  = 1.0f;
float Power_B_V_Adjust  = 1.0f;
float Power_A_I_Adjust  = 0.0f;
float Power_B_I_Adjust  = 0.0f;
float Referee_I_Adjust  = 0.0f;

void Calib_Load(uint32_t UP_Board_ID)
{
    switch(UP_Board_ID)
    {
        case 0x0029003D: //0
        {
            Power_A_V_Adjust  = 24.0f / 23.9636707f; 
            Power_B_V_Adjust  = 24.0f / 23.8395996f;
            Power_A_I_Adjust  = 0 - (-0.100708f);
            Power_B_I_Adjust  = 0 - (-0.12084841f);
            Referee_I_Adjust  = 0 - (-0.0905675909f);
            break;
        }
        case 0x002C0046: //1
        {
            Power_A_V_Adjust  = 20.01f / 19.9756336f;
            Power_B_V_Adjust  = 20.01f / 19.860424f;
            Power_A_I_Adjust  = 0 - (-0.0402837954f);
            Power_B_I_Adjust  = 0 - (-0.0604242049f);
            Referee_I_Adjust  = 0 - (-0.0604242049f);
            break;
        }
        case 0x00240038: //3
        {
            Power_A_V_Adjust  = 20.01f / 19.860424f;
            Power_B_V_Adjust  = 20.01f / 19.8692875f;
            Power_A_I_Adjust  = 0 - (-0.0805675909f);
            Power_B_I_Adjust  = 0 - (-0.0805675909f);
            Referee_I_Adjust  = 0 - (-0.0441404076f);
            break;
        }
        case 0x004B0020: //4
        {
            Power_A_V_Adjust  = 20.0f / 19.913599;
            Power_B_V_Adjust  = 20.0f / 19.8692875f;
            Power_A_I_Adjust  = 0 - (-0.0705675909f);
            Power_B_I_Adjust  = 0 - (-0.0502837954);
            Referee_I_Adjust  = 0 - (-0.0774242049);
            break;
        }
        case 0x00270051: //mini1
        {
            Power_A_V_Adjust  = 24.0f / 23.9725342f;
            Power_B_V_Adjust  = 24.0f / 23.8927727f;
            Power_A_I_Adjust  = 0 - (-0.05f);
            Power_B_I_Adjust  = 0 - (-0.05f);
            Referee_I_Adjust  = 0 - (-0.03f);
            break;
        }
        case 0x00270052: //mini2
        {
            Power_A_V_Adjust  = 24.0f / 23.8484612f;
            Power_B_V_Adjust  = 24.0f / 23.8927727f;
            Power_A_I_Adjust  = 0 - (-0.0402832031f);
            Power_B_I_Adjust  = 0 - (-0.0704956055f);
            Referee_I_Adjust  = 0 - (-0.0503540039f);
            break;
        }
        case 0x0026004D: //mini3
        {
            Power_A_V_Adjust  = 24.0f / 23.9813957f;
            Power_B_V_Adjust  = 24.0f / 23.9016361f;
            Power_A_I_Adjust  = 0 - (-0.05f);
            Power_B_I_Adjust  = 0 - (-0.07f);
            Referee_I_Adjust  = 0 - (-0.02f);
            break;
        }
        case 0x00280003: //mini4
        {
            Power_A_V_Adjust  = 24.0f / 23.7952881;
            Power_B_V_Adjust  = 24.0f / 23.9370842;
            Power_A_I_Adjust  = 0 - (-0.090637207);
            Power_B_I_Adjust  = 0 - (0);
            Referee_I_Adjust  = 0.02 - (-0.0604248047);
            break;
        }
        case 0x00210003: //mini5
        {
            Power_A_V_Adjust  = 24.0f / 23.9103592;
            Power_B_V_Adjust  = 24.0f / 23.8927727;
            Power_A_I_Adjust  = 0 - (-0.0302124023);
            Power_B_I_Adjust  = 0 - (-0.0603540039);
            Referee_I_Adjust  = 0.02 - (-0.06042480477);
            break;
        }
        case 0x00300003: //mini6
        {
            Power_A_V_Adjust  = 24.0f / 23.8661861;
            Power_B_V_Adjust  = 24.0f / 23.7775631;
            Power_A_I_Adjust  = 0 - (0.0101416016);
            Power_B_I_Adjust  = 0 - (-0.110778809);
            Referee_I_Adjust  = 0.02 - (-0.06042480477);
            break;
        }
        case 0x00060030: //mini7
        {
            Power_A_V_Adjust  = 24.0f / 23.8484612;
            Power_B_V_Adjust  = 24.0f / 23.9459476;
            Power_A_I_Adjust  = 0 - (-0.0704956055);
            Power_B_I_Adjust  = 0 - (-0.100708008);
            Referee_I_Adjust  = 0.02 - (-0.05637207);
            break;
        }
        default: //默认
        {
            Power_A_V_Adjust  = 1.0f / 1.0f;
            Power_B_V_Adjust  = 1.0f / 1.0f;
            Power_A_I_Adjust  = 0 - (0);
            Power_B_I_Adjust  = 0 - (0);
            Referee_I_Adjust  = 0 - (0);
            break;
        }
    }
}