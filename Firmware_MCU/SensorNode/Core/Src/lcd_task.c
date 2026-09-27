#include "lcd_task.h"
#include "st7735.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * 屏显任务：ST7735 (SPI2) 的唯一所有者
 *
 * 消息源（订阅者模式，别的任务只投消息，不碰屏幕）：
 *   SensorTask ──► Lcd_PostSensor ──┐
 *   LedTask    ──► Lcd_PostAlarm  ──┴─► LcdQueue ──► LcdTask ──► SPI2/屏幕
 *
 * 500ms 收不到消息就超时一次：报警激活时用来闪烁状态条（和 LedTask
 * 的"条件超时"同一个套路 —— 没事干时连 CPU 都不占）。
 * ========================================================================= */

typedef struct {
    uint8_t  type;
    uint8_t  alarm_on;      /* type = LCD_MSG_ALARM 时有效 */
    float    temp;
    float    hum;
} LcdMsg_t;

#define LCD_MSG_SENSOR   1
#define LCD_MSG_ALARM    2

#define LCD_QUEUE_LEN    8
#define LCD_TASK_STACK   512         /* 单位 word(4B) = 2KB：sprintf + 绘图 */
#define LCD_BLINK_MS     500

/* 界面布局（横屏 160x128；scale=2 的字符高 16px） */
#define TITLE_Y      6
#define LINE_Y       34
#define VALUE_X      12
#define TEMP_Y       44
#define HUM_Y        70
#define BAR_Y        104

static QueueHandle_t LcdQueue;
static uint8_t alarm_active;
static uint8_t alarm_blink;

static void StartLcdTask(void *argument);
static void DrawStaticUI(void);
static void DrawAlarmBar(void);
static void UpdateValues(float t, float h);
static void fmt_fixed(char *out, const char *prefix, const char *suffix, float v);

/* ---------------- 对外接口 ---------------- */

void Lcd_TaskInit(void)
{
    LcdQueue = xQueueCreate(LCD_QUEUE_LEN, sizeof(LcdMsg_t));
    configASSERT(LcdQueue != NULL);
    xTaskCreate(StartLcdTask, "LcdTask", LCD_TASK_STACK, NULL, 1, NULL);
}

void Lcd_PostSensor(float t, float h)
{
    if (!LcdQueue) return;
    LcdMsg_t m = {0};
    m.type = LCD_MSG_SENSOR;
    m.temp = t;
    m.hum  = h;
    xQueueSend(LcdQueue, &m, 0);
}

void Lcd_PostAlarm(uint8_t on)
{
    if (!LcdQueue) return;
    LcdMsg_t m = {0};
    m.type = LCD_MSG_ALARM;
    m.alarm_on = on;
    xQueueSend(LcdQueue, &m, 0);
}

/* ---------------- 任务主体 ---------------- */

static void StartLcdTask(void *argument)
{
    LcdMsg_t msg;
    (void)argument;

    ST7735_Init();      /* 初始化在本任务里做：里面有 vTaskDelay，
                           必须在任务上下文；也只有本任务碰 SPI2 */
    DrawStaticUI();

    for (;;)
    {
        if (xQueueReceive(LcdQueue, &msg, pdMS_TO_TICKS(LCD_BLINK_MS)) == pdTRUE)
        {
            switch (msg.type)
            {
            case LCD_MSG_SENSOR:
                UpdateValues(msg.temp, msg.hum);
                break;

            case LCD_MSG_ALARM:
                if (msg.alarm_on != alarm_active) {   /* 去重：状态没变不重画 */
                    alarm_active = msg.alarm_on;
                    alarm_blink  = 1;
                    DrawAlarmBar();
                }
                break;

            default:
                break;
            }
        }
        else if (alarm_active)      /* 500ms 超时 + 报警中 → 闪烁状态条 */
        {
            alarm_blink ^= 1;
            DrawAlarmBar();
        }
    }
}

/* ---------------- 界面绘制 ---------------- */

/* 浮点转 "前缀整数.一位小数后缀"，避开 printf 的 %f（省库体积），
   负温度也正确（-12.3 不会变成 -12.-3） */
static void fmt_fixed(char *out, const char *prefix, const char *suffix, float v)
{
    int v10 = (int)(v * 10.0f + (v >= 0 ? 0.5f : -0.5f));   /* 四舍五入到 0.1 */

    if (v10 < 0)
        sprintf(out, "%s-%d.%d%s", prefix, -v10 / 10, -v10 % 10, suffix);
    else
        sprintf(out, "%s%d.%d%s", prefix, v10 / 10, v10 % 10, suffix);
}

static void UpdateValues(float t, float h)
{
    char buf[16];

    /* 先铺黑底再写字：覆盖上一次的数字，不用全屏重画 */
    ST7735_FillRect(VALUE_X, TEMP_Y, ST7735_WIDTH - 2 * VALUE_X, 18, LCD_BLACK);
    fmt_fixed(buf, "T:", "C", t);
    ST7735_DrawString(buf, VALUE_X, TEMP_Y, LCD_RED, LCD_BLACK, 2);

    ST7735_FillRect(VALUE_X, HUM_Y, ST7735_WIDTH - 2 * VALUE_X, 18, LCD_BLACK);
    fmt_fixed(buf, "H:", "%", h);
    ST7735_DrawString(buf, VALUE_X, HUM_Y, LCD_CYAN, LCD_BLACK, 2);
}

static void DrawAlarmBar(void)
{
    const char *txt = alarm_active ? "ALARM ON!" : "ALARM OFF";
    uint16_t bg = alarm_active ? (alarm_blink ? LCD_RED : LCD_DRED)
                               : LCD_DGREEN;
    uint16_t tw = (uint16_t)(strlen(txt) * 8);      /* scale=1 字符宽 8 */

    ST7735_FillRect(0, BAR_Y, ST7735_WIDTH, ST7735_HEIGHT - BAR_Y, bg);
    ST7735_DrawString(txt, (ST7735_WIDTH - tw) / 2, BAR_Y + 8,
                      LCD_WHITE, bg, 1);
}

static void DrawStaticUI(void)
{
    const char *title = "SHT30 NODE";
    uint16_t tw = (uint16_t)(strlen(title) * 8);

    ST7735_FillScreen(LCD_BLACK);
    ST7735_DrawString(title, (ST7735_WIDTH - tw) / 2, TITLE_Y,
                      LCD_CYAN, LCD_BLACK, 1);
    ST7735_FillRect(0, LINE_Y, ST7735_WIDTH, 2, LCD_DGRAY);

    /* 占位符，1 秒内会被真实数据覆盖 */
    ST7735_DrawString("T:--.-C", VALUE_X, TEMP_Y, LCD_RED,  LCD_BLACK, 2);
    ST7735_DrawString("H:--.-%", VALUE_X, HUM_Y,  LCD_CYAN, LCD_BLACK, 2);

    alarm_active = 0;
    alarm_blink  = 1;
    DrawAlarmBar();
}
