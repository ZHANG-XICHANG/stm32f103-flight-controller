#include "flight.h"
#include "pid_command.h"

#include "Fusion.h"
#include "com_filter.h"
#include "com_pid.h"
#include "telemetry.h"
#include "receive.h"
#include "flight_state.h"
#include "ESC.h"
#include <stdbool.h>

#define FLIGHT_AHRS_GAIN                         0.5f
#define FLIGHT_GYROSCOPE_RANGE_DPS               2000.0f
#define FLIGHT_ACCELERATION_REJECTION_DEGREES    10.0f
#define FLIGHT_REJECTION_TIMEOUT_SECONDS         5.0f
#define FLIGHT_BIAS_STATIONARY_THRESHOLD_DPS     3.0f
#define FLIGHT_BIAS_STATIONARY_PERIOD_SECONDS    3.0f
#define REMOTE_THROTTLE_MAX                      1000U
#define YAW_MAX_RATE_DPS 90.0f // 搖桿兩端對應 ±90 deg/s

/*
 * 機體座標採 NWU：+X 向前、+Y 向左、+Z 向上。
 * 若 MPU6050 的安裝方向不同，只需修改此映射。
 */
static const FusionRemapAlignment sensor_alignment =
    FusionRemapAlignmentPXPYPZ;

Gyro_Accel_t gyro_accel_data = {0};
EulerAngle_t euler_angle = {0};

// 暫停俯仰角控制，保留參數供後續調整。
//俯仰角PID結構體，後續需要進行專業的PID調整，這裡先給一個初始值
PID_Controller pitch_pid = {
    .Kp = 7.0f,
    .Ki = 0.0f,
    .Kd = 0.05f,

};
//Y軸角速度PID結構體=>內環
PID_Controller gyro_y_pid = {
    .Kp = 0.5f,
    .Ki = 0.0f,
    .Kd = 0.01f,
    .d_on_measurement = 1U,
    .d_cutoff_hz = 10.0f,
};



// X 軸橫滾角 PID（外環）；先將 Kp 設為 0，從角速度內環開始調整。
PID_Controller roll_pid = {
    .Kp = 7.0f,
    .Ki = 0.0f,
    .Kd = 0.05f,
};

// X 軸角速度 PID（內環），其餘欄位自動初始化為 0。
PID_Controller gyro_x_pid = {
    .Kp = 0.5f,
    .Ki = 0.0f,
    .Kd = 0.01f,
    .d_on_measurement = 1U,
    .d_cutoff_hz = 10.0f, // D 專用低通；6 ms 週期時 alpha 約 0.726
};

// Z 軸偏航角 PID（外環）；先將 Kp 設為 0，從角速度內環開始調整。
PID_Controller yaw_pid = {
    .Kp = 1.0f,
    .Ki = 0.0f,
    .Kd = 0.0f,
};

// Z 軸角速度 PID（內環），其餘欄位自動初始化為 0。
PID_Controller gyro_z_pid = {
    .Kp = 2.0f,
    .Ki = 0.1f,
    .Kd = 0.0f,
    .d_on_measurement = 1U,
    .d_cutoff_hz = 10.0f, // D 專用低通；6 ms 週期時 alpha 約 0.726
};


static Gyro_Accel_t last_gyro_accel_data = {0};
/* 機體座標、Fusion 動態零偏修正後的角速度（deg/s），供 AHRS 與 PID 共用。
 * 不寫回感測器座標的低通歷史值。 */
static FusionVector body_rate = {0};
static FusionAhrs fusion_ahrs;
static FusionBias fusion_bias;
static bool attitude_initialised = false;
//static uint8_t scope_divider = 0U;

static Motor_t motor1 = {
    .tim = &htim4,
    .channel = TIM_CHANNEL_1,
    .pulse = ESC_DEFAULT_PULSE
};

static Motor_t motor2 = {
    .tim = &htim4,
    .channel = TIM_CHANNEL_2,
    .pulse = ESC_DEFAULT_PULSE
};

static Motor_t motor3 = {
    .tim = &htim4,
    .channel = TIM_CHANNEL_3,
    .pulse = ESC_DEFAULT_PULSE
};

static Motor_t motor4 = {
    .tim = &htim4,
    .channel = TIM_CHANNEL_4,
    .pulse = ESC_DEFAULT_PULSE
};

/* 將遙控器油門 0~1000 線性映射為 ESC 脈寬 1100~1940 us。 */
static int32_t Flight_MapThrottleToPulse(const uint16_t throttle)
{
    const uint32_t limited_throttle =
        (throttle > REMOTE_THROTTLE_MAX)
            ? REMOTE_THROTTLE_MAX
            : (uint32_t) throttle;

    return ESC_MIN_PULSE +
        (int32_t) (
            limited_throttle * (ESC_MAX_PULSE - ESC_MIN_PULSE) /
            REMOTE_THROTTLE_MAX
        );
}

static void Flight_StopAllMotors(void)
{
    ESC_Stop(&motor1);
    ESC_Stop(&motor2);
    ESC_Stop(&motor3);
    ESC_Stop(&motor4);
}



  //初始化四個 ESC
void ESC_InitAll(void)
{
  ESC_Init(&motor1);
  ESC_Init(&motor2);
  ESC_Init(&motor3);
  ESC_Init(&motor4);
}

void Flight_AttitudeInit(const float sample_rate_hz)
{
    if (sample_rate_hz <= 0.0f)
    {
        attitude_initialised = false;
        return;
    }

    const FusionAhrsSettings ahrs_settings = {
        .sampleRate = sample_rate_hz,
        .convention = FusionConventionNwu,
        .gain = FLIGHT_AHRS_GAIN,
        .gyroscopeRange = FLIGHT_GYROSCOPE_RANGE_DPS,
        .accelerationRejection = FLIGHT_ACCELERATION_REJECTION_DEGREES,
        .magneticRejection = 0.0f,
        .rejectionTimeout = FLIGHT_REJECTION_TIMEOUT_SECONDS,
    };
    const FusionBiasSettings bias_settings = {
        .sampleRate = sample_rate_hz,
        .stationaryThreshold = FLIGHT_BIAS_STATIONARY_THRESHOLD_DPS,
        .stationaryPeriod = FLIGHT_BIAS_STATIONARY_PERIOD_SECONDS,
    };

    FusionAhrsInitialise(&fusion_ahrs);
    FusionAhrsSetSettings(&fusion_ahrs, &ahrs_settings);
    FusionBiasInitialise(&fusion_bias);
    FusionBiasSetSettings(&fusion_bias, &bias_settings);

    last_gyro_accel_data = (Gyro_Accel_t) {0};
    body_rate = (FusionVector) {0};
    euler_angle = (EulerAngle_t) {0};
    attitude_initialised = true;
}


/*
 *@brief 根據三軸角速度和加速度計算歐拉角
 *
 */
void Calculate_EulerAngle(void)
{
    if (!attitude_initialised)
    {
        return;
    }

    if (MPU6050_read_Gyro_Accel(&gyro_accel_data) != HAL_OK)
    {
        /* 讀取失敗時保留上一筆姿態，避免把無效資料送入 AHRS。 */
        return;
    }

    /* 沿用原有濾波器，再將校正後的 deg/s 與 g 輸入 Fusion。 */
    gyro_accel_data.gyro.gyro_x = Common_Filter_LowPass(
        gyro_accel_data.gyro.gyro_x,
        last_gyro_accel_data.gyro.gyro_x
    );
    gyro_accel_data.gyro.gyro_y = Common_Filter_LowPass(
        gyro_accel_data.gyro.gyro_y,
        last_gyro_accel_data.gyro.gyro_y
    );
    gyro_accel_data.gyro.gyro_z = Common_Filter_LowPass(
        gyro_accel_data.gyro.gyro_z,
        last_gyro_accel_data.gyro.gyro_z
    );

    last_gyro_accel_data.gyro = gyro_accel_data.gyro;

    gyro_accel_data.accel.accel_x = (float) Common_Filter_Kalman(
        &kfs[0], gyro_accel_data.accel.accel_x
    );
    gyro_accel_data.accel.accel_y = (float) Common_Filter_Kalman(
        &kfs[1], gyro_accel_data.accel.accel_y
    );
    gyro_accel_data.accel.accel_z = (float) Common_Filter_Kalman(
        &kfs[2], gyro_accel_data.accel.accel_z
    );

    FusionVector gyroscope = {
        .axis = {
            .x = gyro_accel_data.gyro.gyro_x,
            .y = gyro_accel_data.gyro.gyro_y,
            .z = gyro_accel_data.gyro.gyro_z,
        }
    };
    FusionVector accelerometer = {
        .axis = {
            .x = gyro_accel_data.accel.accel_x,
            .y = gyro_accel_data.accel.accel_y,
            .z = gyro_accel_data.accel.accel_z,
        }
    };

    gyroscope = FusionRemap(gyroscope, sensor_alignment);
    accelerometer = FusionRemap(accelerometer, sensor_alignment);
    gyroscope = FusionBiasUpdate(&fusion_bias, gyroscope);
    body_rate = gyroscope;

    FusionAhrsUpdateNoMagnetometer(
        &fusion_ahrs,
        body_rate,
        accelerometer
    );

    const FusionEuler fusion_euler = FusionQuaternionToEuler(
        FusionAhrsGetQuaternion(&fusion_ahrs)
    );
    euler_angle.roll = fusion_euler.angle.roll;
    euler_angle.pitch = fusion_euler.angle.pitch;
    euler_angle.yaw = fusion_euler.angle.yaw;


}


/*
 *@brief 根據歐拉角計算PID輸出
 *
 */
void Calculate_PIDOutput(void)
{
    PidCommand_Apply();
    /* Pitch：遙控器 0~1000 -> -10~+10 deg */
    pitch_pid.desire =
        ((float)remoteData.pitch - 500.0f) * 0.02f;

    pitch_pid.measure = euler_angle.pitch;
    gyro_y_pid.measure = body_rate.axis.y;

    PID_calc_chain(&pitch_pid, &gyro_y_pid);


    /* Roll：遙控器 0~1000 -> -10~+10 deg */
    roll_pid.desire =
        ((float)remoteData.roll - 500.0f) * 0.02f;

    roll_pid.measure = euler_angle.roll;
    gyro_x_pid.measure = body_rate.axis.x;

    PID_calc_chain(&roll_pid, &gyro_x_pid);

    /* Yaw：遙控器 0~1000 -> -90~+90 deg/s */
    // YAW_MAX_RATE_DPS 為自行設定的最大轉速，單位 deg/s
    gyro_z_pid.desire =
        ((float)remoteData.yaw - 500.0f) / 500.0f * YAW_MAX_RATE_DPS;
    gyro_z_pid.measure = body_rate.axis.z;
    PID_calc(&gyro_z_pid);



}



/*
 *@brief 控制電機
 */
void Flight_control_motor(void)
{
    FlightState_t currentState = FlightState_GetFlightState();

    switch (currentState)
    {
    case IDLE:
        Flight_StopAllMotors();
        break;

    case NORMAL:
    {
        /* 油門太低時直接停止所有馬達。 */
        if (remoteData.thr < 10U)
        {
            Flight_StopAllMotors();
            break;
        }

        /*
         * 遙控器油門 0~1000
         * 映射至 ESC_MIN_PULSE ~ ESC_MAX_PULSE。
         */
        const int32_t base_pulse =
            Flight_MapThrottleToPulse(remoteData.thr);

        /*
         * PID 內環輸出：
         *
         * gyro_x_pid.output -> Roll correction
         * gyro_y_pid.output -> Pitch correction
         * gyro_z_pid.output -> Yaw correction
         */
        const int32_t roll_correction =
            (int32_t) gyro_x_pid.output;

        const int32_t pitch_correction =
            (int32_t) gyro_y_pid.output;

        const int32_t yaw_correction =
            (int32_t)gyro_z_pid.output;

        /*
         * Motor Mixer
         *
         *                  Front (+X)
         *
         *              M1           M2
         *               \           /
         *                \         /
         *                 \       /
         *                  \     /
         *                   \   /
         *                    UAV
         *                   /   \
         *                  /     \
         *                 /       \
         *                /         \
         *               /           \
         *              M4           M3
         *
         * +Y = Left
         *
         * Pitch:
         * M1, M2 -= pitch
         * M3, M4 += pitch
         *
         * Roll:
         * M1, M4 -= roll
         * M2, M3 += roll
         * Yaw:
         * M1, M3 -= yaw
         * M2, M4 += yaw
         */

        int32_t motor1_pulse =
            base_pulse
            - pitch_correction
            - roll_correction
            + yaw_correction;

        int32_t motor2_pulse =
            base_pulse
            - pitch_correction
            + roll_correction
            - yaw_correction;

        int32_t motor3_pulse =
            base_pulse
            + pitch_correction
            + roll_correction
            + yaw_correction;

        int32_t motor4_pulse =
            base_pulse
            + pitch_correction
            - roll_correction
            - yaw_correction;

        /*
         * 限制 ESC pulse，避免 PID correction
         * 導致輸出超出合法範圍。
         */
        if (motor1_pulse > ESC_MAX_PULSE)
            motor1_pulse = ESC_MAX_PULSE;
        else if (motor1_pulse < ESC_MIN_PULSE)
            motor1_pulse = ESC_MIN_PULSE;

        if (motor2_pulse > ESC_MAX_PULSE)
            motor2_pulse = ESC_MAX_PULSE;
        else if (motor2_pulse < ESC_MIN_PULSE)
            motor2_pulse = ESC_MIN_PULSE;

        if (motor3_pulse > ESC_MAX_PULSE)
            motor3_pulse = ESC_MAX_PULSE;
        else if (motor3_pulse < ESC_MIN_PULSE)
            motor3_pulse = ESC_MIN_PULSE;

        if (motor4_pulse > ESC_MAX_PULSE)
            motor4_pulse = ESC_MAX_PULSE;
        else if (motor4_pulse < ESC_MIN_PULSE)
            motor4_pulse = ESC_MIN_PULSE;

        ESC_SetPulse(&motor1, motor1_pulse);
        ESC_SetPulse(&motor2, motor2_pulse);
        ESC_SetPulse(&motor3, motor3_pulse);
        ESC_SetPulse(&motor4, motor4_pulse);

        break;
    }

    case FIX_HEIGHT:
        Flight_StopAllMotors();
        break;

    case FAIL:
        Flight_StopAllMotors();
        break;

    default:
        Flight_StopAllMotors();
        break;
    }

    /* Select telemetry only; all three control axes remain active. */
    const uint8_t axis = PidCommand_Axis();
    PID_Controller *rate_pid = axis == 0 ? &gyro_x_pid :
                               axis == 1 ? &gyro_y_pid : &gyro_z_pid;
    const float angle = axis == 0 ? euler_angle.roll :
                        axis == 1 ? euler_angle.pitch : euler_angle.yaw;
    Telemetry_Capture(
        HAL_GetTick(), angle, rate_pid->desire, rate_pid->measure,
        rate_pid->error, rate_pid->Kp * rate_pid->error,
        rate_pid->Kd * rate_pid->derivative, rate_pid->output,
        motor1.pulse, motor2.pulse, motor3.pulse, motor4.pulse, axis
    );
}
