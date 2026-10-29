#ifndef __LED__
#define __LED__

#include "main.h"

typedef enum
{
    R = 0,
    G = 1,
    B = 2
} RGB_EnumTypedef;

typedef enum
{
    OFF = 0,
    ON = 1,
    Flash = 2
} LED_State_EnumTypedef;

void LED_Set(RGB_EnumTypedef RGB, LED_State_EnumTypedef State);
void LED_OFF(void);
void LED_ALL_1Hz_Flash_Set(uint8_t State);

void LED_Flash_100ms_Loop(void);

extern uint8_t LED_R_Flash_Flag;
extern uint8_t LED_G_Flash_Flag;
extern uint8_t LED_B_Flash_Flag;
extern uint8_t LED_ALL_Flash_Flag;

#endif
