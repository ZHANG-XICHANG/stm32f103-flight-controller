# 飛控架構圖（專題發表用）

**入口：[index.html](index.html)**（總覽加四個分頁，← / → 切換，`index.html#3` 可直接開第 3 張）。

依 commit `2040300` 的實際程式碼繪製。每個節點的 `sources` 都指向檔案與行號，在互動 HTML 中點選節點即可看到引用。

| # | 圖 | 互動 HTML | 簡報用（1920×1080 SVG / 3840×2160 PNG） |
|---|---|---|---|
| 1 | 整體系統架構 | [01-system-architecture.html](01-system-architecture.html) | `export/01-system-architecture.{svg,png}` |
| 2 | FreeRTOS 任務與資料流 | [02-freertos-dataflow.html](02-freertos-dataflow.html) | `export/02-freertos-dataflow.{svg,png}` |
| 3 | Roll / Pitch 串級 PID | [03-cascade-pid.html](03-cascade-pid.html) | `export/03-cascade-pid.{svg,png}` |
| 4 | nRF24L01P ACK Payload 時序 | [04-ack-payload-sequence.html](04-ack-payload-sequence.html)（完整） | `export/04a-…`、`export/04b-…`（拆成兩頁） |

時序圖完整版高度約 1250 px，縮進 16:9 頁面後字太小，所以簡報版從同一份資料拆成兩頁：
`04a` 為初始化與控制週期，`04b` 為 PID 調參與失聯保護。

## 主要程式碼依據

- 任務：`Core/Src/freertos.c` — myTask（High，6 ms）、myTask02（Normal，6 ms）、myTask03（Low，20 ms）
- 姿態：`Core/Src/flight.c:151-263` — LPF / Kalman → FusionRemap → FusionBias → FusionAhrsUpdateNoMagnetometer
- 串級 PID：`Core/Src/flight.c:270-301`、`Core/Src/com_pid.c`
- 混控與輸出：`Core/Src/flight.c:308-450`、`Core/Src/ESC.c`、`Core/Src/tim.c`（TIM4 CH1–4，PB6–PB9）
- 無線與 ACK：`Core/Src/receive.c`、`Core/Src/nRF24L01P.c`、`Core/Inc/nRF24L01P.h`
- 調參與遙測：`Core/Src/pid_command.c`、`Core/Src/telemetry.c`、`Core/Inc/pid_wire.h`、`Core/Inc/telemetry_wire.h`
- 失聯保護：`Core/Src/flight_state.c`、`Core/Inc/flight_state.h`（1000 ms）

## 刻意標示為「未確認 / 未實作」的部分

- 遙控器韌體（PTX 端）不在本 repo；圖中只依飛控端的解析格式描述它送出的封包。
- PC → 遙控器的 USB 指令格式取自 `pid_tune.py`；遙控器如何轉成 24 B 封包無法確認。
- `FIX_HEIGHT` 狀態目前只會停止馬達，沒有定高控制（`flight.c:439-441`）。
- `yaw_pid`（偏航角外環）已宣告，但未進入控制路徑。
- PWM 頻率 400 Hz 是由時脈設定推算（HSE 8 MHz × 9、PSC 71、ARR 2499），不是量測值。
- `docs/ack_telemetry.md` 描述的 10 B 遙測格式已過時，圖中以程式碼的 31 B 格式為準。

## 重新產生

1. 圖的內容在 `source/*.json`（Archify candidate）；用 Archify `finalize` 產生 HTML，並傳入 `--repo-root` 驗證引用。
   由於 Z: 是網路磁碟，請在本機目錄執行，完成後再複製回來。
2. 簡報版從專案根目錄執行：`node docs/architecture/tools/export-slides.mjs docs/architecture/tools/slides.json`
   （需要本機 Chrome 與 Node 18 以上；腳本會強制使用淺色主題並關閉動畫）。
