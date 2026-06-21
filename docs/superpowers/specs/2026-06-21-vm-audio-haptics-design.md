# VM 声音与振动模拟设计

## 目标

VM 播放现有蜂鸣器音效，并把振动反馈显示为窗口边缘效果；连接 SDL 游戏手柄时同步产生物理震动。无音频设备或手柄时 VM 仍可正常运行。

## 音频

VM 直接编译现有 `buzzer_def.c`，不复制音效资源。新增纯 C 合成器按 48 kHz、单声道、S16 格式生成方波，支持音符时长、gate、音量和 glissando。`buzzer_vm.c` 用 SDL audio callback 拉取样本，并用 `SDL_LockAudioDevice` 保护播放、停止和音量状态；保持现有音效优先级与抢占语义。

音频子系统初始化失败时打印一次警告，所有 Buzzer API 保持可调用并静默降级。

## 振动

`vib_motor_vm.c` 实现 effect pattern、优先级、间隔和强度缩放，不再是空桩。工作线程只通过 SDL atomic 写入当前强度。

新增主线程模块 `haptics_vm.c/.h`：

- 自动打开第一个支持的 SDL GameController，处理热插拔。
- 将原子强度转换为 `SDL_GameControllerRumble`。
- 对外提供强度快照，`display_vm.c` 在 LCD 画面之外绘制橙色边缘光效和强度条。
- 无手柄时仅显示可视反馈。

## 生命周期与线程

- `Buzzer_Init` 独立初始化 SDL audio，失败不影响视频。
- `Vm_Haptics_Init/Handle_Event/Update/Deinit` 只由 SDL 主线程调用。
- VM HAL 的 5 ms 更新任务同时推进 Buzzer 与 Vib_motor 状态。
- 退出时先停止手柄，再销毁窗口并执行 `SDL_Quit`。

## 验证

- 纯合成器单测覆盖发声、静音 gate、音量和播放结束。
- 振动快照测试覆盖强度设置与清零。
- 源码契约测试确认真实 SFX 库、HAL 更新、主线程事件和显示叠层已接入。
- VM 全量构建；使用 `SDL_AUDIODRIVER=dummy` 做无声设备启动冒烟测试。
