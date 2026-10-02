#include "com_pid.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.01f);
}

int main(void)
{
    PID_Controller rate = {.Kd = 0.1f, .d_on_measurement = 1U, .d_cutoff_hz = 10.0f};
    rate.measure = 50.0f;
    PID_calc(&rate);
    near(rate.output, 0.0f); /* Initial nonzero gyro must not kick. */
    rate.desire = 100.0f;
    PID_calc(&rate);
    near(rate.derivative_raw, 0.0f); /* Target step alone produces no D. */
    near(rate.output, 0.0f);
    rate.measure += 6.0f;
    PID_calc(&rate);
    near(rate.derivative_raw, -1000.0f);
    near(rate.derivative, -273.7789f); /* dt=6ms, fc=10Hz */
    assert(rate.output < 0.0f && rate.output > -100.0f);
    float previous_d = rate.derivative;
    PID_calc(&rate);
    near(rate.derivative_raw, 0.0f);
    assert(rate.derivative > previous_d && rate.derivative < 0.0f);
    for (int i = 0; i < 100; ++i) PID_calc(&rate);
    near(rate.derivative, 0.0f);

    /* Zero cutoff bypasses filtering; resetting initialized clears D history. */
    rate.d_cutoff_hz = 0.0f;
    rate.measure -= 6.0f;
    PID_calc(&rate);
    near(rate.derivative, 1000.0f);
    rate.initialized = 0U;
    rate.measure = -30.0f;
    PID_calc(&rate);
    near(rate.derivative, 0.0f);
    PID_calc(&rate);
    near(rate.derivative, 0.0f);

    /* Unconfigured controllers retain error derivative and integral behavior. */
    PID_Controller legacy = {.Kp = 2.0f, .Ki = 1.0f, .Kd = 0.1f};
    PID_calc(&legacy);
    legacy.desire = 6.0f;
    PID_calc(&legacy);
    near(legacy.derivative, 1000.0f);
    near(legacy.output, 112.036f);

    /* Outer-loop target changes must not produce an inner-loop D kick. */
    PID_Controller outer = {.Kp = 2.0f};
    PID_Controller inner = {.Kp = 3.0f, .Kd = 0.1f,
        .d_on_measurement = 1U, .d_cutoff_hz = 10.0f};
    PID_calc_chain(&outer, &inner);
    outer.desire = 10.0f;
    near(PID_calc_chain(&outer, &inner), 60.0f);
    near(inner.derivative, 0.0f);
    puts("PASS: D on measurement, no target kick, LPF response/decay, startup/reset, bypass, legacy PID and cascade");
    return 0;
}
