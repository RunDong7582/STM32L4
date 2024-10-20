/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY2_Pin GPIO_PIN_6
#define KEY2_GPIO_Port GPIOE
#define KEY2_EXTI_IRQn EXTI9_5_IRQn
#define KEY1_Pin GPIO_PIN_13
#define KEY1_GPIO_Port GPIOC
#define KEY1_EXTI_IRQn EXTI15_10_IRQn
#define WAKE_Pin GPIO_PIN_3
#define WAKE_GPIO_Port GPIOC
#define MODE_Pin GPIO_PIN_0
#define MODE_GPIO_Port GPIOA
#define RST_Pin GPIO_PIN_1
#define RST_GPIO_Port GPIOA
#define STAT_Pin GPIO_PIN_4
#define STAT_GPIO_Port GPIOC
#define STAT_EXTI_IRQn EXTI4_IRQn
#define BUSY_Pin GPIO_PIN_5
#define BUSY_GPIO_Port GPIOC
#define BUSY_EXTI_IRQn EXTI9_5_IRQn
#define LED8_Pin GPIO_PIN_1
#define LED8_GPIO_Port GPIOB
#define LED7_Pin GPIO_PIN_10
#define LED7_GPIO_Port GPIOA
#define LED6_Pin GPIO_PIN_7
#define LED6_GPIO_Port GPIOD
#define LED11_Pin GPIO_PIN_0
#define LED11_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
