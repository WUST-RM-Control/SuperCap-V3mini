/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "fdcan.h"
#include "hrtim.h"
#include "i2c.h"
#include "opamp.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SuperCap_Control.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t Communicate_Cooldown;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint32_t DWT_Counter;
float Main_Dt;
float Main_Hz;

uint8_t IF_OLED_Online;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
//    if(htim == &htim6) //50kHz
//    {
//        /*===| ADC数据更新处理 |===*/
//        ADC_Update();

//        /*===| 计算变压比 |===*/
//        Cap_Calulate_VB_to_VA();

//        /*===| 设置变压比 |===*/
//        Buck_Boost_Control(CapControl.VB_to_VA_Set);
//    }
    if(htim == &htim7) //1kHz
    {
        static uint16_t Ticker_1000ms, Ticker_100ms, Ticker_10ms;
        Ticker_1000ms++; Ticker_100ms++, Ticker_10ms++;
  
        /*===| 1kHz |===*/
        { 
            /*===| 错误判断与处理 |===*/
            Cap_ErrorHandle();
            
        }
        
        /*===| 100Hz |===*/
        if(Ticker_10ms >= 10)                                      
        {
            /*===| 发送反馈帧 |===*/
            Cap_Send_FeedBack_Data();
//            printf("%.2f,%.2f\n",CapData.B_Power_V, CapData.Referee_P);
            
            /*===| 如果通信中断500ms，则关闭输出 |===*/
            Communicate_Cooldown++;
            if(Communicate_Cooldown >= 50)
            {
                CapControl.Enable_Flag = 0;
                Communicate_Cooldown = 50;
            }
            
            Ticker_10ms = 0;
        }
        
        /*===| 10Hz |===*/
        if(Ticker_100ms >= 100)
        {
            /*===| 控制LED状态 |===*/
            LED_Flash_100ms_Loop();
            
             /*===| OLED显示数据 |===*/
            if(IF_OLED_Online)
            {
                OLED_Printf(0,0*8,OLED_6X8,"RefereeP =%+06.2f   ", CapData.Referee_P);
                OLED_Printf(0,1*8,OLED_6X8,"TargetP =%+06.2f   ", CapControl.Target_A_P);
                
                OLED_Printf(0,2*8,OLED_6X8,"AV =%+05.2f   ", CapData.A_Power_V);
                OLED_Printf(0,3*8,OLED_6X8,"AI =%+05.2f   ", CapData.A_Power_I);
                OLED_Printf(0,4*8,OLED_6X8,"AP =%+05.1f   ", CapData.A_Power_P);
                OLED_Printf(64,2*8,OLED_6X8,"BV =%+05.2f   ", CapData.B_Power_V);
                OLED_Printf(64,3*8,OLED_6X8,"BI =%+05.2f   ", CapData.B_Power_I);
                OLED_Printf(64,4*8,OLED_6X8,"BP =%+05.1f   ", CapData.B_Power_P);
                OLED_Printf(0,5*8,OLED_6X8,"mainFre%+06.1fHz ", Main_Hz); 
                OLED_Printf(0,6*8,OLED_6X8,"B/A=%+04.3f            ", CapControl.VB_to_VA_Set); 
                OLED_Printf(0,6*9,OLED_6X8,"A%+04.3f", CapControl.A_Duty);
                OLED_Printf(64,6*9,OLED_6X8,"B%+04.3f", CapControl.B_Duty);

                OLED_Update();
            }    
             
            Ticker_100ms = 0;
        }
        
        if(Ticker_1000ms >= 1000) //1Hz
        {
           
            
            Ticker_1000ms = 0;
            
            if(CapControl.Enable_Flag == 0)
                HAL_NVIC_SystemReset();
        }
    }
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_ADC5_Init();
  MX_OPAMP1_Init();
  MX_OPAMP2_Init();
  MX_OPAMP3_Init();
  MX_OPAMP5_Init();
  MX_OPAMP6_Init();
  MX_FDCAN2_Init();
  MX_HRTIM1_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
    HAL_Delay(300);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /*===| 模块初始化 |===*/
    DWT_Init(170);
    Cap_Init();

  /*===| 自检 |===*/
  for(uint8_t i = 0; i <= 64; i++)
  {
    /*===| ADC数据更新处理 |===*/
    ADC_Update();

    /*===| 错误判断与处理 |===*/
    Cap_ErrorHandle();
  }
  
  uint8_t Data[2] = {0x00, 0xAF};
  if(HAL_I2C_Master_Transmit(&hi2c1, 0x78, Data, 2, 1) == HAL_OK) IF_OLED_Online = 1;
  else IF_OLED_Online = 0;
  
    
    
  while (1)
  {
      Main_Dt = DWT_GetDeltaT(&DWT_Counter);
      Main_Hz = 1.0f/Main_Dt;
    
    /*===| ADC数据更新处理 |===*/
    ADC_Update();

    /*===| 计算变压比 |===*/
    Cap_Calulate_VB_to_VA();

    /*===| 设置变压比 |===*/
    Buck_Boost_Control(CapControl.VB_to_VA_Set);
    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV3;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
