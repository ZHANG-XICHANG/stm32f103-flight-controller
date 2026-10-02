/**@file  	    nRF24L01P_REG.h
* @brief            nRF24L01+ 暫存器說明與設定。
* @author           hyh
* @date             2021.9.17
* @version          1.0
* @copyright        Chengdu Ebyte Electronic Technology Co.Ltd
**********************************************************************************
*/
#ifndef nRF24L01P_REG_H
#define nRF24L01P_REG_H

/*nRF24L01+ 的 SPI 指令*/
#define R_REGISTER                  0x00
#define W_REGISTER                  0x20
#define R_RX_PAYLOAD                0x61
#define W_TX_PAYLOAD                0xA0
#define FLUSH_TX                    0xE1
#define FLUSH_RX                    0xE2
#define REUSE_TX_PL                 0xE3
#define R_RX_PL_WID                 0x60
#define W_ACK_PAYLOAD               0xA8
#define W_TX_PAYLOAD_NOACK          0xB0
#define NOP                         0xFF
/*
================================================================================
-------------------------------暫存器定義------------------------------
================================================================================
*/
/*內部暫存器位址對照與位元定義*/
#define L01REG_CONFIG               0x00 //設定暫存器
    //bit[7]:保留               僅允許設為 '0'
    #define MASK_RX_DR              6 // RW, 遮蔽 RX_DR 引發的中斷
    #define MASK_TX_DS              5 // RW, 遮蔽 TX_DS 引發的中斷
    #define MASK_MAX_PT             4 // RW, 遮蔽 MAX_RT 引發的中斷
    #define EN_CRC                  3 // RW, 啟用 CRC。若 EN_AA 中任一位元為 1，此位元會強制設為 1
    #define CRCO                    2 // RW, CRC 編碼方式：0：1 位元組；1：2 位元組
    #define PWR_UP                  1 // RW, 1：上電；0：斷電
    #define PRIM_RX                 0 // RW, 接收／傳送控制：1：PRX 接收端；0：PTX 傳送端
#define L01REG_ENAA                 0x01 //啟用自動確認（ACK）功能
    //bit[7:6]:保留             僅允許設為 '00'
    #define ENAA_P5                 5 // RW, 啟用資料管道 5 的自動確認（ACK）
    #define ENAA_P4                 4 // RW, 啟用資料管道 4 的自動確認（ACK）
    #define ENAA_P3                 3 // RW, 啟用資料管道 3 的自動確認（ACK）
    #define ENAA_P2                 2 // RW, 啟用資料管道 2 的自動確認（ACK）
    #define ENAA_P1                 1 // RW, 啟用資料管道 1 的自動確認（ACK）
    #define ENAA_P0                 0 // RW, 啟用資料管道 0 的自動確認（ACK）
#define L01REG_EN_RXADDR            0x02 //啟用的接收位址
    //bit[7:6]:保留             僅允許設為 '00'
    #define ERX_P5                  5 // RW, 啟用資料管道 5
    #define ERX_P4                  4 // RW, 啟用資料管道 4
    #define ERX_P3                  3 // RW, 啟用資料管道 3
    #define ERX_P2                  2 // RW, 啟用資料管道 2
    #define ERX_P1                  1 // RW, 啟用資料管道 1
    #define ERX_P0                  0 // RW, 啟用資料管道 0
#define L01REG_SETUP_AW             0X03 //設定所有資料管道共用的位址長度
    //bit[7:2]:保留             僅允許設為 '000000'
    //bit[1:0]:AW                   接收／傳送位址欄位長度 
    #define AW_ILLEGAL              0x00 // 無效設定
    #define AW_3BYTES               0x01 // 3 位元組
    #define AW_4BYTES               0x02 // 4 位元組
    #define AW_5BYTES               0x03 // 5 位元組
#define L01REG_SETUP_RETR           0x04 //自動重傳設定
    //bit[7:4]:ARD                  自動重傳延遲
    #define ARD_250US               (0x00<<4)
    #define ARD_500US               (0x01<<4)
    #define ARD_750US               (0x02<<4)
    #define ARD_1000US              (0x03<<4)
    #define ARD_1250US              (0x04<<4)
    #define ARD_1500US              (0x05<<4)
    #define ARD_1750US              (0x06<<4)
    #define ARD_2000US              (0x07<<4)
    //......
    #define ARD_4000US              (0x0F<<4)
    //bit[3:0]:ARC                  自動重傳次數
    #define ARC_DISABLED            0x00
    #define ARC_1                   0x01
    #define ARC_5                   0x05

    //......
    #define ARC_15                  0x0F
#define L01REG_RF_CH                0x05 //射頻頻道
    //bit[7]:保留               僅允許設為 '0'
    //bit[6:0]:RF_CH:               設定 nRF24L01+ 的工作頻道。範圍：[0–126]，2400 MHz–2525 MHz
#define L01REG_RF_SETUP             0x06 //射頻設定暫存器
    //nRF24L01+:bit[6]:保留
    #define CONT_WAVE               7 // RW,設為 1 時啟用連續載波發射。
    #define RF_DR_LOW               5 // RW,將資料傳輸速率設為 250 Kbps，
    #define PLL_LOCK                4 // RW,強制 PLL 鎖定訊號，僅供測試使用
    #define RF_DR_HIGH              3 // RW,空中資料傳輸速率：[RF_DR_LOW,RF_DR_HIGH]->00：1 Mbps；01：2 Mbps；10：250 Kbps；11：保留
    //bit[2:1]:RF_PWR               設定 TX 傳送模式下的射頻輸出功率
    #define PWR_N_18DB                (0x00<<1)
    #define PWR_N_12DB                (0x01<<1)
    #define PWR_N_6DB                 (0x02<<1)
    #define PWR_N_0DB                 (0x03<<1)
    #define OBSOLETE                0 // 不影響設定
#define L01REG_STATUS               0x07 //狀態暫存器
    //bit[7]:保留               僅允許設為 '0'
    #define RX_DR                   6 // RW,RX FIFO 資料就緒中斷，寫入 1 可清除此位元
    #define TX_DS                   5 // RW,TX FIFO 資料已傳送中斷，寫入 1 可清除此位元
    #define MAX_RT                  4 // RW,TX 重傳達到次數上限中斷，寫入 1 可清除此位元
    //bit[3:1]:RX_P_NO:             R,RX_FIFO 中可讀取酬載所屬的資料管道編號
    //000–101：資料管道編號      110：未使用；111：RX FIFO 為空
    #define TX_FULL_0               0 // R,TX FIFO 已滿旗標。1：已滿；0：TX FIFO 尚有可用空間
#define L01REG_OBSERVE_TX           0x08 // 傳送監測暫存器
    //bit[7:4]:PLOS_CNT:            R,遺失封包計數。
    //bit[3:0]:ARC_CNT:             R,重傳封包計數。
#define L01REG_RPD                  0x09 // 接收功率偵測器
    //bit[7:1]:保留             僅允許設為 '0000000'
    //bit[0]:RPD                    R,接收功率偵測器    
#define L01REG_RX_ADDR_P0           0x0A    
    //bit[39:0]                     資料管道 0 的接收位址，最長 5 位元組，最低有效位元組先寫入
#define L01REG_RX_ADDR_P1           0x0B   
    //bit[39:0]                     資料管道 1 的接收位址，最長 5 位元組，最低有效位元組先寫入
#define L01REG_RX_ADDR_P2           0x0C   
    //bit[7:0]                      資料管道 2 的接收位址，僅設定最低有效位元組，其餘高位元組與 RX_ADDR_P1[39:8] 相同
#define L01REG_RX_ADDR_P3           0x0D    
    //bit[7:0]                      資料管道 3 的接收位址，僅設定最低有效位元組，其餘高位元組與 RX_ADDR_P1[39:8] 相同
#define L01REG_RX_ADDR_P4           0x0E    
    //bit[7:0]                      資料管道 4 的接收位址，僅設定最低有效位元組，其餘高位元組與 RX_ADDR_P1[39:8] 相同
#define L01REG_RX_ADDR_P5           0x0F   
    //bit[7:0]                      資料管道 5 的接收位址，僅設定最低有效位元組，其餘高位元組與 RX_ADDR_P1[39:8] 相同 
#define L01REG_TX_ADDR              0x10
    //bit[39:0]傳送位址，僅供 PTX 傳送端裝置使用，最低有效位元組先寫入。
    //將 RX_ADDR_P0 設為與此位址相同，以處理自動確認（ACK）， 
    //適用於已啟用 Enhanced ShockBurst™ 的 PTX 傳送端裝置。
#define L01REG_RX_PW_P0             0x11
    //bit[7:6]:保留             僅允許設為 '00'
    //bit[5:0]:RX_PW_P0             資料管道 0 的接收酬載位元組數（0 至 32 位元組）。
#define L01REG_RX_PW_P1             0x12
    //bit[7:6]:保留             僅允許設為 '00'
    //bit[5:0]:RX_PW_P1             資料管道 1 的接收酬載位元組數（0 至 32 位元組）。
#define L01REG_RX_PW_P2             0x13
    //bit[7:6]:保留             僅允許設為 '00'
    //bit[5:0]:RX_PW_P2             資料管道 2 的接收酬載位元組數（0 至 32 位元組）。
#define L01REG_RX_PW_P3             0x14
    //bit[7:6]:保留             僅允許設為 '00'
    //bit[5:0]:RX_PW_P3             資料管道 3 的接收酬載位元組數（0 至 32 位元組）。
#define L01REG_RX_PW_P4             0x15
    //bit[7:6]:保留             僅允許設為 '00'
    //bit[5:0]:RX_PW_P4             資料管道 4 的接收酬載位元組數（0 至 32 位元組）。
#define L01REG_RX_PW_P5             0x16
    //bit[7:6]:保留             僅允許設為 '00'
    //bit[5:0]:RX_PW_P5             資料管道 5 的接收酬載位元組數（0 至 32 位元組）。
#define L01REG_FIFO_STATUS          0x17 // FIFO 狀態暫存器
    //bit[7]:保留               僅允許設為 '0'
    #define TX_REUSE                6 // R,設為 1 時重複使用上次傳送的資料封包。
    #define TX_FULL_1               5 // R,TX FIFO 已滿旗標。1：TX FIFO 已滿；0：TX FIFO 尚有可用空間。
    #define TX_EMPTY                4 // R,TX FIFO 為空旗標。1：TX FIFO 為空；0：TX FIFO 中有資料。
    //bit[3:2]:保留             僅允許設為 '00'
    #define RX_FULL                 1 // R,RX FIFO 已滿旗標。1：RX FIFO 已滿；0：RX FIFO 尚有可用空間。
    #define RX_EMPTY                0 // R,RX FIFO 為空旗標。1：RX FIFO 為空；0：RX FIFO 中有資料。
#define L01REG_DYNPD                0x1C // 啟用動態酬載長度
    //bit[7:6]:保留             僅允許設為 '00'
    #define DPL_P5                  5 // RW,啟用資料管道 5 的動態酬載長度（需啟用 EN_DPL 與 ENAA_P5）
    #define DPL_P4                  4 // RW,啟用資料管道 4 的動態酬載長度（需啟用 EN_DPL 與 ENAA_P4）
    #define DPL_P3                  3 // RW,啟用資料管道 3 的動態酬載長度（需啟用 EN_DPL 與 ENAA_P3）
    #define DPL_P2                  2 // RW,啟用資料管道 2 的動態酬載長度（需啟用 EN_DPL 與 ENAA_P2）
    #define DPL_P1                  1 // RW,啟用資料管道 1 的動態酬載長度（需啟用 EN_DPL 與 ENAA_P1）
    #define DPL_P0                  0 // RW,啟用資料管道 0 的動態酬載長度（需啟用 EN_DPL 與 ENAA_P0）
#define L01REG_FEATURE              0x1D // 功能暫存器
    //bit[7:3]:保留             僅允許設為 '00000'
    #define EN_DPL                  2 // RW,啟用動態酬載長度
    #define EN_ACK_PAY              1 // RW,啟用附帶酬載的 ACK
    #define EN_DYN_ACK              0 // RW,啟用 W_TX_PAYLOAD_NOACK 指令

#endif
