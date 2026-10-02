#ifndef TELEMETRY_WIRE_H
#define TELEMETRY_WIRE_H
#include <stdint.h>
#include <stddef.h>

/* Big endian: sequence/time_ms u32, selected angle in 0.01 deg i16,
 * target/gyro/error in 0.1 deg/s i16, P/D/output in native units i16.
 * Both devices must use this 31-byte layout. Never transmit a C struct. */
#define TELEMETRY_WIRE_SIZE 31U
typedef struct {
    uint32_t sequence;
    uint32_t time_ms;
    int16_t angle; /* 0.01 deg */
    int16_t rate_target; /* 0.1 deg/s */
    int16_t rate;
    int16_t rate_error;
    int16_t p_term; /* native output units */
    int16_t d_term; /* native output units */
    int16_t pid_output;
    uint8_t axis; /* 0=X, 1=Y, 2=Z */
    uint16_t esc_pwm[4]; /* M1..M4, applied pulse width in us */
} TelemetryData_t;

static inline void TelemetryWire_Put16(uint8_t *p, int16_t value)
{
    uint16_t bits = (uint16_t)value;
    p[0] = (uint8_t)(bits >> 8);
    p[1] = (uint8_t)bits;
}

static inline int16_t TelemetryWire_Get16(const uint8_t *p)
{
    uint32_t bits = ((uint32_t)p[0] << 8) | p[1];
    return (int16_t)(bits >= 32768U ? (int32_t)bits - 65536 : (int32_t)bits);
}

static inline void TelemetryWire_Encode(uint8_t *p, const TelemetryData_t *data)
{
    p[30] = data->axis;
    p[0] = (uint8_t)(data->sequence >> 24);
    p[1] = (uint8_t)(data->sequence >> 16);
    p[2] = (uint8_t)(data->sequence >> 8);
    p[3] = (uint8_t)data->sequence;
    p[4] = (uint8_t)(data->time_ms >> 24);
    p[5] = (uint8_t)(data->time_ms >> 16);
    p[6] = (uint8_t)(data->time_ms >> 8);
    p[7] = (uint8_t)data->time_ms;
    TelemetryWire_Put16(p + 8, data->angle);
    TelemetryWire_Put16(p + 10, data->rate_target);
    TelemetryWire_Put16(p + 12, data->rate);
    TelemetryWire_Put16(p + 14, data->rate_error);
    TelemetryWire_Put16(p + 16, data->p_term);
    TelemetryWire_Put16(p + 18, data->d_term);
    TelemetryWire_Put16(p + 20, data->pid_output);
    for (size_t i = 0; i < 4; ++i)
    {
        p[22 + 2 * i] = (uint8_t)(data->esc_pwm[i] >> 8);
        p[23 + 2 * i] = (uint8_t)data->esc_pwm[i];
    }
}

static inline uint8_t TelemetryWire_Decode(const uint8_t *p, uint8_t length,
                                           TelemetryData_t *data)
{
    if (p == NULL || data == NULL || length != TELEMETRY_WIRE_SIZE) return 0U;
    if (p[30] > 2U) return 0U;
    data->axis = p[30];
    data->sequence = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8) | p[3];
    data->time_ms = ((uint32_t)p[4] << 24) | ((uint32_t)p[5] << 16) |
                    ((uint32_t)p[6] << 8) | p[7];
    data->angle = TelemetryWire_Get16(p + 8);
    data->rate_target = TelemetryWire_Get16(p + 10);
    data->rate = TelemetryWire_Get16(p + 12);
    data->rate_error = TelemetryWire_Get16(p + 14);
    data->p_term = TelemetryWire_Get16(p + 16);
    data->d_term = TelemetryWire_Get16(p + 18);
    data->pid_output = TelemetryWire_Get16(p + 20);
    for (size_t i = 0; i < 4; ++i)
        data->esc_pwm[i] = (uint16_t)(((uint16_t)p[22 + 2 * i] << 8) | p[23 + 2 * i]);
    return 1U;
}
#endif
