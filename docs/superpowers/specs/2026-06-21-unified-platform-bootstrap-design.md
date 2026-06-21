# 统一平台启动架构设计

## 目标

ARM 和 VM 构建统一使用 `src/main.c`。公共入口负责应用启动顺序，平台专用静态库实现相同的平台接口，并在链接时选择对应的底层实现。

根目录 `CMakeLists.txt` 不再分别维护大段 ARM 和 VM 构建逻辑。平台组合逻辑分别归入 `src/platform` 和 `src/vm`。

## 目录结构

```text
src/platform/
├── CMakeLists.txt
├── platform.h
├── platform_arm.c
└── platform_vm.c

src/vm/
├── CMakeLists.txt
├── display_vm.c
├── input_vm.c
├── haptics_vm.c
├── hal/
├── bsp/
└── freertos/
```

其他现有 VM 实现文件继续保留在 `src/vm`。以上目录树用于说明模块归属，并非完整文件列表。

将 `src/vm/main_vm.c` 中的平台生命周期逻辑迁移至 `src/platform/platform_vm.c` 后，删除 `main_vm.c`。

## 平台接口

`src/platform/platform.h` 提供公共入口所需的完整平台生命周期接口：

```c
int Platform_Init(void);
int Platform_Start(void);
```

`Platform_Init()` 成功时返回零，平台启动失败时返回非零值。该返回值主要用于报告 SDL 初始化失败；ARM 实现正常情况下返回零。

`Platform_Start()` 启动平台任务并接管平台事件循环。在 ARM 上，该函数正常情况下不会返回；在 VM 上，用户关闭模拟器且清理完成后返回零。

## 公共主函数流程

`src/main.c` 不再包含 TI DriverLib、SDL 或调度器专用调用。统一启动流程如下：

```c
int main(void) {
    if (Platform_Init() != 0) {
        return 1;
    }

    Syscall_Init();
    Local_Lib_Init();
    Bsp_Init();
    Hal_Init();
    App_Init();

    Hal_Task_Def();
#if TEST_ANY_ENABLE
    Test_Task_Def();
#endif
    App_Task_Def();

    return Platform_Start();
}
```

两个平台由此共享相同的初始化顺序、任务定义顺序以及入口错误处理方式。

## ARM 实现

`platform_arm.c` 接管当前 `main.c` 中的 ARM 专用代码：

- 根据配置规范化调试器触发的复位；
- 调用 `SYSCFG_DL_init()`；
- 使能 DMA 中断；
- 在 `Platform_Start()` 中调用 `vTaskStartScheduler()`。

如果 `vTaskStartScheduler()` 意外返回，`Platform_Start()` 返回非零错误值，而不是静默继续执行。

## VM 实现

`platform_vm.c` 接管当前 `main_vm.c` 中的平台生命周期逻辑：

- 安装进程信号处理函数；
- 初始化 SDL、显示、输入和振动反馈；
- 启动已经登记的 VM 任务；
- 轮询 SDL 事件；
- 更新输入和振动反馈；
- 渲染显示画面；
- 退出时释放振动反馈、显示和 SDL 资源。

显示、输入、声音、振动反馈、VM HAL、VM BSP 和 FreeRTOS 兼容层等设备实现继续放在 `vm` 静态库中，由 `src/vm/CMakeLists.txt` 管理。

## VM 任务启动语义

当前 VM 的 `xTaskCreate()` 会立即创建 pthread，而 ARM FreeRTOS 只会在调度器启动后执行任务。这一差异可能导致 VM 任务在所有初始化和任务定义完成前提前运行。

VM 兼容层调整为：

1. 在 `Platform_Start()` 之前，`xTaskCreate()` 只登记任务描述，不启动 pthread；
2. `Platform_Start()` 开始时，启动所有已经登记的任务；
3. 任务系统启动后，新创建的任务立即启动，以保留 FreeRTOS 动态创建任务的行为。

这样两个平台具有相同的初始任务启动边界。

## CMake 目标模型

`src/vm/CMakeLists.txt` 创建 `vm` 静态库，且不把 `main_vm.c` 加入该目标。

`src/platform/CMakeLists.txt` 创建：

- 提供 `platform.h` 的平台头文件接口目标；
- 由 `platform_arm.c` 构建的 ARM 平台实现目标；
- 由 `platform_vm.c` 构建的 VM 平台实现目标；
- 统一且仅选择一个实现的 `platform` 接口目标。

所选平台接口的概念链接关系如下：

```text
VM：
platform
└── platform_vm
    └── vm + 公共库

ARM：
platform
└── platform_arm
    └── hal + bsp + syscall + freertos + ti + 公共库
```

HAL、BSP、syscall、FreeRTOS、配置和平台头文件分别通过专用 `INTERFACE` 目标发布。使用方通过 CMake 目标的 usage requirements 获得声明，不再重复手工维护 include 目录列表。

根构建文件只保留必须在 `project()` 之前完成的少量平台设置，例如选择 ARM 工具链和启用的语言。之后添加公共子目录，使用 `src/main.c` 构建可执行文件，并链接 `platform`。

## 依赖规则

- `app` 仅依赖平台无关的 HAL/BSP 头文件契约，不依赖 VM 专用头文件；
- `platform_vm` 可以调用公开的 VM 生命周期函数，但不得向 `main.c` 暴露 SDL 细节；
- `vm` 不得反向链接 `app`，最终可执行文件负责组合 `app` 与所选平台；
- ARM 和 VM 实现库不能同时链接到同一个最终可执行文件，因为两者有意提供相同符号；
- 聚合接口目标不得形成 `vm -> interface -> vm` 链接环。

## 错误处理与关闭流程

- VM 初始化失败时，在返回错误前按逆序释放已经初始化的 SDL 子系统；
- ARM 完成板级初始化后报告成功；不可恢复的板级错误继续使用项目现有断言策略；
- VM 关闭时，先停止平台循环，再释放显示和振动反馈资源；
- 公共 `main.c` 将任何 `Platform_Init()` 失败转换为进程退出码 `1`。

## 验证标准

满足以下条件时，迁移完成：

- ARM 和 VM 均编译同一个 `src/main.c`；
- `src/vm/main_vm.c` 已删除；
- ARM 构建只链接 ARM 平台实现；
- VM 构建只链接 VM 平台实现；
- VM 任务不会在 `Platform_Start()` 前执行；
- ARM 和 VM 构建均成功，且没有新增警告；
- VM 能正常打开、接收输入、渲染、产生声音和振动反馈，并能干净退出；
- ARM 二进制保持现有初始化和调度器行为。

## 非目标

- 重构与平台选择无关的应用、游戏、HAL 或 BSP 行为；
- 替换 VM 的 FreeRTOS 兼容 API；
- 增加运行时平台选择；
- 在同一个可执行文件中支持多个平台实现。
