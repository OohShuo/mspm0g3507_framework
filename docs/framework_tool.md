# Framework 开发者工具

`scripts/framework.py` 提供面向开发者的诊断入口，用于检查构建 target、
主机工具、Flash Manager 开关一致性，以及链接 map 文件中的内存区域摘要。

## 命令

```bash
python3 scripts/framework.py doctor
python3 scripts/framework.py inspect
python3 scripts/framework.py size build/arm/framework.map
```

## 环境诊断

`doctor` 检查主机工具和高层配置是否一致。

当前检查项：

- `PATH` 中是否可以找到 `cmake`、`ninja` 和 `python3`
- `FLASH_MGR_ENABLE` 是否与 ARM target 开关一致
- 启用 Flash Manager 时，`FRAMEWORK_USE_LFS` 和 `FRAMEWORK_USE_UART` 是否同时打开

示例：

```text
[OK] cmake: /usr/bin/cmake
[OK] ninja: /usr/bin/ninja
[OK] python3: /usr/bin/python3
[ERR] flash-manager: FLASH_MGR_ENABLE requires FRAMEWORK_USE_UART
```

## Target 检视

`inspect` 打印从 `config/config.yaml` 转发给 CMake 的实际 `-D` 参数。
当某个 target 的构建行为与预期不一致时，先用它确认开关是否正确传入。

```text
arm (ARM, MinSizeRel, ninja)
  -DFRAMEWORK_USE_FREERTOS=ON
  -DFRAMEWORK_USE_LFS=ON
  -DFRAMEWORK_USE_UART=ON
```

## Map 摘要

`size` 读取链接器 `.map` 文件，打印内存区域的起始地址和长度。
第一版保持很小，只提供基础摘要；后续可以继续扩展 section 排名、
对象文件占用排行和 Flash/RAM 风险提示。
