/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "icache.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  GPIO_TypeDef *porta;
  uint16_t      pino;
  GPIO_PinState nivel_ativo;
  GPIO_PinState estavel;
  GPIO_PinState ultimo_lido;
  uint32_t      t_mudanca;
} botao_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED_EXT0_PIN   GPIO_PIN_9    /* PE9  - D6 (CN10-4)  */
#define LED_EXT1_PIN   GPIO_PIN_11   /* PE11 - D5 (CN10-6)  */
#define LED_EXT2_PIN   GPIO_PIN_13   /* PE13 - D3 (CN10-10) */
#define LED_EXT3_PIN   GPIO_PIN_14   /* PE14 - D4 (CN10-8)  */
#define LEDS_EXT_MASK  (LED_EXT0_PIN | LED_EXT1_PIN | LED_EXT2_PIN | LED_EXT3_PIN)
#define BOT_EXT0_PIN   GPIO_PIN_14   /* PD14 - D10 (CN7-16) */
#define DEBOUNCE_MS    20U
#define N_AMOSTRAS     8U

/* 1 = provoca uma divisao por zero de proposito (secao Fault Analyzer) */
#define DEMO_HARDFAULT 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
/* variaveis globais: aparecem em Live Expressions e no SWV */
uint8_t  contador     = 0;          /* valor mostrado nos 4 LEDs externos */
uint32_t apertos_b1   = 0;          /* quantas vezes o B1 foi pressionado */
uint32_t voltas_laco  = 0;          /* quantas vezes o laco principal rodou */
int32_t  onda         = 0;          /* onda triangular de 0 a 100 (grafico no SWV) */
uint8_t  amostras[N_AMOSTRAS] = { 10, 20, 30, 40, 50, 60, 70, 80 };
uint32_t media        = 0;

static const uint16_t led_ext_pino[4] = { LED_EXT0_PIN, LED_EXT1_PIN, LED_EXT2_PIN, LED_EXT3_PIN };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void gpio_externos_init(void)
{
  GPIO_InitTypeDef cfg = {0};

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOE, LEDS_EXT_MASK, GPIO_PIN_RESET);
  cfg.Pin   = LEDS_EXT_MASK;
  cfg.Mode  = GPIO_MODE_OUTPUT_PP;
  cfg.Pull  = GPIO_NOPULL;
  cfg.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &cfg);

  cfg.Pin  = BOT_EXT0_PIN;
  cfg.Mode = GPIO_MODE_INPUT;
  cfg.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &cfg);
}

static void leds_ext_escreve(uint8_t valor)
{
  uint32_t set = 0;
  for (int i = 0; i < 4; i++)
  {
    if (valor & (1U << i))
    {
      set |= led_ext_pino[i];
    }
  }
  GPIOE->BSRR = ((uint32_t)(LEDS_EXT_MASK & ~set) << 16) | set;
}

static uint8_t botao_pressionou(botao_t *b)
{
  GPIO_PinState lido  = HAL_GPIO_ReadPin(b->porta, b->pino);
  uint32_t      agora = HAL_GetTick();

  if (lido != b->ultimo_lido)
  {
    b->ultimo_lido = lido;
    b->t_mudanca   = agora;
  }
  else if ((lido != b->estavel) && ((agora - b->t_mudanca) >= DEBOUNCE_MS))
  {
    b->estavel = lido;
    if (lido == b->nivel_ativo)
    {
      return 1;
    }
  }
  return 0;
}

/* calcula a media das amostras -- contem um erro proposital (secao 5) */
static uint32_t calcula_media(const uint8_t *v, uint32_t n)
{
  uint32_t soma = 0;
  for (uint32_t i = 0; i <= n; i++)
  {
    soma += v[i];
  }
  return soma / n;
}

/* envia texto pelo canal 0 do ITM (aparece no SWV ITM Data Console) */
static void itm_puts(const char *s)
{
  while (*s)
  {
    ITM_SendChar((uint32_t)*s++);
  }
}

#if DEMO_HARDFAULT
static int32_t divide(int32_t a, int32_t b)
{
  return a / b;
}
#endif
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_ICACHE_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_YELLOW);
  BSP_LED_Init(LED_RED);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  printf("\r\nAula de depuracao - NUCLEO-H563ZI\r\n");
  gpio_externos_init();
  media = calcula_media(amostras, N_AMOSTRAS);

#if DEMO_HARDFAULT
  SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;   /* divisao inteira por zero gera excecao */
  volatile int32_t zero = 0;
  printf("resultado = %ld\r\n", divide(100, zero));
#endif

  botao_t  b1     = { GPIOC, GPIO_PIN_13, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, 0 };
  uint32_t t_led  = HAL_GetTick();
  uint32_t t_onda = HAL_GetTick();
  int32_t  passo  = 1;
  while (1)
  {
    voltas_laco++;

    if (HAL_GetTick() - t_onda >= 10U)          /* a cada 10 ms */
    {
      t_onda += 10U;
      onda += passo;
      if ((onda >= 100) || (onda <= 0))
      {
        passo = -passo;
      }
    }

    if (HAL_GetTick() - t_led >= 500U)          /* a cada 500 ms */
    {
      t_led += 500U;
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
      contador = (contador + 1U) & 0x0FU;
      leds_ext_escreve(contador);
      if (contador == 0U)
      {
        itm_puts("contador voltou a zero\n");
      }
    }

    if (botao_pressionou(&b1))
    {
      apertos_b1++;
      printf("B1 pressionado %lu vezes\r\n", apertos_b1);
    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 250;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* USER CODE BEGIN 4 *//* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x08FFF000;
  MPU_InitStruct.LimitAddress = 0x08FFFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
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
