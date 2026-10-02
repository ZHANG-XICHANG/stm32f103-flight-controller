# USB → 遙控器 → 飛控 RAM PID 調參

本次軸切換與 31-byte 遙測協定需要重新燒錄飛控及遙控器，並使用新版 Python 程式。
關閉獨立 serial_scope.py 或其他佔用同一 COM 埠的程式，再執行：

```powershell
python pid_tune.py
```

選 COM 埠並按 Connect，等待飛控完成校準、開始輸出 TEL。
選擇控制器、填入 Kp／Ki／Kd，再按 Apply PID。只有 Applied 表示收到飛控套用確認。
輸入框初始值不是從飛控讀回的參數，也不是已驗證適合機體的增益。

同一條串口連線同時處理命令回覆及五張即時圖：所選軸角度、目標與實際角速度、
角速度誤差、P／D／總輸出、四顆 ESC PWM。
Telemetry axis 選 X／Y／Z 後按 Switch telemetry axis，獨立於 PID 調參選單。
Actual telemetry axis 和圖表標題依實際收到的封包顯示；命令成功不會把舊封包改標成新軸。
切換清除舊軸曲線，但 CSV 持續寫入，末欄 axis 為 0=X、1=Y、2=Z。
軸切換只影響回傳資料，不改 PID 參數、不清積分，也不關閉任何控制軸；重開機回到 X。
可調整時間窗與各圖 Y 範圍；取消 Follow latest samples 後可使用工具列縮放及平移。
Start CSV recording 選擇新檔案開始記錄，既有檔案不會覆寫。
重複樣本仍寫入 CSV，但不重複畫點；斷線或關閉視窗會關閉 CSV 和串口。
曲線最多保留 4000 個不重複樣本，CSV 持續記錄。參數等待確認期間仍接收遙測。

依賴：`python -m pip install pyserial matplotlib`；Tkinter 通常隨 Windows Python 安裝。

可選控制器：

| 名稱 | 編號 | 控制器 |
|---|---:|---|
| roll | 0 | Roll 角度外環 |
| pitch | 1 | Pitch 角度外環 |
| gyro_x | 2 | Roll 角速度內環 |
| gyro_y | 3 | Pitch 角速度內環 |
| gyro_z | 4 | Yaw 角速度內環 |
| yaw | 5 | Yaw 角度外環參數（目前未接入控制計算） |

`yaw` 可以在線修改 `yaw_pid` 的 Kp／Ki／Kd，必須重新燒錄兩端支援 ID 5 的韌體。
目前飛控只執行 Z 軸角速度單環，沒有呼叫 yaw 角度外環；因此這組參數雖可套用，
尚不影響馬達反應。GUI 選擇 yaw 及套用成功時均會提示 inactive。
若要實際啟用航向保持，還需定義目標航向、角度跨越 ±180° 的誤差處理及外環接法。
此項更新不改變目前搖桿控制 Yaw 角速度的行為。

Kp 接受 0～20、Ki 接受 0～10、Kd 接受 0～2，最多四位小數。
這些是通訊驗證上限，並不表示範圍內的增益都能穩定飛行。
只改 RAM，重開機恢復 flight.c 預設值。沒有修改 D 濾波頻率或 Mixer。

USB 命令為 `PID,sequence,id,kp_x10000,ki_x10000,kd_x10000\n`。
序號為 uint32，每筆新命令必須使用新序號；工具隨機產生序號。
USB 回覆為 `PID_APPLIED/REJECTED/BUSY/TIMEOUT,sequence,id,status,kp_x10000,ki_x10000,kd_x10000`。
APPLIED 的 status 為 0；範圍或 ID 拒絕為 1。拒絕回覆的增益是請求值，並非目前值。
目前沒有 GET 命令。成功回覆的固定小數值，是飛控實際用來轉換 float 並套用的值。

獨立 24-byte 二進位命令：PI 幀頭、版本 1、類型、uint32 序號、ID、狀態、
三個 uint32 固定小數增益、CRC16-CCITT（初始 0xffff），多位元組均為大端序。
類型 1 為 PID SET，2 為 PID 結果；3 為軸切換，4 為軸切換結果。
軸切換命令 ID 為 128+axis，三個增益欄位必須為 0。17-byte 搖桿格式維持不變。
USB 軸命令是 `AXIS,sequence,axis\n`，axis 為 0／1／2；回覆為
`AXIS_APPLIED/REJECTED/BUSY/TIMEOUT,sequence,axis,status,0,0,0`。
軸命令與 PID 命令共用同一筆待確認位置、重試與去重流程，不可同時送兩筆命令。

遙控器一次只追蹤一筆命令，每 100 ms 至多插入一次命令發送，下一次發送必定留給
搖桿封包。2 秒未收到應用層確認則回報 TIMEOUT。USB 佇列滿時成功結果會等待重試排入。
飛控通訊任務只接收待套用資料，飛控任務在下一次 PID 計算前整組套用。
只有 Ki 改變時清除該控制器 integral，微分歷史保持；增益切換仍可能改變輸出。
無解鎖狀態限制，可在控制期間修改，尚未實作平滑增益切換。

飛控記住最後一筆命令序號，同序號重送不會再次套用或清除積分。
不支援多個電腦同時調參，也不可用相同序號送不同內容或重播較舊命令。
PID 命令不刷新飛控的搖桿失聯計時。結果 ACK 與遙測交錯，最多預排 8 次結果；
重送同序號會重新安排結果回覆。ACK 預載機制使結果需要後續無線封包帶回。

TIMEOUT 或 USB 中斷只表示沒有取得確認，不能推論飛控一定沒套用。
待連線恢復後可用新命令序號再次設定所需值。重新燒錄兩端後需實機驗證。

主機測試：`tests\pid_command\run.cmd`。
