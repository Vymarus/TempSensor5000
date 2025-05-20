#ifndef __SENSOR_H
#define __SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include "stdint.h"
#include "stm32f3xx_hal.h"
#include "stm32f302x8.h"
#include "stm32f3xx_hal_tim.h"
#include "main.h"
#include "tim.h"

void setOuput(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void setInput(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
uint8_t startSensor(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
uint8_t ReadDHT11Byte(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void delayMicroS(uint16_t duration, TIM_HandleTypeDef *htimx);
void ReadDHT11Data(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t *buffer);

#endif /*__ SENSOR_H__ */