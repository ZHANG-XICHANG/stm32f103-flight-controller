#ifndef PID_COMMAND_H
#define PID_COMMAND_H
#include "pid_wire.h"
void PidCommand_Receive(const uint8_t *p,uint8_t length);
void PidCommand_Apply(void);
uint8_t PidCommand_Axis(void);
uint8_t PidCommand_Result(uint8_t *p);
#endif
