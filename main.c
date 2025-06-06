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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bmp280.h"
#include "stdio.h"
#include "ssd1306.h"
#include "fonts.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define ADC_CHANNEL_COUNT 4

#define POLINOMIO 0x07 // Polinomio generador CRC-8: x^8 + x^2 + x + 1
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
uint8_t rx_buf[6];
uint8_t tx_buf[16]; // [SYNC][PWM1][PWM2][DIGITAL][ADC11][ADC12][ADC21][ADC22][ADC31][ADC32][DIGITAL][TEMP][TEMP][PRESS][PRESS][CHECKSUM]
uint8_t error = 0xEE;
uint16_t adc_raw[4]; // DMA buffer para 3 canales
volatile uint32_t last_receive_time = 0;

BMP280_t bmp;
BMP280_Data_t bmp_data;

uint8_t pantalla_estado = 5;
static uint8_t boton_anterior = 1;
uint8_t boton_actual;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart3_rx;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for rxTaskHandle */
osThreadId_t rxTaskHandle;
const osThreadAttr_t rxTaskHandle_attributes = {
  .name = "rxTaskHandle",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for txTaskHandle */
osThreadId_t txTaskHandle;
const osThreadAttr_t txTaskHandle_attributes = {
  .name = "txTaskHandle",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for screenTaskHandl */
osThreadId_t screenTaskHandle;
const osThreadAttr_t screenTaskHandle_attributes = {
  .name = "screenTaskHandl",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for mutexBufferHandle */
osMutexId_t mutexBufferHandle;
const osMutexAttr_t mutexBufferHandle_attributes = {
  .name = "mutexBufferHandle"
};
/* Definitions for semRxHandle */
osSemaphoreId_t semRxHandle;
const osSemaphoreAttr_t semRxHandle_attributes = {
  .name = "semRxHandle"
};
/* USER CODE BEGIN PV */
UART_HandleTypeDef *uart;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_SPI2_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART3_UART_Init(void);
void StartDefaultTask(void *argument);
void StartTask02(void *argument);
void StartTask03(void *argument);
void StartTask04(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void inicioPantalla(void){
    SSD1306_GotoXY (35,0); SSD1306_Puts("TDIII", &Font_11x18, 1);
    SSD1306_GotoXY (15,20); SSD1306_Puts("Blue Pill", &Font_11x18, 1);
    SSD1306_GotoXY (5, 40); SSD1306_Puts("Avedano Capdevila", &Font_7x10, 1);
    SSD1306_GotoXY (5, 50); SSD1306_Puts(" 91770    90129", &Font_7x10, 1);
    //SSD1306_GotoXY (15, 40); SSD1306_Puts("Capdevila", &Font_11x18, 1);
    SSD1306_UpdateScreen();
    HAL_Delay(4000);
    SSD1306_Clear();
    SSD1306_GotoXY (10,0); SSD1306_Puts("Bienvenido", &Font_11x18, 1); // print Hello
    SSD1306_GotoXY (55, 20); SSD1306_Puts("al", &Font_11x18, 1);
    SSD1306_GotoXY (25, 40); SSD1306_Puts("Sistema", &Font_11x18, 1);
    SSD1306_UpdateScreen(); // update screen
    HAL_Delay(4000);
}

void verificacionUART(){

	if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET){
		uart = &huart1;
		//HAL_UART_Transmit_DMA(uart, &id, 1);
		SSD1306_Clear();
		SSD1306_GotoXY(0, 0); SSD1306_Puts("Full Duplex", &Font_11x18, 1);
		SSD1306_GotoXY(0, 25); SSD1306_Puts("Asincrono 8N1", &Font_7x10, 1);
		SSD1306_GotoXY(0, 40); SSD1306_Puts("UART1 TTL", &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}
	else
	{
		uart = &huart3;
		//
		SSD1306_Clear();
		SSD1306_GotoXY(0, 0); SSD1306_Puts("Half Duplex", &Font_11x18, 1);
		SSD1306_GotoXY(0, 25); SSD1306_Puts("Asincrono 8N1", &Font_7x10, 1);
		SSD1306_GotoXY(0, 40); SSD1306_Puts("UART3 RS485", &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}
	HAL_Delay(4000);
}

void verificacionBMP(uint8_t id){
	if (id == 0x58)
	  {
	      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // LED fijo
	      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
	      HAL_UART_Transmit_DMA(uart, &id, 1);
	      SSD1306_Clear();
	      SSD1306_GotoXY(0, 0); SSD1306_Puts("Sensor BMP", &Font_11x18, 1);
	      SSD1306_GotoXY(0, 20); SSD1306_Puts("ID Correcto", &Font_11x18, 1);
	      SSD1306_GotoXY(0, 45); SSD1306_Puts("Conectado", &Font_7x10, 1);
	      SSD1306_UpdateScreen(); // update screen
	      HAL_Delay(4000);
	  }
	  else
	  {
	      while (1)
	      {
	          HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // LED parpadea
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
	          HAL_UART_Transmit_DMA(uart, &error, 1);
	          SSD1306_Clear();
	          SSD1306_GotoXY(0, 0); SSD1306_Puts("Sensor BMP", &Font_11x18, 1);
	          SSD1306_GotoXY(0, 20); SSD1306_Puts("Error ID", &Font_11x18, 1);
	          SSD1306_GotoXY(0, 45); SSD1306_Puts("Desconectado", &Font_7x10, 1);
	          SSD1306_UpdateScreen(); // update screen
	          HAL_Delay(1000);
	      }
	  }
}

void actualizarPantalla(void){
	SSD1306_Clear();
	char buffer[20];

	switch(pantalla_estado){
		case 0:
			sprintf(buffer, "PWM1: %u", ((tx_buf[1]<<4) | (tx_buf[2]>>4)));
			SSD1306_GotoXY(0, 0); SSD1306_Puts(buffer, &Font_11x18, 1);
			sprintf(buffer, "PWM2: %u", (((tx_buf[2] & 0x0F) <<8) | (tx_buf[3])));
			SSD1306_GotoXY(0, 40); SSD1306_Puts(buffer, &Font_11x18, 1);
			break;
		case 1:
			sprintf(buffer, "Salidas: %u", (tx_buf[4]>>4) & 0x0F);
			SSD1306_GotoXY(0, 0); SSD1306_Puts(buffer, &Font_11x18, 1);
			sprintf(buffer, "Entradas: %u", tx_buf[4] & 0x0F);
			SSD1306_GotoXY(0, 40); SSD1306_Puts(buffer, &Font_11x18, 1);
			break;
		case 2:
			sprintf(buffer, "Anal1:%u", ((tx_buf[5]<<4) | (tx_buf[6]>>4)) );
			SSD1306_GotoXY(0, 0); SSD1306_Puts(buffer, &Font_11x18, 1);
			sprintf(buffer, "Anal2:%u", (((tx_buf[6] & 0x0F) << 8) | (tx_buf[7])));
			SSD1306_GotoXY(0, 40); SSD1306_Puts(buffer, &Font_11x18, 1);
			break;
		case 3:
			sprintf(buffer, "Anal3:%u", ((tx_buf[8]<<4) | (tx_buf[9]>>4)));
			SSD1306_GotoXY(0, 0); SSD1306_Puts(buffer, &Font_11x18, 1);
			sprintf(buffer, "Anal4:%u", (((tx_buf[9] & 0x0F) << 8) | (tx_buf[10])));
			SSD1306_GotoXY(0, 40); SSD1306_Puts(buffer, &Font_11x18, 1);
			break;
		case 4:
			//sprintf(buffer, "Temp:%u C", ((tx_buf[11] << 8) | tx_buf[12]));
			//uint16_t temp = ((tx_buf[11] << 8) | tx_buf[12]);
			uint16_t temp = bmp_data.temperature;
			sprintf(buffer, "Temp:%u.%02uC", temp / 100, temp % 100);
			SSD1306_GotoXY(0, 0); SSD1306_Puts(buffer, &Font_11x18, 1);
			sprintf(buffer, "Pres:%luhPa", bmp_data.pressure/100);
			//sprintf(buffer, "Pres:%u hPa", (((tx_buf[13]) << 8) | (tx_buf[14])));
			SSD1306_GotoXY(0, 40); SSD1306_Puts(buffer, &Font_11x18, 1);
			break;
		case 5:
			SSD1306_GotoXY(40, 0); SSD1306_Puts("SALIDAS", &Font_7x10, 1);
			sprintf(buffer, "PWM1:%u", ((tx_buf[1]<<4) | (tx_buf[2]>>4)));
			SSD1306_GotoXY(0, 10); SSD1306_Puts(buffer, &Font_7x10, 1);
			sprintf(buffer, "PWM2:%u", (((tx_buf[2] & 0x0F) <<8) | (tx_buf[3])));
			SSD1306_GotoXY(0, 20); SSD1306_Puts(buffer, &Font_7x10, 1);
			sprintf(buffer, "Digitales:%u", (tx_buf[4]>>4) & 0x0F);
			SSD1306_GotoXY(0, 30); SSD1306_Puts(buffer, &Font_7x10, 1);
			uint16_t temp1 = bmp_data.temperature;
			sprintf(buffer, "Temperatura:%u.%02uC", temp1 / 100, temp1 % 100);
			SSD1306_GotoXY(0, 40); SSD1306_Puts(buffer, &Font_7x10, 1);
			sprintf(buffer, "Presion:%luhPa", bmp_data.pressure/100);
			SSD1306_GotoXY(0, 50); SSD1306_Puts(buffer, &Font_7x10, 1);
			break;
		case 6:
			SSD1306_GotoXY(40, 0); SSD1306_Puts("Entradas", &Font_7x10, 1);
			sprintf(buffer, "Analog 1:%u", ((tx_buf[5]<<4) | (tx_buf[6]>>4)) );
			SSD1306_GotoXY(0, 10); SSD1306_Puts(buffer, &Font_7x10, 1);
			sprintf(buffer, "Analog 2:%u", (((tx_buf[6] & 0x0F) << 8) | (tx_buf[7])));
			SSD1306_GotoXY(0, 20); SSD1306_Puts(buffer, &Font_7x10, 1);
			sprintf(buffer, "Analog 3:%u", ((tx_buf[8]<<4) | (tx_buf[9]>>4)));
			SSD1306_GotoXY(0, 30); SSD1306_Puts(buffer, &Font_7x10, 1);
			sprintf(buffer, "Analog 4:%u", (((tx_buf[9] & 0x0F) << 8) | (tx_buf[10])));
			SSD1306_GotoXY(0, 40); SSD1306_Puts(buffer, &Font_7x10, 1);
			sprintf(buffer, "Digitales: %u", tx_buf[4] & 0x0F);
			SSD1306_GotoXY(0, 50); SSD1306_Puts(buffer, &Font_7x10, 1);
			break;
		default:
			break;
	}
	SSD1306_UpdateScreen();
}

void funcion_X(){
	SSD1306_Clear();
	SSD1306_GotoXY(0, 0); SSD1306_Puts("Cargando", &Font_11x18, 1);
	SSD1306_GotoXY(0, 20); SSD1306_Puts("Datos en", &Font_11x18, 1);
	SSD1306_GotoXY(0, 40); SSD1306_Puts("Memoria...", &Font_11x18, 1);
	SSD1306_UpdateScreen();
}
void Set_PWM(uint16_t ch1, uint16_t ch2)
{
  uint16_t duty1 = ch1;
  uint16_t duty2 = ch2;

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, duty1);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty2);
}

void Set_Digital_Outputs(uint8_t digital_byte)
{
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, (digital_byte & (1 << 0)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, (digital_byte & (1 << 1)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, (digital_byte & (1 << 2)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, (digital_byte & (1 << 3)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

}

uint8_t read_Digital_Outputs(){
	uint8_t outputs = 0;
	outputs |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5);
	outputs |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)<<1;
	outputs |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)<<2;
	outputs |= HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)<<3;

	return outputs;
}
uint8_t read_Digital_Inputs(void)
{
	uint8_t inputs =0;
	inputs |= (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET) ? (1 << 0) : 0;
	inputs |= (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET) ? (1 << 1) : 0;
	inputs |= (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_SET) ? (1 << 2) : 0;
	return inputs;
}

uint8_t calcular_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x80)
                crc = (crc << 1) ^ POLINOMIO;
            else
                crc <<= 1;
        }
    }
    return crc;
}

void doBuffer(){
	 osMutexAcquire(mutexBufferHandle, osWaitForever);
	 uint16_t pwm1 = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_2);
	 uint16_t pwm2 = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_3);
	 uint16_t adc0_val= adc_raw[0];
     uint16_t adc1_val= adc_raw[1];
	 uint16_t adc2_val= adc_raw[2];
	 uint16_t adc3_val= adc_raw[3];



	 BMP280_Read(&bmp, &bmp_data);
	 // Escalar temperatura (centésimas de grado °C a 0-255 rango aproximado)
	 int32_t temp_celsius = bmp_data.temperature; //Centesimas de grado
	 // Escalar presión (Pascal a 0-255 para 800hPa - 1100hPa aprox.)
	 uint32_t pressure_hpa = bmp_data.pressure / 100; // de Pa a hPa



		 tx_buf[0] = 0xA5;
		 tx_buf[1] = pwm1>>4;
		 tx_buf[2] = (pwm1<<4) | (pwm2>>8);
		 tx_buf[3] = pwm2;
		 tx_buf[4] = (read_Digital_Outputs()<<4) | read_Digital_Inputs();
		 tx_buf[5] = adc0_val>>4;
		 tx_buf[6] = adc0_val<<4 | adc1_val>>8;
		 tx_buf[7] = adc1_val;
		 tx_buf[8] = adc2_val>>4;
		 tx_buf[9] = adc2_val<<4 | adc3_val>>8;
		 tx_buf[10]= adc3_val;
		 tx_buf[11] = temp_celsius>>8;
		 tx_buf[12] = temp_celsius;
		 tx_buf[13] = 0;
		 tx_buf[14] = 0;
		 tx_buf[13] = pressure_hpa>>8;
		 tx_buf[14] = pressure_hpa;
		 tx_buf[15] = 0;

		 tx_buf[15] = calcular_crc8(tx_buf, 15);
	/*for(int i = 0; i<=13; i++){
		 tx_buf[15] |= tx_buf[i];
	 }*/
	 actualizarPantalla();
	 osMutexRelease(mutexBufferHandle);
}

/*void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
	  if (rx_buf[0] == 0xA5)
	     {
	       // Checksum usando OR bitwise
	       uint8_t chk = rx_buf[0] | rx_buf[1] | rx_buf[2] | rx_buf[3] | rx_buf[4];

      if (chk == rx_buf[5]) // validar checksum
      {
    	uint16_t duty1 = (rx_buf[1]<<4) | (rx_buf[2]>>4);
    	uint16_t duty2 = ((rx_buf[2]& 0x0F)<<8) | (rx_buf[3]);

        Set_PWM(duty1, duty2);
        Set_Digital_Outputs(rx_buf[4]);

        doBuffer();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_UART_Transmit_DMA(uart, tx_buf, 16);
        last_receive_time = HAL_GetTick();
        return;
      }else{
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
           HAL_UART_Transmit_DMA(uart, &error, 1);
    	  return;
      }
    }
    // Si falla sincronismo o checksum: ignorar
   // HAL_UART_Receive_DMA(uart, rx_buf, 6); // reinicia recepción
  }
}*/

/*void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == uart){
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
		HAL_UART_Receive_DMA(uart, rx_buf, 6);

	}
}*/
void StartRxTask(void *arg) {
    for (;;) {
        if (osSemaphoreAcquire(semRxHandle, osWaitForever) == osOK) {
            if (rx_buf[0] == 0xA5) {
                uint8_t chk = rx_buf[0] | rx_buf[1] | rx_buf[2] | rx_buf[3] | rx_buf[4];
                // chk = calcular_crc8(rx_buf, 5);
                if (chk == rx_buf[5]) {
                    uint16_t duty1 = (rx_buf[1] << 4) | (rx_buf[2] >> 4);
                    uint16_t duty2 = ((rx_buf[2] & 0x0F) << 8) | rx_buf[3];
                    Set_PWM(duty1, duty2);
                    Set_Digital_Outputs(rx_buf[4]);
                    doBuffer();
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
                    HAL_UART_Transmit_DMA(uart, tx_buf, 16);
                    last_receive_time = osKernelGetTickCount();
                } else {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
                    HAL_UART_Transmit_DMA(uart, &error, 1);
                }
            }
            HAL_UART_Receive_DMA(uart, rx_buf, 6);
        }
    }
}
void StartTxTask(void *arg) {
    for (;;) {
        if ((osKernelGetTickCount() - last_receive_time) >= 5000) {
            //pantalla_estado = 5;
        	doBuffer();
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_UART_Transmit_DMA(uart, tx_buf, 16);
            last_receive_time = osKernelGetTickCount();
        }
        osDelay(100);
    }
}
void StartScreenTask(void *arg) {
	uint32_t boton_presionado_tiempo = 0;
	const uint32_t TIEMPO_LARGO_MS = 2000; // 3 segundos

    for (;;) {
        boton_actual = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);

        if (boton_anterior == 1 && boton_actual == 0) {
            boton_presionado_tiempo = osKernelGetTickCount();
        }

        if (boton_anterior == 0 && boton_actual == 0) {
            uint32_t duracion = osKernelGetTickCount() - boton_presionado_tiempo;
            if (duracion >= TIEMPO_LARGO_MS) {
                funcion_X();
                while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11) == GPIO_PIN_RESET) {
                osDelay(10); // Espera a que suelte el botón
                }
                boton_anterior = 1;
	            continue;
	        }
	    }

	    if (boton_anterior == 0 && boton_actual == 1) {
            pantalla_estado = (pantalla_estado + 1) % 7;
            doBuffer();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        boton_anterior = boton_actual;
        osDelay(50);
    }


	/*   for (;;) {
        boton_actual = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);
        if (boton_anterior == 1 && boton_actual == 0) {
            pantalla_estado = (pantalla_estado + 1) % 7;
            doBuffer();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
        boton_anterior = boton_actual;
        osDelay(100);*/
 }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == uart) {
        osSemaphoreRelease(semRxHandle);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == uart) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
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
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_SPI2_Init();
  MX_I2C1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_raw, 4);
  last_receive_time = HAL_GetTick();


  bmp.hspi = &hspi2;
  bmp.cs_port = GPIOB;
  bmp.cs_pin = GPIO_PIN_12;


  SSD1306_Init ();
  inicioPantalla();

  BMP280_Init(&bmp);
  uint8_t id = BMP280_ReadID(&bmp);

  verificacionUART();
  verificacionBMP(id);
  HAL_UART_Receive_DMA(uart, rx_buf, 6);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of mutexBufferHandle */
  mutexBufferHandle = osMutexNew(&mutexBufferHandle_attributes);

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* Create the semaphores(s) */
    /* creation of semRxHandle */
    semRxHandle = osSemaphoreNew(1, 0, &semRxHandle_attributes);

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of defaultTask */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

      /* creation of rxTaskHandle */
      rxTaskHandle = osThreadNew(StartRxTask, NULL, &rxTaskHandle_attributes);

      /* creation of txTaskHandle */
      txTaskHandle = osThreadNew(StartTxTask, NULL, &txTaskHandle_attributes);

      /* creation of screenTaskHandl */
      screenTaskHandle = osThreadNew(StartScreenTask, NULL, &screenTaskHandle_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 4095;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_12|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5 PA6 PA7 PA11 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB12 PB3 PB4
                           PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_12|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the rxTaskHandle thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the txTaskHandle thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void *argument)
{
  /* USER CODE BEGIN StartTask03 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask03 */
}

/* USER CODE BEGIN Header_StartTask04 */
/**
* @brief Function implementing the screenTaskHandl thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask04 */
void StartTask04(void *argument)
{
  /* USER CODE BEGIN StartTask04 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask04 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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

#ifdef  USE_FULL_ASSERT
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
