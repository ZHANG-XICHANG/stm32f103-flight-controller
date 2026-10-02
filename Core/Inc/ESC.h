#ifndef ESC_H
#define ESC_H

#include "tim.h"
#include <stdint.h>

#define ESC_MIN_PULSE      1100
#define ESC_MAX_PULSE      1940
#define ESC_DEFAULT_PULSE  1100

typedef struct
{
    TIM_HandleTypeDef *tim;   // 使用哪一個 Timer，例如 &htim4
    uint32_t channel;         // 使用哪一個 Channel，例如 TIM_CHANNEL_1
    uint16_t pulse;           // 目前 PWM 脈寬，例如 1100~1940 us
} Motor_t;

// 初始化單顆 ESC
void ESC_Init(Motor_t *motor);

// 設定單顆 ESC PWM
void ESC_SetPulse(Motor_t *motor, int32_t pulse);

// 停止單顆 ESC
void ESC_Stop(Motor_t *motor);

#endif