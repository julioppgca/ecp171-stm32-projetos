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
  GPIO_PinState nivel_ativo;   /* nivel lido quando o botao esta pressionado */
  GPIO_PinState estavel;       /* ultimo estado considerado estavel          */
  GPIO_PinState ultimo_lido;   /* leitura anterior (ainda pode estar quicando) */
  uint32_t      t_mudanca;     /* instante (ms) da ultima mudanca da leitura */
} botao_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* LEDs externos (GPIOE) e botoes externos (GPIOD) */
#define LED_EXT0_PIN   GPIO_PIN_9    /* PE9  - D6 (CN10-4)  */
#define LED_EXT1_PIN   GPIO_PIN_11   /* PE11 - D5 (CN10-6)  */
#define LED_EXT2_PIN   GPIO_PIN_13   /* PE13 - D3 (CN10-10) */
#define LED_EXT3_PIN   GPIO_PIN_14   /* PE14 - D4 (CN10-8)  */
#define LEDS_EXT_MASK  (LED_EXT0_PIN | LED_EXT1_PIN | LED_EXT2_PIN | LED_EXT3_PIN)
#define BOT_EXT0_PIN   GPIO_PIN_14   /* PD14 - D10 (CN7-16) */
#define BOT_EXT1_PIN   GPIO_PIN_15   /* PD15 - D9  (CN7-18) */
#define DEBOUNCE_MS    20U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
static const uint16_t led_ext_pino[4] = { LED_EXT0_PIN, LED_EXT1_PIN, LED_EXT2_PIN, LED_EXT3_PIN };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void gpio_externos_init_hal(void)
{
  GPIO_InitTypeDef cfg = {0};

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOE, LEDS_EXT_MASK, GPIO_PIN_RESET);  /* nivel inicial antes de virar saida */
  cfg.Pin   = LEDS_EXT_MASK;
  cfg.Mode  = GPIO_MODE_OUTPUT_PP;
  cfg.Pull  = GPIO_NOPULL;
  cfg.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &cfg);

  cfg.Pin  = BOT_EXT0_PIN | BOT_EXT1_PIN;
  cfg.Mode = GPIO_MODE_INPUT;
  cfg.Pull = GPIO_PULLUP;                /* botao liga o pino ao GND: ativo em 0 */
  HAL_GPIO_Init(GPIOD, &cfg);
}

static void gpio_externos_init_reg(void)
{
  /* 1. clock das portas D e E (RCC_AHB2ENR, bits GPIODEN e GPIOEEN) */
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN | RCC_AHB2ENR_GPIOEEN;
  (void)RCC->AHB2ENR;                    /* leitura de volta: garante o clock antes do acesso */

  /* 2. PE9, PE11, PE13, PE14: saida push-pull, low speed, sem pull */
  GPIOE->BSRR    = (uint32_t)LEDS_EXT_MASK << 16;              /* ODR = 0 nesses pinos */
  GPIOE->MODER   = (GPIOE->MODER & ~(GPIO_MODER_MODE9 | GPIO_MODER_MODE11 |
                                     GPIO_MODER_MODE13 | GPIO_MODER_MODE14))
                 | GPIO_MODER_MODE9_0 | GPIO_MODER_MODE11_0
                 | GPIO_MODER_MODE13_0 | GPIO_MODER_MODE14_0;  /* 01 = saida */
  GPIOE->OTYPER  &= ~(uint32_t)LEDS_EXT_MASK;                  /* 0 = push-pull */
  GPIOE->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED11 |
                      GPIO_OSPEEDR_OSPEED13 | GPIO_OSPEEDR_OSPEED14); /* 00 = low */
  GPIOE->PUPDR   &= ~(GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD11 |
                      GPIO_PUPDR_PUPD13 | GPIO_PUPDR_PUPD14);  /* 00 = sem pull */

  /* 3. PD14, PD15: entrada com pull-up */
  GPIOD->MODER &= ~(GPIO_MODER_MODE14 | GPIO_MODER_MODE15);    /* 00 = entrada */
  GPIOD->PUPDR  = (GPIOD->PUPDR & ~(GPIO_PUPDR_PUPD14 | GPIO_PUPDR_PUPD15))
                | GPIO_PUPDR_PUPD14_0 | GPIO_PUPDR_PUPD15_0;   /* 01 = pull-up */
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
  /* uma unica escrita: liga os bits de 'set' e desliga os demais LEDs */
  GPIOE->BSRR = ((uint32_t)(LEDS_EXT_MASK & ~set) << 16) | set;
}

static uint8_t botao_pressionou(botao_t *b)
{
  GPIO_PinState lido  = HAL_GPIO_ReadPin(b->porta, b->pino);
  uint32_t      agora = HAL_GetTick();

  if (lido != b->ultimo_lido)            /* leitura mudou: (re)inicia a contagem */
  {
    b->ultimo_lido = lido;
    b->t_mudanca   = agora;
  }
  else if ((lido != b->estavel) && ((agora - b->t_mudanca) >= DEBOUNCE_MS))
  {
    b->estavel = lido;                   /* ficou igual por DEBOUNCE_MS: aceita */
    if (lido == b->nivel_ativo)
    {
      return 1;                          /* borda de pressionamento */
    }
  }
  return 0;
}

static void dwt_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

#define N_TOGGLE 1000U
static void benchmark_toggle(void)
{
  uint32_t t0, c;

  t0 = DWT->CYCCNT;
  for (uint32_t i = 0; i < N_TOGGLE; i++) { HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0); }
  c = DWT->CYCCNT - t0;
  printf("HAL_GPIO_TogglePin      : %4lu ciclos/toggle\r\n", c / N_TOGGLE);

  t0 = DWT->CYCCNT;
  for (uint32_t i = 0; i < N_TOGGLE / 2; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  }
  c = DWT->CYCCNT - t0;
  printf("HAL_GPIO_WritePin       : %4lu ciclos/escrita\r\n", c / N_TOGGLE);

  t0 = DWT->CYCCNT;
  for (uint32_t i = 0; i < N_TOGGLE; i++) { GPIOB->ODR ^= GPIO_ODR_OD0; }
  c = DWT->CYCCNT - t0;
  printf("GPIOB->ODR ^= OD0       : %4lu ciclos/toggle\r\n", c / N_TOGGLE);

  t0 = DWT->CYCCNT;
  for (uint32_t i = 0; i < N_TOGGLE / 2; i++)
  {
    GPIOB->BSRR = GPIO_BSRR_BS0;
    GPIOB->BSRR = GPIO_BSRR_BR0;
  }
  c = DWT->CYCCNT - t0;
  printf("GPIOB->BSRR (BS0/BR0)   : %4lu ciclos/escrita\r\n", c / N_TOGGLE);
}

static void dump_gpio(const char *nome, GPIO_TypeDef *g)
{
  printf("%s MODER=%08lX OTYPER=%08lX OSPEEDR=%08lX PUPDR=%08lX IDR=%08lX ODR=%08lX\r\n",
         nome, g->MODER, g->OTYPER, g->OSPEEDR, g->PUPDR, g->IDR, g->ODR);
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
  printf("\r\n--- teste GPIO ---\r\n");
  printf("SYSCLK = %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
  dump_gpio("GPIOE reset", GPIOE);
  dump_gpio("GPIOD reset", GPIOD);
  gpio_externos_init_hal();
  dump_gpio("GPIOE HAL  ", GPIOE);
  dump_gpio("GPIOD HAL  ", GPIOD);
  HAL_GPIO_DeInit(GPIOE, LEDS_EXT_MASK);
  HAL_GPIO_DeInit(GPIOD, BOT_EXT0_PIN | BOT_EXT1_PIN);
  gpio_externos_init_reg();
  dump_gpio("GPIOE REG  ", GPIOE);
  dump_gpio("GPIOD REG  ", GPIOD);
  dump_gpio("GPIOB      ", GPIOB);
  dump_gpio("GPIOC      ", GPIOC);
  dwt_init();
  benchmark_toggle();
  leds_ext_escreve(0x5);
  printf("ODR GPIOE apos escreve(5) = %04lX (esperado %04X)\r\n", GPIOE->ODR & LEDS_EXT_MASK, LED_EXT0_PIN | LED_EXT2_PIN);
  leds_ext_escreve(0xA);
  printf("ODR GPIOE apos escreve(A) = %04lX (esperado %04X)\r\n", GPIOE->ODR & LEDS_EXT_MASK, LED_EXT1_PIN | LED_EXT3_PIN);

  botao_t b1   = { GPIOC, GPIO_PIN_13,  GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_RESET, 0 };
  botao_t bext = { GPIOD, BOT_EXT0_PIN, GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_SET,   0 };
  uint8_t  contador = 0;
  uint32_t t_led = HAL_GetTick();
  while (1)
  {
    if (HAL_GetTick() - t_led >= 500U)
    {
      t_led += 500U;
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
      leds_ext_escreve(contador++);
    }
    if (botao_pressionou(&b1))   { printf("B1\r\n"); }
    if (botao_pressionou(&bext)) { printf("EXT0\r\n"); }
    /* LD3 (PG4) segue o B1 (PC13), direto nos registradores */
    if (GPIOC->IDR & GPIO_IDR_ID13) { GPIOG->BSRR = GPIO_BSRR_BS4; }
    else                            { GPIOG->BSRR = GPIO_BSRR_BR4; }
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
