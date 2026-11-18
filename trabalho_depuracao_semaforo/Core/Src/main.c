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
/* estados do semaforo (carros / pedestre) */
typedef enum
{
  VERDE = 0,        /* carros: verde    | pedestre: vermelho        */
  AMARELO,          /* carros: amarelo  | pedestre: vermelho        */
  VERMELHO,         /* carros: vermelho | pedestre: verde           */
  PISCA_PEDESTRE,   /* carros: vermelho | pedestre: verde piscando  */
  N_ESTADOS
} estado_t;

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
/* semaforo dos carros: LEDs da placa (LD1 verde, LD2 amarelo, LD3 vermelho)
 * semaforo do pedestre: LEDs externos com resistor de 330 ohm para o GND */
#define PED_VERMELHO_PIN   GPIO_PIN_9    /* PE9  - D6 (CN10-4) */
#define PED_VERDE_PIN      GPIO_PIN_11   /* PE11 - D5 (CN10-6) */

#define TICK_MS            250U   /* periodo da maquina de estados     */
#define TICKS_POR_SEGUNDO  (1000U / TICK_MS)
#define TEMPO_MIN_VERDE_S  2U     /* com pedido, o verde acaba em 2 s  */
#define DEBOUNCE_MS        20U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
/* variaveis globais: acompanhe em Live Expressions e no SWV */
estado_t estado          = VERDE;  /* estado atual do semaforo              */
uint8_t  restante_s      = 0;      /* segundos que faltam no estado atual   */
uint8_t  pedido_pedestre = 0;      /* 1 = alguem apertou o B1 e quer passar */
uint32_t pedidos         = 0;      /* quantas vezes o B1 foi pressionado    */
uint32_t ciclos          = 0;      /* quantas vezes o semaforo voltou ao verde */

/* duracao de cada estado, em segundos */
static const uint8_t duracao_s[N_ESTADOS] = { 8, 3, 6, 3 };

static const char * const nome_estado[N_ESTADOS] =
{
  "VERDE", "AMARELO", "VERMELHO", "PISCA"
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* LEDs do pedestre: PE9 pela HAL e PE11 direto nos registradores */
static void gpio_pedestre_init(void)
{
  GPIO_InitTypeDef cfg = {0};

  __HAL_RCC_GPIOE_CLK_ENABLE();

  /* PE9 - vermelho do pedestre (HAL) */
  HAL_GPIO_WritePin(GPIOE, PED_VERMELHO_PIN, GPIO_PIN_RESET);
  cfg.Pin   = PED_VERMELHO_PIN;
  cfg.Mode  = GPIO_MODE_OUTPUT_PP;
  cfg.Pull  = GPIO_NOPULL;
  cfg.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &cfg);

  /* PE11 - verde do pedestre (registradores) */
  GPIOE->BRR      = PED_VERDE_PIN;             /* comeca apagado      */
  GPIOE->OTYPER  &= ~(1U << 11);               /* push-pull           */
  GPIOE->OSPEEDR &= ~(3U << (11 * 2));         /* velocidade baixa    */
  GPIOE->PUPDR   &= ~(3U << (11 * 2));         /* sem pull-up/down    */
  GPIOE->MODER   |=  (1U << (11 * 2));         /* modo 01 = saida     */
}

static void carros(uint8_t verde, uint8_t amarelo, uint8_t vermelho)
{
  if (verde)    { BSP_LED_On(LED_GREEN);  } else { BSP_LED_Off(LED_GREEN);  }
  if (amarelo)  { BSP_LED_On(LED_YELLOW); } else { BSP_LED_Off(LED_YELLOW); }
  if (vermelho) { BSP_LED_On(LED_RED);    } else { BSP_LED_Off(LED_RED);    }
}

static void pedestre(uint8_t verde, uint8_t vermelho)
{
  HAL_GPIO_WritePin(GPIOE, PED_VERDE_PIN,    verde    ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, PED_VERMELHO_PIN, vermelho ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* o que cada estado faz nos LEDs; chamadas a cada TICK_MS */
void acao_verde(void)    { carros(1, 0, 0); pedestre(0, 1); }
void acao_amarelo(void)  { carros(0, 1, 0); pedestre(0, 1); }
void acao_vermelho(void) { carros(0, 0, 1); pedestre(1, 0); }
void acao_pisca(void)
{
  static uint8_t aceso = 0;
  aceso = !aceso;
  carros(0, 0, 1);
  pedestre(aceso, 0);
}

static void (* const acao_estado[N_ESTADOS])(void) =
{
  acao_verde, acao_amarelo, acao_vermelho
};

/* sequencia: VERDE -> AMARELO -> VERMELHO -> PISCA_PEDESTRE -> VERDE ... */
static estado_t proximo_estado(estado_t atual)
{
  estado_t prox = VERDE;

  switch (atual)
  {
    case VERDE:
      prox = AMARELO;
    case AMARELO:
      prox = VERMELHO;
      break;
    case VERMELHO:
      prox = PISCA_PEDESTRE;
      break;
    case PISCA_PEDESTRE:
    default:
      prox = VERDE;
      break;
  }
  return prox;
}

/* mostra o estado na serial uma vez por segundo */
static void relatorio_serial(void)
{
  printf("%-8s %2u s   pedido: %s\r\n", nome_estado[estado],
         (unsigned)restante_s, pedido_pedestre ? "sim" : "nao");
  pedido_pedestre = 0;   /* pedido ja informado */
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
  printf("\r\nSemaforo com botao de pedestre - NUCLEO-H563ZI\r\n");
  gpio_pedestre_init();

  botao_t  b1     = { GPIOC, GPIO_PIN_13, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, 0 };
  uint32_t t_tick = HAL_GetTick();
  uint32_t ticks  = 0;

  estado     = VERDE;
  restante_s = duracao_s[estado];
  acao_estado[estado]();

  while (1)
  {
    if (botao_pressionou(&b1))
    {
      pedidos++;
      pedido_pedestre = 1;
    }

    if (HAL_GetTick() - t_tick >= TICK_MS)          /* a cada 250 ms */
    {
      t_tick += TICK_MS;
      ticks++;

      if ((ticks % TICKS_POR_SEGUNDO) == 0U)        /* a cada 1 s */
      {
        relatorio_serial();

        if (restante_s > 0U)
        {
          restante_s--;
        }

        /* pedido de pedestre encurta o verde */
        if ((estado == VERDE) && pedido_pedestre && (restante_s > TEMPO_MIN_VERDE_S))
        {
          restante_s = TEMPO_MIN_VERDE_S;
        }

        if (restante_s == 0U)                        /* fim do estado */
        {
          estado     = proximo_estado(estado);
          restante_s = duracao_s[estado];
          if (estado == VERMELHO)
          {
            pedido_pedestre = 0;                     /* pedestre atendido */
          }
          if (estado == VERDE)
          {
            ciclos++;
          }
          printf("-> %s\r\n", nome_estado[estado]);
        }
      }

      acao_estado[estado]();                         /* atualiza os LEDs */
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
