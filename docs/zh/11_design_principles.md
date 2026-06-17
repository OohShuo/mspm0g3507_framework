# 11 — 设计原则

## 核心理念

```
刚性分层 → 可预测变更
薄 BSP → 可移植硬件
对象 HAL → 可测试多实例驱动
描述符 APP → 无核心变更可扩展
编译期配置 → 零成本特性控制
VM 对等 → 快速迭代，保证代码复用
```

## 关键设计决策

### 为何 4 层（非 2、3 或 5 层）

- 2 层（APP+DriverLib）：VM 不可能，移植需重写一切
- 3 层（合并 HAL+BSP）：引脚映射与驱动逻辑耦合，换 MCU 需改所有 HAL
- 5 层（加平台抽象层）：更多间接调用，Cortex-M0+ 无法承受

4 层是满足 "VM 对等 + MCU 可移植" 的最小集合。详见 [../en/adr/architecture_decisions.md §1](../en/adr/architecture_decisions.md#1-layered-architecture)。

### 为何 APP 绝不能依赖 DriverLib

不是代码整洁问题——是 VM 存在的前提条件。APP include DriverLib → VM 编译失败 → 有人加 `#ifdef` → VM 不可靠。由编译器强制执行，不可协商。

**唯一例外**：`Bsp_Get_Tick_Ms()`（已知技术债务，应移到 syscall 层）。

### 为何需要 HAL（而非 APP 直接调 BSP）

- 多实例：两个 LED = 两个 HAL 对象，非两组全局变量
- 状态封装：按键消抖、蜂鸣器音符序列、摇杆校准
- 测试隔离：可单独创建按键对象测试消抖逻辑
- VM 互换：HAL 接口不变，ARM/VM 实现互换

### 为何选择 SDL2 VM（而非 QEMU/Renode）

- QEMU 无 MSPM0G3507 模型
- Renode 需写 C# 外设模型
- SDL2 VM 只需为 HAL/BSP 写桩（每个 20-50 行）
- 迭代数据：硬件 42s/周期 vs VM 19s/周期，约 3 倍迭代密度

详见 [../en/adr/architecture_decisions.md §5](../en/adr/architecture_decisions.md#5-vm-simulator)。

### 为何选择 FreeRTOS

成熟的 Cortex-M0+ 移植（处理无 CLZ 指令）。约 12KB Flash。heap_4 best-fit。广泛文档。对比 RTX（许可证）、Zephyr（太大）、裸机（需重新发明调度器）。[ADR §2](../en/adr/architecture_decisions.md#2-freertos)。

### 为何选择 LittleFS

写时复制 → 掉电不损坏。动态磨损均衡 → 无固定超块位置。静态 RAM → 528B 固定。对比 FatFS（无磨损均衡，FAT 损坏）、SPIFFS（不再维护，O(n) 目录扫描）。[ADR §3](../en/adr/architecture_decisions.md#3-littlefs)。

### 为何 LVGL 可选

游戏用直接 framebuffer 渲染（0 RAM 开销）。LVGL widget 系统适用于设置/文件管理器。同时存在两者，避免强制成本。[ADR §4](../en/adr/architecture_decisions.md#4-lvgl)。

## 禁止依赖

```
APP ──✕──► DriverLib    VM 对等
HAL ──✕──► DriverLib    BSP 为唯一消费者
BSP ──✕──► APP/HAL      层次反转
APP ──⚠──► BSP          容忍：仅 Bsp_Get_Tick_Ms()
```

## 贡献者必须遵守的规则

1. APP/HAL 永不 include DriverLib
2. BSP 函数 ≤30 行（薄封装）
3. 新模块必须有 VM 桩
4. 配置来自 config 文件，不硬编码
5. `#if MACRO` 不用 `#ifdef`（宏始终定义 0/1）
6. 命名：公开 `PascalCase`，静态 `snake_case`，宏 `UPPER_CASE`

## 已接受的取舍

| 取舍 | 接受原因 |
| --- | --- |
| 4 层间接调用 vs 直接写寄存器 | 实现 VM + MCU 可移植性 |
| 静态分配（无 Destroy） vs 动态生命周期 | 消除 use-after-free；MCU RAM 有限 |
| 编译期配置 vs 运行时配置 | 零 RAM/CPU 成本 |
| 软件 SPI（LCD） vs 硬件 SPI | 节省一个 SPI 外设给 W25Q32 |
| 直接 framebuffer vs LVGL | 节省约 30KB RAM |
| C-only vs C++ | 更小二进制，无 vtable/RTTI 开销 |
