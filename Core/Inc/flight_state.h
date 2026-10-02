#ifndef FLIGHT_STATE_H
#define FLIGHT_STATE_H

#include "FreeRTOS.h"
#include "task.h"

/* 超過此時間沒有收到有效遙控封包，即判定遙控器失聯。 */
#define REMOTE_TIMEOUT_MS 1000

/* 遙控器通訊連線狀態。 */
typedef enum
{
    LINK_DISCONNECTED = 0,  /* 尚未連線或已超時失聯。 */
    LINK_CONNECTED          /* 最近仍有收到有效封包。 */
} LinkState_t;

/* 飛控運作狀態。 */
typedef enum
{
    IDLE,                   /* 未解鎖，馬達應維持安全輸出。 */
    NORMAL,                 /* 已解鎖，執行一般飛行控制。 */
    FAIL,                   /* 遙控器失聯，進入失效保護。 */
    FIX_HEIGHT              /* 已解鎖，執行定高控制。 */
} FlightState_t;

/**
 * @brief 初始化遙控器連線狀態、飛行狀態及接收時間。
 */
void FlightState_Init(void);

/**
 * @brief 通知狀態模組已收到一筆有效的遙控器封包。
 */
void FlightState_RemoteReceived(void);

/**
 * @brief 檢查遙控器封包是否超時，並更新連線狀態。
 */
void FlightState_Update(void);

/**
 * @brief 根據連線狀態及遙控器輸入推進飛控狀態機。
 */
void FlightState_Process(void);

/**
 * @brief 取得目前遙控器連線狀態。
 * @return 目前的 LinkState_t 狀態。
 */
LinkState_t FlightState_GetLinkState(void);

/**
 * @brief 取得目前飛控狀態；此函式不會改變狀態。
 * @return 目前的 FlightState_t 狀態。
 */
FlightState_t FlightState_GetFlightState(void);

#endif
