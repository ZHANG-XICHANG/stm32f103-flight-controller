#include "receive.h"
#include "pid_command.h"
#include "com_debug.h"
#include "telemetry.h"


RemoteData_t remoteData = {0};
static uint8_t rx_buff[32] = {0};  // L01_ReadRXPayload 要求可容納 32 bytes

/* 最小 ACK 協定：4-byte uint32_t，大端序；每成功排入一包才遞增。
 * 重開飛控從 0 開始；無線模組重新初始化時繼續計數。
 */
static uint32_t ack_sequence = 0U;

static HAL_StatusTypeDef Receive_QueueAckTelemetry(void)
{
    uint8_t fifo_status;
    HAL_StatusTypeDef status = L01_ReadSingleReg(L01REG_FIFO_STATUS, &fifo_status);
    if (status != HAL_OK) return status;
    /* 不清空 TX FIFO；保留晶片重送 ACK 所需的內容。 */
    if ((fifo_status & (1U << TX_FULL_1)) != 0U) return HAL_OK;

    uint8_t payload[TELEMETRY_WIRE_SIZE] = {
        (uint8_t)(ack_sequence >> 24), (uint8_t)(ack_sequence >> 16),
        (uint8_t)(ack_sequence >> 8), (uint8_t)ack_sequence
    };
    uint8_t length = 4U;
    TelemetryData_t sample;
    if (Telemetry_Copy(&sample))
    {
        TelemetryWire_Encode(payload, &sample);
        length = TELEMETRY_WIRE_SIZE;
    }
    /* Alternate command results with telemetry; retain result for lost ACK recovery. */
    if ((ack_sequence & 1U) && PidCommand_Result(payload)) length=PID_WIRE_SIZE;
    status = L01_WriteRXPayload_InAck(payload, length);
    if (status == HAL_OK) ++ack_sequence;
    return status;
}

HAL_StatusTypeDef Receive_Init(void)
{
    HAL_StatusTypeDef status = L01_Init();
    if (status == HAL_OK)
    {
        status = L01_Check();
    }
    if (status == HAL_OK)
    {
        /* 必須在 CE 拉高前預載，第一個控制封包才有 ACK 資料。 */
        status = Receive_QueueAckTelemetry();
    }
    if (status == HAL_OK)
    {
        L01_SetCE(CE_HIGH);
    }
    else
    {
        L01_SetCE(CE_LOW);
    }
    return status;
}
/*
*@brief 接收數據
*
*@return 接收結果；硬體錯誤由通訊任務安排重新初始化。
*/
ReceiveResult_t Receive_Data(void)
{
    uint8_t fifo_status;
    uint8_t length;
    if (L01_ReadSingleReg(L01REG_FIFO_STATUS, &fifo_status) != HAL_OK)
    {
        return RECEIVE_RADIO_ERROR;
    }
    /* 以 FIFO 判斷是否有資料，避免清除 IRQ 後漏讀排隊的封包。 */
    if ((fifo_status & (1U << RX_EMPTY)) != 0U)
    {
        return RECEIVE_EMPTY;
    }
    if (L01_ClearIRQ(1U << RX_DR) != HAL_OK ||
        L01_ReadRXPayload(rx_buff, &length) != HAL_OK)
    {
        return RECEIVE_RADIO_ERROR;
    }
    /* 這一包的 ACK 已由硬體送出，現在準備後續 ACK。
     * 即使應用層校驗不符，硬體仍已回 ACK，因此也需要補入。
     */
    if (Receive_QueueAckTelemetry() != HAL_OK)
    {
        return RECEIVE_RADIO_ERROR;
    }
    if (length == PID_WIRE_SIZE)
    {
        PidCommand_Receive(rx_buff, length);
        return RECEIVE_INVALID; /* Commands must not refresh control-link freshness. */
    }
    if (length != REMOTE_PACKET_SIZE)
    {
        return RECEIVE_INVALID;
    }

    //檢查幀頭
    if(rx_buff[0] != FRAME_HEADER_1 || rx_buff[1] != FRAME_HEADER_2 || rx_buff[2] != FRAME_HEADER_3)
    {
        return RECEIVE_INVALID;
    }

    //幀尾檢查
    uint32_t checksum = 0U;
    for (uint8_t i = 0U; i < 13U; i++)
    {
        checksum += rx_buff[i];
    }

    //高位在前，低位在後 將接收到的校驗和組合成一個32位整數
    uint32_t sum_received =
        ((uint32_t)rx_buff[13] << 24) |
        ((uint32_t)rx_buff[14] << 16) |
        ((uint32_t)rx_buff[15] << 8)  |
        ((uint32_t)rx_buff[16]);

    if(checksum != sum_received)
    {
        return RECEIVE_INVALID;
    }

    //將接收到的數據存入 remoteData 結構體
    remoteData.thr = (rx_buff[3] << 8) | rx_buff[4];
    remoteData.yaw = (rx_buff[5] << 8) | rx_buff[6];
    remoteData.pitch = (rx_buff[7] << 8) | rx_buff[8];
    remoteData.roll = (rx_buff[9] << 8) | rx_buff[10];
    remoteData.fixheight = rx_buff[11];
    if (rx_buff[12] == 1U)
    {
        remoteData.power = 1U;
    }

    //debug_printf("Received data: THR=%d, YAW=%d, PITCH=%d, ROLL=%d, FIXHEIGHT=%d\n", remoteData.thr, remoteData.yaw, remoteData.pitch, remoteData.roll, remoteData.fixheight);
    return RECEIVE_OK;


}
