# Prompt：将 PC 端 `flash_mgr` 改造成交互式工作台风格

## 任务背景

当前项目中已经存在 PC 端 `flash_mgr` Python 脚本，用于通过串口与开发板通信，并管理 W25Q32 / 外部 Flash 中的文件或资源。现在需要在**不破坏原有命令行功能和底层通信协议**的前提下，把这个脚本改造成类似 `motion_mode_manager.py` 的“工作台式交互 CLI”。

参考风格包括：

- 启动后显示彩色 banner；
- 无参数运行时进入交互式菜单；
- 有参数运行时仍支持原来的命令行调用；
- 使用 ANSI 颜色输出，不依赖复杂第三方 TUI 框架；
- 支持清屏、配置查看、配置修改、帮助说明；
- 菜单选项清晰，操作结果用 `✅ / ⚠️ / ❌ / ℹ️` 等符号提示；
- 危险操作需要二次确认；
- 工具整体像一个“Flash Manager Workbench”。

目标是让 `flash_mgr` 不再只是零散命令，而是一个适合日常开发使用的 PC 端 Flash 文件管理工作台。

---

## 总体要求

### 1. 保留原有能力

请先阅读当前仓库里的 PC 端脚本，例如：

```text
scripts/flash_manager.py
scripts/flash_mgr.py
scripts/flash_*.py
```

以实际仓库中存在的脚本为准。

必须保留原脚本已有的功能，例如可能包括但不限于：

- 串口连接；
- 查询设备状态；
- 查询文件系统信息；
- 上传文件到 Flash / LittleFS；
- 从 Flash / LittleFS 下载文件；
- 删除文件；
- 列出目录；
- 格式化文件系统；
- 擦除 Flash；
- 读取原始地址；
- 写入原始地址；
- CRC / 校验 / verify；
- 调试输出。

如果当前脚本中没有某项功能，不要凭空强行新增底层协议。可以在菜单中预留入口并显示：

```text
当前固件协议暂不支持该功能
```

除非仓库中 MCU 端协议已经支持，或者本次任务明确要求同步修改 MCU 端。

---

### 2. 保持命令行兼容

改造后必须同时支持两种模式。

#### 交互模式

无参数运行时进入工作台：

```bash
python scripts/flash_manager.py
```

进入类似：

```text
╔══════════════════════════════════════════════╗
║        Flash Manager Workbench              ║
╚══════════════════════════════════════════════╝

Port       : /dev/ttyACM0
Baudrate   : 115200
Remote cwd : /
Local cwd  : ./
Status     : disconnected

[O] Open/Connect     [I] Info          [L] List Files
[U] Upload File      [D] Download      [R] Remove File
[T] Tree/List All    [F] Format FS     [V] Verify File
[C] Config           [H] Help          [Q] Quit

Select an option »
```

#### 命令行模式

带参数运行时继续走原来的 CLI 行为，例如：

```bash
python scripts/flash_manager.py /dev/ttyACM0 upload build/res.bin /res.bin
python scripts/flash_manager.py /dev/ttyACM0 list /
python scripts/flash_manager.py /dev/ttyACM0 delete /res.bin
python scripts/flash_manager.py /dev/ttyACM0 info
```

如果原脚本的参数顺序与上面不同，以原脚本为准。不能破坏已有脚本调用方式，因为这些命令可能已经写进文档、Makefile、README 或 CI。

---

## 代码结构要求

建议把脚本拆成清晰的几层，避免所有逻辑都堆在 `main()` 里。

### 1. 基础输出层

新增或整理以下工具函数：

```python
def color_text(text, color=COLOR_RESET, bold=False):
    ...

def clear_screen():
    ...

def print_banner():
    ...

def print_success(msg):
    ...

def print_warning(msg):
    ...

def print_error(msg):
    ...

def print_info(msg):
    ...
```

颜色建议：

```python
COLOR_RESET   = "\033[0m"
COLOR_RED     = "\033[31m"
COLOR_GREEN   = "\033[32m"
COLOR_YELLOW  = "\033[33m"
COLOR_BLUE    = "\033[34m"
COLOR_MAGENTA = "\033[35m"
COLOR_CYAN    = "\033[36m"
COLOR_BOLD    = "\033[1m"
COLOR_DIM     = "\033[2m"
```

在 Windows 下不强制依赖 `colorama`。可以优先使用 ANSI；如果检测到不支持 ANSI，可以降级为无颜色输出。

---

### 2. 配置层

增加运行时配置对象：

```python
config = {
    "port": None,
    "baudrate": 115200,
    "timeout": 2.0,
    "local_cwd": ".",
    "remote_cwd": "/",
    "auto_connect": False,
    "verify_after_upload": True,
    "chunk_size": 512,
    "show_debug": False,
}
```

要求：

- 配置默认只在当前会话内生效；
- 可以提供可选的保存/加载配置功能，例如保存到 `.flash_mgr_config.json`，但不要强制；
- 配置菜单至少支持：
  - 查看所有配置；
  - 设置串口；
  - 设置波特率；
  - 设置本地工作目录；
  - 设置远端工作目录；
  - 开关上传后校验；
  - 开关 debug 输出；
  - 返回主菜单。

示例：

```text
--- CONFIG MENU ---
[S] show config
[P] set port
[B] set baudrate
[L] set local cwd
[R] set remote cwd
[V] toggle verify after upload
[D] toggle debug
[Q] return

config »
```

---

### 3. 串口/协议封装层

如果当前脚本还没有类封装，建议整理为：

```python
class FlashManagerClient:
    def __init__(self, port, baudrate=115200, timeout=2.0):
        ...

    def connect(self):
        ...

    def close(self):
        ...

    def is_connected(self):
        ...

    def info(self):
        ...

    def list_dir(self, path):
        ...

    def upload_file(self, local_path, remote_path, progress_cb=None):
        ...

    def download_file(self, remote_path, local_path, progress_cb=None):
        ...

    def remove_file(self, remote_path):
        ...

    def format_fs(self):
        ...

    def verify_file(self, local_path, remote_path):
        ...
```

要求：

- UI 菜单层不要直接拼底层串口协议；
- 原来的底层读写、打包、解包、等待 ACK、CRC、超时重试逻辑尽量保留；
- 如果原脚本已经有成熟函数，不要重写协议，只做封装和重排；
- 所有异常统一转成用户能看懂的错误信息；
- 串口打开失败、设备无响应、路径不存在、空间不足、CRC 错误等都要明确提示。

---

## 工作台菜单设计

### 主菜单

无参数启动后进入主菜单。

建议选项：

```text
[O] Open / Connect
[I] Device / FS Info
[L] List Current Remote Directory
[T] Tree / Recursive List
[U] Upload File
[D] Download File
[R] Remove File
[M] Make Directory / Path Tools
[F] Format File System
[V] Verify Local vs Remote File
[C] Config
[H] Help
[Q] Quit
```

如果当前协议不支持目录、树、mkdir 等功能，可以：

- 不显示该选项；或者
- 显示但进入后提示当前协议不支持。

不要为了菜单好看而加入不可用且会报错的假功能。

---

### 连接菜单 / 端口选择

提供端口选择功能：

```text
=== Serial Ports ===
[1] /dev/ttyACM0    USB Serial / CMSIS-DAP
[2] /dev/ttyUSB0    USB Serial
[3] COM5            USB-SERIAL CH340
[M] Manual input
[Q] Return
```

实现建议：

- 使用 `serial.tools.list_ports.comports()` 枚举端口；
- 如果没有安装 `pyserial`，提示用户安装；
- 如果没有发现端口，允许手动输入；
- 连接成功后在 banner 中显示 `connected`；
- 连接失败要显示具体异常。

---

### 文件上传

上传流程设计为：

```text
--- Upload File ---
Local file path  > build/badapple.bard
Remote file path > /badapple.bard
Verify after upload? [Y/n]
```

要求：

- 检查本地文件是否存在；
- 显示文件大小；
- 自动显示上传进度；
- 上传完成后显示耗时、平均速度；
- 如果启用校验，则调用现有 verify/CRC 能力；
- 如果远端文件已存在，必须询问是否覆盖；
- 远端路径默认可根据本地文件名生成，例如本地 `build/res.bin` 默认远端 `/res.bin`；
- 上传过程中允许用户看到分块进度，但不需要复杂进度条库。

进度显示可以简单实现：

```text
Uploading:  73.4%  188416 / 256768 bytes  21.2 KB/s
```

---

### 文件下载

下载流程：

```text
--- Download File ---
Remote file path > /badapple.bard
Local file path  > ./badapple.bard
```

要求：

- 如果本地文件已存在，询问是否覆盖；
- 显示下载进度；
- 下载后显示大小和耗时；
- 如果远端文件不存在，清晰提示。

---

### 列表 / 信息显示

`info` 输出要格式化成更适合阅读的表格或分组：

```text
=== Device / Flash Info ===
Flash chip      : W25Q32
Flash size      : 4 MiB
FS used         : 732 KiB
FS total        : 2048 KiB
Block size      : 4096 B
Status          : OK
```

`list` 输出建议：

```text
=== Remote: / ===
Name                         Type      Size        Modified
badapple.bard                file      812.4 KiB   -
config.bin                   file      128 B       -
assets                       dir       -           -
```

如果协议没有 modified time，就显示 `-`。

---

### 删除 / 格式化 / 擦除等危险操作

删除文件前确认：

```text
Delete remote file '/badapple.bard'? Type 'yes' to confirm >
```

格式化前确认：

```text
WARNING: This will format the external Flash file system.
All files will be lost.
Type 'FORMAT' to continue >
```

擦除 raw flash 前确认：

```text
WARNING: This may erase raw Flash sectors.
Type 'ERASE' to continue >
```

要求：

- 不允许用单个 `y` 触发高危操作；
- 需要输入明确关键词；
- 操作完成后重新查询 info，显示剩余空间。

---

## 命令行模式设计

保留原有命令，同时可以整理帮助输出。

建议支持：

```bash
python scripts/flash_manager.py help
python scripts/flash_manager.py ports
python scripts/flash_manager.py <port> info
python scripts/flash_manager.py <port> list /
python scripts/flash_manager.py <port> upload <local> <remote>
python scripts/flash_manager.py <port> download <remote> <local>
python scripts/flash_manager.py <port> delete <remote>
python scripts/flash_manager.py <port> verify <local> <remote>
python scripts/flash_manager.py <port> format
```

但如果当前脚本已有不同格式，例如：

```bash
python scripts/flash_manager.py upload <local> <remote>
```

则必须兼容旧格式，不能只支持新格式。

可以在内部做兼容解析：

```python
if old_style_args_detected:
    run_old_cli_compatible(argv)
else:
    run_new_cli(argv)
```

---

## 交互体验细节

### 1. 输入循环

所有子菜单都应该支持：

```text
q / quit / return
```

回到上一级菜单。

路径输入为空时使用默认值，例如：

```text
Remote path [/badapple.bard] >
```

用户直接回车则使用默认值。

---

### 2. 自动连接

如果用户执行上传、下载、列表等操作时还没有连接，脚本应提示：

```text
Device is not connected. Connect now? [Y/n]
```

如果 `config["auto_connect"] == True`，则自动连接。

---

### 3. 错误处理

必须处理：

- 串口不存在；
- 串口被占用；
- 固件无响应；
- 协议版本不匹配；
- 本地文件不存在；
- 远端路径非法；
- Flash 空间不足；
- 上传中断；
- CRC 校验失败；
- 用户取消操作。

错误信息不要只打印 Python traceback，除非 debug 模式开启。

普通模式：

```text
❌ Upload failed: device timeout while waiting for ACK
```

debug 模式：

```text
❌ Upload failed: device timeout while waiting for ACK
Traceback:
...
```

---

## 建议的文件修改范围

优先修改：

```text
scripts/flash_manager.py
```

必要时可以新增：

```text
scripts/flash_mgr_workbench.py
```

但更推荐只保留一个入口脚本：

```text
scripts/flash_manager.py
```

如果代码太长，可以拆分为：

```text
scripts/flashmgr/
  __init__.py
  client.py       # 协议和串口通信
  ui.py           # 交互式菜单
  cli.py          # 命令行解析
  utils.py        # 颜色、路径、进度显示
scripts/flash_manager.py
```

其中 `scripts/flash_manager.py` 只作为入口。

如果项目当前结构比较简单，也可以不拆包，但至少要把函数分组清楚。

---

## 代码风格要求

- Python 版本建议兼容 3.8+；
- 不引入重量级依赖；
- 可以依赖 `pyserial`，因为原 flash manager 大概率已经依赖串口；
- 输出使用中文或英文均可，但同一个脚本中要统一；
- 推荐使用英文菜单标题和简短英文命令，错误解释可以中文；
- 不要在退出信息中加入不适合公开仓库的粗俗语句；
- 所有用户输入都要 `.strip()`；
- 所有文件路径都要用 `os.path` 或 `pathlib` 处理；
- 危险操作必须确认；
- 上传/下载中途异常时要关闭文件句柄和串口资源；
- `Ctrl+C` 时优雅退出。

---

## 参考交互框架

可以参考如下伪代码组织：

```python
def main():
    if len(sys.argv) == 1:
        interactive_loop()
    else:
        cli_main(sys.argv[1:])


def interactive_loop():
    clear_screen()
    print_banner()
    while True:
        print_main_menu()
        choice = input(color_text("Select an option » ", COLOR_BOLD)).strip().lower()

        if choice == "o":
            connect_menu()
        elif choice == "i":
            action_info()
        elif choice == "l":
            action_list()
        elif choice == "u":
            action_upload()
        elif choice == "d":
            action_download()
        elif choice == "r":
            action_remove()
        elif choice == "f":
            action_format()
        elif choice == "v":
            action_verify()
        elif choice == "c":
            config_menu()
            clear_screen()
            print_banner()
        elif choice == "h":
            help_command()
        elif choice == "q":
            print(color_text("\n👋 Exiting Flash Manager Workbench.\n", COLOR_CYAN, True))
            break
        else:
            print_error("Invalid choice. Please try again.")
```

---

## 参考菜单输出

启动界面建议：

```text
╔══════════════════════════════════════════════╗
║        Flash Manager Workbench              ║
╚══════════════════════════════════════════════╝
   port       : /dev/ttyACM0
   baudrate   : 115200
   local cwd  : ./
   remote cwd : /
   status     : connected
```

主菜单建议：

```text
[O] Connect       [I] Info          [L] List
[U] Upload        [D] Download      [R] Remove
[T] Tree          [F] Format FS     [V] Verify
[C] Config        [H] Help          [Q] Quit
```

---

## 测试要求

完成后至少验证：

### 1. CLI 兼容性

原有命令仍能运行：

```bash
python scripts/flash_manager.py <原来的参数>
```

### 2. 交互模式启动

```bash
python scripts/flash_manager.py
```

能够进入工作台界面。

### 3. 端口枚举

```bash
python scripts/flash_manager.py ports
```

能够列出可用串口。

### 4. 上传文件

准备一个小文件：

```bash
echo hello > /tmp/hello.txt
```

在工作台中上传到：

```text
/hello.txt
```

### 5. 列出文件

确认 `/hello.txt` 出现在远端文件列表中。

### 6. 下载文件

下载回本地，确认内容一致。

### 7. 删除文件

删除 `/hello.txt`，再次 list 确认不存在。

### 8. 异常路径

测试不存在的本地文件、错误串口、设备未连接等情况，确认不会崩溃。

---

## 最终交付内容

请完成以下交付：

1. 改造后的 `scripts/flash_manager.py`；
2. 如果拆分模块，则提交新增的 `scripts/flashmgr/*.py`；
3. 更新相关文档，例如：

```text
tools/README.md
docs/flash_manager.md
README.md
```

至少写明：

- 如何启动工作台；
- 如何选择串口；
- 如何上传文件；
- 如何下载文件；
- 如何删除文件；
- 如何格式化；
- 如何继续使用旧命令行方式。

---

## 重要限制

- 不要破坏 MCU 端现有 flash_mgr 协议；
- 不要把工作台 UI 逻辑写进 MCU 端；
- 不要要求用户安装重量级 TUI 依赖；
- 不要去掉原来的命令行模式；
- 不要让格式化、擦除、删除这种危险操作一键执行；
- 不要在普通错误下输出一大段 traceback；
- 不要把端口、路径等个人环境写死；
- 不要使用不适合公开仓库的退出语或提示语。

---

## 期望效果

完成后，用户可以像使用一个小型文件管理器一样管理开发板外部 Flash：

```bash
python scripts/flash_manager.py
```

进入工作台后，可以完成：

- 自动/手动选择串口；
- 查看 W25Q32 / LittleFS 状态；
- 上传资源文件；
- 下载资源文件；
- 删除资源文件；
- 列出文件；
- 校验文件；
- 格式化文件系统；
- 调整配置；
- 退出。

同时原来的：

```bash
python scripts/flash_manager.py <args>
```

仍然可用。
