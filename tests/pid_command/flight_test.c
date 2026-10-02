#include "pid_command.h"
#include "com_pid.h"
#include <assert.h>
#include <stdio.h>
PID_Controller roll_pid,pitch_pid,gyro_x_pid,gyro_y_pid,gyro_z_pid,yaw_pid;
int main(void) {
 uint8_t packet[24],reply[24];PidMessage m={0},out;
 m.sequence=0x12345678;m.id=4;m.gain[0]=25000;m.gain[1]=100;m.gain[2]=0;
 PidEncode(packet,&m,1);assert(packet[4]==0x12 && packet[7]==0x78);
 assert(PidDecode(packet,24,&out,1));assert(!PidDecode(packet,23,&out,1));
 for(unsigned i=0;i<24;i++){packet[i]^=1;assert(!PidDecode(packet,24,&out,1));packet[i]^=1;}
 gyro_z_pid.integral=123;gyro_z_pid.derivative=45;
 PidCommand_Receive(packet,24);assert(gyro_z_pid.Kp==0);PidCommand_Apply();
 assert(gyro_z_pid.Kp==2.5f && gyro_z_pid.Ki==0.01f && gyro_z_pid.integral==0);
 assert(gyro_z_pid.derivative==45);assert(PidCommand_Result(reply));
 assert(PidDecode(reply,24,&out,2) && out.status==0 && out.sequence==m.sequence);
 gyro_z_pid.integral=12;PidCommand_Receive(packet,24);PidCommand_Apply();assert(gyro_z_pid.integral==12);
 m.sequence++;m.gain[0]=30000;PidEncode(packet,&m,1);PidCommand_Receive(packet,24);PidCommand_Apply();
 assert(gyro_z_pid.Kp==3 && gyro_z_pid.integral==12);
 m.sequence++;m.gain[0]=200001;PidEncode(packet,&m,1);PidCommand_Receive(packet,24);PidCommand_Apply();
 assert(gyro_z_pid.Kp==3);assert(PidCommand_Result(reply));assert(PidDecode(reply,24,&out,2)&&out.status==1);
 for(unsigned i=0;i<10;i++) PidCommand_Result(reply);
 assert(!PidCommand_Result(reply));
 for(unsigned id=0;id<5;id++) {m.sequence++;m.id=(uint8_t)id;m.gain[0]=10000;PidEncode(packet,&m,1);PidCommand_Receive(packet,24);PidCommand_Apply();}
 assert(roll_pid.Kp==1 && pitch_pid.Kp==1 && gyro_x_pid.Kp==1 && gyro_y_pid.Kp==1 && gyro_z_pid.Kp==1);
 m.sequence++;m.id=5;m.gain[0]=30000;m.gain[1]=100;m.gain[2]=200;
 yaw_pid.integral=99;float inner_kp=gyro_z_pid.Kp;
 PidEncode(packet,&m,1);PidCommand_Receive(packet,24);assert(yaw_pid.Kp==0);
 PidCommand_Apply();assert(yaw_pid.Kp==3 && yaw_pid.Ki==0.01f && yaw_pid.Kd==0.02f);
 assert(yaw_pid.integral==0 && gyro_z_pid.Kp==inner_kp);
 assert(PidCommand_Result(reply)&&PidDecode(reply,24,&out,2)&&out.id==5&&out.status==0);
 yaw_pid.integral=7;PidCommand_Receive(packet,24);PidCommand_Apply();assert(yaw_pid.integral==7);
 m.sequence++;m.id=6;PidEncode(packet,&m,1);PidCommand_Receive(packet,24);PidCommand_Apply();
 assert(PidCommand_Result(reply)&&PidDecode(reply,24,&out,2)&&out.status==1);
 /* Axis commands use a distinct type and never modify gains/integral. */
 m.sequence++;m.id=129;m.gain[0]=m.gain[1]=m.gain[2]=0;
 gyro_y_pid.integral=42;
 PidEncode(packet,&m,3);PidCommand_Receive(packet,24);
 assert(PidCommand_Axis()==0);PidCommand_Apply();assert(PidCommand_Axis()==1);
 assert(gyro_y_pid.Kp==1 && gyro_y_pid.integral==42);
 assert(PidCommand_Result(reply) && PidDecode(reply,24,&out,4) && out.status==0);
 PidCommand_Receive(packet,24);PidCommand_Apply();assert(PidCommand_Axis()==1);
 m.sequence++;m.id=131;PidEncode(packet,&m,3);PidCommand_Receive(packet,24);PidCommand_Apply();
 assert(PidCommand_Axis()==1);assert(PidCommand_Result(reply)&&PidDecode(reply,24,&out,4)&&out.status==1);
 m.sequence++;m.id=130;PidEncode(packet,&m,1);PidCommand_Receive(packet,24);PidCommand_Apply();
 assert(PidCommand_Axis()==1);
 PidEncode(packet,&m,3);m.sequence++;PidEncode(packet,&m,3);PidCommand_Receive(packet,24);PidCommand_Apply();assert(PidCommand_Axis()==2);
 puts("PASS: CRC, ID routing, deferred apply, duplicate suppression, Ki reset, rejection, bounded ACK replies");
}
