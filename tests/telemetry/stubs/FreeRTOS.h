#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
/* Host test is single-threaded; firmware uses the real RTOS critical section. */
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif
