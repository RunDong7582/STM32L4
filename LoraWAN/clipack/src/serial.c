/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
	BASIC INTERRUPT DRIVEN SERIAL PORT DRIVER FOR UART0.
*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

/* Library includes. */
#include "stm32l4xx_hal.h"
#include "stm32l476xx.h"
/* Demo application includes. */
#include "serial.h"
#include "usart.h"
#include "gpio.h"
/*-----------------------------------------------------------*/

uint8_t buff_index = 0;  
uint8_t data_len = 0; 

/* Misc defines. */
#define serINVALID_QUEUE				( ( QueueHandle_t ) 0 )
#define serNO_BLOCK						( ( TickType_t ) 0 )
#define serTX_BLOCK_TIME				( 40 / portTICK_PERIOD_MS )

/*-----------------------------------------------------------*/

/* The queue used to hold received characters. */
static QueueHandle_t xRxedChars;
static QueueHandle_t xCharsForTx;

/*-----------------------------------------------------------*/

/* UART interrupt handler. */
void vUARTInterruptHandler( void );

/*-----------------------------------------------------------*/

/*
 * See the serial2.h header file.
 */
xComPortHandle xSerialPortInitMinimal( unsigned long ulWantedBaud, unsigned portBASE_TYPE uxQueueLength )
{
xComPortHandle xReturn;

	/* Create the queues used to hold Rx/Tx characters. */
	xRxedChars = xQueueCreate( uxQueueLength, ( unsigned portBASE_TYPE ) sizeof( signed char ) );
	xCharsForTx = xQueueCreate( uxQueueLength + 1, ( unsigned portBASE_TYPE ) sizeof( signed char ) );
	
	/* If the queue/semaphore was created correctly then setup the serial port
	hardware. */
	if( ( xRxedChars != serINVALID_QUEUE ) && ( xCharsForTx != serINVALID_QUEUE ) )
	{
		/* 修改：再次初始化的串口2，用于处理接收PC段的命令行 */
		MX_USART2_Init(115200);	
		USART2_Clear_IT(); 
	}
	else
	{
		xReturn = ( xComPortHandle ) 0;
	}

	/* This demo file only supports a single port but we have to return
	something to comply with the standard demo header file. */
	return xReturn;
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xSerialGetChar( xComPortHandle pxPort, signed char *pcRxedChar, TickType_t xBlockTime )
{
	/* The port handle is not required as this driver only supports one port. */
	( void ) pxPort;

	/* Get the next character from the buffer.  Return false if no characters
	are available, or arrive before xBlockTime expires. */
	if( xQueueReceive( xRxedChars, pcRxedChar, xBlockTime ) )
	{
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}
}
/*-----------------------------------------------------------*/

void vSerialPutString( xComPortHandle pxPort, const signed char * const pcString, unsigned short usStringLength )
{
signed char *pxNext;

	/* A couple of parameters that this port does not use. */
	( void ) usStringLength;
	( void ) pxPort;

	/* NOTE: This implementation does not handle the queue being full as no
	block time is used! */

	/* The port handle is not required as this driver only supports UART1. */
	( void ) pxPort;

	/* Send each character in the string, one at a time. */
	pxNext = ( signed char * ) pcString;
	while( *pxNext )
	{
		xSerialPutChar( pxPort, *pxNext, serNO_BLOCK );
		pxNext++;
	}
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xSerialPutChar( xComPortHandle pxPort, signed char cOutChar, TickType_t xBlockTime )
{
signed portBASE_TYPE xReturn;

	if( xQueueSend( xCharsForTx, &cOutChar, xBlockTime ) == pdPASS )
	{
		xReturn = pdPASS;
		uint8_t cChar;
		if(xQueueReceive(xCharsForTx, &cChar, 0) == pdTRUE) 
		{
			// while(HAL_UART_Transmit_IT(&huart2, &cChar, 1) != HAL_OK );
			if((HAL_UART_GetState(&huart2) & HAL_UART_STATE_BUSY_TX) != HAL_UART_STATE_BUSY_TX) {
				HAL_UART_Transmit(&huart2, &cChar, 1, 1000); //轮询方式将数据发送出去
			}
		}
	}
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

void vSerialClose( xComPortHandle xPort )
{
	/* Not supported as not required by the demo application. */
}
/*-----------------------------------------------------------*/


void vUARTInterruptHandler( void )
{
  	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    uint32_t temp;

    if((__HAL_UART_GET_FLAG(&huart2,UART_FLAG_IDLE) != RESET))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        HAL_UART_DMAStop(&huart2);
        temp = huart2.hdmarx->Instance->CNDTR;
        Usart2_RX.rx_len =  RECEIVELEN - temp;
        Usart2_RX.receive_flag=1;
        for(uint16_t i = 0; i < Usart2_RX.rx_len; ++i) {
            xQueueSendFromISR(xRxedChars, &Usart2_RX.RX_Buf[i], &pxHigherPriorityTaskWoken);
        }  
        HAL_UART_Receive_DMA(&huart2,Usart2_RX.RX_Buf,RECEIVELEN);
  }
	HAL_UART_IRQHandler(&huart2);
  	portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void UART2_IDLECallback(UART_HandleTypeDef* huart)  
{  
	BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
	if((__HAL_UART_GET_FLAG(&huart2,UART_FLAG_IDLE) != RESET))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
		HAL_UART_DMAStop(&huart2);  // 停止本次DMA传输  
		data_len = RECEIVELEN - __HAL_DMA_GET_COUNTER(huart2.hdmarx);  // 计算接收到的数据长度  
		Usart2_RX.RX_Buf[data_len] = '/0';  
 		Usart2_RX.receive_flag=1;
		 Usart2_RX.rx_len =  data_len;
		for (uint8_t i = 0; i < data_len; i++)  
		{  	    	
			xQueueSendFromISR(xRxedChars, &Usart2_RX.RX_Buf[i], &xHigherPriorityTaskWoken);  
		}  
	}
	HAL_UART_Receive_DMA(&huart2, (uint8_t*)Usart2_RX.RX_Buf, RECEIVELEN);  // 重启开始DMA传输 每次255字节数据  
	HAL_UART_IRQHandler(&huart2);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  
}




	
