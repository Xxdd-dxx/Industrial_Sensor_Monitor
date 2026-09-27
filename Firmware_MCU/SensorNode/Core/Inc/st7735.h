#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

/* ============================================================
 * 硬件绑定 —— 与 CubeMX 配置、接线一一对应
 * ============================================================ */
#define LCD_SPI_HANDLE   hspi2        /* CubeMX 生成的 SPI2 句柄（spi.c） */

#define LCD_CS_PORT      GPIOB        /* PB12  片选           */
#define LCD_CS_PIN       GPIO_PIN_12
#define LCD_DC_PORT      GPIOB        /* PB11  数据/命令选择  */
#define LCD_DC_PIN       GPIO_PIN_11
#define LCD_RST_PORT     GPIOB        /* PB10  复位           */
#define LCD_RST_PIN      GPIO_PIN_10
#define LCD_BL_PORT      GPIOB        /* PB14  背光           */
#define LCD_BL_PIN       GPIO_PIN_14

#define LCD_CS_LOW()     HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_RESET)
#define LCD_CS_HIGH()    HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_SET)
#define LCD_DC_LOW()     HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_RESET)
#define LCD_DC_HIGH()    HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_SET)
#define LCD_RST_LOW()    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH()   HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)
#define LCD_BL_LOW()     HAL_GPIO_WritePin(LCD_BL_PORT,  LCD_BL_PIN,  GPIO_PIN_RESET)
#define LCD_BL_HIGH()    HAL_GPIO_WritePin(LCD_BL_PORT,  LCD_BL_PIN,  GPIO_PIN_SET)

/* ============================================================
 * 模块差异调校（不同批次 ST7735 参数不同，显示异常改这里）
 * ------------------------------------------------------------
 *  现象                | 改法
 *  --------------------|------------------------------------------
 *  颜色发白、像底片     | ST7735_INVON  1 -> 0
 *  红蓝对调            | ST7735_MADCTL_INIT  0x68 -> 0x60
 *  画面旋转 180 度     | ST7735_MADCTL_INIT  0x68 -> 0xA8
 *  边缘错位/黑边       | ST7735_XSTART/YSTART 试 (1,2) 或 (2,1)
 * ============================================================ */
#define ST7735_MADCTL_INIT   0x68    /* 横屏 + BGR */
#define ST7735_INVON         1       /* 1=像素反转（多数 1.8 寸红板需要） */
#define ST7735_XSTART        0
#define ST7735_YSTART        0

#define ST7735_WIDTH   160           /* 横屏分辨率；改竖屏(MADCTL 0xC8/0x00)
                                        时要连同 HEIGHT 一起换成 128/160 */
#define ST7735_HEIGHT  128

/* ============================================================
 * 颜色表 RGB565
 * ============================================================ */
#define LCD_RGB565(r,g,b)  ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

#define LCD_BLACK   LCD_RGB565(  0,  0,  0)
#define LCD_WHITE   LCD_RGB565(255,255,255)
#define LCD_RED     LCD_RGB565(255,  0,  0)
#define LCD_DRED    LCD_RGB565(120,  0,  0)
#define LCD_GREEN   LCD_RGB565(  0,255,  0)
#define LCD_DGREEN  LCD_RGB565(  0,120,  0)
#define LCD_BLUE    LCD_RGB565(  0,  0,255)
#define LCD_CYAN    LCD_RGB565(  0,255,255)
#define LCD_YELLOW  LCD_RGB565(255,255,  0)
#define LCD_ORANGE  LCD_RGB565(255,165,  0)
#define LCD_DGRAY   LCD_RGB565( 70, 70, 70)

/* ============================================================
 * API —— 只允许 LcdTask 调用（它是 SPI2 的唯一所有者）
 * 初始化内含 vTaskDelay，必须在任务上下文中调用
 * ============================================================ */
void ST7735_Init(void);
void ST7735_FillScreen(uint16_t color);
void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_DrawString(const char *s, uint16_t x, uint16_t y,
                       uint16_t fg, uint16_t bg, uint8_t scale);

#endif
