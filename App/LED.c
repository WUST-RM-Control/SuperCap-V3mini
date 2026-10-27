#include "LED.h"

uint8_t LED_R_Flash_Flag;
uint8_t LED_G_Flash_Flag;
uint8_t LED_B_Flash_Flag;

uint8_t LED_ALL_Flash_Flag;

void LED_Set(RGB_EnumTypedef RGB, LED_State_EnumTypedef State)
{
    if(State != Flash)
    {
        if(RGB == R) {HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, (GPIO_PinState)!State); LED_R_Flash_Flag = 0;}
        if(RGB == G) {HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, (GPIO_PinState)!State); LED_G_Flash_Flag = 0;}
        if(RGB == B) {HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, (GPIO_PinState)!State); LED_B_Flash_Flag = 0;}
    }
    else
    {
        if      (RGB == R) LED_R_Flash_Flag = 1; 
        else if (RGB == G) LED_G_Flash_Flag = 1;
        else if (RGB == B) LED_B_Flash_Flag = 1;
    }
}

void LED_OFF(void)
{
    LED_Set(R, OFF);
    LED_Set(G, OFF);
    LED_Set(B, OFF);
}

void LED_ALL_1Hz_Flash_Set(uint8_t State)
{
    LED_ALL_Flash_Flag = State;
    LED_Set(R, (LED_State_EnumTypedef)State);
    LED_Set(G, (LED_State_EnumTypedef)State);
    LED_Set(B, (LED_State_EnumTypedef)State);
}

void LED_Flash_100ms_Loop(void)
{
    if(LED_ALL_Flash_Flag)
    {
        static uint8_t Ticker_500ms;
        Ticker_500ms++;
        if(Ticker_500ms >= 5)
        {
            HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);
            HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin);
            HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
            Ticker_500ms = 0;
        }
    }
    else
    {
        if(LED_R_Flash_Flag) HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);
        if(LED_G_Flash_Flag) HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin);
        if(LED_B_Flash_Flag) HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
    }
    
}
