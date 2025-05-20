/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora.c
  * @brief   RN2483A LoRa module interface implementation
  * @details Provides interface to RN2483A LoRa module for both OTAA and ABP activation
  *          with support for sending messages and managing network connections
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes */
#include "lora.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/*******************************************************************************
 * Private Constants
 ******************************************************************************/
static const char *LORA_DEVICE_EUI = "70B3D57ED006FF08";    // Device unique identifier
static const char *LORA_APP_EUI = "0004A30B00E86AD4";       // Application identifier
static const char *LORA_APP_KEY = "6C61727500465F18C48AFD206126127E"; // Application key

/*******************************************************************************
 * Private Variables
 ******************************************************************************/
/* External handle for Debug UART */
extern UART_HandleTypeDef huart2;

/* LoRa module configuration */
static UART_HandleTypeDef *lora_uart = NULL;
static GPIO_TypeDef *lora_rst_port = NULL;
static uint16_t lora_rst_pin = 0;

/*******************************************************************************
 * Private Helper Functions - Debug & UART
 ******************************************************************************/

/**
 * @brief Print debug message to debug UART
 * @param message Null-terminated string to print
 * @note Uses huart2 as debug UART
 */
static void Debug_Print(const char *message)
{
    DEBUG_PRINT(message); // Is replaced with HAL_UART_Transmit in the preprocessor depending on DEBUG_ENABLED
}

/**
 * @brief Flush any pending data from UART receive buffer
 * @details Reads and discards any data in the UART buffer to ensure clean state
 */
static void Lora_FlushUART(void)
{
    char dummy;
    while (HAL_UART_Receive(lora_uart, (uint8_t*)&dummy, 1, 0) == HAL_OK);
}

/**
 * @brief Send command to LoRa module with proper termination
 * @param cmd Command string to send
 * @return HAL_OK if command sent successfully, HAL_ERROR otherwise
 * @details Adds CR/LF termination and flushes UART before sending
 */
static HAL_StatusTypeDef Lora_SendCommandInternal(const char *cmd)
{
    Debug_Print("Sending command: ");
    Debug_Print(cmd);
    Debug_Print("\r\n");

    Lora_FlushUART();

    if (HAL_UART_Transmit(lora_uart, (uint8_t*)cmd, strlen(cmd), 100) != HAL_OK)
        return HAL_ERROR;
    if (HAL_UART_Transmit(lora_uart, (uint8_t*)"\r\n", 2, 100) != HAL_OK)
        return HAL_ERROR;
    return HAL_OK;
}

/**
 * @brief Read response from LoRa module
 * @param response Buffer to store response
 * @param resp_len Length of response buffer
 * @param timeout Timeout in milliseconds
 * @return HAL_OK if valid response received, HAL_TIMEOUT if timeout occurred
 * @details Waits for CR/LF terminated response with timeout
 */
static HAL_StatusTypeDef Lora_ReadResponse(char *response, uint16_t resp_len, uint32_t timeout)
{
    uint16_t pos = 0;
    uint32_t start = HAL_GetTick();
    
    memset(response, 0, resp_len);
    
    while ((HAL_GetTick() - start) < timeout && pos < (resp_len - 1))
    {
        if (HAL_UART_Receive(lora_uart, (uint8_t*)&response[pos], 1, 100) == HAL_OK)
        {
            if (pos > 0 && response[pos-1] == '\r' && response[pos] == '\n')
            {
                response[pos-1] = '\0';
                return HAL_OK;
            }
            pos++;
        }
    }
    return HAL_TIMEOUT;
}

/**
 * @brief Convert string data to hex string representation
 * @param data Input data to convert
 * @param len Length of input data
 * @param hexOutput Buffer to store hex string
 * @param hexLen Size of hex buffer
 * @return true if conversion successful, false if buffer too small
 * @note Each input byte becomes two hex characters in output
 */
static bool convert_to_hex(const char *data, size_t len, char *hexOutput, size_t hexLen) 
{
    if (hexLen < (len * 2 + 1)) {
        return false;
    }
    
    for (size_t i = 0; i < len; i++) {
        sprintf(&hexOutput[i * 2], "%02X", (unsigned char)data[i]);
    }
    hexOutput[len * 2] = '\0';
    return true;
}

/**
 * @brief Send MAC set command to LoRa module
 * @param param Parameter name to set
 * @param value Value to set
 * @return HAL_OK if command successful, HAL_ERROR otherwise
 * @details Formats and sends "mac set <param> <value>" command
 */
static HAL_StatusTypeDef Lora_SendMacSet(const char *param, const char *value)
{
    char commandBuffer[128] = {0};
    char responseBuffer[128] = {0};

    strcpy(commandBuffer, "mac set ");
    strcat(commandBuffer, param);
    strcat(commandBuffer, " ");
    strcat(commandBuffer, value);

    return Lora_SendCommand(commandBuffer, responseBuffer, sizeof(responseBuffer));
}

/*******************************************************************************
 * Public Functions - Basic Operations
 ******************************************************************************/

/**
 * @brief Initialize LoRa module interface
 * @param huart UART handle for LoRa communication
 * @param rst_port GPIO port for reset pin
 * @param rst_pin GPIO pin number for reset
 * @details Sets up communication interface and performs initial reset
 */
void Lora_Init(UART_HandleTypeDef *huart, GPIO_TypeDef *rst_port, uint16_t rst_pin)
{
    lora_uart = huart;
    lora_rst_port = rst_port;
    lora_rst_pin = rst_pin;
    Lora_Reset();
}

/**
 * @brief Reset LoRa module using hardware reset pin
 * @details Toggles reset pin with appropriate timing
 */
void Lora_Reset(void)
{
    HAL_GPIO_WritePin(lora_rst_port, lora_rst_pin, GPIO_PIN_RESET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(lora_rst_port, lora_rst_pin, GPIO_PIN_SET);
    HAL_Delay(200);
}

/*******************************************************************************
 * Public Functions - Network Commands
 ******************************************************************************/

/**
 * @brief Send command to LoRa module and get response
 * @param cmd Command string to send
 * @param response Buffer to store response
 * @param resp_len Length of response buffer
 * @return HAL_OK if command successful, HAL_ERROR on failure
 * @details Automatically retries on busy response, max 3 attempts
 */
HAL_StatusTypeDef Lora_SendCommand(const char *cmd, char *response, uint16_t resp_len)
{
    uint8_t tries = 0;
    HAL_StatusTypeDef ret;

    while (tries < 3)
    {
        ret = Lora_SendCommandInternal(cmd);
        if (ret != HAL_OK) {
            Debug_Print("Failed to send command.\r\n");
            return ret;
        }
        
        ret = Lora_ReadResponse(response, resp_len, 5000);
        if (ret == HAL_OK)
        {
            Debug_Print("Received response: ");
            Debug_Print(response);
            Debug_Print("\r\n");

            if (strcmp(response, "busy") == 0)
            {
                Debug_Print("Response 'busy' received, retrying command...\r\n");
                HAL_Delay(1000);
                tries++;
                continue;
            }
            if (strlen(response) > 0)
            {
                return HAL_OK;
            }
            Debug_Print("Empty response received, retrying...\r\n");
        }
        else
        {
            Debug_Print("No valid response received, retrying...\r\n");
        }
        tries++;
        HAL_Delay(1000);
    }
    
    Debug_Print("Command failed after retries\r\n");
    return HAL_ERROR;
}

/**
 * @brief Get device's hardware EUI
 * @return HAL_OK if successful, HAL_ERROR otherwise
 * @details Sends "sys get hweui" command to retrieve unique hardware identifier
 */
HAL_StatusTypeDef Lora_get_hweui(void)
{
    char response[128] = {0};
    return Lora_SendCommand("sys get hweui", response, sizeof(response));
}

/**
 * @brief Get current device status
 * @return HAL_OK if successful, HAL_ERROR otherwise
 * @details Retrieves MAC status information from device
 */
HAL_StatusTypeDef Lora_get_status(void)
{
    char response[128] = {0};
    return Lora_SendCommand("mac get status", response, sizeof(response));
}

/*******************************************************************************
 * Public Functions - Network Join & Setup
 ******************************************************************************/

/**
 * @brief Complete OTAA setup and join using preset credentials
 * @param huart UART handle for LoRa communication
 * @param rst_port GPIO port for reset pin
 * @param rst_pin GPIO pin number for reset
 * @return HAL_OK if join successful, HAL_ERROR otherwise
 * @details Initializes module and attempts OTAA join with built-in credentials
 */
HAL_StatusTypeDef Lora_SetupAndJoinOTAA(UART_HandleTypeDef *huart, GPIO_TypeDef *rst_port, uint16_t rst_pin)
{
    Lora_Init(huart, rst_port, rst_pin);
    Lora_Reset();
    HAL_Delay(1000);
    Lora_FlushUART();
    return Lora_InitOTAA(LORA_DEVICE_EUI, LORA_APP_EUI, LORA_APP_KEY);
}

/**
 * @brief Initialize device with OTAA parameters and join network
 * @param devEui Device EUI (16 hex chars)
 * @param appEui Application EUI (16 hex chars)
 * @param appKey Application Key (32 hex chars)
 * @return HAL_OK if join successful, HAL_ERROR otherwise
 * @details Sets up device parameters and attempts network join with retries
 * @note European parameters are configured automatically
 */
HAL_StatusTypeDef Lora_InitOTAA(const char *devEui, const char *appEui, const char *appKey)
{
    HAL_StatusTypeDef status;
    char response[128] = {0};

    // Input validation
    if (strlen(devEui) != 16 || strlen(appEui) != 16 || strlen(appKey) != 32) {
        Debug_Print("Invalid parameter length\r\n");
        return HAL_ERROR;
    }

    /* MAC reset and configuration sequence */
    if ((status = Lora_SendCommand("mac reset 868", response, sizeof(response))) != HAL_OK) {
        Debug_Print("Failed at 'mac reset 868'\r\n");
        return status;
    }
    HAL_Delay(1000);

    if ((status = Lora_SendMacSet("deveui", devEui)) != HAL_OK ||
        (status = Lora_SendMacSet("appeui", appEui)) != HAL_OK ||
        (status = Lora_SendMacSet("appkey", appKey)) != HAL_OK) {
        Debug_Print("Failed setting device parameters\r\n");
        return status;
    }

    /* Configure European parameters */
    if ((status = Lora_SendMacSet("pwridx", "1")) != HAL_OK ||
        (status = Lora_SendCommand("mac set dr 5", response, sizeof(response))) != HAL_OK ||
        (status = Lora_SendCommand("mac set adr off", response, sizeof(response))) != HAL_OK ||
        (status = Lora_SendCommand("mac set ar off", response, sizeof(response))) != HAL_OK) {
        Debug_Print("Failed configuring device parameters\r\n");
        return status;
    }

    Debug_Print("LoRa OTAA setup complete - attempting join\r\n");
    HAL_Delay(1000);

    if ((status = Lora_SendCommand("mac save", response, sizeof(response))) != HAL_OK) {
        Debug_Print("Failed saving configuration\r\n");
        return status;
    }

    /* Join attempt with retry */
    bool joined = false;
    for (int i = 0; i < 2 && !joined; i++)
    {
        if ((status = Lora_SendCommand("mac join otaa", response, sizeof(response))) != HAL_OK) {
            Debug_Print("Join command failed\r\n");
            return status;
        }
        HAL_Delay(10000);

        if (Lora_CheckJoin() == HAL_OK) {
            Debug_Print("Join successful\r\n");
            return Lora_SendCommand("mac save", response, sizeof(response));
        }
        Debug_Print("Join failed, retrying...\r\n");
    }

    return HAL_ERROR;
}

/**
 * @brief Setup device using ABP activation
 * @param devaddr Device address (8 hex chars)
 * @param nwkskey Network session key (32 hex chars)
 * @param appskey Application session key (32 hex chars)
 * @return HAL_OK if setup successful, HAL_ERROR otherwise
 * @details Configures device with ABP parameters and activates connection
 * @note European parameters are configured automatically
 */
HAL_StatusTypeDef Lora_Setup_ABP(const char *devaddr, const char *nwkskey, const char *appskey)
{
    HAL_StatusTypeDef status;
    char response[128] = {0};

    // Input validation
    if (strlen(devaddr) != 8 || strlen(nwkskey) != 32 || strlen(appskey) != 32) {
        Debug_Print("Invalid parameter length\r\n");
        return HAL_ERROR;
    }

    /* MAC reset and parameter configuration */
    if ((status = Lora_SendCommand("mac reset 868", response, sizeof(response))) != HAL_OK ||
        (status = Lora_SendMacSet("devaddr", devaddr)) != HAL_OK ||
        (status = Lora_SendMacSet("nwkskey", nwkskey)) != HAL_OK ||
        (status = Lora_SendMacSet("appskey", appskey)) != HAL_OK) {
        Debug_Print("Failed setting ABP parameters\r\n");
        return status;
    }

    /* Configure European and specific ABP parameters*/
    if ((status = Lora_SendMacSet("pwridx", "5")) != HAL_OK ||
        (status = Lora_SendCommand("mac set dr 5", response, sizeof(response))) != HAL_OK ||
        (status = Lora_SendCommand("mac set adr off", response, sizeof(response))) != HAL_OK ||
        (status = Lora_SendCommand("mac set ar off", response, sizeof(response))) != HAL_OK) {
        Debug_Print("Failed configuring device parameters\r\n");
        return status;
    }

    Debug_Print("Joining network using ABP\r\n");
    
    if ((status = Lora_SendCommand("mac join abp", response, sizeof(response))) != HAL_OK) {
        Debug_Print("ABP join failed\r\n");
        return status;
    }

    char response2[128] = {0};
    status = Lora_ReadResponse(response2, sizeof(response2), 10000);
    if (status != HAL_OK) {
        Debug_Print("Join confirmation timeout\r\n");
        return status;
    }

    Debug_Print("ABP setup complete\r\n");
    return HAL_OK;
}

/**
 * @brief Check if device is joined to network
 * @return HAL_OK if joined, HAL_ERROR if not joined
 * @details Checks device address to determine join status
 * @note Device address 00000000 indicates not joined
 */
HAL_StatusTypeDef Lora_CheckJoin(void)
{
    char response[128] = {0};

    if (Lora_SendCommand("mac get devaddr", response, sizeof(response)) == HAL_OK)
    {
        if (strcmp(response, "00000000") == 0)
        {
            Debug_Print("Device not joined\r\n");
            return HAL_ERROR;
        }
        Debug_Print("Device joined with address: ");
        Debug_Print(response);
        Debug_Print("\r\n");
        return HAL_OK;
    }
    
    Debug_Print("Failed to get device address\r\n");
    return HAL_ERROR;
}

/*******************************************************************************
 * Public Functions - Data Transmission
 ******************************************************************************/

/**
 * @brief Send data with automatic hex encoding
 * @param data String data to send
 * @return HAL_OK if successful, HAL_ERROR otherwise
 * @details Converts input string to hex and sends as unconfirmed message
 * @note Uses port 1 for transmission
 */
HAL_StatusTypeDef Lora_SendTX(const char *data)
{
    if (!data || strlen(data) == 0) {
        Debug_Print("Invalid data\r\n");
        return HAL_ERROR;
    }

    char hexData[256] = {0};
    char commandBuffer[384] = {0};
    char responseBuffer[128] = {0};

    if (!convert_to_hex(data, strlen(data), hexData, sizeof(hexData))) {
        Debug_Print("Data too long for conversion\r\n");
        return HAL_ERROR;
    }

    strcpy(commandBuffer, "mac tx uncnf 1 ");
    strcat(commandBuffer, hexData);

    return Lora_SendCommand(commandBuffer, responseBuffer, sizeof(responseBuffer));
}

/**
 * @brief Send raw hex data without encoding
 * @param data Array of bytes to send
 * @param len Length of data array (should be 2 bytes)
 * @return HAL_OK if successful, HAL_ERROR otherwise
 * @details Converts bytes to hex string and sends as unconfirmed message
 * @note Uses port 1 for transmission
 */
HAL_StatusTypeDef Lora_SendTXRaw(const uint8_t *data, uint8_t len)
{
    if (!data || len != 2) {
        Debug_Print("Invalid data or length\r\n");
        return HAL_ERROR;
    }

    char hexData[5] = {0};  // 2 bytes = 4 hex chars + null terminator
    char commandBuffer[384] = {0};
    char responseBuffer[128] = {0};
    
    // Convert bytes to hex string
    sprintf(hexData, "%02X%02X", data[0], data[1]);
    
    strcpy(commandBuffer, "mac tx uncnf 1 ");
    strcat(commandBuffer, hexData);

    return Lora_SendCommand(commandBuffer, responseBuffer, sizeof(responseBuffer));
}
