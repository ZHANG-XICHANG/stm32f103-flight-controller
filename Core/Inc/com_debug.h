#ifndef COM_DEBUG_H
#define COM_DEBUG_H

#include <stdio.h>

// printf() 底層字元輸出函數，由 _write() 呼叫並重定向至 UART
int __io_putchar(int ch);

//日誌輸出開關控制宏，定義為1表示開啟日誌輸出，定義為0表示關閉日誌輸出
#define DEBUG_LOG_ENABLE 0

//使用宏定義的方式實現打印日誌之前，在日誌前面加上文件名和行號，方便調試
#if DEBUG_LOG_ENABLE


#define debug_printf(format, ...)                               \
    do {                                                        \
        printf("[%s:%d] " format,                               \
               __FILE__, __LINE__, ##__VA_ARGS__);             \
    } while (0)

#else

#define debug_printf(format, ...) do { } while (0)

#endif

#endif /* COM_DEBUG_H */