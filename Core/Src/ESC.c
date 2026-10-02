#include "ESC.h"
#include "com_debug.h"

/**
 * @brief 限制 PWM 數值在合法範圍內
 */
static uint16_t ESC_LimitPulse(int32_t pulse)
{
    if (pulse < ESC_MIN_PULSE)
    {
        debug_printf(
        "ESC_LimitPulse: pulse %ld < %ld, set to ESC_MIN_PULSE\r\n",
        (long)pulse,
        (long)ESC_MIN_PULSE
        );

        return ESC_MIN_PULSE;
    }

    if (pulse > ESC_MAX_PULSE)
    {
        debug_printf(
        "ESC_LimitPulse: pulse %ld > %ld, set to ESC_MAX_PULSE\r\n",
        (long)pulse,
        (long)ESC_MAX_PULSE
        );

        return ESC_MAX_PULSE;
    }

    return (uint16_t)pulse;
}


/**
 * @brief 初始化一顆 ESC
 */
void ESC_Init(Motor_t *motor)
{
    // 先設定最低油門
    motor->pulse = ESC_DEFAULT_PULSE;

    // 設定 PWM 比較值
    __HAL_TIM_SET_COMPARE(
        motor->tim,
        motor->channel,
        motor->pulse
    );

    // 啟動 PWM
    HAL_TIM_PWM_Start(
        motor->tim,
        motor->channel
    );
}


/**
 * @brief 設定 ESC PWM 脈寬
 */
void ESC_SetPulse(Motor_t *motor, int32_t pulse)
{
    const uint16_t limited_pulse = ESC_LimitPulse(pulse);

    motor->pulse = limited_pulse;
    __HAL_TIM_SET_COMPARE(
        motor->tim,
        motor->channel,
        limited_pulse
    );
}

/**
 * @brief 將 ESC 設為最低油門
 */
void ESC_Stop(Motor_t *motor)
{
    ESC_SetPulse(motor, ESC_DEFAULT_PULSE);
}
