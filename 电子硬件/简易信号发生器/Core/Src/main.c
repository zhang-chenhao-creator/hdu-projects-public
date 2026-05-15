/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "dac.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "oled.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include <stdio.h>
uint8_t uart_recv[256], recv_data[256];
uint8_t u5_recv_len, u5_recv_data, recv_len;
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
#define WAVE_SAMPLES 100
uint16_t triangle_wave[WAVE_SAMPLES];
uint16_t sawtooth_wave[WAVE_SAMPLES];
uint16_t sine_wave[WAVE_SAMPLES];
uint8_t wave_type = 0; // 0: Square, 1: Triangle, 2: Sawtooth, 3: Sine
uint32_t current_freq = 20;
uint8_t duty_cycle = 50;
uint8_t display_page = 0; // 0: Name/ID, 1: Mode, 2: Freq, 3: Duty
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Update_OLED_Display(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void Config_PA5_Mode(uint8_t mode) // 0: TIM2 (AF), 1: DAC (Analog)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    
    if (mode == 0) // TIM2
    {
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    }
    else // DAC
    {
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    }
    
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Generate_Triangle_Wave(void)
{
    for (int i = 0; i < WAVE_SAMPLES; i++)
    {
        if (i < WAVE_SAMPLES / 2)
        {
            triangle_wave[i] = (i * 4095) / (WAVE_SAMPLES / 2);
        }
        else
        {
            triangle_wave[i] = ((WAVE_SAMPLES - i) * 4095) / (WAVE_SAMPLES / 2);
        }
    }
}

void Generate_Sawtooth_Wave(void)
{
    for (int i = 0; i < WAVE_SAMPLES; i++)
    {
        sawtooth_wave[i] = (i * 4095) / WAVE_SAMPLES;
    }
}

void Generate_Sine_Wave(void)
{
    for (int i = 0; i < WAVE_SAMPLES; i++)
    {
        sine_wave[i] = (uint16_t)((sin(i * 2 * 3.1415926535 / WAVE_SAMPLES) + 1) * 2047.5);
    }
}

void Set_DAC_Wave_Freq(uint32_t freq)
{
    if (freq < 10) freq = 10;
    if (freq > 10000) freq = 10000;

    // TIM6 Clock is 84MHz (APB1 x 2)
    // Update Rate = freq * WAVE_SAMPLES
    // Timer Freq = 84000000 / (PSC + 1) / (ARR + 1)
    
    uint32_t psc = 0;
    uint32_t arr = 0;
    uint32_t target_timer_freq = freq * WAVE_SAMPLES;

    if (target_timer_freq < 100000) // Low frequency
    {
        psc = 83; // 1MHz clock
        arr = (1000000 / target_timer_freq) - 1;
    }
    else
    {
        psc = 0; // 84MHz clock
        arr = (84000000 / target_timer_freq) - 1;
    }

    __HAL_TIM_SET_PRESCALER(&htim6, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim6, arr);
}

void Update_OLED_Display(void)
{
    OLED_Clear();
    char buff[32];
    
    switch(display_page)
    {
        case 0: // Demo info
            OLED_ShowString(0, 0, "Author", 16);
            OLED_ShowString(0, 2, "Demo ID", 16);
            OLED_ShowString(0, 4, "Contributor A", 16);
            OLED_ShowString(0, 6, "Demo ID", 16);
            break;
            
        case 1: // Waveform Mode
            if (wave_type == 0) OLED_ShowString(0, 2, "Mode: Square", 16);
            else if (wave_type == 1) OLED_ShowString(0, 2, "Mode: Triangle", 16);
            else if (wave_type == 2) OLED_ShowString(0, 2, "Mode: Sawtooth", 16);
            else if (wave_type == 3) OLED_ShowString(0, 2, "Mode: Sine", 16);
            break;
            
        case 2: // Frequency
            sprintf(buff, "Freq: %lu Hz", current_freq);
            OLED_ShowString(0, 2, buff, 16);
            break;
            
        case 3: // Duty Cycle
            if (wave_type == 0) 
            {
                sprintf(buff, "Duty: %d%%", duty_cycle);
                OLED_ShowString(0, 2, buff, 16);
            }
            else OLED_ShowString(0, 2, "Duty: --", 16);
            break;
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
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_UART5_Init();
  MX_I2C1_Init(); 
  MX_ADC1_Init();
  MX_DAC_Init();
  /* USER CODE BEGIN 2 */
  Generate_Triangle_Wave();
  Generate_Sawtooth_Wave();
  Generate_Sine_Wave();
  
  // Default Start with Square Wave
  Config_PA5_Mode(0); // Set PA5 to TIM2 Mode
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  Set_Square_Wave(current_freq, duty_cycle); 

  OLED_Init();  // 初始化OLED
  Update_OLED_Display();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint16_t adc_val = 0;
  uint8_t btn_prev_sw2 = 1;
  
  // SW2 (PE3) State Variables
  uint8_t btn_sw2_prev = 1;
  uint32_t btn_sw2_start = 0;
  uint8_t btn_sw2_pressed = 0;
  uint8_t btn_sw2_long_handled = 0;
  
  // SW3 (PE5) State Variables
  uint8_t btn_sw3_prev = 1;
  uint32_t btn_sw3_start = 0;
  uint8_t btn_sw3_pressed = 0;
  uint8_t btn_sw3_long_handled = 0;
  
  // Menu State Variables
  uint8_t showing_menu = 0;
  uint8_t menu_select_page = 0;
  uint8_t menu_switched = 0;
  
  while (1)
  {
    /* USER CODE END WHILE */
    
    // Button Handling (PE3 - SW2, Active Low)
    // Short Press: Increase Frequency
    // Long Press: Increase Duty Cycle (Square Wave Only)
    uint8_t btn_curr_sw2 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3);
    
    if (btn_curr_sw2 == 0 && btn_sw2_prev == 1) // Press
    {
        btn_sw2_start = HAL_GetTick();
        btn_sw2_pressed = 1;
        btn_sw2_long_handled = 0;
    }
    else if (btn_curr_sw2 == 1 && btn_sw2_prev == 0) // Release
    {
        if (btn_sw2_pressed && !btn_sw2_long_handled)
        {
             // Short Press: Increase Frequency
             if (current_freq < 100) current_freq += 10;
             else if (current_freq < 1000) current_freq += 100;
             else if (current_freq < 10000) current_freq += 1000;
             else current_freq = 10; // Reset
             
             // Update Waveform
             if (wave_type == 0) Set_Square_Wave(current_freq, duty_cycle);
             else Set_DAC_Wave_Freq(current_freq);
             
             Update_OLED_Display();
        }
        btn_sw2_pressed = 0;
    }
    
    // Check for Long Press SW2
    if (btn_curr_sw2 == 0 && btn_sw2_pressed && !btn_sw2_long_handled)
    {
        if (HAL_GetTick() - btn_sw2_start > 1000) // > 1 second
        {
            // Long Press: Increase Duty Cycle
            if (wave_type == 0) // Only for Square Wave
            {
                duty_cycle += 10;
                if (duty_cycle > 90) duty_cycle = 10;
                
                Set_Square_Wave(current_freq, duty_cycle);
                Update_OLED_Display();
            }
            btn_sw2_long_handled = 1;
        }
    }
    btn_sw2_prev = btn_curr_sw2;
    
    // Button Handling (PE5 - SW3, Active Low)
    // Short Press: Switch Waveform
    // Long Press: Switch Display Page
    uint8_t btn_curr_sw3 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_5);
    
    if (btn_curr_sw3 == 0 && btn_sw3_prev == 1) // Press
    {
        btn_sw3_start = HAL_GetTick();
        btn_sw3_pressed = 1;
        btn_sw3_long_handled = 0;
        showing_menu = 0;
    }
    else if (btn_curr_sw3 == 1 && btn_sw3_prev == 0) // Release
    {
        if (btn_sw3_pressed)
        {
            if (btn_sw3_long_handled)
            {
                // Long Press Handled on Release (Confirm Menu Selection)
                if (showing_menu)
                {
                    display_page = menu_select_page;
                    Update_OLED_Display();
                    showing_menu = 0;
                }
            }
            else
            {
                // Short Press Action: Switch Waveform
                wave_type++;
                if (wave_type > 3) wave_type = 0;
                
                if (wave_type == 0) // Switch to Square
                {
                    HAL_TIM_Base_Stop(&htim6);
                    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2);
                    
                    Config_PA5_Mode(0); // Set PA5 to TIM2 Mode
                    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
                    Set_Square_Wave(current_freq, duty_cycle);
                }
                else if (wave_type == 1) // Switch to Triangle
                {
                    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
                    HAL_TIM_Base_Stop(&htim6);
                    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2);
                    
                    Config_PA5_Mode(1); // Set PA5 to DAC Mode
                    Set_DAC_Wave_Freq(current_freq);
                    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_2, (uint32_t*)triangle_wave, WAVE_SAMPLES, DAC_ALIGN_12B_R);
                    HAL_TIM_Base_Start(&htim6);
                }
                else if (wave_type == 2) // Switch to Sawtooth
                {
                    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
                    HAL_TIM_Base_Stop(&htim6);
                    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2);
                    
                    Config_PA5_Mode(1); // Set PA5 to DAC Mode
                    Set_DAC_Wave_Freq(current_freq);
                    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_2, (uint32_t*)sawtooth_wave, WAVE_SAMPLES, DAC_ALIGN_12B_R);
                    HAL_TIM_Base_Start(&htim6);
                }
                else if (wave_type == 3) // Switch to Sine
                {
                    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
                    HAL_TIM_Base_Stop(&htim6);
                    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2);
                    
                    Config_PA5_Mode(1); // Set PA5 to DAC Mode
                    Set_DAC_Wave_Freq(current_freq);
                    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_2, (uint32_t*)sine_wave, WAVE_SAMPLES, DAC_ALIGN_12B_R);
                    HAL_TIM_Base_Start(&htim6);
                }
                Update_OLED_Display();
            }
        }
        btn_sw3_pressed = 0;
    }
    
    // Check for Long Press
    if (btn_curr_sw3 == 0 && btn_sw3_pressed)
    {
        uint32_t press_duration = HAL_GetTick() - btn_sw3_start;
        
        if (press_duration > 1000) // 1 second threshold
        {
            if (!btn_sw3_long_handled)
            {
                // Enter Menu Mode
                btn_sw3_long_handled = 1; 
                showing_menu = 1;
                menu_select_page = display_page;
                menu_switched = 0;
                OLED_Show_Menu(menu_select_page); // Show initial menu
            }
            
            // Animation: Switch to next page after 500ms more (Total 1.5s press)
            if (showing_menu && !menu_switched && (press_duration > 1500))
            {
                menu_select_page++;
                if (menu_select_page > 3) menu_select_page = 0;
                OLED_Show_Menu(menu_select_page);
                menu_switched = 1;
            }
        }
    }
    
    btn_sw3_prev = btn_curr_sw3;

    // 启动ADC转换
    HAL_ADC_Start(&hadc1);
    // 等待转换完成
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        // 读取ADC值
        adc_val = HAL_ADC_GetValue(&hadc1);
        // 发送数据到串口
        UART_Send_Waveform_Point(adc_val);
    }
    
    // 延时，控制采样率。
    // 去掉延时，全速发送，利用串口发送阻塞来控制速率，以获得最大采样率
    // HAL_Delay(1); 
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168; // VCO = 16MHz / 8 * 168 = 336MHz
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4; // SYSCLK = 336MHz / 4 = 84MHz
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; // 42MHz -> Timers 84MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; // 84MHz -> Timers 84MHz

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) // 84MHz requires 2 wait states
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
