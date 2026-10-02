#include <stdint.h>
typedef void *osMessageQueueId_t;
#define osOK 0
int osMessageQueuePut(osMessageQueueId_t,const void *,uint8_t,uint32_t);
