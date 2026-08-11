#include "app_tasks.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"   // 引入信号量头文件
#include "sht30.h"

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

static uint8_t rx_byte;                 // 串口中断单字节接收缓存

// =========================================================
// 2. 内部任务与函数前置声明
// =========================================================
static void StartSensorTask(void *argument);
static void StartCommTask(void *argument);
static void StartCommandParseTask(void *argument);
static void StartAlarmTask(void *argument);
static uint16_t CalculateCRC16(uint8_t *data, uint16_t len);

// =========================================================
// 3. 公共接口：系统任务与外设中断初始化
// =========================================================
void AppTask_Init(void)
{
    // 1. 创建 RTOS 通信资源
    DataQueue = xQueueCreate(4, sizeof(SensorData_t));
    UartRxQueue = xQueueCreate(20, sizeof(uint8_t)); // 缓存 20 个字节的接收指令
    AlarmSemaphore = xSemaphoreCreateBinary();       // 创建二值信号量

    // 严谨的系统健壮性检查
    if (DataQueue != NULL && UartRxQueue != NULL && AlarmSemaphore != NULL) 
    {
        // 2. 创建系统任务 (注意优先级 Priority 分配，数字越大优先级越高)
        xTaskCreate(StartSensorTask,       "SensorTask", 128, NULL, 1, NULL); // P1: 日常采集 (最低)
        xTaskCreate(StartCommTask,         "CommTask",   128, NULL, 2, NULL); // P2: 数据上报
        xTaskCreate(StartCommandParseTask, "CmdTask",    128, NULL, 3, NULL); // P3: 指令解析 (较高)
        xTaskCreate(StartAlarmTask,        "AlarmTask",  128, NULL, 4, NULL); // P4: 紧急报警 (最高优先级抢占)
    }
    else
    {
        configASSERT(0); // 资源创建失败，停机排错
    }

    // 3. 开启串口 1 的异步中断接收 (每次接收 1 个字节)
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
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

// (2) 串口接收完成中断回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) 
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // 将接收到的单个字节送入指令队列
        xQueueSendFromISR(UartRxQueue, &rx_byte, &xHigherPriorityTaskWoken);
        
        // 重新开启中断，等待下一个字节
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
        
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
        // 阻塞等待按键中断发来的信号量，平时不消耗任何 CPU
        if (xSemaphoreTake(AlarmSemaphore, portMAX_DELAY) == pdTRUE)
        {
            // 收到信号，立即点亮 LED0 (输出低电平亮)
            HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_RESET);
            
            // 组装一帧紧急报警帧发给 Qt 上位机
            uint8_t alarm_frame[10] = {0xAA, 0x55, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D};
            HAL_UART_Transmit(&huart1, alarm_frame, 10, 100);
            
            // 延时 2 秒后自动关闭 LED (输出高电平灭)
            vTaskDelay(pdMS_TO_TICKS(2000));
            HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_SET);
        }
    }
}

// 【任务 2】：指令下发解析任务 (优先级 P3)
static void StartCommandParseTask(void *argument)
{
    uint8_t recv_cmd;
    for(;;)
    {
        // 从串口 RX 队列中读取上位机发来的指令
        if (xQueueReceive(UartRxQueue, &recv_cmd, portMAX_DELAY) == pdTRUE)
        {
            // 制定极简的远端控制协议：
            // 收到 0x01 -> 远程开启 LED
            // 收到 0x00 -> 远程关闭 LED
            if (recv_cmd == 0x01) {
                HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_RESET);
            } 
            else if (recv_cmd == 0x00) {
                HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_SET);
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
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

// 【任务 4】：串口上报任务 (优先级 P2)
static void StartCommTask(void *argument)
{
    SensorData_t recv_data;
    uint8_t tx_buffer[10];
    for(;;)
    {
        if (xQueueReceive(DataQueue, &recv_data, portMAX_DELAY) == pdTRUE)
        {
            int16_t send_temp = (int16_t)(recv_data.temperature * 100.0f);
            uint16_t send_hum = (uint16_t)(recv_data.humidity * 100.0f);
            
            tx_buffer[0] = 0xAA;
            tx_buffer[1] = 0x55;
            tx_buffer[2] = 0x04;
            tx_buffer[3] = (uint8_t)((send_temp >> 8) & 0xFF);
            tx_buffer[4] = (uint8_t)(send_temp & 0xFF);
            tx_buffer[5] = (uint8_t)((send_hum >> 8) & 0xFF);
            tx_buffer[6] = (uint8_t)(send_hum & 0xFF);
            
            uint16_t crc_val = CalculateCRC16(tx_buffer, 7);
            tx_buffer[7] = (uint8_t)((crc_val >> 8) & 0xFF);
            tx_buffer[8] = (uint8_t)(crc_val & 0xFF);
            tx_buffer[9] = 0x5D;
            
            HAL_UART_Transmit(&huart1, tx_buffer, 10, 100);
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
