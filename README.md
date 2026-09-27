# Industrial Sensor Monitor

工业级温湿度实时监控系统 —— 一套完整的软硬协同数据采集与双向控制系统。

> 本项目面向嵌入式软件岗位，用于展示 STM32 裸机/RTOS、工业通信协议、上位机开发及 OTA 远程升级能力。

---

## 一、系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                         上位机 (Qt)                          │
│  串口通信 · 实时波形 · 阈值巡航 · 远端控制 · 报警弹窗          │
└──────────────────────────┬──────────────────────────────────┘
│  12 字节自定义帧协议 (CRC16)
│  USB 转串口 / UART 115200
┌──────────────────────────┴──────────────────────────────────┐
│                        下位机 (STM32F103ZET6)                  │
│  FreeRTOS 多任务 · DMA+IDLE 串口 · 双缓冲队列 · ST7735 屏显   │
│  SHT30 I2C 采集 · 报警任务抢占 · 按键/上位机双向控制 LED        │
└───────────────────────────────────────────────────────────────┘
```

---

## 二、硬件平台

| 模块 | 型号 | 接口 | 说明 |
|---|---|---|---|
| 主控 | STM32F103ZET6 | - | 72 MHz, 512 KB Flash, 64 KB RAM |
| 传感器 | SHT30 | I2C1 | 温湿度采集，精度 ±0.2℃ / ±2%RH |
| 通信模块 | ESP32-C3 | USART | Wi-Fi / 蓝牙，用于 OTA 与远程上报 |
| 显示屏 | ST7735 1.8" TFT | SPI2 | 128×160 彩屏本地显示 |
| 按键 | KEY0 (PE4) | EXTI4 | 紧急报警触发 |
| LED | LED_ALARM | GPIO | 报警/远端控制指示灯 |

### ST7735 接线

| 屏幕 | STM32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | PB13 (SPI2_SCK) |
| SDA | PB15 (SPI2_MOSI) |
| RST | PB10 |
| DC | PB11 |
| CS | PB12 |
| BLK | PB14 |

---

## 三、软件架构

### 下位机任务设计

| 任务 | 优先级 | 职责 |
|---|---|---|
| `UartTxTask` | P5 | 串口发送唯一所有者，全局仅此处调用 `HAL_UART_Transmit` |
| `AlarmTask` | P4 | 紧急报警，最高业务优先级 |
| `CmdTask` | P3 | 解析上位机指令（0x01 开灯 / 0x00 关灯） |
| `CommTask` | P2 | 温湿度组帧上报 |
| `SensorTask` | P1 | SHT30 定时采集 |
| `LcdTask` | P1 | ST7735 屏显唯一所有者 |
| `LedTask` | P1 | LED 计时管理唯一所有者 |

### 关键设计思想

- **共享资源单一所有者**：`huart1` 发送与 SPI2/屏幕分别由独立任务独占，避免并发冲突
- **队列解耦**：采集/上报、接收/解析互不阻塞
- **DMA + IDLE**：整帧接收，避免单字节中断高频打断 CPU
- **计数信号量**：报警事件不丢失，连按多次可排队处理

---

## 四、通信协议

12 字节自定义帧，高字节在前 CRC16：

```
AA 55 | TYPE | SEQ | PAYLOAD(5B) | CRC16(2B) | 5D
```

| TYPE | 含义 | 载荷说明 |
|---|---|---|
| 0x04 | 温湿度上报 | 温度 int16 ×100 (大端) + 湿度 uint16 ×100 (大端) + 保留 1B |
| 0xFF | 紧急报警 | 报警源 1B + 设备 Tick 时间戳 4B (小端) |
| 0xFE | 延迟诊断 | 最坏响应延迟 cycles 4B (小端) |

---

## 五、功能特性

- 每秒自动采集温湿度并上报上位机
- 上位机实时曲线显示与阈值设置
- 按键紧急报警，LED 亮 2 秒后自动熄灭
- 上位机可远程开关 LED/报警
- 下位机本地 TFT 彩屏同步显示温湿度与报警状态
- 12 字节统一帧 + SEQ 丢帧检测 + CRC16 校验
- 中断到任务的响应延迟可通过 DWT 实测

---

## 六、快速开始

### 下位机

1. 使用 STM32CubeMX 打开 `Firmware_MCU/SensorNode/SensorNode.ioc`
2. 确认 SPI2 配置为 **Transmit Only Master**，USART1 开启 RX DMA Circular
3. 使用 Keil MDK-ARM 打开 `Firmware_MCU/SensorNode/MDK-ARM/SensorNode.uvprojx`
4. 编译并下载到 STM32F103ZET6

### 上位机

1. 使用 Qt Creator 打开 `Software_PC/SensorMonitor/SensorMonitor.pro`
2. 选择 MinGW / MSVC 编译套件，构建运行
3. 连接串口，波特率 115200

---

## 七、目录结构

```
Industrial_Sensor_Monitor/
├── Firmware_MCU/
│   └── SensorNode/              # STM32 Keil 工程
│       ├── Core/
│       │   ├── Inc/
│       │   │   ├── app_tasks.h
│       │   │   ├── lcd_task.h
│       │   │   ├── st7735.h
│       │   │   └── ...
│       │   └── Src/
│       │       ├── app_tasks.c
│       │       ├── lcd_task.c
│       │       ├── st7735.c
│       │       ├── sht30.c
│       │       └── ...
│       └── SensorNode.ioc
├── Software_PC/
│   └── SensorMonitor/           # Qt 上位机
│       ├── mainwindow.cpp
│       ├── mainwindow.h
│       ├── serialworker.cpp
│       ├── serialworker.h
│       ├── mainwindow.ui
│       └── SensorMonitor.pro
├── README.md
└── LICENSE
```

---

## 八、版本历史

| 版本 | 日期 | 说明 |
|---|---|---|
| v1.0 | 2026.07 | 基础温湿度采集 + Qt 上位机 |
| v2.0 | 2026.09 | 重构为 FreeRTOS 多任务、12 字节统一帧、DMA+IDLE、ST7735 屏显 |

---

## 九、作者

- [Xxdd-dxx](https://github.com/Xxdd-dxx)

## 十、许可证

本项目基于 [MIT License](LICENSE) 开源。
