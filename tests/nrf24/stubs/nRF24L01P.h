#ifndef NRF24_TEST_BOARD_H
#define NRF24_TEST_BOARD_H
#include <stdint.h>
#include <stddef.h>
#include "spi.h"
/* Use the real driver API and registers without the STM32 device headers. */
#define __MAIN_H
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define nRF24_CSN_GPIO_Port 0
#define nRF24_CE_GPIO_Port 0
#define nRF24_IRQ_GPIO_Port 0
#define nRF24_CSN_Pin 1
#define nRF24_CE_Pin 2
#define nRF24_IRQ_Pin 3
#define SPI_CSN_GPIO_Port nRF24_CSN_GPIO_Port
#define SPI_CSN_Pin nRF24_CSN_Pin
#define SPI_CE_GPIO_Port nRF24_CE_GPIO_Port
#define SPI_CE_Pin nRF24_CE_Pin
void HAL_GPIO_WritePin(int port, int pin, int value);
int HAL_GPIO_ReadPin(int port, int pin);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t milliseconds);
void Error_Handler(void);
#include "../../../Core/Inc/nRF24L01P.h"
#endif
