#ifndef __CAN_Driver__
#define __CAN_Driver__

#include "main.h"
#define CAP_CAN_RX_ID 0x30
#define CAP_CAN_TX_ID 0x31


void FDCAN_Init(void);

void FDCAN_Send_Data(FDCAN_HandleTypeDef *hfdcan, uint16_t ID, uint8_t *TX_Data);

void FDCAN_Filter_Init(void);

#endif
