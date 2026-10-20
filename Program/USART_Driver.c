#include "USART_Driver.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>


uint8_t UART2_RxData[8] = {0};
extern uint16_t Communicate_Cooldown;

/**
 * @brief USART接收空闲事件回调函数
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart == &huart2)
	{
		HAL_UARTEx_ReceiveToIdle_DMA(&huart2, UART2_RxData, 8);
        
        if(UART2_RxData[7] == 0xAA && UART2_RxData[6] == 0x55)
        {
            Cap_Unpack_Control_Data(UART2_RxData);
            Communicate_Cooldown = 0;
        }
        
	}
}

/**
 * @brief 串口初始化
 *
 * @param 无
 */
void USART_Init(void)
{
    /*===| USART2 |===*/
//    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
//    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
	HAL_UARTEx_ReceiveToIdle_DMA(&huart2, (uint8_t *)UART2_RxData, 8);
}

/**
 * @brief 串口错误处理 解决调试断点导致串口接收出错的问题
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart == &huart2)
	{
        memset(&huart2,0,sizeof(UART_HandleTypeDef)); 
        HAL_UART_MspDeInit(&huart2);
        MX_USART2_UART_Init();
	}
    USART_Init();
}


int fputc(int ch,FILE *f)
{
	uint8_t temp[1] = {ch};
	HAL_UART_Transmit(&huart2, temp, 1, 100);
	return ch;
}
