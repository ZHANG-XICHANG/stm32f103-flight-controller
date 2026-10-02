# nRF24 transmit tests

These tests compile the actual driver with simulated SPI registers, GPIO and time.
They cover successful TX, maximum retries, timeout cancellation, stale TX flags,
RX flag preservation, tick wrap, invalid arguments, CE high duration, startup
delay, and both RTOS and pre-scheduler waits. They do not validate RF hardware,
electrical timing or RTOS scheduling on the board.

Fault injection covers HAL_ERROR, HAL_BUSY and HAL_TIMEOUT at every byte of
successful, retry-exhausted and timed-out TX paths (including RX restoration).
It also checks initialization/check failures, discarded partial RX data, GPIO
cleanup, shared transaction deadlines across tick wrap, and recovery after a
transient failure. Error_Handler is an assertion trap and must never be called.

Run from the repository root in a Visual Studio x64 Native Tools Command Prompt:

```bat
if not exist build\nrf24-tests mkdir build\nrf24-tests
cl /nologo /utf-8 /std:c11 /W4 /Itests/nrf24/stubs /ICore/Inc /FInRF24L01P.h Core/Src/nRF24L01P.c Core/Src/receive.c tests/nrf24/test_transmit.c /Febuild/nrf24-tests/test_transmit.exe /Fobuild/nrf24-tests/
build\nrf24-tests\test_transmit.exe
```

Keep assertions enabled (do not define `NDEBUG`). The stub headers are for this
test command only and must not be added to the firmware include paths.

The F103 port also tests SPI1 selection, fixed 17-byte receive framing, FIFO
draining after RX_DR is cleared, invalid header/checksum/length rejection,
preservation of a pending power event, receive-side SPI fault injection and
reinitialization. RTOS task retry timing and actual radio links require board tests.

ACK sequence tests use the default dynamic-payload configuration. They cover
preloading before RX starts, four-byte big-endian encoding, monotonic enqueue
numbers, TX FIFO full handling without flushing, failed enqueue/reinitialization,
and readback rejection when ACK Payload is not enabled. The control packet
still has exactly 17 bytes even though the radio uses dynamic payload lengths.
