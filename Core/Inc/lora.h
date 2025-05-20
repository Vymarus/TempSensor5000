/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora.h
  * @brief   Header for RN2483A LoRa module interface implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LORA_H
#define __LORA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes */
#include "main.h"

/* Public interface */
void Lora_Init(UART_HandleTypeDef *huart, GPIO_TypeDef *rst_port, uint16_t rst_pin);
void Lora_Reset(void);
HAL_StatusTypeDef Lora_SetupAndJoinOTAA(UART_HandleTypeDef *huart, GPIO_TypeDef *rst_port, uint16_t rst_pin);
HAL_StatusTypeDef Lora_Setup_ABP(const char *devaddr, const char *nwkskey, const char *appskey);
HAL_StatusTypeDef Lora_InitOTAA(const char *devEui, const char *appEui, const char *appKey);
HAL_StatusTypeDef Lora_CheckJoin(void);
HAL_StatusTypeDef Lora_get_hweui(void);
HAL_StatusTypeDef Lora_get_status(void);
HAL_StatusTypeDef Lora_SendTX(const char *data);
HAL_StatusTypeDef Lora_SendTXRaw(const uint8_t *data, uint8_t len);
HAL_StatusTypeDef Lora_SendCommand(const char *cmd, char *response, uint16_t resp_len);


#ifdef __cplusplus
}
#endif

#endif /* __LORA_H */
