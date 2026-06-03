# MSP430G3507 Framework

一个用于 MSP430G3507 微控制器的嵌入式框架，采用分层架构设计，集成了 FreeRTOS 实时操作系统和 TI DriverLib。

## 项目概述

本项目是一个专为 TI MSP430G3507（Arm Cortex-M0+）微控制器优化的嵌入式开发框架，提供了完整的系统级支持，包括硬件抽象层、板级支持包和应用层接口。

## 项目组织结构

```
framework/
├── cmake/                      # CMake构建系统配置
│   ├── toolchain.cmake        # ARM交叉编译工具链配置
│   ├── utils.cmake            # CMake辅助函数和宏
│   └── tools.cmake            # 工具链配置和检查
│
├── src/                       # 源代码主目录
│   ├── main.c                # 系统入口点
│   ├── app/                  # 应用层
│   │   ├── app.c/h           # 应用程序主体
│   │   └── CMakeLists.txt
│   ├── bsp/                  # 板级支持包 (Board Support Package)
│   │   ├── bsp.c/h           # BSP核心接口
│   │   ├── gpio/             # GPIO驱动模块
│   │   ├── time/             # 定时器模块
│   │   └── CMakeLists.txt
│   ├── hal/                  # 硬件抽象层 (Hardware Abstraction Layer)
│   │   ├── hal.c/h           # HAL核心接口
│   │   ├── led_simple/       # LED驱动模块
│   │   └── CMakeLists.txt
│   ├── lib/                  # 通用库
│   │   ├── vector/           # 向量/数据结构库
│   │   └── CMakeLists.txt
│   └── syscfg/               # 系统配置自动生成文件
│       ├── ti_msp_dl_config.c/h  # TI配置文件
│       └── device.opt            # 设备选项配置
│
├── ti_device/               # TI设备支持库
│   ├── cmsis/               # CMSIS核心接口
│   │   └── Core/            # CMSIS-CORE标准库
│   ├── freertos/            # FreeRTOS 实时操作系统
│   │   ├── tasks.c
│   │   ├── queue.c
│   │   ├── timers.c
│   │   ├── event_groups.c
│   │   ├── stream_buffer.c
│   │   ├── portable/        # 平台适配层
│   │   └── include/         # FreeRTOS头文件
│   └── ti/                  # TI厂商库
│       ├── devices/         # 设备定义
│       └── driverlib/       # 驱动库
│
├── config/                  # 配置文件目录
│   ├── framework.syscfg     # SysConfig项目配置文件
│   ├── board_config.h       # 板级配置头文件
│   └── FreeRTOSConfig.h     # FreeRTOS配置文件
│
├── tools/                   # TI工具链和资源
│   ├── ti/
│   │   ├── clockTree/       # 时钟树配置工具
│   │   ├── devices/         # 设备信息
│   │   ├── driverlib/       # DriverLib元数据
│   │   └── tinyusb_meta/    # USB支持（可选）
│   └── product.json         # 工具产品配置
│
├── scripts/                 # 构建和辅助脚本
│   ├── cm.bash             # 主编译脚本
│   ├── clear.bash          # 清理构建输出
│   ├── gen_syscfg_files.bash       # 生成SysConfig文件
│   ├── install_sysconfig.bash      # 安装SysConfig
│   └── open_syscfg_gui.bash        # 打开SysConfig GUI
│
├── CMakeLists.txt          # 主CMake配置文件
├── .clang-format           # 代码格式化配置
└── .clangd                 # Clangd语言服务器配置
```

## 架构分层关系

```
┌─────────────────────────┐
│   Application Layer     │  应用层
│   (src/app/)            │  - app.c/h
└────────────┬────────────┘
             │ 依赖
┌────────────▼────────────┐
│   Library Layer         │  库层
│   (src/lib/)            │  - vector (数据结构)
└────────────┬────────────┘
             │ 依赖 (2个分支)
      ┌──────┴──────┐
      │             │
┌─────▼──────┐ ┌───▼──────┐
│   HAL      │ │   BSP    │  硬件抽象层 & 板支持包
│ (src/hal/) │ │(src/bsp/)│  - HAL: led_simple 等驱动抽象
│            │ │          │  - BSP: gpio, time 等板级接口
└─────┬──────┘ └───┬──────┘
      │            │
      └──────┬─────┘
             │ 依赖
┌────────────▼────────────┐
│   Device Layer          │  设备支持层
│   (ti_device/)          │  - CMSIS (标准接口)
│                         │  - FreeRTOS (RTOS)
│   - ti/driverlib        │  - TI DriverLib (厂商库)
└─────────────────────────┘
```

### 各层职责

| 层级 | 目录 | 职责 |
|------|------|------|
| **应用层** | `src/app/` | 业务逻辑实现，调用BSP/HAL接口 |
| **库层** | `src/lib/` | 通用数据结构、工具函数 |
| **HAL** | `src/hal/` | 硬件驱动抽象，屏蔽硬件细节 |
| **BSP** | `src/bsp/` | 板级支持，初始化、IO配置 |
| **设备层** | `ti_device/` | 芯片级支持、RTOS、DriverLib |

## 编译要求

### 必需工具

| 工具 | 版本 | 用途 |
|------|------|------|
| **CMake** | >= 3.15 | 构建系统 |
| **ARM GCC** | arm-none-eabi | ARM交叉编译工具链（已包含在项目中） |
| **arm-none-eabi-objcopy** | 随工具链 | 生成hex/bin固件文件 |
| **arm-none-eabi-size** | 随工具链 | 检查程序大小 |

### 可选工具

| 工具 | 用途 |
|------|------|
| **TI SysConfig** | GUI配置系统参数、时钟树、外设 |
| **Git** | 版本控制 |
| **Ninja** | 构建系统（CMake的替代后端） |

### 系统要求

- **操作系统**: Linux / macOS / Windows (with MSYS2 or WSL)
- **C编译器**: GCC (用于native编译工具)
- **内存**: >= 512MB (用于编译)

## 编译指南

### 1. 基本编译流程

```bash
# 进入项目目录
cd /path/to/framework

# 创建构建目录
mkdir -p build
cd build

# 配置构建（使用CMake）
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain.cmake ..

# 编译
make -j4

# 输出文件
# - framework.elf      : ELF可执行文件
# - framework.hex      : Intel HEX格式固件
# - framework.bin      : 二进制固件
# - framework.map      : 链接地图文件
```

### 2. 使用编译脚本

项目提供了便利的编译脚本：

```bash
# 完整编译流程（使用 cm.bash 脚本）
./scripts/cm.bash

# 清理构建输出
./scripts/clear.bash
```

### 3. 配置编译选项

编辑 `CMakeLists.txt` 中的编译选项：

```cmake
# 编译优化级别
-O0              # 无优化（调试用）
-O2              # 优化（生产用）

# CPU目标
-mcpu=cortex-m0plus
-march=armv6-m
-mthumb          # Thumb模式
-mfloat-abi=soft # 软浮点
```

### 4. 系统配置生成

如需修改系统配置（时钟、外设等）：

```bash
# 打开SysConfig GUI进行配置
./scripts/open_syscfg_gui.bash

# 或手动生成SysConfig文件
./scripts/gen_syscfg_files.bash
```

## 编译目标详解

### ELF文件 (`framework.elf`)
- 完整的可执行文件，包含调试符号
- 用于调试器（如GDB）加载
- 文件最大，不能直接烧写到MCU

### HEX文件 (`framework.hex`)
- Intel HEX格式固件
- 易于阅读和传输，常用于烧写工具
- 大多数编程器支持此格式

### BIN文件 (`framework.bin`)
- 二进制原始格式
- 最小的固件文件
- 用于某些高级烧写工具

### MAP文件 (`framework.map`)
- 链接地图，显示符号地址和大小信息
- 用于内存使用分析和调试

## 工具链配置

### ARM交叉编译工具链

工具链位置：`tools/gcc-arm-none-eabi/`

工具链配置在 `cmake/toolchain.cmake` 中指定：

```cmake
set(TOOLCHAIN_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/gcc-arm-none-eabi/bin")

# 编译器
CMAKE_C_COMPILER       = arm-none-eabi-gcc
CMAKE_ASM_COMPILER     = arm-none-eabi-gcc
CMAKE_CXX_COMPILER     = arm-none-eabi-g++

# 工具
CMAKE_OBJCOPY          = arm-none-eabi-objcopy  (生成hex/bin)
CMAKE_SIZE             = arm-none-eabi-size    (检查大小)
```

### 链接配置

主要链接参数：

| 参数 | 说明 |
|------|------|
| `-T${TI_LINKER_SCRIPTS}` | 链接脚本（定义内存布局） |
| `-Wl,-Map,framework.map` | 生成链接地图文件 |
| `--specs=nano.specs` | 使用newlib-nano库（节省空间） |
| `-Wl,--gc-sections` | 删除未使用的段 |
| `-nostartfiles` | 不使用标准启动文件 |

## 依赖关系

### 库依赖链

```
framework.elf
├── app (应用层库)
│   ├── hal (硬件抽象层)
│   │   └── ti_device
│   ├── bsp (板级支持)
│   │   └── ti_device
│   └── lib (通用库)
├── hal
│   └── ti_device
├── bsp
│   └── ti_device
└── lib
```

### TI Device层组件

- **CMSIS**: ARM Cortex Microcontroller Software Interface Standard
- **FreeRTOS**: 实时操作系统内核
- **DriverLib**: TI官方驱动库（GPIO、UART、SPI等）

## 构建输出结构

```
build/
├── framework.elf           # 主可执行文件
├── framework.hex           # HEX格式固件
├── framework.bin           # 二进制固件
├── framework.map           # 链接地图
├── CMakeFiles/             # CMake中间文件
├── cmake_install.cmake
└── Makefile                # 生成的Makefile
```

## 常见编译命令

```bash
# 完整重新编译
cmake --build build --clean-first

# 增量编译
cmake --build build

# 并行编译（4个线程）
cmake --build build -- -j4

# 详细输出
cmake --build build -- VERBOSE=1

# 只编译特定目标
cmake --build build --target app
cmake --build build --target hal
cmake --build build --target bsp
```

## 编译输出示例

```
[100%] Linking C executable framework.elf
arm-none-eabi-size framework.elf
   text    data     bss     dec     hex filename
   8192     256    1024    9472    2500 framework.elf
[100%] Built target framework.elf
Generating hex/bin files and checking size for framework.elf
```

## 故障排除

| 问题 | 解决方案 |
|------|---------|
| 找不到编译器 | 确认工具链路径：`tools/gcc-arm-none-eabi/bin/` 存在 |
| CMake版本过低 | 升级CMake至3.15+：`cmake --version` |
| 链接错误 | 检查 `framework.map`，确认符号定义 |
| 内存溢出 | 检查编译输出中的 `bss`、`data` 段大小 |
| SysConfig文件更新 | 运行 `gen_syscfg_files.bash` 重新生成 |

## 代码风格

项目使用 **Clang Format** 保持一致的代码风格：

```bash
# 格式化所有源文件
clang-format -i src/**/*.c src/**/*.h

# 检查格式
clang-format --dry-run src/**/*.c
```

格式化配置：`.clang-format`

## 开发工作流

### 1. 添加新驱动

1. 在 `src/hal/` 下创建驱动目录
2. 在 `src/hal/CMakeLists.txt` 中添加新文件
3. 在 `hal.h` 中暴露驱动接口
4. 应用层通过 `hal.h` 调用

### 2. 修改系统配置

1. 使用 `open_syscfg_gui.bash` 打开SysConfig
2. 修改配置（时钟、外设等）
3. 保存后运行 `gen_syscfg_files.bash`
4. 重新编译

### 3. 集成第三方库

1. 将库源码放入 `src/lib/` 或 `ti_device/`
2. 创建 `CMakeLists.txt` 并配置编译选项
3. 在对应上层库中通过 `target_link_libraries` 链接

## 参考资源

- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [ARM GCC Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- [TI MSP430G3507 Datasheet](https://www.ti.com/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/161204_FreeRTOS_Reference_Manual_v9.0.0.pdf)
- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/General/html/index.html)

---

**最后更新**: 2026年6月3日
