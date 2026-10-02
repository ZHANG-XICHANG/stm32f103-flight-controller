#include "pid_command.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
void *DebugQueueHandle=(void*)1;
static uint32_t now;
static unsigned sent;
static uint8_t last[24];
static char logline[128];
uint32_t HAL_GetTick(void){return now;}
uint8_t L01_TransmitPacket(uint8_t *p,uint8_t n,uint32_t t){(void)t;assert(n==24);memcpy(last,p,24);sent++;return 0x20;}
int osMessageQueuePut(void *q,const void *p,uint8_t pri,uint32_t t){(void)q;(void)pri;(void)t;memcpy(logline,p,128);return 0;}
static void input(const char *s){PidCommand_USB((const uint8_t*)s,(uint32_t)strlen(s));PidCommand_Poll();}
int main(void){uint8_t status,ack[24];PidMessage m;
 input("PID,123,4,25000,");assert(!PidCommand_Send(&status));input("100,0\n");
 assert(PidCommand_Send(&status)&&sent==1);assert(!PidCommand_Send(&status));
 now=100;assert(PidCommand_Send(&status));assert(!PidCommand_Send(&status));
 assert(PidDecode(last,24,&m,1));m.sequence++;PidEncode(ack,&m,2);PidCommand_Ack(ack,24);
 now=200;assert(PidCommand_Send(&status));assert(!PidCommand_Send(&status));
 m.sequence--;PidEncode(ack,&m,2);PidCommand_Ack(ack,24);PidCommand_Poll();
 assert(strstr(logline,"PID_APPLIED,123,4,0,25000,100,0"));
 now=300;assert(!PidCommand_Send(&status));
 input("PID,124,4,25000,0,0\nPID,125,4,25000,0,0\n");assert(strstr(logline,"PID_BUSY,125"));
 now=2300;PidCommand_Poll();assert(strstr(logline,"PID_TIMEOUT,124"));
 input("PID,126,4,25000,0,0x\n");assert(!PidCommand_Send(&status));
 input("PID,127,9,0,0,0\n");assert(strstr(logline,"PID_REJECTED,127"));
 uint8_t flood[300];memset(flood,'x',sizeof(flood));PidCommand_USB(flood,sizeof(flood));PidCommand_Poll();
 input("PID,128,4,25000,0,0\n");assert(!PidCommand_Send(&status));
 input("PID,129,4,25000,0,0\n");assert(PidCommand_Send(&status));
 assert(PidDecode(last,24,&m,1));PidEncode(ack,&m,2);PidCommand_Ack(ack,24);PidCommand_Poll();
 input("AXIS,130,1\n");assert(!PidCommand_Send(&status));assert(PidCommand_Send(&status));
 assert(PidDecode(last,24,&m,3) && m.id==129 && m.gain[0]==0);
 PidEncode(ack,&m,2);PidCommand_Ack(ack,24); /* Wrong result type must be ignored. */
 now+=100;assert(!PidCommand_Send(&status));assert(PidCommand_Send(&status));
 PidEncode(ack,&m,4);PidCommand_Ack(ack,24);PidCommand_Poll();
 assert(strstr(logline,"AXIS_APPLIED,130,1,0,0,0,0"));
 input("PID,140,5,30000,100,200\n");
 assert(!PidCommand_Send(&status));assert(PidCommand_Send(&status));
 assert(PidDecode(last,24,&m,1)&&m.id==5&&m.gain[0]==30000);
 PidEncode(ack,&m,2);PidCommand_Ack(ack,24);PidCommand_Poll();
 assert(strstr(logline,"PID_APPLIED,140,5,0,30000,100,200"));
 input("PID,141,6,30000,100,200\n");assert(strstr(logline,"PID_REJECTED,141"));
 puts("PASS: fragmented USB, strict parsing, retry spacing, control slots, stale ACK, busy, timeout, overflow recovery");
}
