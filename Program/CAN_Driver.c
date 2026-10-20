#include "CAN_Driver.h"

extern uint8_t Communicate_Cooldown;

uint8_t FDCAN_RX_Data[8];
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    FDCAN_RxHeaderTypeDef RxHeader;
    HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, FDCAN_RX_Data);
    
    uint16_t CAN_ID = RxHeader.Identifier;
    
    if(hfdcan == &hfdcan2 && CAN_ID == CAP_CAN_RX_ID) 
    {
        Cap_Unpack_Control_Data(FDCAN_RX_Data);
        Communicate_Cooldown = 0;
    }
}

void FDCAN_Send_Data(FDCAN_HandleTypeDef *hfdcan, uint16_t ID, uint8_t *TX_Data)
{
    FDCAN_TxHeaderTypeDef TxHeader;
    TxHeader.Identifier = ID;
    TxHeader.IdType = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.DataLength = FDCAN_DLC_BYTES_8;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker = 0; 
    
    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, TX_Data);
}

void FDCAN_Init(void)
{
    HAL_FDCAN_Start(&hfdcan2);
    FDCAN_Filter_Init();
}

void FDCAN_Filter_Init(void) 
{
	FDCAN_FilterTypeDef sFilterConfig;

    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = 0x00;
    sFilterConfig.FilterID2 = 0x7FF;
    
    HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig);
    HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
    HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
}

