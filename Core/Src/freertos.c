/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "com_debug.h"
#include "nRF24L01P.h"
#include "ESC.h"
#include "flight_state.h"
#include "receive.h"
#include "MPU6050.h"
#include "flight.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NRF24_TASK_PERIOD_MS  6U
#define NRF24_RETRY_PERIOD_MS 100U
#define FLIGHT_TASK_PERIOD_MS  6U
#define LED_TASK_PERIOD_MS  20U
#define LED_CONNECTED_TOGGLE_MS  500U
#define LED_DISCONNECTED_TOGGLE_MS  100U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */




/* USER CODE END Variables */
/* Definitions for myTask */
osThreadId_t myTaskHandle;
const osThreadAttr_t myTask_attributes = {
  .name = "myTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for myTask02 */
osThreadId_t myTask02Handle;
const osThreadAttr_t myTask02_attributes = {
  .name = "myTask02",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for myTask03 */
osThreadId_t myTask03Handle;
const osThreadAttr_t myTask03_attributes = {
  .name = "myTask03",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartTask(void *argument);
void StartTask02(void *argument);
void StartTask03(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  FlightState_Init();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of myTask */
  myTaskHandle = osThreadNew(StartTask, NULL, &myTask_attributes);

  /* creation of myTask02 */
  myTask02Handle = osThreadNew(StartTask02, NULL, &myTask02_attributes);

  /* creation of myTask03 */
  myTask03Handle = osThreadNew(StartTask03, NULL, &myTask03_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartTask */
/**
  * @brief  Function implementing the myTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTask */
void StartTask(void *argument)
{
  /* USER CODE BEGIN StartTask */
  //飛控任務
  //初始化四個 ESC
  ESC_InitAll();
  /* 初始化 MPU6050 的量程、取樣率、低通濾波器及時鐘源。 */
  MPU6050_Init();
  /*
   * 開機校準約需數秒；期間飛控必須保持水平靜止且 Z 軸朝上。
   * 校準函式會使用 vTaskDelay()，因此不會阻塞其他已就緒的任務。
   */
  HAL_StatusTypeDef calibrationStatus = MPU6050_Calibrate_offset();
  if (calibrationStatus != HAL_OK)
  {
    debug_printf(
        "MPU6050 calibration failed: status=%d, i2c_error=0x%08lX\r\n",
        (int)calibrationStatus,
        (unsigned long)HAL_I2C_GetError(&hi2c2)
    );
  }
  Flight_AttitudeInit(1000.0f / (float) FLIGHT_TASK_PERIOD_MS);
  /* Infinite loop */
  //獲取基準時間
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for(;;)
  {
    FlightState_Process();
    //1.獲取三軸角速度與加速度計算歐拉角
    Calculate_EulerAngle();
    //2.根據歐拉角計算PID輸出
    Calculate_PIDOutput();
    //3.依飛行狀態、油門及 PID 修正量更新四顆馬達
    Flight_control_motor();


    //6ms執行一次
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(FLIGHT_TASK_PERIOD_MS));
  }
  /* USER CODE END StartTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the myTask02 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
    //通訊任務
  uint8_t radioReady = 0U;
  TickType_t lastInitAttempt = xTaskGetTickCount() -
      pdMS_TO_TICKS(NRF24_RETRY_PERIOD_MS);
  /* Infinite loop */
    //獲取基準時間
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for(;;)
  {
    if (!radioReady &&
        (TickType_t)(xTaskGetTickCount() - lastInitAttempt) >=
            pdMS_TO_TICKS(NRF24_RETRY_PERIOD_MS))
    {
        radioReady = (Receive_Init() == HAL_OK);
        lastInitAttempt = xTaskGetTickCount();
        /* 初始化含上電等待，重新建立週期基準，避免補跑過期週期。 */
        xLastWakeTime = lastInitAttempt;
    }

    /* 每輪最多處理 FIFO 的三包，避免積壓舊搖桿值或無限佔用 CPU。 */
    for (uint8_t packet = 0U; radioReady && packet < 3U; ++packet)
    {
        ReceiveResult_t result = Receive_Data();
        if (result == RECEIVE_OK)
        {
            FlightState_RemoteReceived();
        }
        else if (result == RECEIVE_EMPTY)
        {
            break;
        }
        else if (result == RECEIVE_RADIO_ERROR)
        {
            L01_SetCE(CE_LOW);
            radioReady = 0U;
            lastInitAttempt = xTaskGetTickCount();
            break;
        }
        /* 格式或校驗錯誤時繼續讀下一包，不更新連線時間。 */
    }
    FlightState_Update();

    //6ms執行一次
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(NRF24_TASK_PERIOD_MS));
  }
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the myTask03 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void *argument)
{
  /* USER CODE BEGIN StartTask03 */
  /* LED 任務固定每 20 ms 執行一次，各 LED 使用自己的時間判斷。 */
  TickType_t xLastWakeTime = xTaskGetTickCount();
  TickType_t lastLed1ToggleTime = xLastWakeTime;
  LinkState_t previousLinkState = FlightState_GetLinkState();

  for (;;)
  {
    TickType_t now = xTaskGetTickCount();
    /* LED2：飛控未解鎖（IDLE）時長亮，其餘狀態熄滅。 */
    FlightState_t currentState = FlightState_GetFlightState();
    HAL_GPIO_WritePin(
        LED2_GPIO_Port,
        LED2_Pin,
        (currentState == IDLE) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* LED1：連線時每 500 ms 切換，失聯時每 100 ms 切換。 */
    LinkState_t currentLinkState = FlightState_GetLinkState();
    TickType_t led1TogglePeriod =
        (currentLinkState == LINK_CONNECTED)
            ? pdMS_TO_TICKS(LED_CONNECTED_TOGGLE_MS)
            : pdMS_TO_TICKS(LED_DISCONNECTED_TOGGLE_MS);

    /* 連線狀態改變時重新開始計時，避免沿用上一狀態的閃爍週期。 */
    if (currentLinkState != previousLinkState)
    {
        previousLinkState = currentLinkState;
        lastLed1ToggleTime = now;
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    }
    else if ((now - lastLed1ToggleTime) >= led1TogglePeriod)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        lastLed1ToggleTime = now;
    }

    /* 固定週期喚醒，讓 LED2 狀態最多延遲約 20 ms。 */
    vTaskDelayUntil(
        &xLastWakeTime,
        pdMS_TO_TICKS(LED_TASK_PERIOD_MS)
    );
  }
  /* USER CODE END StartTask03 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

