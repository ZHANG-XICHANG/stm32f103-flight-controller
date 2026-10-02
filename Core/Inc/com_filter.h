#ifndef COM_FILTER_H
#define COM_FILTER_H

//#include "stdint.h"
#include "com_debug.h"

/*卡爾曼濾波結構體*/
typedef struct
{
    float LastP;        //上一次的估計誤差協方差
    float NowP;         //當前的估計誤差協方差
    float Output;       //濾波器輸出值，即估計狀態
    float K;            //卡爾曼增益
    float Q;            //過程噪聲協方差
    float R;            //測量噪聲協方差

} Kalman_t;


extern Kalman_t kfs[3];

float Common_Filter_LowPass(float newValue, float preFilteredValue);

double Common_Filter_Kalman(Kalman_t *kf, float input);

#endif