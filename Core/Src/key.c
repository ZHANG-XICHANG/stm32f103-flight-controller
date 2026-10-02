// #include "key.h"

// #define KEY_DEBOUNCE_COUNT 3

// /*
//  * @brief 讀取按鍵釋放事件
//  *
//  * 使用 bit mask 表示按鍵事件。
//  * 按下與放開皆經過消抖，
//  * 有效按下後，再有效放開時觸發一次。
//  *
//  * @return uint8_t
//  */
// uint8_t Key_Read(void)
// {
//     static uint8_t left_count = 0;
//     static uint8_t right_count = 0;

//     static uint8_t left_release = 0;
//     static uint8_t right_release = 0;

//     static uint8_t left_pressed = 0;
//     static uint8_t right_pressed = 0;

//     uint8_t key = KEY_NONE;

//     /* ================= LEFT ================= */

//     if (HAL_GPIO_ReadPin(KEY_LEFT_GPIO_Port, KEY_LEFT_Pin) == GPIO_PIN_RESET)
//     {
//         /* 如果又偵測到按下，表示放開尚未穩定 */
//         left_release = 0;

//         if (!left_pressed)//1. 連續讀到低電位 3 次。
//         {
//             if (left_count < KEY_DEBOUNCE_COUNT)
//             {
//                 left_count++;
//             }

//             if (left_count >= KEY_DEBOUNCE_COUNT)
//             {
//                 left_pressed = 1;//2. 設定 left_pressed = 1，確認按鍵真的按下。
//             }
//         }
//     }
//     else//判斷是否放開
//     {
//         if (left_pressed)//3. 接著必須連續讀到高電位 3 次。
//         {
//             if (left_release < KEY_DEBOUNCE_COUNT)
//             {
//                 left_release++;
//             }

//             if (left_release >= KEY_DEBOUNCE_COUNT)
//             {
//                 /* 確認有效放開 */
//                 left_pressed = 0;
//                 left_count = 0;
//                 left_release = 0;

//                 key |= KEY_LEFT;//4. 設定 key |= KEY_LEFT，表示按鍵事件發生。
//             }
//         }
//         else
//         {
//             /* 尚未形成有效按下 */
//             left_count = 0;
//         }
//     }

//     /* ================= RIGHT ================= */

//     if (HAL_GPIO_ReadPin(KEY_RIGHT_GPIO_Port, KEY_RIGHT_Pin) == GPIO_PIN_RESET)
//     {
//         right_release = 0;

//         if (!right_pressed)
//         {
//             if (right_count < KEY_DEBOUNCE_COUNT)
//             {
//                 right_count++;
//             }

//             if (right_count >= KEY_DEBOUNCE_COUNT)
//             {
//                 right_pressed = 1;
//             }
//         }
//     }
//     else//判斷是否放開
//     {
//         if (right_pressed)
//         {
//             if (right_release < KEY_DEBOUNCE_COUNT)
//             {
//                 right_release++;
//             }

//             if (right_release >= KEY_DEBOUNCE_COUNT)
//             {
//                 /* 確認有效放開 */
//                 right_pressed = 0;
//                 right_count = 0;
//                 right_release = 0;

//                 key |= KEY_RIGHT;
//             }
//         }
//         else
//         {
//             right_count = 0;
//         }
//     }

//     return key;
// }