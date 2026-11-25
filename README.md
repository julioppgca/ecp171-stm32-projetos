<table>
  <tr>
    <td><img src="assets/upf-logo.png" alt="UPF" width="72"></td>
    <td>
      <strong>Universidade de Passo Fundo</strong> · <em>ITec</em> – Instituto de Tecnologia<br>
      <strong>ECP171</strong> – Arquitetura e programação de Microcontroladores<br>
      Prof. Julio Cesar dos Santos
    </td>
  </tr>
</table>

# Projetos STM32 — NUCLEO-H563ZI

Projetos usados nas aulas de STM32 da disciplina, prontos para importar no STM32CubeIDE e gravar na placa **NUCLEO-H563ZI** (STM32H563ZI, Arm Cortex-M33 a 250 MHz).

| Pasta | Aula | O que o programa faz | Hardware extra |
|---|---|---|---|
| [`aula01_primeiro_projeto`](aula01_primeiro_projeto) | 01 — Primeiro projeto (CubeMX + CubeIDE) | Projeto criado a partir da placa, com BSP e clock de 250 MHz. Imprime o clock pela serial, pisca o LD1 a 1 Hz e alterna o LD3 a cada toque no botão azul (B1) | — |
| [`aula02_avr_arm`](aula02_avr_arm) | 02 — Do AVR ao ARM Cortex-M | Arquivos C usados para comparar o assembly gerado para o ATmega328P e para o Cortex-M33 (não é um projeto do CubeIDE) | — |
| [`aula03_gpio`](aula03_gpio) | 03 — GPIO | Programa de teste dos exemplos da aula: configuração dos pinos externos com HAL e com registradores, valores de `MODER`/`PUPDR` impressos pela serial, contador binário em 4 LEDs com uma única escrita no `BSRR`, botões com debounce sem bloqueio e medição de ciclos do toggle (HAL × registradores) | 4 LEDs e 1 botão (abaixo) |
| [`aula04_depuracao`](aula04_depuracao) | 04 — Depuração no STM32CubeIDE | Programa da aula de depuração: variáveis para Live Expressions e SWV, um erro proposital em `calcula_media()`, mensagens pelo ITM e uma divisão por zero opcional (`DEMO_HARDFAULT`) para o Fault Analyzer | 4 LEDs (opcional) |
| [`trabalho_depuracao_semaforo`](trabalho_depuracao_semaforo) | Trabalho — Caça aos bugs no semáforo | Semáforo de carros (LEDs da placa) com botão de pedestre (B1) e semáforo de pedestre (LEDs externos). **O programa tem quatro defeitos de propósito**: encontre cada um com o depurador, seguindo o enunciado do trabalho | 2 LEDs (PE9 vermelho, PE11 verde) |
| [`aula05_adc_pwm`](aula05_adc_pwm) | 05 — ADC e PWM | ADC1 lendo o potenciômetro no A0 (PA6) por polling e PWM de 1 kHz no TIM1 (CH1 a CH4 nos 4 LEDs externos). Cinco exemplos escolhidos com `#define EXEMPLO`: leitura em mV, média de 16 leituras, duty de 10/50/90 %, *fade* e dimmer (potenciômetro → brilho do PE9) | 4 LEDs e 1 potenciômetro de 10 kΩ (abaixo) |

## Ferramentas

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) **2.2.0** ou mais recente
- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) **6.18.1** ou mais recente (só para alterar a configuração no arquivo `.ioc`)
- Pacote [STM32Cube FW_H5](https://www.st.com/en/embedded-software/stm32cubeh5.html) **V1.7.0** (o CubeMX baixa automaticamente ao gerar código pela primeira vez)

Os projetos já trazem a pasta `Drivers` (HAL, CMSIS e BSP), então **compilam sem precisar do CubeMX**.

## Como usar

1. Clone ou baixe o repositório numa pasta **sem espaços e sem acentos** e com caminho curto, por exemplo:

   ```
   C:\STM32\ecp171-stm32-projetos
   ```

2. Abra o STM32CubeIDE e escolha um workspace (pode ser a mesma pasta).
3. **File → Import… → General → Existing Projects into Workspace → Next**. Em *Select root directory*, escolha a pasta do repositório: os projetos aparecem na lista. Deixe **desmarcado** *Copy projects into workspace* e clique em **Finish**.
4. Selecione o projeto e compile (`Ctrl+B`).
5. Ligue a placa pelo USB-C do ST-LINK (CN1) e use **Run → Run** para gravar, ou **Run → Debug** (`F11`) para depurar.
6. Para ver as mensagens da serial, abra um terminal (PuTTY, Tera Term, monitor serial do VS Code…) na porta **STMicroelectronics STLink Virtual COM Port**, a **115200 bps, 8N1**.

Para mudar a configuração de um projeto (pinos, clock, periféricos), abra o arquivo `.ioc` no STM32CubeMX, altere e clique em **GENERATE CODE**. O código escrito entre os comentários `USER CODE BEGIN` e `USER CODE END` é preservado.

## Montagem para as aulas 03 e 04

Os LEDs são ligados com resistor de **330 Ω** ao GND (pino → resistor → anodo → catodo → GND). Os botões ligam o pino ao **GND** e usam o pull-up interno.

| Sinal | Pino STM32 | Conector (Zio) |
|---|---|---|
| LED_EXT0 | PE9 | CN10 pino 4 (D6) |
| LED_EXT1 | PE11 | CN10 pino 6 (D5) |
| LED_EXT2 | PE13 | CN10 pino 10 (D3) |
| LED_EXT3 | PE14 | CN10 pino 8 (D4) |
| BOT_EXT0 | PD14 | CN7 pino 16 (D10) |
| BOT_EXT1 | PD15 | CN7 pino 18 (D9) |
| GND | — | CN10 pino 5 ou CN7 pino 8 |

> Os pinos do STM32H563 trabalham com **3,3 V**. Não ligue 5 V nas saídas.

## Montagem para a aula 05

Os mesmos 4 LEDs das aulas 03 e 04 (agora acionados pelo TIM1, em função alternativa AF1) e um potenciômetro de **10 kΩ**:

| Potenciômetro | Ligação | Conector (Zio) |
|---|---|---|
| Extremo 1 | AVDD (3,3 V) | CN10 pino 1 |
| Extremo 2 | AGND | CN10 pino 3 |
| Cursor | A0 / PA6 (ADC1_INP3) | CN9 pino 1 |

> O potenciômetro **não** pode ser ligado no 5 V: a entrada analógica suporta no máximo 3,3 V.

## Aula 02: comparando o assembly

Os arquivos de `aula02_avr_arm` podem ser compilados com os compiladores que já vêm com a IDE do Arduino (`avr-gcc`) e com o STM32CubeIDE (`arm-none-eabi-gcc`):

```bash
avr-gcc -Os -mmcu=atmega328p -c comum.c -o comum_avr.o
avr-objdump -d comum_avr.o

arm-none-eabi-gcc -Os -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard -c comum.c -o comum_arm.o
arm-none-eabi-objdump -d comum_arm.o
```

Faça o mesmo com `led_avr.c` (só para AVR) e `led_arm.c` (só para ARM).

## Documentação da placa e do microcontrolador

- [NUCLEO-H563ZI](https://www.st.com/en/evaluation-tools/nucleo-h563zi.html) — manual da placa (UM3115) e esquemático
- [STM32H563ZI](https://www.st.com/en/microcontrollers-microprocessors/stm32h563zi.html) — datasheet (DS14258) e manual de referência (RM0481)
