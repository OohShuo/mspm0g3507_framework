# Config — 配置系统

项目配置分为两层：构建时配置（CMake / config.yaml）和编译时配置（C 头文件）。

## config.yaml

`config/config.yaml` — 构建级特性开关和参数：

```yaml
build:
  build_type: Debug          # Debug | Release | RelWithDebInfo | MinSizeRel
  generator: ninja           # ninja | make | auto
  graphviz: ON               # ON | OFF — 生成 framework.dot 依赖图

  FRAMEWORK_USE_FREERTOS: ON
  FRAMEWORK_USE_RTT: ON
  FRAMEWORK_USE_LVGL: OFF
  FRAMEWORK_USE_LFS: ON
  FRAMEWORK_USE_WIZNET: ON
```

CMake 读取 `config.yaml` 生成对应的 C 宏定义（如 `FRAMEWORK_USE_LFS`），源码中通过 `#if` 条件编译控制代码参与。

## board_config.h

`config/board_config.h` — 板级外设引脚映射。所有 BSP/HAL 代码通过 `*_IDX` 宏引用外设索引，换板时只需修改此文件。

## test_config.h

`config/test_config.h` — 测试模块开关。每个测试模块一个宏（`TEST_*_ENABLE`），`Test_Task_Def()` 中按宏创建对应测试任务。

## FreeRTOSConfig.h

`config/FreeRTOSConfig.h` — FreeRTOS 内核配置（时钟频率、堆大小、任务数上限等）。

## lvgl_config.h

`config/lvgl_config.h` — LVGL 库配置（分辨率、色深、内存池、字体等），由 `lib/lvgl/lv_conf.h` 引用。

## lfs_config.h

`config/lfs_config.h` — LittleFS 配置（块大小、缓存大小等）。

## SEGGER_RTT_Conf.h

`config/SEGGER_RTT_Conf.h` — RTT 通道配置（缓冲区大小等）。

## SysConfig

`config/framework.syscfg` — TI SysConfig 项目文件，GUI 配置时钟树和外设引脚复用，构建时自动生成 `src/syscfg/ti_msp_dl_config.c/h`。
