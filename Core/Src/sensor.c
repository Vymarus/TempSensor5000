#include "sensor.h"

/**
 * @brief Configures a GPIO pin as an output.
 * 
 * The setOuput function configures a specified GPIO pin on a given GPIO port (GPIOx) as 
 * a push-pull output with low-speed frequency. It uses the HAL_GPIO_Init function to initialize 
 * the pin with the provided settings.
 * 
 * @param GPIOx Pointer to the GPIO port where the pin is located 
 * @param GPIO_Pin Specifies the GPIO pin to be configured. This parameter 
 *                 should be a value from the GPIO_PIN_x macros
 */
void setOuput(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}


/**
 * @brief Configures a GPIO pin as an input.
 * 
 * This function initializes the specified GPIO pin to operate in input mode
 * with no pull-up or pull-down resistors. It uses the HAL library to configure
 * the GPIO settings.
 * 
 * @param GPIOx Pointer to the GPIO port where the pin is located 
 * @param GPIO_Pin Specifies the GPIO pin to be configured. This parameter can be
 *                 any value from the GPIO_PIN_x macros
 */
void setInput(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}


/**
 * @brief Initializes communication with the DHT11 sensor by sending 
 * a start signal and checking the response.
 * 
 * This function configures the specified GPIO pin to send a start signal to the sensor, 
 * waits for the sensor's response, and determines the status of the sensor based on the response.
 * 
 * @param GPIOx Pointer to the GPIO port where the sensor is connected.
 * @param GPIO_Pin The specific GPIO pin number connected to the sensor.
 * @return uint8_t response
 *         - 1: Sensor responded successfully.
 *         - 2: Sensor responded with an unexpected signal.
 * 
 * @note This function assumes the presence of helper functions `setOuput`, `setInput`, 
 *       and `delayMicroS` for GPIO configuration and precise timing.
 * @note The function uses HAL (Hardware Abstraction Layer) functions for GPIO operations.
 * 
 * @details
   * The function performs the following steps:
   * 1. Configures the GPIO pin as an output and sets it low to send the start signal.
   * 2. Waits for 18 milliseconds to ensure the sensor detects the start signal.
   * 3. Configures the GPIO pin as an input to read the sensor's response.
   * 4. Waits for the sensor to pull the pin low for 80 microseconds.
   * 5. Checks if the sensor pulls the pin high after 80 microseconds.
   * 6. Waits for the pin to go low again, indicating the end of the response signal.
   * 7. Returns the response status based on the observed signal timings.

 */
uint8_t startSensor(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin){
  uint8_t response = 0; // Response variable used to indicate the status of the sensor

  setOuput(GPIOx, GPIO_Pin);
  HAL_GPIO_WritePin(GPIOx, GPIO_Pin, 0); 
  delayMicroS(18000, &htim6);
  setInput(GPIOx,GPIO_Pin); 

  delayMicroS(40, &htim6);
    if(!(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin))){
      delayMicroS(80, &htim6); 
      if(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)){
        response = 1;
      }
      else{
        response = 2;
        return response; // Sensor did not respond correctly
      }
    }
    else{
      response = 2; // Sensor did not respond correctly
      return response; // Sensor did not respond correctly

    }
  
    while(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)); // Wait until pin goes low

    return response; // Return the response status on sucess
}


/**
 * @brief Reads an 8-bit data value from the DHT11 sensor.
 * 
 * This function reads a single byte of data from the DHT11 sensor by interpreting 
 * the timing of high and low signals on the specified GPIO pin. Each bit is determined 
 * based on the duration of the high signal.
 * 
 * @param GPIOx Pointer to the GPIO port where the sensor is connected.
 * @param GPIO_Pin The specific GPIO pin number connected to the sensor.
 * @return uint8_t data
 *         - The 8-bit data value read from the sensor.
 * 
 * @note This function assumes the presence of a helper function `delayMicroS` 
 *       for precise timing and uses HAL functions 
 *       for GPIO operations.
 * 
 * @details
  * The function performs the following steps:
  * 1. Waits for the pin to go high, indicating the start of a bit transmission.
  * 2. Waits for 40 microseconds to determine the duration of the high signal.
  * 3. Reads the pin state:
  *    - If the pin is still high, the bit is interpreted as 1.
  *    - If the pin is low, the bit is interpreted as 0.
  * 4. Sets or clears the corresponding bit in the data variable based on the pin state.
  * 5. Waits for the pin to go low, indicating the end of the bit transmission.
  * 6. Repeats the process for all 8 bits to construct the full byte.
 */
uint8_t ReadDHT11Byte(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin){
  uint8_t data = 0;
  for(uint8_t i = 0; i < 8; i++){
    while(!(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin))); // Wait until the pin goes high

    delayMicroS(35, &htim6); // Wait 35 microseconds

    if(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)){
      data |= (1 << (7 - i)); // Set the corresponding bit in the data variable
    }
    else{
      data &= ~(1 << (7 - i)); // Clear the corresponding bit in the data variable
    }
    while(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin));
  }
  return data; // Return the read data
}

/**
 * @brief Reads data from the DHT11 sensor and stores it in the provided buffer.
 * 
 * This function reads humidity and temperature data from the DHT11 sensor by 
 * sequentially reading bytes of data. The first byte corresponds to the integer 
 * part of the humidity, and the third byte corresponds to the integer part of 
 * the temperature. The data is stored in the provided buffer for further use.
 * 
 * @param GPIOx The GPIO port connected to the DHT11 sensor.
 * @param GPIO_Pin The GPIO pin connected to the DHT11 sensor.
 * @param buffer Pointer to a buffer where the humidity and temperature data 
 *               will be stored. The first element (buffer[0]) contains the 
 *               humidity data, and the second element (buffer[1]) contains 
 *               the temperature data.
 * 
 * @note Ensure proper initialization of the GPIO pin and adherence to the 
 *       DHT11 communication protocol before calling this function.
 */
void ReadDHT11Data(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t *buffer){
  uint8_t HumData1 = ReadDHT11Byte(GPIOx, GPIO_Pin); // Read the first byte of data from the DHT11 sensor
  uint8_t HumData2 = ReadDHT11Byte(GPIOx, GPIO_Pin); // Read the second byte of data from the DHT11 sensor
  uint8_t TempData1 = ReadDHT11Byte(GPIOx, GPIO_Pin); // Read the third byte of data from the DHT11 sensor
  uint8_t TempData2 = ReadDHT11Byte(GPIOx, GPIO_Pin); // Read the fourth byte of data from the DHT11 sensor  
  uint8_t CheckSum = ReadDHT11Byte(GPIOx, GPIO_Pin); // Read the checksum byte from the DHT11 sensor

  buffer[0] = HumData1; // Humidity data
  buffer[1] = TempData1; // Temperature data
}

/**
 * @brief Delays execution for a specified duration in microseconds.
 * 
 * This function uses a hardware timer to create a precise delay in microseconds.
 * It resets the timer counter to zero and waits until the counter reaches the 
 * specified duration.
 * 
 * @param duration The delay duration in microseconds.
 * @param htimx The handle to the hardware timer (TIM_HandleTypeDef) used for the delay.
 * 
 * @note Ensure that the timer is properly initialized and configured before calling 
 *       this function. The timer's clock frequency must be set appropriately to 
 *       achieve accurate delays.
 */
void delayMicroS(uint16_t duration, TIM_HandleTypeDef *htimx) {
  __HAL_TIM_SET_COUNTER(htimx, 0); // Reset the timer counter
  while(__HAL_TIM_GET_COUNTER(htimx) < duration); // Wait until the timer counter reaches the specified duration
}


