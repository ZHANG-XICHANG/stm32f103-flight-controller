/**@file  	    nRF24L01P.c
* @brief            nRF24L01+ 底層操作與設定。
* @author           hyh
* @date             2021.9.17
* @version          1.0
* @copyright        Chengdu Ebyte Electronic Technology Co.Ltd
**********************************************************************************
*/
#include "nRF24L01P.h"
#include "spi.h"
#include "cmsis_os2.h"
#include "com_debug.h"

/* 毫秒級等待：排程器運行時讓出 CPU；啟動前使用 HAL tick。 */
static void L01_DelayMs(uint32_t milliseconds)
{
    if (osKernelGetState() == osKernelRunning)
    {
        /* 多等待一個 tick，避免呼叫恰逢 tick 邊界而縮短最小延遲。 */
        uint32_t ticks = (milliseconds * osKernelGetTickFreq() + 999U) / 1000U;
        (void)osDelay(ticks + 1U);
    }
    else
    {
        HAL_Delay(milliseconds);
    }
}

/* The entire CSN transaction shares one SPI deadline. */
static HAL_StatusTypeDef SPI_ExchangeByte(uint8_t tx, uint8_t *rx, uint32_t timeout)
{
    return HAL_SPI_TransmitReceive(&hspi1, &tx, rx, 1U, timeout);
}

static HAL_StatusTypeDef L01_Transfer(uint8_t command, const uint8_t *tx,
                                      uint8_t *rx, uint8_t size, uint8_t *status)
{
    uint8_t received[32];
    uint8_t commandStatus;
    uint32_t start = HAL_GetTick();
    HAL_StatusTypeDef result;

    if (size > sizeof(received)) return HAL_ERROR;
    L01_CSN_LOW();
    result = SPI_ExchangeByte(command, &commandStatus, L01_SPI_TIMEOUT_MS);
    for (uint8_t i = 0; result == HAL_OK && i < size; ++i)
    {
        uint32_t elapsed = (uint32_t)(HAL_GetTick() - start);
        if (elapsed >= L01_SPI_TIMEOUT_MS)
        {
            result = HAL_TIMEOUT;
            break;
        }
        result = SPI_ExchangeByte(tx != NULL ? tx[i] : NOP, &received[i],
                                  L01_SPI_TIMEOUT_MS - elapsed);
    }
    L01_CSN_HIGH();
    if (result != HAL_OK)
    {
        L01_SetCE(CE_LOW);
        return result;
    }
    /* Publish only complete reads. */
    if (status != NULL) *status = commandStatus;
    if (rx != NULL)
    {
        for (uint8_t i = 0; i < size; ++i) rx[i] = received[i];
    }
    return HAL_OK;
}

/* Preserve the first HAL error and stop dependent operations. */
#define L01_TRY(operation) do { \
    HAL_StatusTypeDef l01_result = (operation); \
    if (l01_result != HAL_OK) return l01_result; \
} while (0)

/*CE 腳位的電位狀態*/
static uint8_t CE_Status = 0;
/*!
================================================================================
------------------------------------函式-----------------------------------
================================================================================
*/
/*!
 *  @brief          取得 CE 腳位的電位狀態
 *  @param          無     
 *  @return         CE_LOW 或 CE_HIGH
 *  @note          
*/
uint8_t L01_GetCEStatus(void)
{
    return CE_Status;
}
/*!
 *  @brief          將 CE 腳位設為低電位或高電位
 *  @param          status：CE 腳位的電位狀態    
 *  @return         無
 *  @note          
*/
void L01_SetCE(CE_STAUS status)
{
    CE_Status = status;
    if (status == CE_LOW)    { L01_CE_LOW(); }
    else                     { L01_CE_HIGH(); }
}
/*!
 *  @brief        讀取指定暫存器的值   
 *  @param        addr：暫存器位址
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ReadSingleReg(uint8_t addr, uint8_t *value)
{
    if (value == NULL) return HAL_ERROR;
    return L01_Transfer(R_REGISTER | addr, NULL, value, 1U, NULL);
}
/*!
 *  @brief        讀取指定暫存器的資料並存入緩衝區
 *  @param        start_addr：暫存器的起始位址
 *  @param        buffer：儲存讀取資料的緩衝區
*  @param         size：要讀取的位元組數
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ReadMultiReg(uint8_t start_addr, uint8_t *buffer, uint8_t size)
{
    if (buffer == NULL || size == 0U) return HAL_ERROR;
    return L01_Transfer(R_REGISTER | start_addr, NULL, buffer, size, NULL);
}
/*!
 *  @brief        將數值寫入指定暫存器   
 *  @param        addr：暫存器位址
 *  @param        value：要寫入的值  
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_WriteSingleReg(uint8_t addr, uint8_t value)
{
    return L01_Transfer(W_REGISTER | addr, &value, NULL, 1U, NULL);
}
/*!
 *  @brief        將緩衝區資料寫入指定暫存器  
 *  @param        start_addr：暫存器的起始位址
 *  @param        buffer：存放待寫入資料的緩衝區
 *  @param        size：要寫入的位元組數  
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_WriteMultiReg(uint8_t start_addr, uint8_t *buffer, uint8_t size)
{
    if (buffer == NULL || size == 0U) return HAL_ERROR;
    return L01_Transfer(W_REGISTER | start_addr, buffer, NULL, size, NULL);
}
/*!
 *  @brief        將 nRF24L01 設為斷電模式（PowerDown）          
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_SetPowerDown(void)
{
    uint8_t controlreg;
    L01_TRY(L01_ReadSingleReg(L01REG_CONFIG, &controlreg));
    return L01_WriteSingleReg(L01REG_CONFIG, controlreg & (~(1U << PWR_UP)));
}
/*!
 *  @brief        將 nRF24L01 設為上電模式（PowerUp）       
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_SetPowerUp(void)
{
    uint8_t controlreg;
    L01_TRY(L01_ReadSingleReg(L01REG_CONFIG, &controlreg));
    if ((controlreg & (1U << PWR_UP)) == 0U)
    {
        L01_TRY(L01_WriteSingleReg(L01REG_CONFIG, controlreg | (1U << PWR_UP)));
        L01_DelayMs(5U);
    }
    return HAL_OK;
}
/*!
 *  @brief        清空 TX 傳送緩衝區             
 *  @param        無 
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_FlushTX(void)
{
    return L01_Transfer(FLUSH_TX, NULL, NULL, 0U, NULL);
}
/*!
 *  @brief        清空 RX 接收緩衝區           
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_FlushRX(void)
{
    return L01_Transfer(FLUSH_RX, NULL, NULL, 0U, NULL);
}
/*!
 *  @brief        重複使用上次傳送的酬載資料           
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ReuseTXPayload(void)
{
    return L01_Transfer(REUSE_TX_PL, NULL, NULL, 0U, NULL);
}
/*!
 *  @brief        對 nRF24L01+ 執行空操作（NOP）           
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_Nop(void)
{
    return L01_Transfer(NOP, NULL, NULL, 0U, NULL);
}
/*!
 *  @brief        讀取 nRF24L01+ 的狀態暫存器           
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ReadStatusReg(uint8_t *status)
{
    if (status == NULL) return HAL_ERROR;
    return L01_Transfer(R_REGISTER | L01REG_STATUS, NULL, NULL, 0U, status);
}
/*!
 *  @brief        清除 nRF24L01+ 產生的中斷（IRQ）           
 *  @param        irqMask：RX_DR（位元 [6]）、TX_DS（位元 [5]）、MAX_RT（位元 [4]）
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ClearIRQ(uint8_t irqMask)
{
    return L01_WriteSingleReg(L01REG_STATUS, irqMask & IRQ_ALL);
}
/*!
 *  @brief        讀取 nRF24L01+ 的中斷（IRQ）狀態           
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ReadIRQSource(uint8_t *irqMask)
{
    uint8_t status;
    if (irqMask == NULL) return HAL_ERROR;
    L01_TRY(L01_ReadStatusReg(&status));
    *irqMask = status & IRQ_ALL;
    return HAL_OK;
}

HAL_StatusTypeDef L01_ReadTXStatus(uint8_t *txStatus)
{
    uint8_t status;
    if (txStatus == NULL) return HAL_ERROR;
    L01_TRY(L01_ReadIRQSource(&status));
    *txStatus = status & ((1U << TX_DS) | (1U << MAX_RT));
    return HAL_OK;
}

uint8_t L01_TransmitPacket(uint8_t *buffer, uint8_t size, uint32_t timeout_ms)
{
    const uint8_t txFlags = (1U << TX_DS) | (1U << MAX_RT);
    uint8_t result;
    uint32_t start;

    if (buffer == NULL || size == 0U || size > 32U || timeout_ms == 0U)
    {
        return L01_TX_INVALID_PARAM;
    }
#if DYNAMIC_PACKET == 0
    if (size != FIXED_PACKET_LEN)
    {
        return L01_TX_INVALID_PARAM;
    }
#endif

    L01_SetCE(CE_LOW);
    /* 等待正在接收的封包/ACK 結束，再更改模式。 */
    L01_DelayMs(1U);
    if (L01_SetTRMode(TX_MODE) != HAL_OK) goto spi_error;
    if (L01_SetPowerUp() != HAL_OK) goto spi_error;
    if (L01_ClearIRQ(txFlags) != HAL_OK) goto spi_error;
    /* 此 API 會先清空 TX FIFO，確保本次只送一個封包。 */
    if (L01_WriteTXPayload_Ack(buffer, size) != HAL_OK) goto spi_error;

    start = HAL_GetTick();
    L01_SetCE(CE_HIGH);
    /* CE 高電位至少 10 us；採用現有毫秒時基，避免未校準的空迴圈。 */
    L01_DelayMs(1U);
    L01_SetCE(CE_LOW);

    for (;;)
    {
        uint8_t status;
        if (L01_ReadTXStatus(&status) != HAL_OK) goto spi_error;
        /* 若異常地同時出現兩個旗標，優先回報失敗。 */
        if ((status & (1U << MAX_RT)) != 0U)
        {
            result = L01_TX_MAX_RETRY;
            break;
        }
        if ((status & (1U << TX_DS)) != 0U)
        {
            result = L01_TX_SUCCESS;
            break;
        }
        /* 無號相減可處理 HAL tick 溢位。 */
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            result = L01_TX_TIMEOUT;
            break;
        }
        L01_DelayMs(1U);
    }

    if (result == L01_TX_TIMEOUT)
    {
        /* CE 拉低不會中止既有自動重傳；先斷電停止，再清理 FIFO。 */
        if (L01_SetPowerDown() != HAL_OK) goto spi_error;
        L01_DelayMs(1U);
    }
    if (result != L01_TX_SUCCESS)
    {
        if (L01_FlushTX() != HAL_OK) goto spi_error;
    }
    if (L01_ClearIRQ(txFlags) != HAL_OK) goto spi_error;
    if (L01_SetTRMode(RX_MODE) != HAL_OK) goto spi_error;
    if (L01_SetPowerUp() != HAL_OK) goto spi_error;
    L01_SetCE(CE_HIGH);
    /* RX 啟動需要最多 130 us。 */
    L01_DelayMs(1U);
    return result;

spi_error:
    /* Radio mode is unknown; recovery belongs to the owning task. */
    L01_CSN_HIGH();
    L01_SetCE(CE_LOW);
    return L01_TX_SPI_ERROR;
}

/*!
 *  @brief        讀取 FIFO 頂端緩衝區的酬載長度           
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ReadTopFIFOWidth(uint8_t *width)
{
    if (width == NULL) return HAL_ERROR;
    return L01_Transfer(R_RX_PL_WID, NULL, width, 1U, NULL);
}
/*!
 *  @brief        從 FIFO 讀取 RX 接收酬載並存入緩衝區            
 *  @param        buffer：用於儲存資料的緩衝區
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_ReadRXPayload(uint8_t *buffer, uint8_t *length)
{
    uint8_t width;
    uint8_t received[32];
    if (buffer == NULL || length == NULL) return HAL_ERROR;
    *length = 0U;
#if DYNAMIC_PACKET == 1
    L01_TRY(L01_ReadTopFIFOWidth(&width));
#else
    /* 固定長度模式使用已設定的管道 0 長度。 */
    L01_TRY(L01_ReadSingleReg(L01REG_RX_PW_P0, &width));
#endif
    if (width == 0U || width > 32U)
    {
        L01_TRY(L01_FlushRX());
        return HAL_ERROR;
    }
    L01_TRY(L01_Transfer(R_RX_PAYLOAD, NULL, received, width, NULL));
    /* R_RX_PAYLOAD consumes one packet; preserve the remaining RX FIFO. */
    for (uint8_t i = 0; i < width; ++i) buffer[i] = received[i];
    *length = width;
    return HAL_OK;
}
/*!
 *  @brief        將 TX 傳送酬載寫入資料管道，接收端（PRX）會回傳確認訊號（ACK）         
 *  @param        buffer：存放資料的緩衝區
 *  @param        size：要寫入的位元組數  
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_WriteTXPayload_Ack(uint8_t *buffer, uint8_t size)
{
    if (buffer == NULL || size == 0U || size > 32U) return HAL_ERROR;
    L01_TRY(L01_FlushTX());
    return L01_Transfer(W_TX_PAYLOAD, buffer, NULL, size, NULL);
}
/*!
 *  @brief        將 TX 傳送酬載寫入資料管道，接收端（PRX）不會回傳確認訊號（ACK）         
 *  @param        buffer：存放資料的緩衝區
 *  @param        size：要寫入的位元組數  
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_WriteTXPayload_NoAck(uint8_t *buffer, uint8_t size)
{
    if (buffer == NULL || size == 0U || size > 32U) return HAL_ERROR;
    return L01_Transfer(W_TX_PAYLOAD_NOACK, buffer, NULL, size, NULL);
}
/*!
 *  @brief        在 RX 接收模式下，將傳送酬載寫入資料管道         
 *  @param        buffer：存放資料的緩衝區
 *  @param        size：要寫入的位元組數  
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_WriteRXPayload_InAck(uint8_t *buffer, uint8_t size)
{
    if (buffer == NULL || size == 0U || size > 32U) return HAL_ERROR;
    return L01_Transfer(W_ACK_PAYLOAD, buffer, NULL, size, NULL);
}
/*!
 *  @brief        將傳送位址寫入 TX_ADDR 暫存器          
 *  @param        addrBuffer：存放位址的緩衝區
 *  @param        addr_size：位址的位元組數
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note         僅供 PTX 傳送端裝置使用 
*/
HAL_StatusTypeDef L01_SetTXAddr(uint8_t *addrBuffer,uint8_t addr_size)
{
    uint8_t size = (addr_size > 5) ? 5 : addr_size;
    return L01_WriteMultiReg(L01REG_TX_ADDR,addrBuffer,size);
}
/*!
 *  @brief        設定 RX 接收管道的位址 
 *  @param        pipeNum：資料管道編號         
 *  @param        addrBuffer：存放位址的緩衝區
 *  @param        addr_size：位址的位元組數
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_SetRXAddr(uint8_t pipeNum,uint8_t *addrBuffer,uint8_t addr_size)
{
    uint8_t size = (addr_size > 5) ? 5 : addr_size;
    uint8_t num = (pipeNum > 5) ? 5 : pipeNum;
    return L01_WriteMultiReg(L01REG_RX_ADDR_P0 + num,addrBuffer,size);
}
/*!
 *  @brief        設定 nRF24L01+ 的資料傳輸速率          
 *  @param        drate：250K、1M、2M
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_SetDataRate(L01_DRATE drate)
{
    uint8_t mask;
    L01_TRY(L01_ReadSingleReg(L01REG_RF_SETUP, &mask));
    mask &= ~((1 << RF_DR_LOW) | (1 << RF_DR_HIGH));
    if(drate == DRATE_250K)
    {
        mask |= (1 << RF_DR_LOW);
    }
    else if(drate == DRATE_1M)
    {
        /* Both rate bits were cleared above: 00 selects 1 Mbps. */
    }
    else if(drate == DRATE_2M)
    {
        mask |= (1 << RF_DR_HIGH);
    }
    return L01_WriteSingleReg(L01REG_RF_SETUP,mask);
}
/*!
 *  @brief        設定 nRF24L01+ 的發射功率          
 *  @param        power：18dB、12dB、6dB、0dB
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_SetPower(L01_PWR power)
{
    uint8_t mask;
    L01_TRY(L01_ReadSingleReg(L01REG_RF_SETUP, &mask));
    mask &= ~0x07;
    switch (power)
    {
    case POWER_N_18:
        mask |= PWR_N_18DB;
        break;
    case POWER_N_12:
        mask |= PWR_N_12DB;
        break;
    case POWER_N_6:
        mask |= PWR_N_6DB;
        break;
    case POWER_N_0:
        mask |= PWR_N_0DB;
        break;
    default:
        break;
    }
    return L01_WriteSingleReg(L01REG_RF_SETUP,mask);
}
/*!
 *  @brief        設定 nRF24L01+ 的頻率          
 *  @param        freq：跳頻頻點，範圍為 0–125，對應 2400 MHz–2525 MHz
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_WriteHoppingPoint(uint8_t freq)
{
    return L01_WriteSingleReg(L01REG_RF_CH,freq <= 125 ? freq : 125);
}
/*!
 *  @brief        將 nRF24L01+ 設為 TX 傳送模式或 RX 接收模式         
 *  @param        mode：TX 傳送／RX 接收
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_SetTRMode(L01_MODE mode)
{
    uint8_t mask;
    L01_TRY(L01_ReadSingleReg(L01REG_CONFIG, &mask));
    if (mode == TX_MODE)
    {
        mask &= ~(1 << PRIM_RX);
    }
    else if (mode == RX_MODE)
    {
        mask |= (1 << PRIM_RX);
    }
    return L01_WriteSingleReg(L01REG_CONFIG,mask);
}
/*!
 *  @brief        初始化 nRF24L01+         
 *  @param        無
 *  @return       HAL_OK on success; otherwise a HAL error. Data is returned via output pointers.
 *  @note          
*/
HAL_StatusTypeDef L01_Init(void)
{
    uint8_t addr[5] = {INIT_ADDR};
    L01_SetCE(CE_LOW);
    L01_TRY(L01_SetPowerDown());
    L01_TRY(L01_ClearIRQ(IRQ_ALL));
#if DYNAMIC_PACKET == 1
    //動態酬載長度
    L01_TRY(L01_WriteSingleReg(L01REG_DYNPD,(1 << DPL_P0)));//啟用管道 0 的動態酬載長度
    L01_TRY(L01_WriteSingleReg(L01REG_FEATURE,(1 << EN_DPL)|(1 << EN_ACK_PAY)));
#elif DYNAMIC_PACKET == 0
    //固定酬載長度
    L01_TRY(L01_WriteSingleReg(L01REG_DYNPD, 0U));
    L01_TRY(L01_WriteSingleReg(L01REG_FEATURE, 0U));
    L01_TRY(L01_WriteSingleReg(L01REG_RX_PW_P0,FIXED_PACKET_LEN));
#endif
    L01_TRY(L01_WriteSingleReg(L01REG_CONFIG, L01_INIT_CONFIG));
    L01_TRY(L01_WriteSingleReg(L01REG_ENAA,(1 << ENAA_P0)));//啟用管道 0 的自動確認（ACK）
    L01_TRY(L01_WriteSingleReg(L01REG_EN_RXADDR,(1 << ERX_P0)));//啟用 RX 接收管道 0
    L01_TRY(L01_WriteSingleReg(L01REG_SETUP_AW,AW_5BYTES));//位址長度：5 位元組
    L01_TRY(L01_WriteSingleReg(L01REG_SETUP_RETR, L01_INIT_RETR));
    L01_TRY(L01_WriteSingleReg(L01REG_RF_SETUP, L01_INIT_RF_SETUP));
    L01_TRY(L01_SetTXAddr(addr,5));//設定 TX 傳送位址
    L01_TRY(L01_SetRXAddr(0,addr,5));//設定 RX 接收位址
    L01_TRY(L01_SetTRMode(RX_MODE));
    L01_TRY(L01_WriteHoppingPoint(L01_INIT_RF_CHANNEL));
    L01_TRY(L01_SetPowerUp());
    L01_TRY(L01_FlushTX());
    L01_TRY(L01_FlushRX());
    return HAL_OK;
}

/**
* @brief nRF24L01的硬件接口層初始化檢查
*
* @return HAL_OK if register verification passes; HAL error otherwise.
*/
HAL_StatusTypeDef L01_Check(void)
{
  /* 初始化最後會上電並設定 RX 模式，CONFIG 預期值包含這兩個位元。 */
  const uint8_t expectedConfig = L01_INIT_CONFIG | (1U << PWR_UP) | (1U << PRIM_RX);
  uint8_t config;
  L01_TRY(L01_ReadSingleReg(L01REG_CONFIG, &config));
  uint8_t channel;
  L01_TRY(L01_ReadSingleReg(L01REG_RF_CH, &channel));
  uint8_t rfSetup;
  L01_TRY(L01_ReadSingleReg(L01REG_RF_SETUP, &rfSetup));
  uint8_t retrySetup;
  L01_TRY(L01_ReadSingleReg(L01REG_SETUP_RETR, &retrySetup));
#if DYNAMIC_PACKET == 1
  uint8_t feature, dynpd;
  L01_TRY(L01_ReadSingleReg(L01REG_FEATURE, &feature));
  L01_TRY(L01_ReadSingleReg(L01REG_DYNPD, &dynpd));
  if ((feature & ((1U << EN_DPL) | (1U << EN_ACK_PAY))) !=
          ((1U << EN_DPL) | (1U << EN_ACK_PAY)) ||
      (dynpd & (1U << DPL_P0)) == 0U)
  {
      L01_SetCE(CE_LOW);
      return HAL_ERROR;
  }
#endif

  /* 無線連線仍需透過發送封包收到 ACK 確認。 */
  if (config == expectedConfig &&
      channel == L01_INIT_RF_CHANNEL &&
      rfSetup == L01_INIT_RF_SETUP &&
      retrySetup == L01_INIT_RETR)
  {
    debug_printf("nRF24L01+ initialized successfully \r\n");
    return HAL_OK;
  }
  else
  {
    debug_printf(
        "nRF24L01+ check failed CFG=%02X/%02X CH=%02X/%02X RF=%02X/%02X RETR=%02X/%02X\r\n",
        (unsigned)config, (unsigned)expectedConfig,
        (unsigned)channel, (unsigned)L01_INIT_RF_CHANNEL,
        (unsigned)rfSetup, (unsigned)L01_INIT_RF_SETUP,
        (unsigned)retrySetup, (unsigned)L01_INIT_RETR
    );
    L01_SetCE(CE_LOW);
    return HAL_ERROR;
  }
}


