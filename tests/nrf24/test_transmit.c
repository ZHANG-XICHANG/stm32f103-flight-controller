#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "nRF24L01P.h"
#include "spi.h"
#include "cmsis_os2.h"
#include "../../Core/Inc/receive.h"

SPI_HandleTypeDef hspi1;
static uint8_t regs[32], command, payload[32], outcome;
static unsigned int byteIndex, payloadSize, spiCalls, powerDowns, osWaits, halWaits;
static uint32_t now, triggeredAt;
static int ce, active, running;
static int csn;
static unsigned int failAt, byteDelay;
static HAL_StatusTypeDef injectedError;
static uint8_t rxPackets[3][32], rxWidths[3];
static unsigned int rxHead, rxCount, rxFlushes;
static uint8_t ackPayloads[3][4];
static unsigned int ackCount, txFlushes;

static void advance(uint32_t ms)
{
    now += ms;
    if (active && outcome && (uint32_t)(now - triggeredAt) >= 4U)
    {
        regs[L01REG_STATUS] |= outcome;
        if (outcome == (1U << TX_DS)) payloadSize = 0;
        active = 0;
    }
}

void HAL_GPIO_WritePin(int port, int pin, int value)
{
    (void)port;
    if (pin == nRF24_CSN_Pin)
    {
        if (value == GPIO_PIN_SET && !csn && command == W_ACK_PAYLOAD && byteIndex == 5U)
            ++ackCount;
        if (value == GPIO_PIN_SET && !csn && command == R_RX_PAYLOAD &&
            rxCount && byteIndex == (unsigned int)rxWidths[rxHead] + 1U)
        {
            rxHead = (rxHead + 1U) % 3U;
            --rxCount;
        }
        csn = value;
        if (value == GPIO_PIN_RESET) byteIndex = 0;
    }
    if (pin != nRF24_CE_Pin) return;
    if (!value && ce && !(regs[L01REG_CONFIG] & (1U << PRIM_RX)))
        assert((uint32_t)(now - triggeredAt) >= 1U);
    ce = value;
    if (value && !(regs[L01REG_CONFIG] & (1U << PRIM_RX)))
    {
        assert(regs[L01REG_CONFIG] & (1U << PWR_UP));
        assert(payloadSize == FIXED_PACKET_LEN);
        triggeredAt = now;
        active = 1;
    }
}

int HAL_GPIO_ReadPin(int port, int pin) { (void)port; (void)pin; return 1; }
uint32_t HAL_GetTick(void) { return now; }
void HAL_Delay(uint32_t ms) { halWaits++; advance(ms + 1U); }
void Error_Handler(void) { assert(!"Unexpected SPI error"); }
void Debug_Print(const char *text) { (void)text; }
void Debug_Printf(const char *format, ...) { (void)format; }
int osKernelGetState(void) { return running ? osKernelRunning : osKernelInactive; }
uint32_t osKernelGetTickFreq(void) { return 1000U; }
int osDelay(uint32_t ticks) { osWaits++; advance(ticks); return 0; }

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, const uint8_t *tx,
                           uint8_t *rx, uint16_t size, uint32_t timeout)
{
    assert(spi == &hspi1);
    assert(timeout > 0U && timeout <= L01_SPI_TIMEOUT_MS);
    assert(!csn);
    assert(size == 1U);
    assert(++spiCalls < 10000U); /* Detect unbounded polling. */
    if (spiCalls == failAt)
    {
        *rx = 0xFFU; /* Failed data must never be mistaken for TX success. */
        if (injectedError == HAL_TIMEOUT) advance(timeout);
        return injectedError;
    }
    advance(byteDelay);
    *rx = regs[L01REG_STATUS];
    if (byteIndex++ == 0U)
    {
        command = *tx;
        if (command == FLUSH_TX) { payloadSize = ackCount = 0; ++txFlushes; }
        if (command == FLUSH_RX) { rxCount = 0; ++rxFlushes; }
    }
    else if ((command & 0xE0U) == W_REGISTER)
    {
        unsigned int reg = command & 0x1FU;
        if (reg == L01REG_STATUS) regs[reg] &= (uint8_t)~*tx;
        else
        {
            if (reg == L01REG_CONFIG && (regs[reg] & (1U << PWR_UP)) &&
                !(*tx & (1U << PWR_UP)))
            {
                powerDowns++;
                active = 0;
            }
            regs[reg] = *tx;
        }
    }
    else if ((command & 0xE0U) == R_REGISTER)
    {
        *rx = (command == L01REG_FIFO_STATUS)
            ? ((rxCount ? 0U : (1U << RX_EMPTY)) | (ackCount == 3U ? (1U << TX_FULL_1) : 0U))
            : regs[command & 0x1FU];
    }
    else if (command == W_TX_PAYLOAD)
    {
        assert(payloadSize < sizeof(payload));
        payload[payloadSize++] = *tx;
    }
    else if (command == R_RX_PL_WID) *rx = rxCount ? rxWidths[rxHead] : 0U;
    else if (command == W_ACK_PAYLOAD)
    {
        assert(ackCount < 3U && byteIndex - 2U < 4U);
        ackPayloads[ackCount][byteIndex - 2U] = *tx;
    }
    else if (command == R_RX_PAYLOAD)
    {
        assert(rxCount && byteIndex - 2U < rxWidths[rxHead]);
        *rx = rxPackets[rxHead][byteIndex - 2U];
    }
    return HAL_OK;
}

static void reset(uint8_t result, uint32_t start, int scheduler)
{
    memset(regs, 0, sizeof(regs));
    regs[L01REG_CONFIG] = (1U << PWR_UP) | (1U << PRIM_RX);
    regs[L01REG_RX_PW_P0] = FIXED_PACKET_LEN;
    /* Old TX result must be cleared; pending RX notification must survive. */
    regs[L01REG_STATUS] = (1U << RX_DR) | (1U << TX_DS);
    outcome = result;
    now = start;
    running = scheduler;
    ce = active = 0;
    L01_SetCE(CE_LOW);
    csn = 1;
    failAt = byteDelay = 0;
    spiCalls = powerDowns = osWaits = halWaits = payloadSize = 0;
    rxHead = rxFlushes = 0;
    ackCount = txFlushes = 0;
    rxCount = 1;
    rxWidths[0] = FIXED_PACKET_LEN;
    memset(rxPackets, 0x5A, sizeof(rxPackets));
}

static void checkTransfer(uint8_t outcomeFlags, uint8_t expected, uint32_t start, int scheduler)
{
    uint8_t data[FIXED_PACKET_LEN];
    for (unsigned int i = 0; i < sizeof(data); i++) data[i] = (uint8_t)i;
    reset(outcomeFlags, start, scheduler);
    assert(L01_TransmitPacket(data, sizeof(data), 10U) == expected);
    assert(memcmp(payload, data, sizeof(data)) == 0);
    assert(payloadSize == 0U);
    assert(ce && !active);
    assert((regs[L01REG_CONFIG] & 3U) == 3U); /* Powered RX. */
    assert(regs[L01REG_STATUS] == (1U << RX_DR));
    assert(powerDowns == (expected == L01_TX_TIMEOUT ? 1U : 0U));
    assert(scheduler ? (osWaits && !halWaits) : (halWaits && !osWaits));
}

static void checkSpiErrors(void)
{
    uint8_t data[32] = {0};
    const uint8_t outcomes[] = {1U << TX_DS, 1U << MAX_RT, 0U};
    const HAL_StatusTypeDef errors[] = {HAL_ERROR, HAL_BUSY, HAL_TIMEOUT};
    for (unsigned int o = 0; o < sizeof(outcomes); ++o)
    {
        reset(outcomes[o], 0, 1);
        (void)L01_TransmitPacket(data, FIXED_PACKET_LEN, 10U);
        unsigned int calls = spiCalls;
        for (unsigned int e = 0; e < sizeof(errors) / sizeof(errors[0]); ++e)
        {
            for (unsigned int i = 1; i <= calls; ++i)
            {
                reset(outcomes[o], 0, 1);
                failAt = i;
                injectedError = errors[e];
                assert(L01_TransmitPacket(data, FIXED_PACKET_LEN, 10U) == L01_TX_SPI_ERROR);
                assert(spiCalls == i); /* Includes cleanup failures after TX success. */
                assert(csn && !ce && L01_GetCEStatus() == CE_LOW);
            }
        }
    }

    reset(0, 0, 1);
    assert(L01_Init() == HAL_OK);
    unsigned int initCalls = spiCalls;
    for (unsigned int i = 1; i <= initCalls; ++i)
    {
        reset(0, 0, 1);
        failAt = i;
        injectedError = HAL_BUSY;
        assert(L01_Init() == HAL_BUSY);
        assert(spiCalls == i && csn && !ce);
    }
    for (unsigned int i = 1; i <= 12U; ++i)
    {
        reset(0, 0, 1);
        failAt = i;
        injectedError = HAL_TIMEOUT;
        assert(L01_Check() == HAL_TIMEOUT);
        assert(spiCalls == i && csn && !ce);
    }

    for (unsigned int i = 1; i <= FIXED_PACKET_LEN + 3U; ++i)
    {
        uint8_t length = 99U;
        reset(0, 0, 1);
        memset(data, 0xA5, sizeof(data));
        failAt = i;
        injectedError = HAL_ERROR;
        assert(L01_ReadRXPayload(data, &length) == HAL_ERROR);
        assert(length == 0U && spiCalls == i && csn && !ce);
        for (unsigned int j = 0; j < sizeof(data); ++j) assert(data[j] == 0xA5U);
    }
    reset(0, 0, 1);
    failAt = 2U;
    injectedError = HAL_BUSY;
    uint8_t value = 0xA5U;
    assert(L01_ReadSingleReg(L01REG_CONFIG, &value) == HAL_BUSY);
    assert(value == 0xA5U && csn);

    /* Deadline is shared across bytes, including tick wrap. */
    reset(0, UINT32_MAX, 1);
    byteDelay = 1U;
    assert(L01_ReadMultiReg(L01REG_CONFIG, data, 5U) == HAL_TIMEOUT);
    assert(spiCalls == L01_SPI_TIMEOUT_MS && csn && !ce);

    /* A transient failure can recover through reinitialization. */
    reset(1U << TX_DS, 0, 1);
    failAt = 1U;
    injectedError = HAL_ERROR;
    assert(L01_Init() == HAL_ERROR);
    failAt = 0U;
    assert(L01_Init() == HAL_OK);
    assert(L01_Check() == HAL_OK);
    assert(L01_TransmitPacket(data, FIXED_PACKET_LEN, 10U) == L01_TX_SUCCESS);
}

static void checkRxQueueAndRates(void)
{
    uint8_t data[32], length;
    reset(0, 0, 1);
    rxCount = 3;
    for (unsigned int i = 0; i < 3U; ++i)
    {
        rxWidths[i] = FIXED_PACKET_LEN;
        memset(rxPackets[i], (int)(0x30U + i), FIXED_PACKET_LEN);
    }
    for (unsigned int i = 0; i < 3U; ++i)
    {
        assert(L01_ReadRXPayload(data, &length) == HAL_OK);
        assert(length == FIXED_PACKET_LEN);
        for (unsigned int j = 0; j < length; ++j) assert(data[j] == 0x30U + i);
        assert(rxCount == 2U - i && rxFlushes == 0U);
    }
    reset(0, 0, 1);
    rxWidths[0] = 33U;
    regs[L01REG_RX_PW_P0] = 33U;
    memset(data, 0xA5, sizeof(data));
    assert(L01_ReadRXPayload(data, &length) == HAL_ERROR);
    assert(length == 0U && rxCount == 0U && rxFlushes == 1U);
    assert(data[0] == 0xA5U);

    const L01_DRATE rates[] = {DRATE_250K, DRATE_1M, DRATE_2M};
    const uint8_t rateBits[] = {0x20U, 0x00U, 0x08U};
    for (unsigned int r = 0; r < 3U; ++r)
    {
        for (unsigned int original = 0; original <= 255U; ++original)
        {
            reset(0, 0, 1);
            regs[L01REG_RF_SETUP] = (uint8_t)original;
            assert(L01_SetDataRate(rates[r]) == HAL_OK);
            assert(regs[L01REG_RF_SETUP] == ((original & 0xD7U) | rateBits[r]));
        }
    }
    puts("PASS: queued RX packets preserved, invalid width flushed, all rates preserve unrelated bits");
}

static void makeControlPacket(unsigned int slot, uint16_t throttle, uint8_t power)
{
    uint8_t *p = rxPackets[slot];
    memset(p, 0, 32U);
    rxWidths[slot] = REMOTE_PACKET_SIZE;
    p[0] = 0xAA; p[1] = 0x55; p[2] = 0xAA;
    p[3] = (uint8_t)(throttle >> 8); p[4] = (uint8_t)throttle;
    p[5] = p[7] = p[9] = 1U;
    p[6] = p[8] = p[10] = 244U; /* 500 */
    p[12] = power;
    uint32_t sum = 0U;
    for (unsigned int i = 0; i < 13U; ++i) sum += p[i];
    p[13] = (uint8_t)(sum >> 24); p[14] = (uint8_t)(sum >> 16);
    p[15] = (uint8_t)(sum >> 8); p[16] = (uint8_t)sum;
}

static void checkReceiver(void)
{
    reset(0, 0, 1);
    regs[L01REG_DYNPD] = 0x3FU;
    regs[L01REG_FEATURE] = 0x07U;
    assert(Receive_Init() == HAL_OK && ce);
    assert(regs[L01REG_DYNPD] == (1U << DPL_P0));
    assert(regs[L01REG_FEATURE] == ((1U << EN_DPL) | (1U << EN_ACK_PAY)));
    assert(ackCount == 1U); /* First ACK is ready before reception starts. */
    assert(Receive_Data() == RECEIVE_EMPTY);

    remoteData = (RemoteData_t){0};
    rxCount = 3;
    makeControlPacket(0, 100U, 1U);
    makeControlPacket(1, 200U, 0U);
    makeControlPacket(2, 300U, 0U);
    rxPackets[1][16] ^= 1U; /* Bad checksum must not hide the third packet. */
    regs[L01REG_STATUS] = (1U << RX_DR) | (1U << TX_DS);
    unsigned int flushes = rxFlushes;
    assert(Receive_Data() == RECEIVE_OK);
    assert(remoteData.thr == 100U && remoteData.roll == 500U && remoteData.power == 1U);
    assert(regs[L01REG_STATUS] == (1U << TX_DS));
    assert(Receive_Data() == RECEIVE_INVALID);
    assert(remoteData.thr == 100U);
    assert(Receive_Data() == RECEIVE_OK);
    assert(remoteData.thr == 300U && remoteData.power == 1U);
    assert(Receive_Data() == RECEIVE_EMPTY && rxFlushes == flushes);

    reset(0, 0, 1);
    makeControlPacket(0, 400U, 0U);
    rxPackets[0][0] = 0U;
    assert(Receive_Data() == RECEIVE_INVALID && remoteData.thr == 300U);

    reset(0, 0, 1);
    rxWidths[0] = regs[L01REG_RX_PW_P0] = 16U;
    assert(Receive_Data() == RECEIVE_INVALID && remoteData.thr == 300U);

    /* Fail each SPI byte in FIFO check, IRQ clear and payload read. */
    for (unsigned int i = 1U; i <= REMOTE_PACKET_SIZE + 7U; ++i)
    {
        reset(0, 0, 1);
        makeControlPacket(0, 400U, 0U);
        failAt = i;
        injectedError = HAL_TIMEOUT;
        assert(Receive_Data() == RECEIVE_RADIO_ERROR);
        assert(remoteData.thr == 300U && csn && !ce);
    }
    failAt = 0U;
    assert(Receive_Init() == HAL_OK && ce);
    puts("PASS: F103 SPI1, receiver init/recovery, fixed 17-byte protocol, queued packets, checksum/header/length rejection, latched power event, SPI errors");
}

static uint32_t queuedSequence(unsigned int slot)
{
    const uint8_t *p = ackPayloads[slot];
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void checkAckSequence(void)
{
    reset(0, 0, 1);
    assert(Receive_Init() == HAL_OK && ackCount == 1U);
    uint32_t first = queuedSequence(0);
    unsigned int flushes = txFlushes;
    for (unsigned int i = 0; i < 4U; ++i)
    {
        rxHead = 0; rxCount = 1;
        makeControlPacket(0, 100U, 0U);
        assert(Receive_Data() == RECEIVE_OK);
    }
    /* FIFO full does not flush data or advance the counter. */
    assert(ackCount == 3U && txFlushes == flushes);
    assert(queuedSequence(1) == first + 1U && queuedSequence(2) == first + 2U);
    ackCount = 0U; /* Simulate hardware consuming the queued ACKs. */
    rxHead = 0; rxCount = 1;
    makeControlPacket(0, 100U, 0U);
    assert(Receive_Data() == RECEIVE_OK && queuedSequence(0) == first + 3U);

    /* Failure while filling the next ACK must not publish controls or increment sequence. */
    ackCount = 0U; rxHead = 0; rxCount = 1;
    makeControlPacket(0, 500U, 0U);
    failAt = spiCalls + REMOTE_PACKET_SIZE + 10U; /* W_ACK_PAYLOAD command. */
    injectedError = HAL_TIMEOUT;
    assert(Receive_Data() == RECEIVE_RADIO_ERROR && remoteData.thr == 100U);
    failAt = 0U;
    assert(Receive_Init() == HAL_OK && queuedSequence(0) == first + 4U);

    regs[L01REG_FEATURE] = 0U;
    assert(L01_Check() == HAL_ERROR && !ce);
    puts("PASS: ACK preload, big-endian sequence, FIFO full/backpressure, SPI failure/recovery, feature readback");
}

int main(void)
{
    uint8_t data[32] = {0};
    reset(0, 0, 1);
    assert(L01_TransmitPacket(NULL, FIXED_PACKET_LEN, 100) == L01_TX_INVALID_PARAM);
    assert(L01_TransmitPacket(data, 0, 100) == L01_TX_INVALID_PARAM);
    assert(L01_TransmitPacket(data, 33, 100) == L01_TX_INVALID_PARAM);
#if DYNAMIC_PACKET == 0
    assert(L01_TransmitPacket(data, FIXED_PACKET_LEN - 1, 100) == L01_TX_INVALID_PARAM);
#endif
    assert(L01_TransmitPacket(data, FIXED_PACKET_LEN, 0) == L01_TX_INVALID_PARAM);
    assert(spiCalls == 0U);
    checkTransfer(1U << TX_DS, L01_TX_SUCCESS, 0, 1);
    checkTransfer(1U << MAX_RT, L01_TX_MAX_RETRY, 0, 1);
    checkTransfer((1U << TX_DS) | (1U << MAX_RT), L01_TX_MAX_RETRY, 0, 1);
    checkTransfer(0, L01_TX_TIMEOUT, 0, 1);
    checkTransfer(0, L01_TX_TIMEOUT, UINT32_MAX - 4U, 1);
    checkTransfer(1U << TX_DS, L01_TX_SUCCESS, UINT32_MAX - 4U, 0);
    reset(0, 0, 1);
    regs[L01REG_CONFIG] &= (uint8_t)(0xFFU ^ (1U << PWR_UP));
    assert(L01_SetPowerUp() == HAL_OK);
    assert(now >= 5U);
    checkSpiErrors();
    checkRxQueueAndRates();
    checkReceiver();
    checkAckSequence();
    puts("PASS: TX success/retry/timeout, SPI fault injection, init/recovery, atomic RX, shared deadline, tick wrap, CE timing, invalid arguments, RTOS/HAL waits");
    return 0;
}
