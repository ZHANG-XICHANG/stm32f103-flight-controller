#ifndef NRF24_TEST_SPI_H
#define NRF24_TEST_SPI_H
#include <stdint.h>
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef int SPI_HandleTypeDef;
extern SPI_HandleTypeDef hspi1;
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, const uint8_t *tx,
                           uint8_t *rx, uint16_t size, uint32_t timeout);
#endif
