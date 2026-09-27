#ifndef LCD_TASK_H
#define LCD_TASK_H

#include <stdint.h>

/* 屏显任务对外只暴露三个接口：
 *   Lcd_TaskInit  —— 在 AppTask_Init（调度器启动前）调用一次
 *   Lcd_PostSensor —— SensorTask 每读到一组新数据调用
 *   Lcd_PostAlarm  —— 报警状态变化处调用（本项目里 = LedTask 状态翻转处）
 * LcdTask 是 SPI2 屏幕的唯一所有者，其他任务只发消息，绝不直接碰屏幕 */
void Lcd_TaskInit(void);
void Lcd_PostSensor(float t, float h);
void Lcd_PostAlarm(uint8_t on);

#endif
