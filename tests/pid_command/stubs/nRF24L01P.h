#include <stdint.h>
#define L01_TX_TIMEOUT_MS 4U
#define __DMB() ((void)0)
#define __get_PRIMASK() 0U
#define __disable_irq() ((void)0)
#define __set_PRIMASK(x) ((void)(x))
uint32_t HAL_GetTick(void);
uint8_t L01_TransmitPacket(uint8_t *,uint8_t,uint32_t);
