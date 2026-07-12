# Wiznet/W5500 分支重建设计

## 目标

将旧 `feat-wiznet` 分支的 Wiznet 功能重建到当前 `main` 之上。最终分支相对 `main` 只增加完整 Wiznet ioLibrary、W5500 HAL、UDP 通信封装和对应测试；配置体系、目录结构、BSP 以及所有其他功能均以 `main` 为准。

## 历史处理

旧分支包含多个 merge base，并混入大量早期项目结构和非 Wiznet 功能，不能直接保留其提交序列。实施时先为旧分支头创建备份引用，再从 `main` 构造线性历史，最后将 `feat-wiznet` 指向重建后的提交。

## 移植范围

完整保留旧分支的 `lib/wiznet/`，包括各型号以太网芯片驱动、通用 socket/wizchip 层、Application 示例和 Internet 协议实现。完整库作为第三方源码存在，但本项目 BSP 以上的产品集成只使用 W5500。

项目层只新增：

- `src/hal/w5500/`：连接当前 GPIO、硬件 SPI 和 FreeRTOS 临界区接口。
- `src/hal/com_udp/`：封装 W5500 静态网络配置、UDP socket、轮询收发和回调。
- `src/test/w5500_udp/`：按当前测试注册方式提供 W5500 UDP 测试入口。

不移植旧分支中的 `docs/wiznet_w5500.md`、BSP、SysConfig、RTT、LFS、UART、应用、脚本、资源及旧测试结构改动。

## 配置与构建

在当前 YAML 构建配置中增加 `FRAMEWORK_USE_WIZNET`，默认值为 `OFF`。该选项控制 Wiznet 库及依赖它的 W5500/UDP HAL 代码是否参与构建。

`lib/CMakeLists.txt` 在选项开启时加入完整 `lib/wiznet`。Wiznet CMake 配置固定 `_WIZCHIP_=W5500`，因为本项目的上层集成仅支持 W5500；库中其他芯片源码仍完整保留。

测试配置增加 `TEST_W5500_UDP_ENABLE`。启用该测试必须同时满足：

- `FRAMEWORK_USE_WIZNET=ON`
- `FRAMEWORK_USE_FREERTOS=ON`
- ARM 平台具备有效的 SPI、CS 和 RESET GPIO 配置

VM 默认保持 `FRAMEWORK_USE_WIZNET=OFF`，不为硬件网络设备引入虚假模拟实现。

## 初始化与数据流

硬件 SPI 和 GPIO 仍由当前 `main` 的 BSP 初始化。W5500 HAL 注册 Wiznet 库需要的片选、单字节/突发 SPI 和临界区回调。UDP 层负责一次性复位芯片、分配 socket 缓冲区、设置静态网络参数并创建 UDP socket。

应用或测试通过 `Com_Udp_Poll()` 驱动接收；收到数据后调用已注册回调。发送通过 `Com_Udp_Send()` 完成。HAL 不引入独立后台任务，以保持调度责任清晰。

## 错误处理

创建接口对空配置、重复 W5500 实例、未初始化子系统、无效缓冲区大小、内存分配失败和 socket 创建失败返回失败。发送接口拒绝空数据、超出缓冲区的数据以及未打开的 UDP socket。硬件 RESET 可通过无效 GPIO 索引明确禁用。

## 测试与验收

测试代码覆盖配置校验、实例创建和 UDP 轮询/发送入口；需要硬件的收发验证以显式测试项运行，不影响默认构建。

完成标准：

1. 配置生成流程能识别 `FRAMEWORK_USE_WIZNET` 和 `TEST_W5500_UDP_ENABLE`。
2. 默认 VM 配置继续构建，且不链接 Wiznet。
3. 开启 Wiznet 的 ARM 配置至少完成编译和链接验证；若本地工具链或生成文件不足，明确记录具体阻塞点。
4. `git diff main...feat-wiznet` 只包含本规格列出的文件和必要接入修改。
5. `feat-wiznet` 历史以当前 `main` 为祖先，不包含旧分支的无关提交。
