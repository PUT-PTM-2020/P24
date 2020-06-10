/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VERSION  		"100"
#define OP_INIT  		"00000"
#define OP_LOGIN 		"00001"
#define OP_REGISTER		"00111"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */



uint8_t command[50];
uint16_t comm_size = 0;
char response[4];
uint16_t response_size = 4;
volatile char  msg_buffer[1024];
volatile char esp_recv_buffer[100];
UART_HandleTypeDef * esp_uart = &huart3;
char pattern[] = "+IPD,";
char send_command[] = "AT+CIPSEND=0,4\r\n";
uint16_t init_msg_size = 33;
uint16_t msg_buffer_size = 30;

char header[4];
char bitversion[3];
char bitoperation[5];
char bitid[8];
char bitanswer [4];
char bitmsg_size[10];
char result[5];
char clear_buffer[100];
uint8_t clear =0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
uint8_t configure_esp();
uint8_t send_at_comm(char *, uint16_t timeout);
void handle_request();
uint8_t check_header()
{

	if(strcmp(bitversion,VERSION))
		return 1;
	do
	{
		if(!strcmp(bitoperation,OP_INIT))
			break;
		else if(!strcmp(bitoperation,OP_LOGIN))
			break;
		else if(!strcmp(bitoperation,OP_REGISTER))
			break;
		else if(!strcmp(bitoperation,OP_INIT))
			break;
		else if(!strcmp(bitoperation,OP_INIT))
			break;
		else if(!strcmp(bitoperation,OP_INIT))
			break;
		else if(!strcmp(bitoperation,OP_INIT))
			break;
		return 1;
	}while(0);

	return 0;
}
uint8_t getbits(char* header)
{
	for(int j=0;j<3;j++)
	{
		if(!!((header[0] << j) & 0x80))
			bitversion[j] = '1';
		else
			bitversion[j] = '0';
	}
	int i=0;
	for(int j=3;j<8;j++)
	{
		if(!!((header[0] << j) & 0x80))
			bitoperation[i] = '1';
		else
			bitoperation[i] = '0';
		i++;
	}
	for(int j=0;j<8;j++)
	{
		if(!!((header[1] << j) & 0x80))
			bitid[j] = '1';
	}
	for(int j=0;j<4;j++)
	{
		if(!!((header[2] << j) & 0x80))
			bitanswer[j] = '1';
		else
			bitanswer[j] = '0';
	}
	i = 0;
	for(int j=4;j<8;j++)
	{
		if(!!((header[2] << j) & 0x80))
			bitmsg_size[i] = '1';
		else
			bitmsg_size[i] = '0';
		i++;
	}
	for(int j=0;j<6;j++)
	{
		if(!!((header[3] << j) & 0x80))
			bitmsg_size[i] = '1';
		else
			bitmsg_size[i] = '0';
		i++;
	}

}
uint16_t count_nm_size()
{
	uint16_t acc =0;
	uint16_t pow=1;
	for(int i=9;i>=0;i--)
	{
		if(bitmsg_size[i]=='1')
		{
			acc = acc + pow;
		}
		pow = pow * 2;
	}


}
void prepare_response()
{
	char firstbyte=0;
	char secondbyte=0;
	char thirdbyte=0;
	char fourthbyte=0;
	for(int i=0;i<3;i++)
	{
		if( bitversion[i]=='1') firstbyte |= 1 << (7-i);
	}
	int i=0;
	for(int j=3;j<8;j++)
	{
		if(bitoperation[i]=='1') firstbyte |= 1 << (7-j);
		i++;
	}
	for(int j=0;j<8;j++)
		if(bitid[j] == '1') secondbyte |= 1 << (7-j);

	for(int j=0;j<4;j++)
		if(bitanswer[j] == '1') thirdbyte |= 1 << (7-j);

	i = 0;

	for(int j=4;j<8;j++)
	{
		if(bitmsg_size[i] == '1') thirdbyte |= 1 << (7-j);
			i++;
	}
	for(int j=0;j<6;j++)
	{
		if(bitmsg_size[i] == '1') fourthbyte |=1 << (7-j);
		i++;
	}
	for(int j=6;j<8;j++)
	{
		fourthbyte |= 1 << (7-j);
	}
	response[0] = firstbyte;
	response[1] = secondbyte;
	response[2] = thirdbyte;
	response[3] = fourthbyte;
}
int check_size()
{
	for(int i=0;i<15;i++)
		if(msg_buffer[i]=='\0')
			return 1;
	return 0;
}
uint16_t process_data()
{

		memcpy(header,msg_buffer+23,4);
		getbits(header);
		//if(!check_header())
		//{

			prepare_response();
			memset(esp_recv_buffer, '\0',100);
			HAL_UART_Transmit(&huart3, send_command,strlen(send_command),100);
			memset(esp_recv_buffer, '\0',100);
			HAL_Delay(200);
			HAL_UART_Transmit(&huart3, response, response_size,100);
			HAL_UART_Transmit(&huart3, "\r\n", 2,100);
			int x =0;
		//}
		//else
		//{

		//}

			memset(msg_buffer, '\0',30);

}
uint8_t configure_esp()
 {

 	HAL_Delay(1000);
 	if(!send_at_comm( "AT+CWMODE=1\r\n",2000))
 		return 0;
 	HAL_Delay(100);
 	if(!send_at_comm( "AT+CIPMUX=1\r\n",2000))
 		return 0;
 	HAL_Delay(100);
 	if(!send_at_comm( "AT+CIPSERVER=1,80\r\n",2000))
 		return 0;
 	HAL_Delay(100);
 	if(!send_at_comm("AT+CWJAP=\"HUAWEI-2.4G-zJ52\",\"q95R2T9c\"",5000))
 	 	 	return 0;
 	 	HAL_Delay(100);
 	 	if(!send_at_comm( "AT+CIPSTATUS\r\n",5000))
 	 	 		return 0;
 	 	 	HAL_Delay(100);

 	return 1;
 }
uint8_t send_at_comm( char * at_command, uint16_t timeout)
{
	HAL_UART_Transmit(&huart3, at_command, strlen(at_command),1000);
	if(timeout>2000)
	{
		HAL_Delay(15000);
		return 1;
	}
	HAL_UART_Receive(&huart3, &esp_recv_buffer, 100,timeout);

	char line[100];
	memset(line, '\0',50);
	uint16_t j=0;
	for(uint16_t i=0;i<1024;i++)
	{
		line[j]=esp_recv_buffer[i];

		if(!strcmp(line,"OK\r\n")||!strcmp(line,"no change\r\n")||!strcmp(line,"STATUS:2\r\n"))
		{
			HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
			HAL_Delay(200);
			HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
			return 1;
		}

		if(!strcmp(line,"ERROR\r\n"))
			return 0;

		if(line[j]=='\n')
		{
			j=0;
			memset(line, '\0',50);
		}

		else
			j++;
	}
	return 0;
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  if(configure_esp())
	 HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
 else
	 HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  int x=0;
  while (1)
  {

	  HAL_UART_Receive(&huart3, msg_buffer, msg_buffer_size,200);
	  x =check_size();
	  if(!x)
		  process_data();

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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
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
  /** Initializes the CPU, AHB and APB busses clocks 
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pins : PD12 PD13 PD14 PD15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
