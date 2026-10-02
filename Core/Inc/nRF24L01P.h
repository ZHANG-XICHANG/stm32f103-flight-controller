/**@file  	    nRF24L01P.h
* @brief            nRF24L01+ 底層操作與設定。
* @author           hyh
* @date             2021.6.9
* @version          1.0
* @copyright        Chengdu Ebyte Electronic Technology Co.Ltd
**********************************************************************************
*/
#ifndef nRF24L01P_H
#define nRF24L01P_H

#include <stdint.h>
#include "main.h"
#include "nRF24L01P_REG.h"

#define IRQ_ALL ((1 << RX_DR) | (1 << TX_DS) | (1 << MAX_RT))
/*資料傳輸速率選擇*/
typedef enum {DRATE_250K,DRATE_1M,DRATE_2M}L01_DRATE;
/*發射功率選擇*/
typedef enum {POWER_N_0,POWER_N_6,POWER_N_12,POWER_N_18}L01_PWR;
/*模式選擇*/
typedef enum {TX_MODE,RX_MODE}L01_MODE;
/*CE 腳位電位選擇*/
typedef enum {CE_LOW,CE_HIGH}CE_STAUS;

/*選擇傳送時是否使用 ACK 確認機制 
nrf_init_flag  : 1--->使用 ACK
                 0--->不使用 ACK
*/
#define nrf_init_flag   1



/*
================================================================================
============================設定與選項==========================
================================================================================
*/
#ifndef DYNAMIC_PACKET
#define DYNAMIC_PACKET      1 // ACK Payload 需要動態長度；控制封包仍為 17 bytes。
#endif
#define FIXED_PACKET_LEN    17//固定長度模式下的封包大小
#define INIT_ADDR           0x0A,0x01,0x06,0x0E,0x01

/* 初始化與讀回比對共用設定；頻道使用十進位，範圍 0～125。 */
#define L01_INIT_CONFIG     ((1U << EN_CRC) | (1U << CRCO)) // 2-byte CRC
#define L01_INIT_RF_CHANNEL 40U
#define L01_INIT_RF_SETUP   ((1U << RF_DR_HIGH) | PWR_N_0DB) // 2 Mbps、0 dBm
/* 2 Mbps: ACK payloads above 15 bytes require ARD >= 500 us (31-byte telemetry). */
#define L01_INIT_RETR       (ARD_500US | ARC_5)

/* L01_TransmitPacket() 的單一回傳結果；與 IRQ 位元遮罩分開使用。 */
#define L01_TX_SUCCESS       0x20U
#define L01_TX_MAX_RETRY     0x10U
#define L01_TX_TIMEOUT       0x01U
#define L01_TX_INVALID_PARAM 0x02U
#define L01_TX_SPI_ERROR     0x03U
/* Shared SPI wait budget per CSN transaction; scheduling can extend elapsed time. */
#define L01_SPI_TIMEOUT_MS   2U
#define L01_TX_TIMEOUT_MS    4U
/*
================================================================================
==========================外部提供的函式列表 ================
================================================================================
*/
/* F103：SPI1、PA4 CSN、PA3 CE；IRQ 未接線，透過 SPI 輪詢狀態。 */
#define L01_CSN_LOW()      HAL_GPIO_WritePin(SPI_CSN_GPIO_Port, SPI_CSN_Pin, GPIO_PIN_RESET)
#define L01_CSN_HIGH()     HAL_GPIO_WritePin(SPI_CSN_GPIO_Port, SPI_CSN_Pin, GPIO_PIN_SET)
#define L01_CE_LOW()       HAL_GPIO_WritePin(SPI_CE_GPIO_Port, SPI_CE_Pin, GPIO_PIN_RESET)
#define L01_CE_HIGH()      HAL_GPIO_WritePin(SPI_CE_GPIO_Port, SPI_CE_Pin, GPIO_PIN_SET)
/*
================================================================================
-------------------------------------對外提供的 API------------------------------
================================================================================
*/
/* HAL_StatusTypeDef API: only HAL_OK makes output data valid.
 * SPI failure releases CSN and lowers CE without calling Error_Handler().
 * One task must own the module (or lock the whole operation); no ISR/critical-section calls.
 */
/*將 CE 腳位設為低電位或高電位*/
void L01_SetCE(CE_STAUS status);
/*讀取指定暫存器的值 */
HAL_StatusTypeDef L01_ReadSingleReg(uint8_t addr, uint8_t *value);
/*讀取指定暫存器的資料並存入緩衝區*/
HAL_StatusTypeDef L01_ReadMultiReg(uint8_t start_addr, uint8_t *buffer, uint8_t size);
/*將數值寫入指定暫存器*/
HAL_StatusTypeDef L01_WriteSingleReg(uint8_t addr, uint8_t value);
/*將緩衝區資料寫入指定暫存器 */
HAL_StatusTypeDef L01_WriteMultiReg(uint8_t start_addr, uint8_t *buffer, uint8_t size);
/*將 nRF24L01 設為斷電模式（PowerDown） */
HAL_StatusTypeDef L01_SetPowerDown(void);
/*將 nRF24L01 設為上電模式；由斷電恢復時等待晶振穩定，期間 CE 應保持低。*/
HAL_StatusTypeDef L01_SetPowerUp(void);
/*清空 TX 傳送緩衝區*/
HAL_StatusTypeDef L01_FlushTX(void);
/*清空 RX 接收緩衝區*/
HAL_StatusTypeDef L01_FlushRX(void);
/*重複使用上次傳送的酬載資料*/
HAL_StatusTypeDef L01_ReuseTXPayload(void);
/*讀取 nRF24L01 的狀態暫存器*/
HAL_StatusTypeDef L01_ReadStatusReg(uint8_t *status);
/*清除 nRF24L01+ 產生的中斷（IRQ）*/
HAL_StatusTypeDef L01_ClearIRQ(uint8_t irqMask);
/*讀取 nRF24L01+ 的中斷（IRQ）狀態*/
HAL_StatusTypeDef L01_ReadIRQSource(uint8_t *irqMask);
/**
 * @brief 讀取 TX_DS 與 MAX_RT 旗標，不等待發送完成，也不清除旗標。
 * @param txStatus 成功時輸出位元遮罩：0x00 無結果、0x20 發送成功、0x10 達最大重傳次數；
 *         若兩個旗標同時存在則為 0x30，呼叫端應使用位元運算判斷。
 */
HAL_StatusTypeDef L01_ReadTXStatus(uint8_t *txStatus);
/**
 * @brief 發送一個封包，等待成功、重傳失敗或逾時，最後恢復 RX 模式。
 * @param buffer 封包資料，不可為 NULL。
 * @param size 1～32 bytes；固定長度模式下必須等於 FIXED_PACKET_LEN。
 * @param timeout_ms 從 CE 觸發起計算的等待上限，必須大於 0。
 * @return L01_TX_SUCCESS、L01_TX_MAX_RETRY、L01_TX_TIMEOUT 或
 *         L01_TX_INVALID_PARAM、L01_TX_SPI_ERROR。參數錯誤時不操作硬體。
 * @note 須先初始化 GPIO、SPI 並呼叫 L01_Init()。僅由單一 Task 呼叫，
 *       或由呼叫端鎖定整個無線模組；不可在 ISR 或臨界區呼叫。
 *       RTOS 運行時等待會讓出 CPU。上電、清理及恢復 RX 時間不含於
 *       timeout_ms；排程及 SPI 傳輸亦可能使實際返回時間超過此值。
 *       SPI 失敗時 CE 保持低，不保證恢復 RX；須重新初始化並檢查後再使用。
 *       逾時或 SPI 失敗不代表對端一定未收到，避免盲目重送單次命令。
 */
uint8_t L01_TransmitPacket(uint8_t *buffer, uint8_t size, uint32_t timeout_ms);
/*讀取 FIFO 頂端緩衝區的酬載長度 */
HAL_StatusTypeDef L01_ReadTopFIFOWidth(uint8_t *width);
/* buffer 須容納 32 bytes；HAL_OK 才更新 buffer，有效參數下失敗時 length 為 0。 */
HAL_StatusTypeDef L01_ReadRXPayload(uint8_t *buffer, uint8_t *length);
/*將 TX 傳送酬載寫入資料管道，接收端（PRX）會回傳確認訊號（ACK）*/
HAL_StatusTypeDef L01_WriteTXPayload_Ack(uint8_t *buffer, uint8_t size);
/*將 TX 傳送酬載寫入資料管道，接收端（PRX）不會回傳確認訊號（ACK）*/
HAL_StatusTypeDef L01_WriteTXPayload_NoAck(uint8_t *buffer, uint8_t size);
/*在 RX 接收模式下，將傳送酬載寫入資料管道*/
HAL_StatusTypeDef L01_WriteRXPayload_InAck(uint8_t *buffer, uint8_t size);
/*將傳送位址寫入 TX_ADDR 暫存器 */
HAL_StatusTypeDef L01_SetTXAddr(uint8_t *addrBuffer,uint8_t addr_size);
/*設定 RX 接收管道的位址*/
HAL_StatusTypeDef L01_SetRXAddr(uint8_t pipeNum,uint8_t *addrBuffer,uint8_t addr_size);
/*設定 nRF24L01+ 的資料傳輸速率 */
HAL_StatusTypeDef L01_SetDataRate(L01_DRATE drate);
/*設定 nRF24L01+ 的發射功率 */
HAL_StatusTypeDef L01_SetPower(L01_PWR power);
/*設定 nRF24L01+ 的頻率*/
HAL_StatusTypeDef L01_WriteHoppingPoint(uint8_t freq);
/*將 nRF24L01+ 設為 TX 傳送模式或 RX 接收模式*/
HAL_StatusTypeDef L01_SetTRMode(L01_MODE mode);
/*初始化 nRF24L01+ */
HAL_StatusTypeDef L01_Init(void);
/*nRF24L01的硬件接口層初始化檢查*/
HAL_StatusTypeDef L01_Check(void);
uint8_t L01_GetCEStatus(void);
HAL_StatusTypeDef L01_Nop(void);
#endif
