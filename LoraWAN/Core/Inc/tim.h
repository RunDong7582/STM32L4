/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.h
  * @brief   This file contains all the function prototypes for
  *          the tim.c file
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
#ifndef __TIM_H__
#define __TIM_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "stm32l4xx_hal.h"
/* USER CODE END Includes */

extern TIM_HandleTypeDef htim6;

extern TIM_HandleTypeDef htim15;

/* USER CODE BEGIN Private defines */
extern void _Error_Handler(char *, int);
                  
void Tim6_Conf(uint16_t time);                
void MX_TIM15_Init(uint32_t pwm_puty);

#define SYSCLOCK_FREQ 80000000

#define BASE_TIM_10000HZ 10000

/* USER CODE END Private defines */

void MX_TIM6_Init(void);
// void MX_TIM15_Init(void);

// void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __TIM_H__ */

