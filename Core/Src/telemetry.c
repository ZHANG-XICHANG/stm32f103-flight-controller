#include "telemetry.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>

static TelemetryData_t latest;
static uint8_t valid;

static int16_t Telemetry_Quantize(float value, float scale)
{
    float scaled = value * scale;
    if (scaled >= 32767.0f) return INT16_MAX;
    if (scaled <= -32768.0f) return INT16_MIN;
    return (int16_t)scaled; /* Truncate toward zero. */
}

void Telemetry_Capture(uint32_t time_ms, float angle, float rate_target,
                       float rate, float rate_error, float p_term,
                       float d_term,
                       float pid_output,
                       uint16_t esc1_pwm, uint16_t esc2_pwm,
                       uint16_t esc3_pwm, uint16_t esc4_pwm, uint8_t axis)
{
    TelemetryData_t sample = {0};
    if (axis > 2U || !isfinite(angle) || !isfinite(rate_target) ||
        !isfinite(rate) || !isfinite(rate_error) ||
        !isfinite(p_term) || !isfinite(d_term) || !isfinite(pid_output))
    {
        taskENTER_CRITICAL();
        valid = 0U;
        taskEXIT_CRITICAL();
        return;
    }
    sample.esc_pwm[0] = esc1_pwm;
    sample.esc_pwm[1] = esc2_pwm;
    sample.esc_pwm[2] = esc3_pwm;
    sample.esc_pwm[3] = esc4_pwm;
    sample.axis = axis;
    sample.time_ms = time_ms;
    sample.angle = Telemetry_Quantize(angle, 100.0f);
    sample.rate_target = Telemetry_Quantize(rate_target, 10.0f);
    sample.rate = Telemetry_Quantize(rate, 10.0f);
    sample.rate_error = Telemetry_Quantize(rate_error, 10.0f);
    sample.p_term = Telemetry_Quantize(p_term, 1.0f);
    sample.d_term = Telemetry_Quantize(d_term, 1.0f);
    sample.pid_output = Telemetry_Quantize(pid_output, 1.0f);
    taskENTER_CRITICAL();
    sample.sequence = latest.sequence + 1U;
    latest = sample;
    valid = 1U;
    taskEXIT_CRITICAL();
}

uint8_t Telemetry_Copy(TelemetryData_t *data)
{
    uint8_t available;
    if (data == NULL) return 0U;
    taskENTER_CRITICAL();
    available = valid;
    if (available) *data = latest;
    taskEXIT_CRITICAL();
    return available;
}
