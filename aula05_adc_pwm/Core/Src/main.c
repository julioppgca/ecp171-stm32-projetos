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
#include "adc.h"
#include "icache.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Escolha o exemplo da aula (1 a 5), compile e grave de novo:
 *  1 - ADC: leitura do potenciometro (valor bruto e mV) na serial
 *  2 - ADC: media de 16 leituras para reduzir o ruido
 *  3 - PWM: LED do PE9 com 10 %, 50 % e 90 % de duty cycle
 *  4 - PWM: "fade" (rampa de brilho) no LED do PE9
 *  5 - ADC + PWM: o potenciometro controla o brilho do LED do PE9 */
#define EXEMPLO       1

#define VREF_MV       3300U   /* tensao de referencia do ADC (VREF+ = 3,3 V) */
#define ADC_MAX       4095U   /* maior valor de 12 bits                      */
#define N_MEDIA       16U     /* leituras usadas na media (exemplo 2)        */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
/* variaveis globais: acompanhe em Live Expressions */
uint32_t adc_raw   = 0;   /* ultima leitura do ADC (0 a 4095)          */
uint32_t tensao_mv = 0;   /* leitura convertida para milivolts         */
uint32_t duty_ccr  = 0;   /* valor do CCR1: 0 a 1000 (ARR + 1 = 1000)  */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* uma conversao por polling: dispara, espera o fim e le o resultado */
uint32_t adc_le(void)
{
  uint32_t valor = 0;

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
  {
    valor = HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);
  return valor;
}

/* media de n leituras: reduz o ruido da medida */
uint32_t adc_le_media(uint32_t n)
{
  uint32_t soma = 0;

  for (uint32_t i = 0; i < n; i++)
  {
    soma += adc_le();
  }
  return soma / n;
}

/* 0..4095 -> 0..3300 mV */
uint32_t adc_para_mv(uint32_t raw)
{
  return (raw * VREF_MV) / ADC_MAX;
}

/* 0..4095 -> 0..1000 (CCR para ARR = 999) */
uint32_t adc_para_ccr(uint32_t raw)
{
  return (raw * (__HAL_TIM_GET_AUTORELOAD(&htim1) + 1U)) / (ADC_MAX + 1U);
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
  MX_ADC1_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  /* calibra o ADC (com ele desligado) antes da primeira conversao */
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }

  /* liga o PWM nos 4 canais do TIM1 (PE9, PE11, PE13, PE14), duty = 0 */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
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
  printf("\r\nAula 05 - ADC e PWM - exemplo %d\r\n", EXEMPLO);

#if EXEMPLO == 3
  const uint32_t duty_pct[3] = { 10, 50, 90 };
  uint32_t       passo       = 0;
#elif EXEMPLO == 4
  int32_t        sobe        = 1;
#endif

  while (1)
  {
#if EXEMPLO == 1
    adc_raw   = adc_le();
    tensao_mv = adc_para_mv(adc_raw);
    printf("raw = %4lu   tensao = %4lu mV\r\n", adc_raw, tensao_mv);
    HAL_Delay(200);

#elif EXEMPLO == 2
    uint32_t uma = adc_le();
    adc_raw      = adc_le_media(N_MEDIA);
    tensao_mv    = adc_para_mv(adc_raw);
    printf("uma leitura = %4lu   media de %lu = %4lu   (%4lu mV)\r\n",
           uma, (unsigned long)N_MEDIA, adc_raw, tensao_mv);
    HAL_Delay(200);

#elif EXEMPLO == 3
    duty_ccr = duty_pct[passo] * 10U;               /* % -> CCR (0..1000) */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_ccr);
    printf("duty = %2lu %%   CCR1 = %4lu\r\n", duty_pct[passo], duty_ccr);
    passo = (passo + 1U) % 3U;
    HAL_Delay(1000);

#elif EXEMPLO == 4
    if (sobe)
    {
      duty_ccr += 5U;
      if (duty_ccr >= 1000U) { sobe = 0; }
    }
    else
    {
      duty_ccr -= 5U;
      if (duty_ccr == 0U) { sobe = 1; }
    }
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_ccr);
    HAL_Delay(2);                                   /* 0 -> 100 % em 0,4 s */

#elif EXEMPLO == 5
    adc_raw   = adc_le_media(N_MEDIA);
    tensao_mv = adc_para_mv(adc_raw);
    duty_ccr  = adc_para_ccr(adc_raw);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_ccr);
    printf("%4lu mV -> duty %3lu.%lu %%\r\n", tensao_mv, duty_ccr / 10U, duty_ccr % 10U);
    HAL_Delay(100);
#endif
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
