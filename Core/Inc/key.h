#ifndef KEY_H
#define KEY_H

#include "main.h"

#define KEY_NONE   0x00
#define KEY_LEFT   0x01   // 0000 0001
#define KEY_RIGHT  0x02   // 0000 0010

/*
*@brief 讀取按鍵狀態
*bit mask（位元遮罩 / 位元旗標）
*用一個整數裡面的不同 bit，分別代表不同按鍵是否被按下
*@return uint8_t 按鍵狀態
*/
uint8_t Key_Read(void);

#endif // KEY_H