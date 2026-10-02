#include "com_pid.h"


// 單次計算PID計算
// 單次 PID 計算
void PID_calc(PID_Controller *pid)
{
    // 1. 計算誤差
    pid->error = pid->desire - pid->measure;

    // 2. 計算積分誤差
    pid->integral += pid->error * PID_PERIOD;

    // 3. 首次取樣建立基準，不產生微分尖峰。
    if (pid->initialized == 0)
    {
        pid->derivative_raw = 0.0f;
        pid->derivative = 0.0f;
        pid->initialized = 1;
    }
    else
    {
        pid->derivative_raw = pid->d_on_measurement
            ? -(pid->measure - pid->previous_measurement) / PID_PERIOD
            : (pid->error - pid->previous_error) / PID_PERIOD;

        if (pid->d_cutoff_hz > 0.0f)
        {
            /* alpha = tau / (tau + dt) = 1 / (1 + 2*pi*fc*dt)。
             * alpha 是「舊微分值」的權重，與 gyro 濾波器的 ALPHA 定義不同。
             */
            const float alpha = 1.0f /
                (1.0f + 6.28318530718f * pid->d_cutoff_hz * PID_PERIOD);
            pid->derivative = alpha * pid->derivative +
                (1.0f - alpha) * pid->derivative_raw;
        }
        else
        {
            pid->derivative = pid->derivative_raw;
        }
    }

    // 4. 計算控制輸出
    pid->output =
        (pid->Kp * pid->error) +
        (pid->Ki * pid->integral) +
        (pid->Kd * pid->derivative);

    // 5. 保存本次誤差
    pid->previous_error = pid->error;
    pid->previous_measurement = pid->measure;
}

//串級PID計算
float PID_calc_chain(PID_Controller *output_pid, PID_Controller *input_pid)
{
    // 1. 先計算外環PID
    PID_calc(output_pid);
    
    // 2. 將外環PID的輸出作為內環PID的目標值
    input_pid->desire = output_pid->output;

    // 3. 計算內環PID
    PID_calc(input_pid);

    return input_pid->output;
}
