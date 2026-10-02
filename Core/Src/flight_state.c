#include "flight_state.h"
#include "receive.h"

/* 目前的遙控器連線狀態與飛控狀態。 */
static volatile LinkState_t link_state = LINK_DISCONNECTED;
static volatile FlightState_t flight_state = IDLE;

/* 最後一次收到有效遙控器封包時的 FreeRTOS tick。 */
static volatile TickType_t last_rx_time = 0;

/* 初始化時一律從未連線、未解鎖狀態開始。 */
void FlightState_Init(void)
{
    link_state = LINK_DISCONNECTED;
    flight_state = IDLE;
    last_rx_time = xTaskGetTickCount();
}

void FlightState_RemoteReceived(void)
{
    /* 記錄本次有效封包時間，供失聯超時判斷使用。 */
    last_rx_time = xTaskGetTickCount();

    /* 收到有效封包後立即恢復連線狀態。 */
    link_state = LINK_CONNECTED;
}

void FlightState_Update(void)
{
    TickType_t now = xTaskGetTickCount();

    /* TickType_t 的無號減法可正確處理 tick 計數器回繞。 */
    if ((now - last_rx_time) > pdMS_TO_TICKS(REMOTE_TIMEOUT_MS))
    {
        /* 超過指定時間沒有有效封包，判定遙控器失聯。 */
        link_state = LINK_DISCONNECTED;
    }
}

LinkState_t FlightState_GetLinkState(void)
{
    return link_state;
}

/**
 * @brief 檢查搖桿是否在解鎖位置。
 * @return 1 表示已達解鎖，0 表示尚未達成。
 */
static uint8_t flight_state_unlocked(void)
{
    /* 油門需接近最低，其餘三軸需位於中點附近。 */
    uint8_t sticks_in_unlock_position =
        (remoteData.thr < 10 &&
         remoteData.yaw >= 490 && remoteData.yaw <= 510 &&
         remoteData.pitch >= 490 && remoteData.pitch <= 510 &&
         remoteData.roll >= 490 && remoteData.roll <= 510);

    if (sticks_in_unlock_position)
    {
     return 1;   /* 已持續足夠時間，允許解鎖。 */
    }

    return 0;
}

/**
 * @brief 執行一次飛控狀態轉移判斷。
 *
 * IDLE 等待解鎖；NORMAL 與 FIX_HEIGHT 在失聯時進入 FAIL；
 * FAIL 在遙控器恢復連線後回到 NORMAL。
 */
void FlightState_Process(void)
{
    uint8_t powerCommand;

    taskENTER_CRITICAL();
    powerCommand = remoteData.power;
    remoteData.power = 0U;
    taskEXIT_CRITICAL();
    
    switch (flight_state)
    {
    case IDLE:
        /* 閒置狀態下等待搖桿完成解鎖動作。 */
        if (flight_state_unlocked() && (powerCommand == 1U))
        {
            flight_state = NORMAL;
        }
        break;
    case NORMAL:
        /* 失聯的安全處理優先於模式切換。 */
        if (link_state == LINK_DISCONNECTED)
        {
            flight_state = FAIL;
        }
        /* 遙控器收到關機指令時返回閒置狀態。 */
        else if (powerCommand == 1U)
        {
            flight_state = IDLE;
        }
        /* 遙控器開啟定高功能時切換至定高模式。 */
        else if (remoteData.fixheight == 1U)
        {
            flight_state = FIX_HEIGHT;
        }
        break;

    case FIX_HEIGHT:

        /* 定高期間失聯仍必須進入失效保護。 */
        if (link_state == LINK_DISCONNECTED)
        {
            flight_state = FAIL;
        }
        /* 遙控器關閉定高功能時返回一般控制。 */
        if (remoteData.fixheight == 0U)
        {
            flight_state = NORMAL;
        }        
        break;

    case FAIL:
        /* 遙控器恢復連線後返回一般控制。 */
        if (link_state == LINK_CONNECTED)
        {
            flight_state = NORMAL;
        }
        break;    
    default:
        /* 非預期狀態一律回到安全的閒置狀態。 */
        flight_state = IDLE;
        break;  
    }
}

FlightState_t FlightState_GetFlightState(void)
{
    /* Getter 只回傳狀態，不執行狀態轉移或硬體控制。 */
    return flight_state;
}
