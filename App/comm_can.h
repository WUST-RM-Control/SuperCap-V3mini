/**
  ******************************************************************************
  * @file    comm_can.h
  * @brief   FDCAN2：0x30 控制帧接收 / 0x31 反馈帧发送（协议与旧固件逐字节一致）
  ******************************************************************************
  */
#ifndef __COMM_CAN_H
#define __COMM_CAN_H

#include "main.h"
#include "params.h"

typedef __PACKED_STRUCT
{
    uint8_t  State;              /* 超电开关状态 0/1 */
    uint16_t SuperCap_V_x100;    /* 电容电压 x100 */
    uint16_t Referee_P_x10;      /* 电管功率 x10（负值截 0） */
    uint16_t Chassis_P_x10;      /* 底盘功率 x10（负值截 0） */
} Comm_Feedback_StructTypedef;   /* 7 字节，DLC 仍发 8（仅作文档参考，实际按字节组装） */

void Comm_Can_Init(void);
void Comm_Can_Send_Feedback(void);
void Comm_Can_Monitor(void);      /* 主循环里反复调用：总线异常自动恢复 */
uint8_t Comm_Can_Is_Online(void); /* 1 = 总线正常（能收发） */

#endif /* __COMM_CAN_H */
