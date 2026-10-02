# 可切換軸的 PID 遙測（31-byte ACK）

兩塊板子都需燒錄新版，舊版 10/20/22/30-byte 遙測不相容。
控制封包與 PID 參數不變。ACK 無有效姿態時仍可回傳 4-byte 序號。

無線使用 2 Mbps、5-byte 位址；31-byte ACK 必須將發射端的
`L01_INIT_RETR` 設為 `ARD_500US | ARC_5`（兩端驅動已同步）。
原本 250 µs 僅支援最多 15-byte ACK，可能在啟動時的 `TEL_SEQ`（4 bytes）
切換成 `TEL`（31 bytes）後收不到完整 ACK，造成遙控器顯示斷線、飛控仍收到控制。
這是無線 ACK 時序限制，不是序號到 1998 的上限；修改後需重新燒錄，尤其是遙控器。

USB 格式（整數縮放值）：

```text
TEL,sequence,time_ms,angle,rate_target,rate,rate_error,p_term,d_term,pid_output,esc1_pwm,esc2_pwm,esc3_pwm,esc4_pwm,axis
```

| 欄位 | 二進位大小 | USB 整數換算成實際值 |
|---|---:|---|
| sequence | 4 | 飛控樣本序號 |
| time_ms | 4 | HAL_GetTick()，同次 PID 更新後的時間 |
| angle | 2 | 除以 100，deg |
| rate_target / rate / rate_error | 各 2 | 除以 10，deg/s |
| p_term / d_term / pid_output | 各 2 | 不縮放，PWM 修正量單位 |
| esc1_pwm / esc2_pwm / esc3_pwm / esc4_pwm | 各 2 | uint16，實際寫入的脈寬 µs |
| axis | 1 | uint8：0=X、1=Y、2=Z；位於二進位 offset 30 |

多位元組數值為大端序；時間與序號為 uint32，ESC 脈寬為 uint16，axis 為 uint8，其餘為 int16，超出範圍會限幅。
P 項是 Kp × 當次角速度誤差；D 項是 Kd × 濾波後微分值。
每次馬達更新後發布同一輪的 PID 與四顆 PWM。PID 總輸出是混控前修正量；
PWM 來自 motor1.pulse～motor4.pulse，已經混控與限幅，包含停機狀態的指令。
這是控制指令，不是 ESC 回報的實際轉速。封包共 31 bytes，尚餘 1 byte 未使用。
開機預設 X 軸；GUI 的 Telemetry axis 可在線切換，四顆 PWM 不受選軸影響。
每包資料自帶 axis；切換時清除舊軸曲線，CSV 保留所有樣本及數字軸編號。
角度分別是 Roll／Pitch／Yaw，角速度及 P／D／總輸出取對應的角速度內環。

```powershell
python serial_scope.py --port COM3 --csv telemetry_new.csv
```

將 COM3 改為遙控器 USB 埠。CSV 檔必須是新檔名，不覆寫舊紀錄。
繪圖與 CSV 自動還原角度／角速度單位，時間軸使用飛控相對取樣時間。
五張圖依序為：所選軸姿態角、目標與實際角速度、角速度誤差、P／D 項與 PID 總輸出、四馬達 PWM。
Yaw 角度可能超出預設 ±30°，可將角度圖範圍調成 -180～180。
可用 --angle-range、--gyro-range、--error-range、--output-range、--pwm-range 設定範圍。
重複序號仍記錄在 CSV，但不重複畫點；時間回繞會展開，重新開機則清除舊曲線。
序號缺口不能直接當成無線丟包率。舊版 TEL 文字列會被忽略。
