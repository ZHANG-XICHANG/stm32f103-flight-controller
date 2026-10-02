#ifndef __MPU6050_H
#define __MPU6050_H

#include "i2c.h"

/* HAL I2C API 使用左移一位後的 7-bit 裝置地址，讀寫共用同一地址。 */
#define MPU6050_ADDR                  0x68U
#define MPU6050_I2C_ADDR              (MPU6050_ADDR << 1)
#define MPU6050_I2C_TIMEOUT_MS        5U

/* 從 ACCEL_XOUT_H 起依序包含 Accel、Temperature 與 Gyro，共 14 bytes。 */
#define MPU6050_BURST_LENGTH          14U

/* 目前量程的靈敏度，用於將校正後的原始 LSB 轉成 g 與 degree/s。 */
#define MPU6050_ACCEL_SCALE_2G        (32768.0f/2.0f)      /* ±2g：16384 LSB/g。 */
#define MPU6050_GYRO_SCALE_2000DPS    (32768.0f/2000.0f)     /* ±2000 degree/s：16.4 LSB/(degree/s)。 */

/* 零偏校準參數；校準時板子必須水平靜止且 Z 軸朝上。 */
#define MPU6050_CALIBRATION_SAMPLES          1000U  /* 實際用來計算平均零偏的樣本數。 */
#define MPU6050_STABLE_REQUIRED_SAMPLES      100U   /* 開始校準前必須連續穩定的次數。 */
#define MPU6050_STABILITY_MAX_CHECKS         2000U  /* 靜止等待上限，避免永久阻塞。 */
#define MPU6050_CALIBRATION_PERIOD_MS        6U     /* 校準取樣間隔。 */
#define MPU6050_ACCEL_1G_RAW                 16384L /* ±2g 量程下的 +1g 原始值。 */
#define MPU6050_ACCEL_STABLE_DELTA_RAW       164L   /* 相鄰樣本差小於約 0.01g。 */
#define MPU6050_GYRO_STABLE_LIMIT_RAW        82L    /* 各軸角速度需小於約 5 degree/s。 */
#define MPU6050_ACCEL_NORM_MIN_RAW           14746L /* 合加速度下限約 0.9g。 */
#define MPU6050_ACCEL_NORM_MAX_RAW           18022L /* 合加速度上限約 1.1g。 */
#define MPU6050_ACCEL_LEVEL_XY_MAX_RAW       2458L  /* 水平時 X/Y 各軸最多約 0.15g。 */


// 定義 MPU6050 的寄存器地址
//技術文檔未公佈的寄存器 主要用於官方DMP操作
#define MPU6050_RA_XG_OFFS_TC       0x00 //[bit7] PWR_MODE, [6:1] XG_OFFS_TC, [bit 0] OTP_BNK_VLD
#define MPU6050_RA_YG_OFFS_TC       0x01 //[7] PWR_MODE, [6:1] YG_OFFS_TC, [0] OTP_BNK_VLD
//bit7的定義,當設置為1,輔助I2C總線高電平是VDD。當設置為0,輔助I2C總線高電平是VLOGIC

#define MPU6050_RA_ZG_OFFS_TC       0x02 //[7] PWR_MODE, [6:1] ZG_OFFS_TC, [0] OTP_BNK_VLD
#define MPU6050_RA_X_FINE_GAIN      0x03 //[7:0] X_FINE_GAIN
#define MPU6050_RA_Y_FINE_GAIN      0x04 //[7:0] Y_FINE_GAIN
#define MPU6050_RA_Z_FINE_GAIN      0x05 //[7:0] Z_FINE_GAIN

#define MPU6050_RA_XA_OFFS_H        0x06 //[15:0] XA_OFFS 兩個寄存器合在一起
#define MPU6050_RA_XA_OFFS_L_TC     0x07

#define MPU6050_RA_YA_OFFS_H        0x08 //[15:0] YA_OFFS 兩個寄存器合在一起
#define MPU6050_RA_YA_OFFS_L_TC     0x09

#define MPU6050_RA_ZA_OFFS_H        0x0A //[15:0] ZA_OFFS 兩個寄存器合在一起
#define MPU6050_RA_ZA_OFFS_L_TC     0x0B

#define MPU6050_RA_XG_OFFS_USRH     0x13 //[15:0] XG_OFFS_USR 兩個寄存器合在一起
#define MPU6050_RA_XG_OFFS_USRL     0x14

#define MPU6050_RA_YG_OFFS_USRH     0x15 //[15:0] YG_OFFS_USR 兩個寄存器合在一起
#define MPU6050_RA_YG_OFFS_USRL     0x16

#define MPU6050_RA_ZG_OFFS_USRH     0x17 //[15:0] ZG_OFFS_USR 兩個寄存器合在一起
#define MPU6050_RA_ZG_OFFS_USRL     0x18

/*陀螺儀的採樣頻率*/
/*傳感器的寄存器輸出,FIFO輸出,DMP採樣、運動檢測、
 *零運動檢測和自由落體檢測都是基於採樣率。
 *通過SMPLRT_DIV把陀螺儀輸出率分頻即可得到採樣率
 *採樣率=陀螺儀輸出率/ (1 + SMPLRT_DIV)
 *禁用DLPF的情況下(DLPF_CFG = 0或7) ，陀螺儀輸出率= 8 khz
 *在啟用DLPF(見寄存器26)時，陀螺儀輸出率= 1 khz
 *加速度傳感器輸出率是1 khz。這意味著,採樣率大於1 khz時,
 *同一個加速度傳感器的樣品可能會多次輸入到FIFO、DMP和傳感器寄存器*/
#define MPU6050_RA_SMPLRT_DIV       0x19 //[0-7] 陀螺儀輸出分頻採樣率

/*配置外部引腳採樣和DLPF數字低通濾波器*/
#define MPU6050_RA_CONFIG           0x1A
//bit5-bit3  一個連接到FSYNC端口的外部信號可以通過配置EXT_SYNC_SET來採樣
//			 也就是說,這裡設置之後,FSYNC的電平0或1進入最終數據寄存器,具體如下
//			0 不使用 1 FSYNC電平進入所有數據寄存器 2 FSYNC電平進入GYRO_XOUT_L 3 FSYNC電平進入GYRO_YOUT_L
//			4 FSYNC電平進入GYRO_ZOUT_L 5 FSYNC電平進入ACCEL_XOUT_L 6 FSYNC電平進入ACCEL_YOUT_L
//			7 FSYNC電平進入SYNC_ACCEL_ZOUT_L
//bit2-bit0 數字低通濾波器 用於濾除高頻幹擾 高於這個頻率的幹擾被濾除掉
/*對應關係如下
 * *				  |   加速度傳感器  |          陀螺儀
 * * DLPF_CFG |    帶寬   |  延遲  |    帶寬   |  延遲  | 採樣率
 * -------------+--------+-------+--------+------+-------------
 * 0			| 260Hz     | 0ms    | 256Hz   | 0.98ms | 8kHz
 * 1			| 184Hz     | 2.0ms  | 188Hz   | 1.9ms  | 1kHz
 * 2			| 94Hz      | 3.0ms  | 98Hz    | 2.8ms  | 1kHz
 * 3			| 44Hz      | 4.9ms  | 42Hz    | 4.8ms  | 1kHz
 * 4			| 21Hz      | 8.5ms  | 20Hz    | 8.3ms  | 1kHz
 * 5			| 10Hz      | 13.8ms | 10Hz    | 13.4ms | 1kHz
 * 6			| 5Hz       | 19.0ms | 5Hz     | 18.6ms | 1kHz
 * 7			| Reserved  | Reserved | Reserved
 * */


/*陀螺儀的配置,主要是配置陀螺儀的量程與自檢(通過相應的位7 6 5 開啟自檢)*/
#define MPU6050_RA_GYRO_CONFIG      0x1B
//bit4-bit3 量程設置如下
//			 0 = +/- 250 度/秒
//			 1 = +/- 500 度/秒
//			 2 = +/- 1000 度/秒
//			 3 = +/- 2000 度/秒*/

/*加速度計的配置,主要是配置加速度計的量程與自檢(通過相應的位7 6 5 開啟自檢)
 *另外,還能配置系統的高通濾波器*/
#define MPU6050_RA_ACCEL_CONFIG     0x1C
//bit7 啟動X自檢 加速度計的自檢
//bit6 啟動Y自檢
//bit5 啟動Z自檢
//bit4-bit3 加速度傳感器的量程配置
//			 0 = +/- 2g
//			 1 = +/- 4g
//			 2 = +/- 8g
//			 3 = +/- 16g*/
//bit0到bit2 加速度傳感器的高通濾波器
/*DHPF是在路徑中連接於運動探測器(自由落體,運動閾值,零運動)的一個濾波器模塊。
 *高通濾波器的輸出值不在數據寄存器中
 *高通濾波器有三種模式：
 *重置:在一個樣本中將濾波器輸出值設為零。這有效的禁用了高通濾波器。這種模式可以快速切換濾波器的設置模式。
 *開啟:高通濾波器能通過高於截止頻率的信號
 *持續:觸發後,過濾器持續當前採樣。過濾器輸出值是輸入樣本和持續樣本之間的差異
 *設置值如下所示
 * ACCEL_HPF | 高通濾波模式| 截止頻率
 * ----------+-------------+------------------
 * 0         | Reset       | None
 * 1         | On          | 5Hz
 * 2         | On          | 2.5Hz
 * 3         | On          | 1.25Hz
 * 4         | On          | 0.63Hz
 * 7         | Hold        | None
 */

#define MPU6050_RA_FF_THR           0x1D
/*自由落體加速度的閾值
 *這個寄存器為自由落體的閾值檢測進行配置。
 *FF_THR的單位是1LSB = 2mg。當加速度傳感器測量而得的三個軸的絕對值
 *都小於檢測閾值時，就可以測得自由落體值。這種情況下，(加速度計每次檢測到就+1以下,所以還要依靠加速度採樣率)
 *自由落體時間計數器計數一次 (寄存器30)。當自由落體時間計數器達到
 *FF_DUR中規定的時間時，自由落體被中斷(或發生自由落體中斷)
 **/

#define MPU6050_RA_FF_DUR           0x1E
/*
 *自由落體加速度的時間閾值
* 這個寄存器為自由落體時間閾值計數器進行配置。
* 時間計數頻率為1 khz,因此FF_DUR的單位是 1 LSB = 1毫秒。
* 當加速度器測量而得的絕對值都小於檢測閾值時，
* 自由落體時間計數器計數一次。當自由落體時間計數器
* 達到該寄存器的規定時間時，自由落體被中斷。
* (或發生自由落體中斷)
* */

#define MPU6050_RA_MOT_THR          0x1F
/*
 *運動檢測的加速度閾值
 *這個寄存器為運動中斷的閾值檢測進行配置。
 *MOT_THR的單位是 1LSB = 2mg。
 *當加速度器測量而得的絕對值都超過該運動檢測的閾值時，
 *即可測得該運動。這一情況下，運動時間檢測計數器計數一次。
 *當運動檢測計數器達到MOT_DUR的規定時間時，運動檢測被中斷。
 * 運動中斷表明了被檢測的運動MOT_DETECT_STATUS (Register 97)的軸和極性。
 */

#define MPU6050_RA_MOT_DUR          0x20
/*
*運動檢測時間的閾值。
*這個寄存器為運動中斷的閾值檢測進行配置。
*時間計數器計數頻率為1 kHz ，因此MOT_THR的單位是 1LSB = 1ms。
*當加速度器測量而得的絕對值都超過該運動檢測的閾值時(Register 31)，
*運動檢測時間計數器計數一次。當運動檢測計數器達到該寄存器規定的時間時，
*運動檢測被中斷。
 **/

#define MPU6050_RA_ZRMOT_THR        0x21
/*
*零運動檢測加速度閾值。
* 這個寄存器為零運動中斷檢測進行配置。
* ZRMOT_THR的單位是1LSB = 2mg。
* 當加速度器測量而得的三個軸的絕對值都小於檢測閾值時，
* 就可以測得零運動。這種情況下，零運動時間計數器計數一次 (寄存器34)。
* 當自零運動時間計數器達到ZRMOT_DUR (Register 34)中規定的時間時，零運動被中斷。
* 與自由落體或運動檢測不同的是，當零運動首次檢測到以及當零運動檢測不到時，零運動檢測都被中斷。
* 當零運動被檢測到時,其狀態將在MOT_DETECT_STATUS寄存器(寄存器97) 中顯示出來。
* 當運動狀態變為零運動狀態被檢測到時,狀態位設置為1。當零運動狀態變為運動狀態被檢測到時,
* 狀態位設置為0。
 **/

#define MPU6050_RA_ZRMOT_DUR        0x22
/*
*零運動檢測的時間閾值
* 這個寄存器為零運動中斷檢測進行時間計數器的配置。
* 時間計數器的計數頻率為16 Hz,因此ZRMOT_DUR的單位是1 LSB = 64 ms。
* 當加速度器測量而得的絕對值都小於檢測器的閾值(Register 33)時，
* 運動檢測時間計數器計數一次。當零運動檢測計數器達到該寄存器規定的時間時，
* 零運動檢測被中斷。
 **/


/*
 *設備的各種FIFO使能,包括溫度 加速度 陀螺儀 從機
 *將相關的數據寫入FIFO緩衝中
 **/
#define MPU6050_RA_FIFO_EN          0x23
//bit7 溫度fifo使能
//bit6 陀螺儀Xfifo使能
//bit5 陀螺儀Yfifo使能
//bit4 陀螺儀Zfifo使能
//bit3 加速度傳感器fifo使能
//bit2 外部從設備2fifo使能
//bit1 外部從設備1fifo使能
//bit0 外部從設備0fifo使能

#define MPU6050_RA_I2C_MST_CTRL     0x24
//配置單主機或者多主機下的IIC總線
//bit7 監視從設備總線,看總線是否可用 MULT_MST_EN設置為1時,MPU-60X0的總線仲裁檢測邏輯被打開
//bit6 延遲數據就緒中斷,直達從設備數據也進入主機再觸發 相當於數據同步等待
//bit5 當設置為1時,與Slave3 相連的外部傳感器數據(寄存器73 到寄存器 96)寫入FIFO緩衝中,每次都寫入
//bit4 主機讀取一個從機到下一個從機讀取之間的動作 為0 讀取之間有一個restart,為1 下一次讀取前會有一個重啟,然後
//		一直讀取直到切換寫入或者切換設備
//bit3-bit0 配置MPU作為IIC主機時的時鐘,基於MPU內部8M的分頻
/* I2C_MST_CLK | I2C 主時鐘速度 | 8MHz 時鐘分頻器
* ------------+------------------------+-------------------
* 0			    | 348kHz          | 23
* 1			    | 333kHz          | 24
* 2			    | 320kHz          | 25
* 3				| 308kHz          | 26
* 4				| 296kHz          | 27
* 5				| 286kHz          | 28
* 6				| 276kHz          | 29
* 7				| 267kHz          | 30
* 8				| 258kHz          | 31
* 9				| 500kHz          | 16
* 10			| 471kHz          | 17
* 11			| 444kHz          | 18
* 12			| 421kHz          | 19
* 13			| 400kHz          | 20
* 14			| 381kHz          | 21
* 15			| 364kHz          | 22
* */



/**************************MPU鏈接IIC從設備控制寄存器,沒使用從機連接的基本不用考慮這些************************************/
/*指定slave (0-3)的I2C地址
* 注意Bit 7 (MSB)控制了讀/寫模式。如果設置了Bit 7,那麼這是一個讀取操作,
* 如果將其清除,那麼這是一個編寫操作。其餘位(6-0)是slave設備的7-bit設備地址。
* 在讀取模式中,讀取結果是存儲於最低可用的EXT_SENS_DATA寄存器中。
* MPU-6050支持全5個slave，但Slave 4有其特殊功能(getSlave4* 和setSlave4*)。
* 如寄存器25中所述，I2C數據轉換通過採樣率體現。用戶負責確保I2C數據轉換能夠
* 在一個採樣率週期內完成。
* I2C slave數據傳輸速率可根據採樣率來減小。
* 減小的傳輸速率是由I2C_MST_DLY(寄存器52)所決定的。
* slave數據傳輸速率是否根據採樣率來減小是由I2C_MST_DELAY_CTRL (寄存器103)所決定的。
* slave的處理指令是固定的。Slave的處理順序是Slave 1, Slave 2, Slave 3 和 Slave 4。
* 如果某一個Slave被禁用了，那麼它會被自動忽略。
* 每個slave可按採樣率或降低的採樣率來讀取。在有些slave以採樣率讀取有些以減小
* 的採樣率讀取的情況下，slave的讀取順序依舊不變。然而，
* 如果一些slave的讀取速率不能在特定循環中進行讀取，那麼它們會被自動忽略
* 更多降低的讀取速率相關信息,請參閱寄存器52。
* Slave是否按採樣率或降低的採樣率來讀取由寄存器103得Delay Enable位來決定
 **/

//從機0設置相關
#define MPU6050_RA_I2C_SLV0_ADDR    0x25
//bit7 當前IIC 從設備0的操作,1為讀取 0寫入
//bit6-bit0 從機設備的地址
/* 要讀取或者要寫入的設備內部的寄存器地址,不管讀取還是寫入*/
#define MPU6050_RA_I2C_SLV0_REG     0x26
/*iic從機系統配置寄存器*/
#define MPU6050_RA_I2C_SLV0_CTRL    0x27
//bit7 啟動或者禁止這個設備的IIC數據傳送過程
//bit6 當設置為1時,字節交換啟用。當啟用字節交換時,詞對的高低字節即可交換
//bit5 當 I2C_SLV0_REG_DIS 置 1，只能進行讀取或者寫入數據。當該位清 0，可以再讀取
//		或寫入數據之前寫入一個寄存器地址。當指定從機設備內部的寄存器地址進行發送或接收
//		數據時，該位必須等於 0
//bit4	指定從寄存器收到的字符對的分組順序。當該位清 0，寄存器地址
// 		0和 1, 2 和 3 的字節是分別成對（甚至，奇數寄存器地址 ） ，作為一個字符對。當該位置 1，
//		寄存器地址 1 和 2， 3 和 4 的字節是分別成對的，作為一個字符對
//bit3-bit0  指定從機 0 發送字符的長度。由Slave 0轉換而來和轉換至Slave 0的字節數,(IIC一次傳輸的長度)
// 			該位清 0，I2C_SLV0_EN 位自動置 0.

/*IIC SLAVE1配置寄存器,與0相同*/

#define MPU6050_RA_I2C_SLV1_ADDR    0x28
#define MPU6050_RA_I2C_SLV1_REG     0x29
#define MPU6050_RA_I2C_SLV1_CTRL    0x2A

/*IIC SLAVE2配置寄存器,與0相同*/
#define MPU6050_RA_I2C_SLV2_ADDR    0x2B
#define MPU6050_RA_I2C_SLV2_REG     0x2C
#define MPU6050_RA_I2C_SLV2_CTRL    0x2D

/*IIC SLAVE3配置寄存器,與0相同*/
#define MPU6050_RA_I2C_SLV3_ADDR    0x2E
#define MPU6050_RA_I2C_SLV3_REG     0x2F
#define MPU6050_RA_I2C_SLV3_CTRL    0x30

/*slave4的I2C地址 IIC4與前幾個的寄存器定義有所不同*/
#define MPU6050_RA_I2C_SLV4_ADDR    0x31		//與IIC SLAVE1類似 
#define MPU6050_RA_I2C_SLV4_REG     0x32	/*slave4的當前內部寄存器*/
#define MPU6050_RA_I2C_SLV4_DO      0x33
	/*寫於slave4的新字節這一寄存器可儲存寫於slave4的數據。
	* 如果I2C_SLV4_RW設置為1（設置為讀取模式），那麼該寄存器無法執行操作*/
#define MPU6050_RA_I2C_SLV4_CTRL    0x34
//當設置為1時，此位啟用了slave4的轉換操作。當設置為0時，則禁用該操作
#define MPU6050_I2C_SLV4_EN_BIT         7
//當設置為1時，此位啟用了slave4事務完成的中斷信號的生成。
// 當清除為0時，則禁用了該信號的生成。這一中斷狀態可在寄存器54中看到。
#define MPU6050_I2C_SLV4_INT_EN_BIT     6
//當設置為1時,只進行數據的讀或寫操作。當設置為0時,
// 在讀寫數據之前將編寫一個寄存器地址。當指定寄存器地址在slave設備中時
// ，這應該等於0，而在該寄存器中會進行數據處理。
#define MPU6050_I2C_SLV4_REG_DIS_BIT    5
//採樣率延遲,這為根據採樣率減小的I2C slaves傳輸速率進行了配置。
// 當一個slave的傳輸速率是根據採樣率而降低的,那麼該slave是以每1 / (1 + I2C_MST_DLY) 個樣本進行傳輸。
// 這一基本的採樣率也是由SMPLRT_DIV (寄存器 25)和DLPF_CFG (寄存器26)所決定的的。
// slave傳輸速率是否根據採樣率來減小是由I2C_MST_DELAY_CTRL (寄存器103)所決定的
#define MPU6050_I2C_SLV4_MST_DLY_BIT    4	//[4:0]
#define MPU6050_I2C_SLV4_MST_DLY_LENGTH 5
/*slave4中可讀取的最後可用字節*/
#define MPU6050_RA_I2C_SLV4_DI      0x35

/*
 * IIC輔助從機系統中斷狀態
 **/
#define MPU6050_RA_I2C_MST_STATUS   0x36
//bit7 此位反映了一個與MPU-60X0相連的外部設備的FSYNC中斷狀態。
//		當設置為1且在INT_PIN_CFG(寄存器55)中斷言FSYNC_INT_EN時，中斷產生。
//bit6 當slave4事務完成時，設備會自動設置為1 如果定義了INT_ENABLE中的I2C_MST_INT_EN則產生中斷
//bit5 I2C主機失去輔助I2C總線（一個錯誤狀態）的仲裁，此位自動設置為1.如果斷言了INT_ENABLE寄存器
//		（寄存器56）中的I2C_MST_INT_EN位，則中斷產生
//bit4	slave4的NACK狀態
//bit3  slave3的NACK狀態
//bit2  slave2的NACK狀態
//bit1  slave1的NACK狀態
//bit0  slave0的NACK狀態


/*中斷引腳配置寄存器*/
#define MPU6050_RA_INT_PIN_CFG      0x37
//bit7  中斷的邏輯電平模式,高電平時，設置為0；低電平時，設置為1
//bit6  中斷驅動模式,推拉模式設置為0，開漏模式設置為1.
//bit5  中斷鎖存模式.50us-pulse模式設置為0，latch-until-int-cleared模式設置為1
//bit4  中斷鎖存清除模式 status-read-only狀態設置為0，any-register-read狀態設置為1.
//bit3  FSYNC中斷邏輯電平模式 0=active-high, 1=active-low
//bit2  FSYNC端口中斷啟用設置設置為0時禁用，設置為1時啟用
//bit1  I2C支路啟用狀態,此位等於1且I2C_MST_EN (寄存器 106 位[5])等於0時,主機應用程序處理器能夠直接訪問MPU-60X0的輔助I2C總線
//		否則無論如何都不能直接訪問
//bit0  當此位為1時，CLKOUT端口可以輸出參考時鐘。當此位為0時，輸出禁用


/*部分中斷使能*/
#define MPU6050_RA_INT_ENABLE       0x38
//bit7  自由落體中斷使能
//bit6  運動檢測中斷使能
//bit5  零運動檢測中斷使能
//bit4  FIFO溢出中斷使能
//bit3  IIC主機所有中斷源使能
//bit0  數據就緒中斷使能


/*DMP中斷使能*/
#define MPU6050_RA_DMP_INT_STATUS   0x39
//不知道這些位的具體作用是什麼,官方語焉不詳,但是的確存在
#define MPU6050_DMPINT_4_BIT            4
#define MPU6050_DMPINT_3_BIT            3
#define MPU6050_DMPINT_2_BIT            2
#define MPU6050_DMPINT_1_BIT            1
#define MPU6050_DMPINT_0_BIT            0

/*DMP中斷配置*/
#define MPU6050_RA_INT_STATUS       0x3A
//DMP中斷位之一使能
#define MPU6050_INTERRUPT_PLL_RDY_INT_BIT   2
//DMP中斷位之二使能
#define MPU6050_INTERRUPT_DMP_INT_BIT       1

/*加速度X輸出*/
#define MPU6050_RA_ACCEL_XOUT_H     0x3B
#define MPU6050_RA_ACCEL_XOUT_L     0x3C

/*加速度Y輸出*/
#define MPU6050_RA_ACCEL_YOUT_H     0x3D
#define MPU6050_RA_ACCEL_YOUT_L     0x3E

/*加速度Z輸出*/
#define MPU6050_RA_ACCEL_ZOUT_H     0x3F
#define MPU6050_RA_ACCEL_ZOUT_L     0x40

/*溫度值輸出*/
#define MPU6050_RA_TEMP_OUT_H       0x41
#define MPU6050_RA_TEMP_OUT_L       0x42

/*陀螺儀X輸出*/
#define MPU6050_RA_GYRO_XOUT_H      0x43
#define MPU6050_RA_GYRO_XOUT_L      0x44

/*陀螺儀Y輸出*/
#define MPU6050_RA_GYRO_YOUT_H      0x45
#define MPU6050_RA_GYRO_YOUT_L      0x46

/*陀螺儀Z輸出*/
#define MPU6050_RA_GYRO_ZOUT_H      0x47
#define MPU6050_RA_GYRO_ZOUT_L      0x48

/*從IIC從機上獲取到的數據*/
#define MPU6050_RA_EXT_SENS_DATA_00 0x49
#define MPU6050_RA_EXT_SENS_DATA_01 0x4A
#define MPU6050_RA_EXT_SENS_DATA_02 0x4B
#define MPU6050_RA_EXT_SENS_DATA_03 0x4C
#define MPU6050_RA_EXT_SENS_DATA_04 0x4D
#define MPU6050_RA_EXT_SENS_DATA_05 0x4E
#define MPU6050_RA_EXT_SENS_DATA_06 0x4F
#define MPU6050_RA_EXT_SENS_DATA_07 0x50
#define MPU6050_RA_EXT_SENS_DATA_08 0x51
#define MPU6050_RA_EXT_SENS_DATA_09 0x52
#define MPU6050_RA_EXT_SENS_DATA_10 0x53
#define MPU6050_RA_EXT_SENS_DATA_11 0x54
#define MPU6050_RA_EXT_SENS_DATA_12 0x55
#define MPU6050_RA_EXT_SENS_DATA_13 0x56
#define MPU6050_RA_EXT_SENS_DATA_14 0x57
#define MPU6050_RA_EXT_SENS_DATA_15 0x58
#define MPU6050_RA_EXT_SENS_DATA_16 0x59
#define MPU6050_RA_EXT_SENS_DATA_17 0x5A
#define MPU6050_RA_EXT_SENS_DATA_18 0x5B
#define MPU6050_RA_EXT_SENS_DATA_19 0x5C
#define MPU6050_RA_EXT_SENS_DATA_20 0x5D
#define MPU6050_RA_EXT_SENS_DATA_21 0x5E
#define MPU6050_RA_EXT_SENS_DATA_22 0x5F
#define MPU6050_RA_EXT_SENS_DATA_23 0x60

//運動檢測的狀態
#define MPU6050_RA_MOT_DETECT_STATUS    0x61
//bit7  x軸反向運動檢測中斷狀態
//bit6  x軸正向運動檢測中斷狀態
//bit5  Y軸反向運動檢測中斷狀態
//bit4  Y軸正向運動檢測中斷狀態
//bit3  Z軸反向運動檢測中斷狀態
//bit2  Z軸正向運動檢測中斷狀態
//bit1
//bit0  零運動檢測中斷狀態
//


/*寫入到IIC從機中的數據,指定的slv數據輸出容器*/
#define MPU6050_RA_I2C_SLV0_DO      0x63
#define MPU6050_RA_I2C_SLV1_DO      0x64
#define MPU6050_RA_I2C_SLV2_DO      0x65
#define MPU6050_RA_I2C_SLV3_DO      0x66

/*外部影子寄存器的配置,這個寄存器用於指定外部傳感器數據影子的時間
*當啟用了某一特定的slave，其傳輸速率就會減小。
*當一個slave的傳輸速率是根據採樣率而降低的,那麼該slave是以
*每1 / (1 + I2C_MST_DLY) 個樣本進行傳輸。
*     1 / (1 + I2C_MST_DLY) Samples
* 這一基本的採樣率也是由SMPLRT_DIV (寄存器 25)和DLPF_CFG (寄存器26)所決定的的。*/
#define MPU6050_RA_I2C_MST_DELAY_CTRL   0x67
//DELAY_ES_SHADOW設置為1,跟隨外部傳感器數據影子將會延遲到所有的數據接收完畢。
#define MPU6050_DELAYCTRL_DELAY_ES_SHADOW_BIT   7
//slv4-0的配置
#define MPU6050_DELAYCTRL_I2C_SLV4_DLY_EN_BIT   4
#define MPU6050_DELAYCTRL_I2C_SLV3_DLY_EN_BIT   3
#define MPU6050_DELAYCTRL_I2C_SLV2_DLY_EN_BIT   2
#define MPU6050_DELAYCTRL_I2C_SLV1_DLY_EN_BIT   1
#define MPU6050_DELAYCTRL_I2C_SLV0_DLY_EN_BIT   0

/*用於陀螺儀，加速度計，溫度傳感器的模擬和數字信號通道的復位。
復位會還原模數轉換信號通道和清除他們的上電配置*/
#define MPU6050_RA_SIGNAL_PATH_RESET    0x68
//bit2  重置陀螺儀的信號路徑
//bit1  重置加速度傳感器的信號路徑
//bit0  重置溫度傳感器的信號路徑


/*獲取加速度傳感器啟動延遲 還有濾波器的一些配置
* 加速度傳感器數據路徑為傳感器寄存器、運動檢測、
* 零運動檢測和自由落體檢測模塊提供樣本。在檢測模塊開始操作之前，
* 包含過濾器的信號路徑必須用新樣本來啟用。
* 默認的4毫秒喚醒延遲時間可以加長3毫秒以上。在ACCEL_ON_DELAY中規定
* 這個延遲以1 LSB = 1 毫秒為單位。除非InvenSense另行指示，
* 用戶可以選擇任何大於零的值。*/
#define MPU6050_RA_MOT_DETECT_CTRL      0x69
//具體的有效控制位
//bit5-bit4 [5:4]1-4ms 延時時間1-4ms選擇
//bit3-bit2 自由落體檢測計數器的減量配置。
// 			當指定數量的樣本的加速度測量都滿足其各自的閾值條件時，
//  		檢測結果存儲於自由落體檢測模塊中。當滿足閾值條件時，
//  		相應的檢測計數器遞增1。用戶可通過FF_COUNT配置不滿足閾值條件來減量。
//  		減量率可根據下表進行設置：
			/* FF_COUNT | 計數器減量
			* ---------+------------------
			* 0				| 重置
			* 1				| 1
			* 2				| 2
			* 3				| 4
			* 當FF_COUNT配置為0(復位)時,任何不合格的樣品都將計數器重置為0*/
//bit1-bit0  運動檢測計數器的減量配置。
// 			當指定數量的樣本的加速度測量都滿足其各自的閾值條件時，
// 			檢測結果存儲於運動檢測模塊中。當滿足閾值條件時，相應的檢測計數器遞增1。
// 			用戶可通過MOT_COUNT配置不滿足閾值條件來減量。減量率可根據下表進行設置：
// 			MOT_COUNT | 計數器減量
			/* ----------+------------------
			* 0				 | 重置
			* 1				 | 1
			* 2				 | 2
			* 3				 | 4
			* 當MOT_COUNT配置為0(復位)時,任何不合格的樣品都將計數器重置為0*/
			

/*這個寄存器允許用戶使能或使能 FIFO 緩衝區，
 *I2C 主機模式和主要 I2C 接口。FIFO 緩衝
區，I2C 主機，傳感器信號通道和傳感器寄存器也可以使用這個寄存器復位*/
#define MPU6050_RA_USER_CTRL        0x6A
//bit7  DMP禁止
//bit6  當此位設置為0,FIFO緩衝是禁用的
//bit5  當這個模式被啟用,MPU-60X0即成為輔助I2C總線上的外部傳感器slave設備的I2C主機
//		當此位被清除為0時,輔助I2C總線線路(AUX_DA and AUX_CL)理論上是由I2C總線
//		(SDA和SCL)驅動的。這是啟用旁路模式的一個前提
//bit4  I2C轉換至SPI模式(只允許MPU-6000)
//bit3  重置DMP模式,官方文檔未說明的寄存器
//bit2  重置FIFO當設置為1時，此位將重置FIFO緩衝區，此時FIFO_EN等於0。觸發重置後，此位將自動清為0
//bit1	重置I2C主機當設置為1時，此位將重置I2C主機，此時I2C_MST_EN等於0。觸發重置後，此位將自動清為0
//bit0  重置所有傳感器寄存器和信號路徑 如果只重置信號路徑（不重置傳感器寄存器），請使用寄存器104


/*允許用戶配置電源模式和時鐘源。還提供了復位整個設備和禁用溫度傳感器的位*/
#define MPU6050_RA_PWR_MGMT_1       0x6B
//bit7  觸發一個設備的完整重置。 觸發重置後，一個~ 50 毫秒的小延遲是合理的
//bit6  寄存器的SLEEP位設置使設備處於非常低功率的休眠模式。
//bit5  喚醒週期啟用狀態當此位設為1且SLEEP禁用時.在休眠模式和喚醒模式間循環，以此從活躍的傳感器中獲取數據樣本
//bit3  溫度傳感器啟用狀態控制內部溫度傳感器的使用
//bit2-bit0 設定時鐘源設置,一個頻率為8 mhz的內部振盪器,基於陀螺儀的時鐘或外部信息源都可以被選為MPU-60X0的時鐘源
			/* CLK_SEL | 時鐘源
			* --------+--------------------------------------
			* 0			 | 內部振盪器
			* 1			 | PLL with X Gyro reference
			* 2			 | PLL with Y Gyro reference
			* 3			 | PLL with Z Gyro reference
			* 4			 | PLL with external 32.768kHz reference
			* 5			 | PLL with external 19.2MHz reference
			* 6			 | Reserved
			* 7			 | Stops the clock and keeps the timing generator in reset
			* */


/*這個寄存器允許用戶配置加速度計在低功耗模式下喚起的頻率。也允許用戶讓加速度計和
陀螺儀的個別軸進入待機模式。*/
#define MPU6050_RA_PWR_MGMT_2       0x6C
//bit7-bit6 Accel-Only低電量模式下的喚醒頻率
			/* 通過把Power Management 1寄存器（寄存器107）中的PWRSEL設為1，
			* MPU-60X0可以處於Accerlerometer Only的低電量模式。在這種模式下,
			設備將關閉除了原I2C接口以外的所有設備，只留下accelerometer以固定時間
			間隔醒來進行測量。喚醒頻率可用LP_WAKE_CTRL進行配置，如下表所示：
			* LP_WAKE_CTRL | 　喚醒頻率
			* -------------+------------------
			* 0            | 1.25 Hz
			* 1            | 2.5 Hz
			* 2            | 5 Hz
			* 3            | 10 Hz
			* */
//bit5  備用的x軸加速度傳感器啟用狀態,也就是進入待機模式
//bit4  備用的Y軸加速度傳感器啟用狀態
//bit3  備用的Z軸加速度傳感器啟用狀態
//bit2  備用的x軸陀螺儀啟用狀態
//bit1  備用的Y軸陀螺儀啟用狀態
//bit0  備用的Z軸陀螺儀啟用狀態

/*設定DMP模式下的bank*/
#define MPU6050_RA_BANK_SEL         0x6D
//DMP內存配置
#define MPU6050_BANKSEL_PRFTCH_EN_BIT       6
#define MPU6050_BANKSEL_CFG_USER_BANK_BIT   5
#define MPU6050_BANKSEL_MEM_SEL_BIT         4
#define MPU6050_BANKSEL_MEM_SEL_LENGTH      5
//dmp內存地址設置
#define MPU6050_DMP_MEMORY_BANKS        8
#define MPU6050_DMP_MEMORY_BANK_SIZE    256
#define MPU6050_DMP_MEMORY_CHUNK_SIZE   16

/*設定DMP模式下的起始地址*/
#define MPU6050_RA_MEM_START_ADDR   0x6E
/*一個字節的dmp數據緩存*/
#define MPU6050_RA_MEM_R_W          0x6F
/*DMP配置寄存器1*/
#define MPU6050_RA_DMP_CFG_1        0x70
/*DMP配置寄存器2*/
#define MPU6050_RA_DMP_CFG_2        0x71

/*當前FIFO緩衝區大小
* 這個值表明了存儲於FIFO緩衝區的字節數。
* 而這個數字也是能從FIFO緩衝區讀取的字節數，
* 它與存儲在FIFO(寄存器35和36)中的傳感器數據組所提供的可用樣本數成正比。
* 兩個寄存器一起構成一個16位數據*/
#define MPU6050_RA_FIFO_COUNTH      0x72
#define MPU6050_RA_FIFO_COUNTL      0x73

/*這個寄存器用於從FIFO緩衝區中讀取和編寫數據。數據在寄存器編號(從低到高)的指
 *令下編寫入數據寫入FIFO。如果所有的FIFO啟用標誌(見下文)都被啟用了且
 *所有外部傳感器數據寄存器(寄存器73至寄存器96)都與一個slave設備相連
 *,那麼寄存器59到寄存器96的內容都將在採樣率的指令下編寫。
* 當傳感器數據寄存器（寄存器59到寄存器96）的相關FIFO啟用標誌在FIFO_EN 寄存
* 器35)中都設為1時，它們的內容將被寫入FIFO緩衝區。在I2C_MST_CTRL (寄存器 36)
* 中能找到一個與I2C Slave 3相連的額外的傳感器數據寄存器標誌。
* 如果FIFO緩衝區溢出,狀態位FIFO_OFLOW_INT自動設置為1。
* 此位位於INT_STATUS (寄存器58)中。當FIFO緩衝區溢出時,最早的數據將會丟失
* 而新數據將被寫入FIFO。如果FIFO緩衝區為空, 讀取將返回原來從FIFO中讀取的
* 最後一個字節，直到有可用的新數據。用戶應檢查FIFO_COUNT,以確保不在FIFO緩衝為空時讀取。*/
#define MPU6050_RA_FIFO_R_W         0x74

/*寄存器是用來驗證設備的身份的 默認值是0X34*/
#define MPU6050_RA_WHO_AM_I         0x75
//bit6-bit1 設備身份驗證 0x34 最高位和最低位都剔除掉

//陀螺儀數據結構體
typedef struct
{
    float gyro_x;//向右偏為正值
    float gyro_y;//向前偏為正值
    float gyro_z;//逆時針為正值
} GyroData_t;

//加速度計數據結構體
typedef struct
{
    float accel_x;
    float accel_y;
    float accel_z;
} AccelData_t;

//陀螺儀和加速度計數據結構體
typedef struct
{
    GyroData_t gyro;
    AccelData_t accel;
} Gyro_Accel_t;

/* MPU6050 暫存器輸出的原始 signed 16-bit 六軸資料。 */
typedef struct
{
    int16_t accel_x;  /* X 軸加速度原始 LSB。 */
    int16_t accel_y;  /* Y 軸加速度原始 LSB。 */
    int16_t accel_z;  /* Z 軸加速度原始 LSB。 */
    int16_t gyro_x;   /* X 軸角速度原始 LSB。 */
    int16_t gyro_y;   /* Y 軸角速度原始 LSB。 */
    int16_t gyro_z;   /* Z 軸角速度原始 LSB。 */
} MPU6050_RawData_t;

//解算後的角度數據(歐拉角)結構體
typedef struct
{
    float roll;  //滾轉角
    float pitch; //俯仰角
    float yaw;   //偏航角
} EulerAngle_t;

/*
 * @brief 初始化 MPU6050
 *
 * @param None
 * @retval None
 */
void MPU6050_Init(void);


/*
*@brief 讀取三軸角速度
*
*@param gyro_data: 指向 GyroData_t 結構體的指針，用於存儲讀取到的角速度數據
*@retval None
*/
void MPU6050_read_Gyro(GyroData_t *gyro_data);

/*
 *@brief 讀取三軸加速度
 *
 *@param accel_data: 指向 AccelData_t 結構體的指針，用於存儲讀取到的加速度數據
 *@retval None
 */
void MPU6050_read_Accel(AccelData_t *accel_data);

/*
 *@brief 讀取三軸角速度和加速度
 *
 *@param gyro_accel_data: 指向 Gyro_Accel_t 結構體的指針，用於存儲讀取到的角速度和加速度數據
 *@return HAL_OK 表示讀取成功，其餘值表示 I2C 讀取失敗
 */
HAL_StatusTypeDef MPU6050_read_Gyro_Accel(Gyro_Accel_t *gyro_accel_data);

/*
 *@brief 使用整數原始值校正 MPU6050 六軸零偏
 *@return HAL_OK 表示校準成功，HAL_TIMEOUT 表示未能確認靜止，
 *        其他值表示 I2C 讀取失敗
 */
HAL_StatusTypeDef MPU6050_Calibrate_offset(void);

#endif /* __MPU6050_H */
