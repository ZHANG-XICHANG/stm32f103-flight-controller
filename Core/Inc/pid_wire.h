#ifndef PID_WIRE_H
#define PID_WIRE_H
#include <stdint.h>
#include <stddef.h>
#define PID_WIRE_SIZE 24U
/* Version 1. Gains are unsigned integers in units of 0.0001.
 * IDs: 0 roll angle, 1 pitch angle, 2 roll rate, 3 pitch rate, 4 yaw rate, 5 yaw angle.
 * Status: 0 applied, 1 invalid ID/range. PID request=1/result=2; axis request=3/result=4.
 * Axis command IDs 128..130 select X/Y/Z and require all gains zero.
 * operation is local metadata populated by Decode; never struct-cast on wire. */
typedef struct { uint32_t sequence; uint32_t gain[3]; uint8_t id, status, operation; } PidMessage;
static inline uint16_t PidCRC(const uint8_t *p, size_t n) {
    uint16_t crc=0xffffU;
    while(n--) { crc ^= (uint16_t)*p++ << 8;
        for(unsigned i=0;i<8;i++) crc=(uint16_t)((crc<<1)^((crc&0x8000U)?0x1021U:0U)); }
    return crc;
}
static inline void PidPut32(uint8_t *p,uint32_t v) {
    p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;
}
static inline uint32_t PidGet32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static inline void PidEncode(uint8_t *p,const PidMessage *m,uint8_t type) {
    p[0]=0x50;p[1]=0x49;p[2]=1;p[3]=type;PidPut32(p+4,m->sequence);
    p[8]=m->id;p[9]=m->status;
    for(unsigned i=0;i<3;i++) PidPut32(p+10+4*i,m->gain[i]);
    uint16_t crc=PidCRC(p,22);p[22]=(uint8_t)(crc>>8);p[23]=(uint8_t)crc;
}
static inline int PidDecode(const uint8_t *p,size_t n,PidMessage *m,uint8_t type) {
    if(!p || !m || n!=PID_WIRE_SIZE || p[0]!=0x50 || p[1]!=0x49 || p[2]!=1 || p[3]!=type) return 0;
    if(PidCRC(p,22)!=(((uint16_t)p[22]<<8)|p[23])) return 0;
    m->operation=type;
    m->sequence=PidGet32(p+4);m->id=p[8];m->status=p[9];
    for(unsigned i=0;i<3;i++) m->gain[i]=PidGet32(p+10+4*i);
    return 1;
}
static inline int PidValid(const PidMessage *m) {
    /* Transport limits, NOT guaranteed stable tuning ranges. */
    return m->id<6 && m->gain[0]<=200000U && m->gain[1]<=100000U && m->gain[2]<=20000U;
}
#endif
