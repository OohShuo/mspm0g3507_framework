# Test — 测试子系统

测试模块位于 `src/test/`，每个外设/功能对应一个独立测试，通过 `config/test_config.h` 中的宏开关控制。

## 架构

```c
// test.h
void Test_Task_Def(void);  // 在 main() 中调用，按宏创建各测试任务
```

每个测试模块遵循统一模式：

- 头文件定义 `test_xxx.h` — 声明 `Test_Xxx_Init()` 和 `Test_Xxx_Loop()`
- 源文件 `test_xxx.c` — 实现初始化和循环逻辑
- `Test_Task_Def()` 按 `#if TEST_XXX_ENABLE` 创建 FreeRTOS 任务

## 测试列表

| 宏 | 测试目标 | 说明 |
| --- | --- | --- |
| `TEST_BUTTON_ENABLE` | 按键 | 轮询按键状态并打印 |
| `TEST_BUZZER_ENABLE` | 蜂鸣器 | 播放曲目库 |
| `TEST_COM_UART_ENABLE` | 串口通信 | 协议帧收发测试 |
| `TEST_JOYSTICK_ENABLE` | 摇杆 | ADC 坐标读取 |
| `TEST_LCD_ENABLE` | LCD | 基础显示测试 |
| `TEST_LED_BREATH_ENABLE` | 呼吸灯 | PWM 呼吸效果 |
| `TEST_LED_SIMPLE_ENABLE` | 简易 LED | 开关/闪烁测试 |
| `TEST_LFS_ENABLE` | LittleFS | 挂载/读写/格式化测试 |
| `TEST_LVGL_BALL_ENABLE` | LVGL 小球 | LVGL 交互 demo |
| `TEST_LVGL_HELLO_ENABLE` | LVGL Hello | LVGL 基础控件 demo |
| `TEST_RTT_ENABLE` | RTT 日志 | SEGGER RTT 输出测试 |
| `TEST_SLIP_RECV_ENABLE` | SLIP 接收 | 7D7E 帧解析测试 |
| `TEST_ST7789_IMG_ENABLE` | LCD 图像 | ST7789 位图显示测试 |
| `TEST_W25Q32_ENABLE` | Flash | W25Q32 读写/擦除测试 |

默认全部关闭（`0`），按需打开。
