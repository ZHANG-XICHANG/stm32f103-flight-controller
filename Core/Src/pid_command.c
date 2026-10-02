#include "pid_command.h"
#include "com_pid.h"
#include "FreeRTOS.h"
#include "task.h"
extern PID_Controller roll_pid,pitch_pid,gyro_x_pid,gyro_y_pid,gyro_z_pid,yaw_pid;
static PidMessage pending,result;
static uint8_t telemetry_axis; /* 0=X, 1=Y, 2=Z; reset to X on boot. */
uint8_t PidCommand_Axis(void) { return telemetry_axis; }
static uint8_t has_pending,has_result,result_budget;
void PidCommand_Receive(const uint8_t *p,uint8_t length) {
    PidMessage m;
    if((!PidDecode(p,length,&m,1) && !PidDecode(p,length,&m,3)) || m.status!=0) return;
    taskENTER_CRITICAL();
    /* One outstanding command; retransmissions never reset PID state again.
     * A sequence must never be reused for a different command. */
    if(has_result && result.sequence==m.sequence) result_budget=8;
    if(!has_pending && !(has_result && result.sequence==m.sequence)) {
        pending=m;has_pending=1;
    }
    taskEXIT_CRITICAL();
}
void PidCommand_Apply(void) {
    PidMessage m;
    taskENTER_CRITICAL();
    if(!has_pending) { taskEXIT_CRITICAL();return; }
    m=pending;has_pending=0;
    taskEXIT_CRITICAL();
    m.status=1;
    if(m.operation==3 && m.id>=128 && m.id<=130 &&
       m.gain[0]==0 && m.gain[1]==0 && m.gain[2]==0) {
        telemetry_axis=m.id-128;
        m.status=0;
    } else if(m.operation==1 && PidValid(&m)) {
        PID_Controller *controllers[]={&roll_pid,&pitch_pid,&gyro_x_pid,&gyro_y_pid,&gyro_z_pid,&yaw_pid};
        PID_Controller *pid=controllers[m.id];
        float ki=(float)m.gain[1]/10000.0f;
        if(pid->Ki!=ki) pid->integral=0.0f;
        pid->Kp=(float)m.gain[0]/10000.0f;
        pid->Ki=ki;pid->Kd=(float)m.gain[2]/10000.0f;
        m.status=0;
    }
    taskENTER_CRITICAL();result=m;has_result=1;result_budget=8;taskEXIT_CRITICAL();
}
uint8_t PidCommand_Result(uint8_t *p) {
    PidMessage m;uint8_t valid;
    taskENTER_CRITICAL();m=result;valid=has_result && result_budget;
    if(valid) --result_budget;
    taskEXIT_CRITICAL();
    if(valid) PidEncode(p,&m,(uint8_t)(m.operation+1));
    return valid;
}
