#ifndef TELEMETRY_H
#define TELEMETRY_H
#include "telemetry_wire.h"

/* Flight task publishes once after each motor update.
 * Communication task copies the coherent snapshot; no SPI in the flight task. */
void Telemetry_Capture(uint32_t time_ms, float angle, float rate_target,
                       float rate, float rate_error, float p_term,
                       float d_term,
                       float pid_output,
                       uint16_t esc1_pwm, uint16_t esc2_pwm,
                       uint16_t esc3_pwm, uint16_t esc4_pwm, uint8_t axis);
uint8_t Telemetry_Copy(TelemetryData_t *data);
#endif
