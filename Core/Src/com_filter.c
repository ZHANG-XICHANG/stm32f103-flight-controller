#include "com_filter.h"

#define ALPHA 0.15 /* 一階低通濾波器的加權係數 */

/*
 * @description: 一階低通濾波器
 * 用於濾除高頻噪聲，保留低頻信號。
 * @param {int16_t} newValue - 新測量值
 * @param {int16_t} preFilteredValue - 上一次濾波後的值
 * @return {int16_t} - 濾波後的值
 */
float Common_Filter_LowPass(float newValue, float preFilteredValue)
{
    return ALPHA * newValue + (1 - ALPHA) * preFilteredValue;
}

/*卡爾曼濾波參數*/
 Kalman_t kfs[3] = {
    {0.02f, 0.0f, 0.0f, 0.0f, 0.001f, 0.534f}, // X軸卡爾曼濾波器
    {0.02f, 0.0f, 0.0f, 0.0f, 0.001f, 0.534f}, // Y軸卡爾曼濾波器
    {0.02f, 0.0f, 0.0f, 0.0f, 0.001f, 0.534f}  // Z軸卡爾曼濾波器
};

double Common_Filter_Kalman(Kalman_t *kf, float input)
{
    // 預測更新
    kf->NowP = kf->LastP + kf->Q;

    // 計算卡爾曼增益
    kf->K = kf->NowP / (kf->NowP + kf->R);

    // 更新估計值
    kf->Output = kf->Output + kf->K * (input - kf->Output);

    // 更新估計誤差協方差
    kf->LastP = (1 - kf->K) * kf->NowP;

    return kf->Output;
}