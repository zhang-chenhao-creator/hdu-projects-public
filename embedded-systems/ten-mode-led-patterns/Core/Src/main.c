/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 流水灯控制程序 - 完全轮询版本
  * @note           : 不使用中断和RTOS
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum {
    LED_STYLE_0 = 0,  // 依次点亮
    LED_STYLE_1,       // 依次熄灭
    LED_STYLE_2,       // 两端向中间
    LED_STYLE_3,       // 中间向两端
    LED_STYLE_4        // 随机闪烁
} LED_Style_t;

typedef struct {
    uint8_t running;           // 运行状态 0-暂停 1-运行
    uint8_t speed_index;       // 速度索引 0-9
    uint16_t speed_delay[10];  // 速度档位 50-500ms
    LED_Style_t style;         // 当前样式
    uint8_t current_pos;       // 当前位置
    uint8_t direction;         // 方向（部分样式用）
    uint32_t last_update_time; // 上次更新时间
} LED_Control_t;

typedef struct {
    uint8_t current_state;
    uint8_t last_state;
    uint8_t stable_state;
    uint32_t last_debounce_time;
} Key_State_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
LED_Control_t led_ctrl;

Key_State_t key_run;
Key_State_t key_speed;
Key_State_t key_style;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void LED_Update(void);
void LED_Style0_Update(void);
void LED_Style1_Update(void);
void LED_Style2_Update(void);
void LED_Style3_Update(void);
void LED_Style4_Update(void);
void Set_LED(uint8_t led_num, uint8_t state);
void ALL_LED_Off(void);
void Key_Scan(void);
void Key_Process(void);
uint8_t Key_Read(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief 读取按键当前电平（已取反，按下为1）
  */
uint8_t Key_Read(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
    return !HAL_GPIO_ReadPin(GPIOx, GPIO_Pin);
}

/**
  * @brief 按键扫描（带软件消抖）
  */
void Key_Scan(void)
{
    uint32_t current_time = HAL_GetTick();
    
    // RUN
    uint8_t raw_run = Key_Read(RUN_GPIO_Port, RUN_Pin);
    if(raw_run != key_run.last_state)
    {
        key_run.last_debounce_time = current_time;
        key_run.last_state = raw_run;
    }
    if((current_time - key_run.last_debounce_time) > 20)
    {
        if(raw_run != key_run.stable_state)
        {
            key_run.stable_state = raw_run;
            if(key_run.stable_state == 1)
            {
                key_run.current_state = 1;
            }
        }
    }

    // SPEED
    uint8_t raw_speed = Key_Read(SPEED_GPIO_Port, SPEED_Pin);
    if(raw_speed != key_speed.last_state)
    {
        key_speed.last_debounce_time = current_time;
        key_speed.last_state = raw_speed;
    }
    if((current_time - key_speed.last_debounce_time) > 20)
    {
        if(raw_speed != key_speed.stable_state)
        {
            key_speed.stable_state = raw_speed;
            if(key_speed.stable_state == 1)
            {
                key_speed.current_state = 1;
            }
        }
    }

    // STYLE
    uint8_t raw_style = Key_Read(STYLE_GPIO_Port, STYLE_Pin);
    if(raw_style != key_style.last_state)
    {
        key_style.last_debounce_time = current_time;
        key_style.last_state = raw_style;
    }
    if((current_time - key_style.last_debounce_time) > 20)
    {
        if(raw_style != key_style.stable_state)
        {
            key_style.stable_state = raw_style;
            if(key_style.stable_state == 1)
            {
                key_style.current_state = 1;
            }
        }
    }
}

/**
  * @brief 按键处理
  */
void Key_Process(void)
{
    if(key_run.current_state == 1)
    {
        key_run.current_state = 0;
        led_ctrl.running = !led_ctrl.running;
        if(!led_ctrl.running)
        {
            ALL_LED_Off();
        }
    }
    
    if(key_speed.current_state == 1)
    {
        key_speed.current_state = 0;
        led_ctrl.speed_index = (led_ctrl.speed_index + 1) % 10;
    }
    
    if(key_style.current_state == 1)
    {
        key_style.current_state = 0;
        led_ctrl.style = (LED_Style_t)(((uint8_t)led_ctrl.style + 1) % 5);
        led_ctrl.current_pos = 0;
    }
}

/**
  * @brief LED样式总更新
  */
void LED_Update(void)
{
    switch(led_ctrl.style)
    {
        case LED_STYLE_0: LED_Style0_Update(); break;
        case LED_STYLE_1: LED_Style1_Update(); break;
        case LED_STYLE_2: LED_Style2_Update(); break;
        case LED_STYLE_3: LED_Style3_Update(); break;
        case LED_STYLE_4: LED_Style4_Update(); break;
        default: break;
    }
}

void LED_Style0_Update(void)
{
    ALL_LED_Off();
    Set_LED(led_ctrl.current_pos, 1);
    led_ctrl.current_pos = (led_ctrl.current_pos + 1) % 8;
}

void LED_Style1_Update(void)
{
    static uint8_t first = 1;
    if(first)
    {
        for(int i = 0; i < 8; i++) Set_LED(i, 1);
        first = 0;
    }
    Set_LED(led_ctrl.current_pos, 0);
    led_ctrl.current_pos = (led_ctrl.current_pos + 1) % 8;
    if(led_ctrl.current_pos == 0) first = 1;
}

void LED_Style2_Update(void)
{
    ALL_LED_Off();
    Set_LED(led_ctrl.current_pos, 1);
    Set_LED(7 - led_ctrl.current_pos, 1);
    led_ctrl.current_pos++;
    if(led_ctrl.current_pos >= 4) led_ctrl.current_pos = 0;
}

void LED_Style3_Update(void)
{
    ALL_LED_Off();
    Set_LED(3 - led_ctrl.current_pos, 1);
    Set_LED(4 + led_ctrl.current_pos, 1);
    led_ctrl.current_pos++;
    if(led_ctrl.current_pos >= 4) led_ctrl.current_pos = 0;
}

void LED_Style4_Update(void)
{
    ALL_LED_Off();
    uint8_t count = (HAL_GetTick() % 3) + 1;
    for(int i = 0; i < count; i++)
    {
        uint8_t led = (HAL_GetTick() + i * 37) % 8;
        Set_LED(led, 1);
    }
}

void Set_LED(uint8_t led_num, uint8_t state)
{
    GPIO_PinState pin_state = (state == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    switch(led_num)
    {
        case 0: HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, pin_state); break;
        case 1: HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, pin_state); break;
        case 2: HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, pin_state); break;
        case 3: HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, pin_state); break;
        case 4: HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, pin_state); break;
        case 5: HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, pin_state); break;
        case 6: HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, pin_state); break;
        case 7: HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, pin_state); break;
        default: break;
    }
}

void ALL_LED_Off(void)
{
    for(int i = 0; i < 8; i++)
    {
        Set_LED(i, 0);
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
  /* USER CODE BEGIN 2 */
  
  // 初始化LED控制结构体
  led_ctrl.running = 1;
  led_ctrl.speed_index = 4; // 默认250ms
  const uint16_t delays[10] = {50,100,150,200,250,300,350,400,450,500};
  for(int i = 0; i < 10; i++) led_ctrl.speed_delay[i] = delays[i];
  led_ctrl.style = LED_STYLE_0;
  led_ctrl.current_pos = 0;
  led_ctrl.direction = 0;
  led_ctrl.last_update_time = HAL_GetTick();

  // 初始化按键状态
  key_run.current_state = key_run.last_state = key_run.stable_state = 0;
  key_speed.current_state = key_speed.last_state = key_speed.stable_state = 0;
  key_style.current_state = key_style.last_state = key_style.stable_state = 0;

  ALL_LED_Off();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Key_Scan();
    Key_Process();

    uint32_t now = HAL_GetTick();
    if(led_ctrl.running && (now - led_ctrl.last_update_time) >= led_ctrl.speed_delay[led_ctrl.speed_index])
    {
        led_ctrl.last_update_time = now;
        LED_Update();
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_Delay(5); // 轻微延时降低CPU占用（可选）
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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
  while(1)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
    HAL_Delay(100);
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
  while(1)
  {
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
    HAL_Delay(500);
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
