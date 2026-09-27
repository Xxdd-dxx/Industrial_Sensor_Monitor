#include "app_tasks.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"   // 引入信号量头文件
#include "sht30.h"
#include "lcd_task.h"    /* ST7735 屏显任务 */
#include <limits.h>
#include "string.h"
#include "dma.h"
// 声明外部串口句柄
extern UART_HandleTypeDef huart1;

// =========================================================
// 1. 私有数据结构与变量定义
// =========================================================
typedef struct {
    float temperature;
    float humidity;
} SensorData_t;

static QueueHandle_t DataQueue;         // 传感器数据队列 (温湿度上报)
static QueueHandle_t UartRxQueue;       // 串口接收队列 (指令下发)
static SemaphoreHandle_t AlarmSemaphore;// 紧急报警二值信号量


static TaskHandle_t LedTaskHandle = NULL;

#define FRAME_MAX_LEN 16
typedef struct { uint8_t len; uint8_t buf[FRAME_MAX_LEN]; } TxFrame_t;
static QueueHandle_t TxQueue;

#define RX_DMA_BUF_SIZE 64
typedef struct { uint16_t len; uint8_t buf[RX_DMA_BUF_SIZE]; } RxFrame_t;
static uint8_t g_dma_buf[RX_DMA_BUF_SIZE];


#define FRAME_LEN  12
#define TYPE_DATA  0x04
#define TYPE_ALARM 0xFF

static uint8_t g_frame_seq = 0;


#define ALARM_HOLD_MS  2000

typedef enum { LED_OFF = 0, LED_ON = 1 } LedMsg_t;
static QueueHandle_t LedQueue;



// =========================================================
// 2. 内部任务与函数前置声明
// =========================================================
static void StartSensorTask(void *argument);
static void StartCommTask(void *argument);
static void StartCommandParseTask(void *argument);
static void StartAlarmTask(void *argument);
static  void StartLedTask(void *argument);
static uint16_t CalculateCRC16(uint8_t *data, uint16_t len);
static void StartUartTxTask(void *argument);
static void SendFrame(const uint8_t *p, uint8_t len);
static void BuildFrame(uint8_t type, const uint8_t payload[5], uint8_t out[12]);


// =========================================================
// 3. 公共接口：系统任务与外设中断初始化
// =========================================================
void AppTask_Init(void)
{
    // 1. 创建 RTOS 通信资源
    DataQueue = xQueueCreate(4, sizeof(SensorData_t));
    UartRxQueue = xQueueCreate(4, sizeof(RxFrame_t));
    AlarmSemaphore = xSemaphoreCreateCounting(8, 0);        // 创建二值信号量
		TxQueue = xQueueCreate(8, sizeof(TxFrame_t));
		LedQueue = xQueueCreate(8, sizeof(LedMsg_t));
    if (DataQueue != NULL && UartRxQueue != NULL && AlarmSemaphore != NULL&& TxQueue != NULL&& LedQueue != NULL) 
    {
        // 2. 创建系统任务 (注意优先级 Priority 分配，数字越大优先级越高)
        xTaskCreate(StartSensorTask,       "SensorTask", 128, NULL, 1, NULL); // P1: 日常采集 (最低)
        xTaskCreate(StartCommTask,         "CommTask",   128, NULL, 2, NULL); // P2: 数据上报
        xTaskCreate(StartCommandParseTask, "CmdTask",    128, NULL, 3, NULL); // P3: 指令解析 (较高)
        xTaskCreate(StartAlarmTask,        "AlarmTask",  128, NULL, 4, NULL); // P4: 紧急报警 (最高优先级抢占)
				xTaskCreate(StartLedTask,"LedTask",128,NULL,1,&LedTaskHandle);
				xTaskCreate(StartUartTxTask, "TxTask", 128, NULL, 5, NULL);

        Lcd_TaskInit();   /* 屏显任务：SPI2 的唯一所有者 */
    }
    else
    {
        configASSERT(0); // 资源创建失败，停机排错
    }
		HAL_UART_Receive_DMA(&huart1, g_dma_buf, RX_DMA_BUF_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);



}

// =========================================================
// 4. 中断回调服务 (ISR) - 桥接裸机中断与 RTOS 任务
// =========================================================

// (1) 外部中断回调 (按键 KEY0 触发)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // 确认是 PE4 (BTN_EMERGENCY) 被按下
    if (GPIO_Pin == BTN_EMERGENCY_Pin) 
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // 在中断中释放二值信号量，唤醒最高优先级的 AlarmTask
        xSemaphoreGiveFromISR(AlarmSemaphore, &xHigherPriorityTaskWoken);
        
        // 如果唤醒的任务优先级高于当前被中断的任务，立即进行上下文切换！
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


// =========================================================
// 5. RTOS 任务逻辑实现区
// =========================================================

// 【任务 1】：紧急报警任务 (最高优先级 P4，死等信号量)
static void StartAlarmTask(void *argument)
{
    for(;;)
    {
        xSemaphoreTake(AlarmSemaphore, portMAX_DELAY);

        LedMsg_t m = LED_ON;                        /* ★ 不再直接碰 GPIO */
        xQueueSend(LedQueue, &m, 0);

        uint32_t tick = xTaskGetTickCount();
        uint8_t payload[5] = {
            0x01,
            (uint8_t)(tick & 0xFF),        (uint8_t)((tick >> 8)  & 0xFF),
            (uint8_t)((tick >> 16) & 0xFF),(uint8_t)((tick >> 24) & 0xFF)
        };
        uint8_t frame[12];
        BuildFrame(TYPE_ALARM, payload, frame);
        SendFrame(frame, 12);
    }
}



// 【任务 2】：指令下发解析任务 (优先级 P3)
static void StartCommandParseTask(void *argument)
{		
		RxFrame_t frame;
		for (;;) {
				if (xQueueReceive(UartRxQueue, &frame, portMAX_DELAY) == pdTRUE) {
						for (uint16_t i = 0; i < frame.len; i++)
{
    if (frame.buf[i] == 0x01) {
        LedMsg_t m = LED_ON;   xQueueSend(LedQueue, &m, 0);   /* ★ */
    }
    else if (frame.buf[i] == 0x00) {
        LedMsg_t m = LED_OFF;  xQueueSend(LedQueue, &m, 0);   /* ★ */
    }
}

				}
		}
}

// 【任务 3】：日常传感器采集任务 (优先级 P1)
static void StartSensorTask(void *argument)
{
    SensorData_t sensor_data;
    for(;;)
    {
        if (SHT30_Read_TempHum(&sensor_data.temperature, &sensor_data.humidity) == 1) 
        {
            xQueueSend(DataQueue, &sensor_data, 0);
            Lcd_PostSensor(sensor_data.temperature, sensor_data.humidity);  /* 屏幕同步显示 */
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

// 【任务 4】：串口上报任务 (优先级 P2)
static void StartCommTask(void *argument)
{
    SensorData_t recv_data;
    for(;;)
    {
        if (xQueueReceive(DataQueue, &recv_data, portMAX_DELAY) == pdTRUE)
        {
            int16_t  send_temp = (int16_t)(recv_data.temperature * 100.0f);
            uint16_t send_hum  = (uint16_t)(recv_data.humidity   * 100.0f);

            uint8_t payload[5] = {
                (uint8_t)((send_temp >> 8) & 0xFF), (uint8_t)(send_temp & 0xFF),
                (uint8_t)((send_hum  >> 8) & 0xFF), (uint8_t)(send_hum  & 0xFF),
                0x00 };
            uint8_t frame[12];
            BuildFrame(TYPE_DATA, payload, frame);
            SendFrame(frame, 12);        // ← 只有这一行变了
        }
    }
}




static void StartUartTxTask(void *argument)
{
    TxFrame_t frame;
    for (;;) {
        if (xQueueReceive(TxQueue, &frame, portMAX_DELAY) == pdTRUE)
            HAL_UART_Transmit(&huart1, frame.buf, frame.len, 100);   /* 全局唯一 */
    }
}

static void SendFrame(const uint8_t *p, uint8_t len)
{
    TxFrame_t f;
    f.len = len;
    memcpy(f.buf, p, len);
    xQueueSend(TxQueue, &f, pdMS_TO_TICKS(10));
}

void App_UartIdleHandler(void)
{
    BaseType_t woken = pdFALSE;
    RxFrame_t  frame;
    uint16_t   received;

    /* 从 DMA 剩余计数算出本帧长度 */
    received = (uint16_t)(RX_DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx));

    if (received > 0 && received <= RX_DMA_BUF_SIZE)
    {
        frame.len = received;
        memcpy(frame.buf, g_dma_buf, received);
        xQueueSendFromISR(UartRxQueue, &frame, &woken);
    }

    /* 关键：整帧交给 HAL 重启。HAL 会重新配置 CNDTR/CPAR/CMAR，
       比手动 ENABLE 干净 —— 尤其禁用期间万一又来了字节，也能被正确接收 */
    if (HAL_UART_Receive_DMA(&huart1, g_dma_buf, RX_DMA_BUF_SIZE) != HAL_OK)
    {
        /* 极端情况重启失败，强制复位状态机再试一次 */
        HAL_UART_AbortReceive(&huart1);
        HAL_UART_Receive_DMA(&huart1, g_dma_buf, RX_DMA_BUF_SIZE);
    }

    portYIELD_FROM_ISR(woken);
}


static void BuildFrame(uint8_t type, const uint8_t payload[5], uint8_t out[12])
{
    out[0] = 0xAA;
    out[1] = 0x55;
    out[2] = type;
    taskENTER_CRITICAL();
out[3] = g_frame_seq++;
taskEXIT_CRITICAL();
    for (int i = 0; i < 5; i++) out[4 + i] = payload[i];

    uint16_t crc = CalculateCRC16(out, 9);      /* 校验 [0..8] 共 9 字节 */
    out[9]  = (uint8_t)((crc >> 8) & 0xFF);     /* 高字节在前，与 Qt 端一致 */
    out[10] = (uint8_t)(crc & 0xFF);
    out[11] = 0x5D;
}


static void StartLedTask(void *argument)
{
    LedMsg_t msg;
    uint8_t  led_on = 0;                          /* 灯当前状态，唯一的"真相" */

    for (;;)
    {
        /* ★ 条件超时：灯亮 → 等2s自动灭；灯灭 → 永久阻塞，绝不会空转 */
        if (xQueueReceive(LedQueue, &msg,
                led_on ? pdMS_TO_TICKS(ALARM_HOLD_MS) : portMAX_DELAY) == pdTRUE)
        {
            if (msg == LED_ON) {
                HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_RESET);
                led_on = 1;
                Lcd_PostAlarm(1);      /* LED 状态的每一次翻转都发布给屏幕 */
            } else {                                /* LED_OFF：远端强制关 */
                HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_SET);
                led_on = 0;
                Lcd_PostAlarm(0);
            }
        }
        else
        {
            /* 超时且当前亮着 → 自动熄灭 */
            HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_SET);
            led_on = 0;
            Lcd_PostAlarm(0);
        }
    }
}



// 辅助函数：CRC16 校验 (完全私有化)
static uint16_t CalculateCRC16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)data[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
