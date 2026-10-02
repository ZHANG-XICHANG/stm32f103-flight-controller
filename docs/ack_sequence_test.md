# 最小 ACK Payload 測試

飛控：`Z:\Darcy\STM32\P01_flight_hal`

遙控器：`Z:\Darcy\STM32\F411_remote_hal`

## 封包與輸出

- 兩端 `DYNAMIC_PACKET = 1`，管道 0 啟用 `EN_DPL`、`EN_ACK_PAY`、`DPL_P0`。
- 原有控制封包內容仍為 17 bytes；頻道 40、2 Mbps、位址不變。
- ACK 只有 4 bytes，為大端序 `uint32_t` 序號，例如 `00 00 00 01` 代表 1。
- 飛控開機從 0 開始，每成功排入 ACK TX FIFO 一筆才遞增；模組恢復初始化不歸零。
- ACK 在收到指令前預載，代表回傳封包序號，不代表這次控制指令已執行。
- 遙控器由 USB Debug Queue 印出 `ACK_SEQ,<序號>\r\n`，不用開啟飛控的 DEBUG_LOG_ENABLE。

## 實機步驟

1. 兩個專案各執行 `cmake --build --preset Debug`，並將各自的 ELF 燒錄到對應板子。
2. 接上遙控器 USB，開啟對應的 USB CDC COM 埠（終端可設 115200）。
3. 飛控上電、遙控器持續傳送控制封包。保持未解鎖即可測試。
4. 在串口輸出中找 `ACK_SEQ,`，正常示例：

```text
ACK_SEQ,0
ACK_SEQ,1
ACK_SEQ,2
```

若終端開得較晚，第一筆可能大於 0。重送、空 ACK、排程或 USB 佇列丟棄日誌，
可能讓觀察到的序號重複或跳號；不能只靠 USB 行數計算 RF 丟包率。
飛控重新開機會從 0 開始；uint32_t 計滿後回到 0。

若只有傳送成功但沒有 `ACK_SEQ`，表示一般 ACK 成功尚不足以證明資料回傳成功。
確認兩端皆燒錄新版、遙控器 `DEBUG_PRINT_ENABLE = 1`，以及初始化讀回檢查成功。
`ACK_INVALID_LEN,n` 表示收到資料但長度不是預期的 4 bytes。

## 驗證範圍

已完成兩端韌體編譯，以及模擬 SPI 的 ACK 排隊／解析／故障測試。
尚未燒錄、驗證 RF 實際通訊或 USB 實際輸出。

設計參考：[RF24 ACK Payload 範例](https://nrf24.github.io/RF24/examples_2AcknowledgementPayloads_2AcknowledgementPayloads_8ino-example.html)。
