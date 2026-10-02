# X-axis ACK telemetry

Flight task captures gyro_x_pid.measure, .error and .output immediately after
the roll cascade calculation. No PID gains or motor control logic are changed.
Communication task copies a protected snapshot and preloads the existing ACK
FIFO. ACK data describes an earlier sample, not the command just received.

Wire format: exactly 10 bytes, big endian, signed fields use two's complement.

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | uint32 | PID sample sequence; first sample 1, wraps, resets on MCU restart |
| 4 | int16 | X gyro measurement, 0.1 deg/s |
| 6 | int16 | X rate PID error (desired - measured), 0.1 deg/s |
| 8 | int16 | X rate PID output, native controller units, before motor mixing |

Conversion truncates toward zero and clamps to int16 limits. Nonfinite inputs
invalidate the snapshot. Before a valid sample exists (including startup
calibration), ACK uses the legacy 4-byte transport sequence only. The two
sequence types have separate meanings. Repeated telemetry sequence means the
same sample was queued; gaps can reflect sampling, radio or USB drops.

Remote accepts both formats. USB lines:

```
TEL_SEQ,transport_sequence
TEL,sample_sequence,gyro_x_x10,error_x_x10,pid_output_x,remote_throttle
```

The PC divides the gyro and error columns by 10. There is no timestamp or
saturation flag in this first format. Sample sequence is not an exact timebase.
remote_throttle is the remote's current 0..1000 command sampled when publishing
the USB line. It is not motor PWM or a flight-confirmed command, and is not
time-aligned with the earlier ACK sample. It does not change the 10-byte RF format.
Full telemetry is emitted through Host_SendTelemetry and the existing USB task;
USB congestion may drop a line without blocking radio control traffic.

Both boards currently use 2 Mbps, 5-byte addresses and ARD 250 us. This 10-byte
ACK fits the 15-byte limit for that configuration; changing rate or payload
length requires reviewing ARD and the remote TX timeout.

Both projects contain an identical Core/Inc/telemetry_wire.h. Keep them in sync;
do not serialize sizeof(TelemetryData_t), which may include alignment padding.

Board check: flash both builds, observe TEL_SEQ during calibration and TEL after
PID sampling begins. Compare gyro/error/output with the flight debugger; check
sign and scale, and verify disconnecting USB does not stop radio communication.
