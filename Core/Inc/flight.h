#ifndef FLIGHT_H
#define FLIGHT_H

#include "MPU6050.h"
#include "com_debug.h"
#include "com_filter.h"

extern Gyro_Accel_t gyro_accel_data;
extern EulerAngle_t euler_angle;

/*
 *@brief 初始化所有 ESC
 *
 */
void ESC_InitAll(void);

/* 必須在週期性呼叫 Calculate_EulerAngle() 前初始化。 */
/* Fusion 姿態解算器的初始化函式，應在 MPU6050 初始化與校正完成後呼叫一次。 */
void Flight_AttitudeInit(float sample_rate_hz);

/*
 *@brief 根據三軸角速度和加速度計算歐拉角
 *
 */
void Calculate_EulerAngle(void);

/*
 *@brief 根據歐拉角計算PID輸出
 *
 */
void Calculate_PIDOutput(void);


/*
 *@brief 控制電機
 *
 */
void Flight_control_motor(void);

#endif // FLIGHT_H
