#include "telemetry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    TelemetryData_t data = {0}, decoded = {0};
    uint8_t wire[TELEMETRY_WIRE_SIZE];
    const uint8_t expected[] = {0x12,0x34,0x56,0x78,0x01,0x02,0x03,0x04,
        0xFB,0x1E,0x00,0x64,0xFF,0x85,0x01,0xC8,0x00,0x7B,0xFF,0xF6,0x80,0x00,0x04,0x4C,0x07,0x94,0x00,0x00,0xFF,0xFF,0x02};
    assert(!Telemetry_Copy(&data));
    assert(!Telemetry_Copy(NULL));
    Telemetry_Capture(0x01020304U, -12.5f, 10.0f, -12.3f, 45.6f, 123.0f, -10.0f, -32768.0f, 1100U, 1940U, 0U, 65535U, 2U);
    assert(Telemetry_Copy(&data));
    assert(data.sequence == 1U && data.rate == -123 && data.rate_error == 456);
    assert(data.pid_output == INT16_MIN);
    assert(data.angle == -1250 && data.rate_target == 100 && data.d_term == -10);
    assert(data.p_term == 123);
    data.sequence = 0x12345678U;
    TelemetryWire_Encode(wire, &data);
    assert(memcmp(wire, expected, sizeof(wire)) == 0);
    assert(TelemetryWire_Decode(expected, sizeof(expected), &decoded));
    assert(decoded.p_term == 123);
    assert(decoded.axis == 2);
    assert(!TelemetryWire_Decode(wire, 30U, &decoded));
    wire[30]=3;assert(!TelemetryWire_Decode(wire,31,&decoded));wire[30]=2;
    assert(decoded.esc_pwm[0] == 1100U && decoded.esc_pwm[1] == 1940U);
    assert(decoded.esc_pwm[2] == 0U && decoded.esc_pwm[3] == 65535U);
    assert(!TelemetryWire_Decode(wire, 22U, &decoded));
    assert(decoded.sequence == data.sequence && decoded.rate == -123);
    assert(decoded.rate_error == 456 && decoded.pid_output == INT16_MIN);
    assert(decoded.time_ms == 0x01020304U && decoded.angle == -1250);
    assert(decoded.rate_target == 100 && decoded.d_term == -10);
    assert(!TelemetryWire_Decode(wire, 9U, &decoded));
    assert(!TelemetryWire_Decode(wire, 20U, &decoded));
    assert(!TelemetryWire_Decode(wire, 10U, &decoded)); /* Reject old format. */
    assert(!TelemetryWire_Decode(NULL, TELEMETRY_WIRE_SIZE, &decoded));
    assert(!TelemetryWire_Decode(wire, TELEMETRY_WIRE_SIZE, NULL));
    Telemetry_Capture(UINT32_MAX, 100000.0f, -100000.0f, 100000.0f, -100000.0f, 100000.0f, -100000.0f, 100000.0f, 1100U, 1940U, 0U, 65535U, 2U);
    assert(Telemetry_Copy(&data) && data.sequence == 2U);
    assert(data.rate == INT16_MAX && data.rate_error == INT16_MIN);
    assert(data.pid_output == INT16_MAX && data.d_term == INT16_MIN);
    assert(data.p_term == INT16_MAX);
    assert(data.angle == INT16_MAX && data.rate_target == INT16_MIN);
    Telemetry_Capture(0, NAN, 0, 0, 0, 0, 0, 0, 1100U, 1940U, 0U, 65535U, 2U);
    assert(!Telemetry_Copy(&data));
    Telemetry_Capture(0, 0, 0, 0, 0, 0, INFINITY, 0, 1100U, 1940U, 0U, 65535U, 2U);
    assert(!Telemetry_Copy(&data));
    Telemetry_Capture(0, 0, 0, 0, 0, NAN, 0, 0, 1100U, 1940U, 0U, 65535U, 2U);
    assert(!Telemetry_Copy(&data));
    Telemetry_Capture(0, 0, 0, 0, 0, -100000.0f, 0, 0, 1100U, 1940U, 0U, 65535U, 2U);
    assert(Telemetry_Copy(&data) && data.sequence == 3U);
    assert(data.p_term == INT16_MIN);
    for (int value = -32768; value <= 32767; ++value)
    {
        TelemetryWire_Put16(wire, (int16_t)value);
        assert(TelemetryWire_Get16(wire) == value);
    }
    puts("PASS: telemetry capture, scaling, clamping, validity, big endian and signed values");
    return 0;
}
