#ifndef RECEIVE_H
#define RECEIVE_H

#include "nRF24L01P.h"



typedef struct
{
    uint16_t thr;       /* 油門，2 bytes，映射範圍 0～1000。 */
    uint16_t yaw;       /* 偏航，2 bytes，映射範圍 0～1000。 */
    uint16_t pitch;     /* 俯仰，2 bytes，映射範圍 0～1000。 */
    uint16_t roll;      /* 滾轉，2 bytes，映射範圍 0～1000。 */
    uint8_t fixheight;  /* 定高單次事件，封包 byte 11。 */
    uint8_t power;      /* 開關機單次事件，封包 byte 12。 */
} RemoteData_t;

extern RemoteData_t remoteData;

/* 控制封包格式獨立於無線驅動，固定為 17 bytes。 */
#define REMOTE_PACKET_SIZE 17U

typedef enum
{
    RECEIVE_OK = 0,
    RECEIVE_EMPTY,
    RECEIVE_INVALID,
    RECEIVE_RADIO_ERROR
} ReceiveResult_t;

/* 初始化、驗證無線模組並啟動接收；只由通訊任務呼叫。 */
HAL_StatusTypeDef Receive_Init(void);

//定義幀頭較驗值
#define FRAME_HEADER_1 0xAA
#define FRAME_HEADER_2 0x55
#define FRAME_HEADER_3 0xAA

/*
*@brief 接收數據
*
*@return RECEIVE_OK / RECEIVE_EMPTY / RECEIVE_INVALID / RECEIVE_RADIO_ERROR
*/
ReceiveResult_t Receive_Data(void);

#endif /* RECEIVE_H */
