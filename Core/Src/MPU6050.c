#include "MPU6050.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * 以 MPU6050 原始 LSB 保存六軸零偏。
 * 使用 static 限制在本模組內，避免其他任務直接修改校準結果。
 */
static int32_t accel_offset_x = 0;
static int32_t accel_offset_y = 0;
static int32_t accel_offset_z = 0;
static int32_t gyro_offset_x = 0;
static int32_t gyro_offset_y = 0;
static int32_t gyro_offset_z = 0;

static HAL_StatusTypeDef MPU6050_ReadRaw(MPU6050_RawData_t *raw_data);

/**
 * @brief 取得 int32_t 的絕對值，供原始整數門檻比較使用。
 */
static int32_t MPU6050_AbsInt32(int32_t value)
{
    return (value < 0) ? -value : value;
}

/**
 * @brief 向 MPU6050 寫入寄存器
 * @param reg: 寄存器地址
 * @param value: 要寫入的值
 * @retval None
 */
void MPU6050_Write_Reg(uint8_t reg, uint8_t value)
{
    /* 使用 I2C2 將一個 byte 寫入指定的 8-bit MPU6050 暫存器。 */
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                      &value, 1U, MPU6050_I2C_TIMEOUT_MS);

}

void MPU6050_Read_Reg(uint8_t reg, uint8_t *value)
{
    /* 使用 I2C2 從指定的 8-bit MPU6050 暫存器讀取一個 byte。 */
    HAL_I2C_Mem_Read(&hi2c2, MPU6050_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                     value, 1U, MPU6050_I2C_TIMEOUT_MS);
}




/*
 * @brief 初始化 MPU6050
 *
 *
 */
void MPU6050_Init(void)
{
    /* 1. 軟體重置所有暫存器，並等待裝置重新啟動完成。 */
    MPU6050_Write_Reg(MPU6050_RA_PWR_MGMT_1, 0x80);
    /* 重置後 PWR_MGMT_1 會回到預設睡眠狀態。 */
    HAL_Delay(100U);
    
    /* 2. 清除睡眠位，喚醒 MPU6050。 */
    MPU6050_Write_Reg(MPU6050_RA_PWR_MGMT_1, 0x00);

    //3.選擇合適的量程範圍
    //越小的量程範圍，越精確，但也越容易飽和
    //加速度計量程範圍: ±2g, ±4g, ±8g, ±16g
    //陀螺儀量程範圍: ±250°/s, ±500°/s, ±1000°/s, ±2000°/s
    //這裡選擇加速度計量程範圍為±2g，陀螺儀量程範圍為±2000°/s
    MPU6050_Write_Reg(MPU6050_RA_ACCEL_CONFIG, 0<<3); // ±2g => 0<<3 = 0x00
    MPU6050_Write_Reg(MPU6050_RA_GYRO_CONFIG, 3<<3);  // ±2000°/s => 3<<3 = 0x18

    //4.關閉中斷使能沒用到
    MPU6050_Write_Reg(MPU6050_RA_INT_ENABLE, 0x00); // INT_ENABLE寄存器地址為MPU6050_RA_INT_ENABLE，寫入0表示關閉中斷使能

    //5.用戶控制寄存器 USER_CTRL 和 FIFO_EN 寄存器設置為0，關閉FIFO和主機模式
    MPU6050_Write_Reg(MPU6050_RA_FIFO_EN, 0x00); // FIFO_EN寄存器地址為MPU6050_RA_FIFO_EN，寫入0表示關閉FIFO
    MPU6050_Write_Reg(MPU6050_RA_USER_CTRL, 0x00); // USER_CTRL寄存器地址為MPU6050_RA_USER_CTRL，寫入0表示關閉主機模式

    //6.設置採樣率，這裡設置為500Hz，SMPLRT_DIV寄存器地址為MPU6050_RA_SMPLRT_DIV，寫入1表示採樣率為500Hz
    MPU6050_Write_Reg(MPU6050_RA_SMPLRT_DIV, 0x01);

    /* 7. DLPF_CFG=1：加速度頻寬約 184Hz、陀螺儀頻寬約 188Hz。 */
    MPU6050_Write_Reg(MPU6050_RA_CONFIG, 0x01);

    //8配置使用的系統時鐘源，這裡選擇使用X軸陀螺儀作為時鐘源，寫入1表示使用X軸陀螺儀作為時鐘源
    MPU6050_Write_Reg(MPU6050_RA_PWR_MGMT_1, 0x01); // PWR_MGMT_1寄存器地址為MPU6050_RA_PWR_MGMT_1，寫入1表示使用X軸陀螺儀作為時鐘源

    //9.使能(打開)加速度計和陀螺儀，這裡設置為使能，寫入0表示使能
    MPU6050_Write_Reg(MPU6050_RA_PWR_MGMT_2, 0x00); // PWR_MGMT_2寄存器地址為MPU6050_RA_PWR_MGMT_2，寫入0表示使能加速度計和陀螺儀
}

/*
*@brief 讀取三軸角速度
*
*@param gyro_data: 指向 GyroData_t 結構體的指針，用於存儲讀取到的角速度數據
*@retval None
*/
void MPU6050_read_Gyro(GyroData_t *gyro_data)
{
    Gyro_Accel_t data;

    if ((gyro_data != NULL) && (MPU6050_read_Gyro_Accel(&data) == HAL_OK))
    {
        *gyro_data = data.gyro;
    }
}

/*
 *@brief 讀取三軸加速度
 *
 *@param accel_data: 指向 AccelData_t 結構體的指針，用於存儲讀取到的加速度數據
 *@retval None
 */
void MPU6050_read_Accel(AccelData_t *accel_data)
{
    Gyro_Accel_t data;

    if ((accel_data != NULL) && (MPU6050_read_Gyro_Accel(&data) == HAL_OK))
    {
        *accel_data = data.accel;
    }
}

/*
 *@brief 讀取三軸角速度和加速度
 *
 *@param gyro_accel_data: 指向 Gyro_Accel_t 結構體的指針，用於存儲讀取到的角速度和加速度數據
 *@return HAL_OK 表示讀取成功，其餘值表示 I2C 讀取失敗
 */
HAL_StatusTypeDef MPU6050_read_Gyro_Accel(Gyro_Accel_t *gyro_accel_data)
{
    MPU6050_RawData_t raw_data;

    if (gyro_accel_data == NULL)
    {
        return HAL_ERROR;
    }

    /* 取得未校準的原始 LSB；I2C 失敗時不覆寫呼叫端資料。 */
    HAL_StatusTypeDef status = MPU6050_ReadRaw(&raw_data);

    if (status != HAL_OK)
    {
        /* 讀取失敗時保留上一筆有效資料。 */
        return status;
    }

    /* 先在整數域扣除零偏，避免浮點累加與截斷誤差。 */
    int32_t accel_corrected_x = (int32_t)raw_data.accel_x - accel_offset_x;
    int32_t accel_corrected_y = (int32_t)raw_data.accel_y - accel_offset_y;
    int32_t accel_corrected_z = (int32_t)raw_data.accel_z - accel_offset_z;
    int32_t gyro_corrected_x = (int32_t)raw_data.gyro_x - gyro_offset_x;
    int32_t gyro_corrected_y = (int32_t)raw_data.gyro_y - gyro_offset_y;
    int32_t gyro_corrected_z = (int32_t)raw_data.gyro_z - gyro_offset_z;

    /* 將校正後的加速度 LSB 轉換為 g。 */
    gyro_accel_data->accel.accel_x =
        (float)accel_corrected_x / MPU6050_ACCEL_SCALE_2G;
    gyro_accel_data->accel.accel_y =
        (float)accel_corrected_y / MPU6050_ACCEL_SCALE_2G;
    gyro_accel_data->accel.accel_z =
        (float)accel_corrected_z / MPU6050_ACCEL_SCALE_2G;

    /* 將校正後的陀螺儀 LSB 轉換為 degree/s。 */
    gyro_accel_data->gyro.gyro_x =
        (float)gyro_corrected_x / MPU6050_GYRO_SCALE_2000DPS;
    gyro_accel_data->gyro.gyro_y =
        (float)gyro_corrected_y / MPU6050_GYRO_SCALE_2000DPS;
    gyro_accel_data->gyro.gyro_z =
        (float)gyro_corrected_z / MPU6050_GYRO_SCALE_2000DPS;

    return HAL_OK;
}


/**
 * @brief 使用原始整數資料進行 MPU6050 零偏校準。
 *
 * 校準時板子必須水平靜止且 Z 軸朝上。先確認連續穩定狀態，
 * 再平均 1000 筆原始值；Z 軸加速度保留 +1g，不把重力當成零偏。
 */
HAL_StatusTypeDef MPU6050_Calibrate_offset(void)
{
    MPU6050_RawData_t previous;
    MPU6050_RawData_t current;
    uint16_t stable_count = 0U;
    uint16_t stability_checks = 0U;

    /* 第一筆資料只作為後續相鄰樣本差值的基準。 */
    HAL_StatusTypeDef status = MPU6050_ReadRaw(&previous);
    if (status != HAL_OK)
    {
        return status;
    }

    while ((stable_count < MPU6050_STABLE_REQUIRED_SAMPLES) &&
           (stability_checks < MPU6050_STABILITY_MAX_CHECKS))
    {
        vTaskDelay(pdMS_TO_TICKS(MPU6050_CALIBRATION_PERIOD_MS));

        status = MPU6050_ReadRaw(&current);
        if (status != HAL_OK)
        {
            return status;
        }

        /* 相鄰加速度樣本差用來排除震動與移動。 */
        int32_t diff_ax = MPU6050_AbsInt32(
            (int32_t)current.accel_x - previous.accel_x);
        int32_t diff_ay = MPU6050_AbsInt32(
            (int32_t)current.accel_y - previous.accel_y);
        int32_t diff_az = MPU6050_AbsInt32(
            (int32_t)current.accel_z - previous.accel_z);

        /*
         * 比較合加速度平方，避免在 Cortex-M3 上執行昂貴的 sqrtf()。
         * 靜止時合加速度應接近 1g。
         */
        int64_t accel_norm_squared =
            (int64_t)current.accel_x * current.accel_x +
            (int64_t)current.accel_y * current.accel_y +
            (int64_t)current.accel_z * current.accel_z;
        int64_t accel_norm_min_squared =
            (int64_t)MPU6050_ACCEL_NORM_MIN_RAW * MPU6050_ACCEL_NORM_MIN_RAW;
        int64_t accel_norm_max_squared =
            (int64_t)MPU6050_ACCEL_NORM_MAX_RAW * MPU6050_ACCEL_NORM_MAX_RAW;

        /* 同時確認變化量、合加速度、水平程度及 Z 軸朝向。 */
        uint8_t acceleration_is_stable =
            (diff_ax < MPU6050_ACCEL_STABLE_DELTA_RAW) &&
            (diff_ay < MPU6050_ACCEL_STABLE_DELTA_RAW) &&
            (diff_az < MPU6050_ACCEL_STABLE_DELTA_RAW) &&
            (accel_norm_squared >= accel_norm_min_squared) &&
            (accel_norm_squared <= accel_norm_max_squared) &&
            (MPU6050_AbsInt32(current.accel_x) <
                MPU6050_ACCEL_LEVEL_XY_MAX_RAW) &&
            (MPU6050_AbsInt32(current.accel_y) <
                MPU6050_ACCEL_LEVEL_XY_MAX_RAW) &&
            (current.accel_z > 0);

        /* 三軸角速度都必須低於門檻，避免旋轉中進行零偏校準。 */
        uint8_t gyro_is_stable =
            (MPU6050_AbsInt32(current.gyro_x) < MPU6050_GYRO_STABLE_LIMIT_RAW) &&
            (MPU6050_AbsInt32(current.gyro_y) < MPU6050_GYRO_STABLE_LIMIT_RAW) &&
            (MPU6050_AbsInt32(current.gyro_z) < MPU6050_GYRO_STABLE_LIMIT_RAW);

        if (acceleration_is_stable && gyro_is_stable)
        {
            stable_count++;
        }
        else
        {
            /* 任一條件失敗都必須重新累積連續穩定次數。 */
            stable_count = 0U;
        }

        previous = current;
        stability_checks++;
    }

    if (stable_count < MPU6050_STABLE_REQUIRED_SAMPLES)
    {
        return HAL_TIMEOUT;
    }

    /*
     * 使用 int64_t 累加原始 int16_t，避免增加取樣數後發生溢位；
     * 校準階段不先轉 float，因此不會產生逐筆截斷誤差。
     */
    int64_t accel_sum_x = 0;
    int64_t accel_sum_y = 0;
    int64_t accel_sum_z = 0;
    int64_t gyro_sum_x = 0;
    int64_t gyro_sum_y = 0;
    int64_t gyro_sum_z = 0;

    for (uint16_t i = 0U; i < MPU6050_CALIBRATION_SAMPLES; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(MPU6050_CALIBRATION_PERIOD_MS));

        status = MPU6050_ReadRaw(&current);
        if (status != HAL_OK)
        {
            return status;
        }

        accel_sum_x += current.accel_x;
        accel_sum_y += current.accel_y;
        accel_sum_z += current.accel_z;
        gyro_sum_x += current.gyro_x;
        gyro_sum_y += current.gyro_y;
        gyro_sum_z += current.gyro_z;
    }

    /* 只有完整取樣成功後才更新全域 offset。 */
    accel_offset_x = (int32_t)(accel_sum_x / MPU6050_CALIBRATION_SAMPLES);
    accel_offset_y = (int32_t)(accel_sum_y / MPU6050_CALIBRATION_SAMPLES);
    /* 水平且 Z 軸朝上時應量到 +1g，因此 Z offset 只扣除超出的部分。 */
    accel_offset_z =
        (int32_t)(accel_sum_z / MPU6050_CALIBRATION_SAMPLES) -
        MPU6050_ACCEL_1G_RAW;
    gyro_offset_x = (int32_t)(gyro_sum_x / MPU6050_CALIBRATION_SAMPLES);
    gyro_offset_y = (int32_t)(gyro_sum_y / MPU6050_CALIBRATION_SAMPLES);
    gyro_offset_z = (int32_t)(gyro_sum_z / MPU6050_CALIBRATION_SAMPLES);

    return HAL_OK;
}

/**
 * @brief 一次連續讀取尚未套用零偏的六軸原始資料。
 */
static HAL_StatusTypeDef MPU6050_ReadRaw(MPU6050_RawData_t *raw_data)
{
    uint8_t raw[MPU6050_BURST_LENGTH];

    if (raw_data == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * 從 0x3B 連續讀 14 bytes：
     * [0..5] Accel XYZ、[6..7] Temperature、[8..13] Gyro XYZ。
     */
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        &hi2c2,
        MPU6050_I2C_ADDR,
        MPU6050_RA_ACCEL_XOUT_H,
        I2C_MEMADD_SIZE_8BIT,
        raw,
        MPU6050_BURST_LENGTH,
        MPU6050_I2C_TIMEOUT_MS
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /* 每個量測值皆為高 byte 在前的 two's-complement signed 16-bit。 */
    raw_data->accel_x = (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    raw_data->accel_y = (int16_t)(((uint16_t)raw[2] << 8) | raw[3]);
    raw_data->accel_z = (int16_t)(((uint16_t)raw[4] << 8) | raw[5]);
    raw_data->gyro_x = (int16_t)(((uint16_t)raw[8] << 8) | raw[9]);
    raw_data->gyro_y = (int16_t)(((uint16_t)raw[10] << 8) | raw[11]);
    raw_data->gyro_z = (int16_t)(((uint16_t)raw[12] << 8) | raw[13]);

    return HAL_OK;
}
