#include "sht30.h"
#include "FreeRTOS.h"
#include "task.h"

// 内部辅助函数：SHT30 官方 CRC-8 校验算法
static uint8_t SHT30_CheckCRC(uint8_t *data, uint8_t len, uint8_t checksum)
{
    uint8_t crc = 0xFF; // SHT30 初始值为 0xFF
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31; // SHT30 多项式 0x31
            } else {
                crc = (crc << 1);
            }
        }
    }
    return (crc == checksum);
}

/**
 * @brief  读取 SHT30 温湿度数据 (工业级高可靠版本)
 * @param  temp: 指向存储温度变量的指针
 * @param  hum:  指向存储湿度变量的指针
 * @retval 1: 读取成功且校验通过; 0: 读取或校验失败
 */
uint8_t SHT30_Read_TempHum(float *temp, float *hum)
{
    // 0x2C, 0x06 是一条标准的 SHT30 测量指令 (高重复性，开启时钟拉伸)
    uint8_t cmd[2] = {0x2C, 0x06};
    uint8_t rx_data[6];
    uint16_t raw_temp, raw_hum;

    // 1. 下发测量指令 (超时时间设为 100ms 避免总线锁死)
    if (HAL_I2C_Master_Transmit(&hi2c1, SHT30_I2C_ADDR, cmd, 2, 100) != HAL_OK) {
        return 0; // 通信失败
    }

    // 2. 使用 FreeRTOS 的非阻塞延时等待测量完成，期间让出 CPU
    vTaskDelay(pdMS_TO_TICKS(20));

    // 3. 读取 6 个字节的数据
    // 格式: [Temp MSB] [Temp LSB] [Temp CRC] [Hum MSB] [Hum LSB] [Hum CRC]
    if (HAL_I2C_Master_Receive(&hi2c1, SHT30_I2C_ADDR, rx_data, 6, 100) != HAL_OK) {
        return 0; // 接收失败
    }

    // 4. 严谨的硬件级 CRC 校验，有效剔除 I2C 总线受到干扰时的错乱数据
    if (!SHT30_CheckCRC(&rx_data[0], 2, rx_data[2]) || 
        !SHT30_CheckCRC(&rx_data[3], 2, rx_data[5])) {
        return 0; // 校验失败，坚决抛弃该帧脏数据
    }

    // 5. 提取原始值
    raw_temp = (rx_data[0] << 8) | rx_data[1];
    raw_hum = (rx_data[3] << 8) | rx_data[4];

    // 6. 代入官方公式计算真实值
    *temp = -45.0f + (175.0f * (float)raw_temp / 65535.0f);
    *hum = 100.0f * (float)raw_hum / 65535.0f;

    return 1; // 成功
}

