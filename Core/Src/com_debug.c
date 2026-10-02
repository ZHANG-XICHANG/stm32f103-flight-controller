#include "com_debug.h"
#include "usart.h"

int __io_putchar(int ch)
{
    uint8_t data = (uint8_t)ch;

    if (HAL_UART_Transmit(&huart1, &data, 1, 100) != HAL_OK)
    {
        return EOF;
    }

    return ch;
}