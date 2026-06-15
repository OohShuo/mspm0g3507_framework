# 蜂鸣器音乐模块

## 当前方案

项目已经使用 `TIMA0 CC3` 在 `PA12` 输出硬件 PWM，并由
`src/hal/buzzer/` 中的音序器播放单声道方波音乐。

- CPU 主频：80 MHz
- 蜂鸣器 PWM 时钟：4 MHz
- 音符调度周期：5 ms
- 输出形式：单声道方波、休止符、断奏、音量占空比和滑音
- 播放轨道：背景音乐一轨、可抢占的游戏音效一轨

4 MHz 的定时器时钟已经足以准确覆盖常见旋律音域。PWM 波形由定时器
自动产生，CPU 不需要按照音频采样率持续翻转 GPIO。

## 蜂鸣器选择与接线

必须使用无源蜂鸣器。带固定振荡器的有源蜂鸣器只能发出固定音高，
不能正常播放旋律。

小型低电流无源压电蜂鸣器可以用于首次验证：

| 蜂鸣器端 | 开发板 |
| --- | --- |
| `+` / `S` | `PA12` |
| `-` / `GND` | `GND` |

若使用三针无源蜂鸣器模块：

| 模块端 | 开发板 |
| --- | --- |
| `S` | `PA12` |
| `VCC` | 按模块规格接 `3.3V` |
| `GND` | `GND` |

磁式蜂鸣器、小喇叭或工作电流不明确的模块不要直接由 `PA12` 驱动。
应增加 NPN 三极管或逻辑电平 MOSFET，并让 MCU 与驱动电源共地。

## 验证方法

1. 在 `config/test_config.h` 中设置：

   ```c
   #define TEST_BUZZER_ENABLE 1
   ```

2. 为了只验证声音，可以暂时在 `config/app_config.h` 中关闭当前应用。
3. 编译并烧录。自检会轮播内置主题，并在每首曲目中插入一次音效，
   用于验证背景音乐暂停和恢复。

完成验证后将 `TEST_BUZZER_ENABLE` 恢复为 `0`。游戏主机模式已经直接
创建蜂鸣器实例，不需要同时打开测试任务。

## 音质边界

当前方案适合蜂鸣器版游戏音乐、提示音和可辨识旋律，程序体积很小。
它不能直接还原 MP3、WAV 中的人声或复杂伴奏，因为一个无源蜂鸣器
一次只能产生一个主要音高。

项目另有实验性的采样音乐播放器，可以直接使用当前无源蜂鸣器：

| 编码 | 采样率 | 数据量 | 预期效果 |
| --- | ---: | ---: | --- |
| PCM8 | 8 kHz、8 bit | 8 KB/s | 较容易辨认，适合优先测试 |
| ADPCM4 | 8 kHz、4 bit | 4 KB/s | 推荐，兼顾容量和可辨识度 |
| PDM1 | 32 kHz、1 bit | 4 KB/s | 仅兼容旧文件，不推荐无源蜂鸣器使用 |

它们可以保留旋律、节奏以及一部分人声，但不会得到普通喇叭的音质。
无源蜂鸣器的机械谐振会突出某些频段，复杂伴奏和低音损失尤其明显。

## 采样音乐转换与上传

电脑端需要安装 FFmpeg。转换器接受 FFmpeg 支持的 MP3、WAV、AAC，
也可以直接读取 MP4 等视频文件中的第一条音轨。

先在电脑上生成文件试听大小：

```powershell
python scripts/flash_manager.py pack-audio assets/music.mp3 build/music_pcm8.aud --codec pcm8
python scripts/flash_manager.py pack-audio assets/music.mp3 build/music_adpcm.aud --codec adpcm4
```

制作从第 30 秒开始、长度 20 秒的 PCM8 片段：

```powershell
python scripts/flash_manager.py pack-audio assets/music.mp3 build/clip.aud --codec pcm8 --start 30 --duration 20
```

也可以直接指定绝对起止时间，例如截取第 30 秒到第 50 秒：

```powershell
python scripts/flash_manager.py pack-audio assets/music.mp3 build/clip.aud --codec pcm8 --start 30 --end 50
```

上传时同样支持起止时间：

```powershell
python scripts/flash_manager.py COM6 upload-audio assets/music.mp3 /demo.aud --codec adpcm4 --start 30 --end 90
```

`--duration` 和 `--end` 二选一，不能同时指定。

上传时先启用 `FLASH_MGR_ENABLE`，然后执行：

```powershell
python scripts/flash_manager.py COM6 upload-audio assets/music.mp3 /demo.aud --codec pcm8
```

上传完成后，在 `config/app_config.h` 中配置：

```c
#define AUDIO_PLAYER_ENABLE 1
#define AUDIO_PLAYER_LOOP 1
#define AUDIO_PLAYER_PATH "/demo.aud"
```

同时关闭 `VIDEO_PLAYER_ENABLE`、`FLASH_MGR_ENABLE` 和其他独立应用，
重新编译烧录即可播放。播放器从 W25Q32 流式读取，只使用两个
256 字节缓冲区。
