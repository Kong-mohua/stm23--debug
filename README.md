# 循迹小车 —— 2026 DEBUG 实验室软件组考核(大部分代码是ai写的)

STM32F103C8T6 循迹小车（CubeMX + Keil MDK-ARM + HAL 库）。

## 硬件

| 器件 | 型号 / 说明 |
|---|---|
| 主控 | STM32F103C8T6 最小系统板（Blue Pill，72MHz 目标频率） |
| 显示 | 0.96" OLED，SSD1306，128×64，I2C，地址 0x3C |
| 按键 | 4 个轻触开关（内部上拉，按下为低电平） |
| LED | 2 个（1 个板载 + 1 个外接），低电平点亮 |
| 调试 | ST-Link V2（SWD） |

## 引脚分配

| 引脚 | 功能 | 说明 |
|---|---|---|
| PC13 | LED1 | 板载 LED，低电平点亮 |
| PB5  | LED2 | 外接 LED，低电平点亮 |
| PB12 | KEY1 | 菜单：上移 |
| PB13 | KEY2 | 菜单：下移 |
| PB14 | KEY3 | 菜单：进入子菜单 |
| PB15 | KEY4 | 菜单：返回主菜单 |
| PB6  | I2C1_SCL | OLED 时钟线 |
| PB7  | I2C1_SDA | OLED 数据线 |

> 预留（后续巡线用）：PA0–PA7 巡线传感器 ADC，PB0/PB1 电机 PWM（TIM3 CH3/CH4），PA9/PA10 串口。

## 目录结构

```
Core/Inc, Core/Src   应用代码
  main.c             主程序：按键扫描 + 菜单状态机
  oled.c / oled.h    SSD1306 驱动（帧缓冲 + 刷新 + ASCII/中文显示）
  oledfont.c/.h      ASCII 8x16 字库（列主序）
  cnfont.c/.h        中文 16x16 字库（列主序）
Drivers/             STM32 HAL 库 + CMSIS
MDK-ARM/             Keil 工程文件（.uvprojx）
diangdeng.ioc        CubeMX 配置
```

## 已实现功能

- [x] LED 点亮（GPIO 输出）
- [x] 按键输入 + 内部上拉 + 软件消抖（非阻塞、按下降沿触发）
- [x] 4 按键扫描驱动 `key_scan()`，返回位掩码
- [x] OLED（SSD1306，I2C 400kHz）驱动：帧缓冲、整屏刷新、ASCII+中文混合显示
- [x] 主菜单显示 + 按键上下移动箭头（菜单状态机 `menu_index`）
- [ ] 子菜单：LED 控制（常亮/常灭、交替闪烁）
- [ ] 串口通信（上位机修改变量、读取变量）
- [ ] 巡线传感器 + 循迹
- [ ] 特殊元素处理（断路/圆环/十字/直角/折线）

## 编译与烧录

1. 用 STM32CubeMX 打开 `diangdeng.ioc`，如需改配置后点 GENERATE CODE
2. 用 Keil MDK-ARM 打开 `MDK-ARM/diangdeng.uvprojx`
3. F7 编译，F8 通过 ST-Link 烧录

输出文件：`MDK-ARM/diangdeng/diangdeng.hex`

## 字库说明

OLED 的显存是**列主序**的（每字节 = 8 个竖直像素，bit0 在顶部），
字库必须按列存储，否则画面会转置错乱。中文字库按 Unicode 码点排序后二分查找。
