#ifndef COM_PID_H
#define COM_PID_H

#include <stdint.h>

#define PID_PERIOD 0.006f   // PID 計算週期，單位：秒，約 166.7 Hz

// PID 控制器結構體
// STM32F103 建議使用 float；只有確實需要更高精度時才考慮 double
typedef struct
{
    float Kp;              // 比例增益：誤差越大，比例輸出越大
    float Ki;              // 積分增益：用於消除穩態誤差
    float Kd;              // 微分增益：抑制誤差快速變化與過衝

    float desire;          // 目標值
    float measure;         // 測量值

    float error;           // 當前誤差
    float previous_error;  // 上一次誤差
    float previous_measurement; // 上一次量測值，供 D on measurement 使用

    float integral;        // 誤差積分值 ∫e dt
    float derivative_raw;  // 濾波前的微分值
    float derivative;      // 實際用於 D 項的微分值（可經低通濾波）
    float d_cutoff_hz;     // D 項截止頻率；<= 0 表示不濾波
    uint8_t d_on_measurement; // 1: -d(measure)/dt；0: d(error)/dt

    float output;          // PID 控制輸出

    uint8_t initialized;   // 微分項初始化標誌
} PID_Controller;


// 單次計算PID計算
void PID_calc(PID_Controller *pid);

//串級PID計算
float PID_calc_chain(PID_Controller *output_pid, PID_Controller *input_pid);

#endif // COM_PID_H
