# Windows 环境配置说明（MSYS2 UCRT64）

> 在官网安装 MSYS2 UCRT64

## 1. 推荐环境

Windows 下建议使用：

```text
MSYS2 UCRT64
├── gcc
├── g++
├── cmake
├── ninja
├── pkg-config / pkgconf
├── SDL2
├── python
├── pip
├── PyYAML
├── Pillow
└── pyserial
```

---

## 2. 打开 MSYS2 UCRT64

从 Windows 开始菜单搜索并打开：

```text
MSYS2 UCRT64
```

打开后检查：

```bash
echo $MSYSTEM
```

正确输出应为：

```text
UCRT64
```

---

## 3. 安装依赖

在 MSYS2 UCRT64 中执行：

```bash
pacman -Syu
```

如果提示关闭窗口，就关闭 UCRT64 终端，重新打开后再执行一次：

```bash
pacman -Syu
```

然后安装项目依赖：

```bash
pacman -S --needed \
  git \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-SDL2 \
  mingw-w64-ucrt-x86_64-python \
  mingw-w64-ucrt-x86_64-python-pip \
  mingw-w64-ucrt-x86_64-python-yaml \
  mingw-w64-ucrt-x86_64-python-pillow \
  mingw-w64-ucrt-x86_64-python-pyserial
```

说明：

| 包 | 用途 |
|---|---|
| `mingw-w64-ucrt-x86_64-gcc` | Windows VM 的 C/C++ 编译器 |
| `mingw-w64-ucrt-x86_64-cmake` | CMake 配置工具 |
| `mingw-w64-ucrt-x86_64-ninja` | Ninja 构建工具 |
| `mingw-w64-ucrt-x86_64-pkgconf` | 提供 `pkg-config`，用于查找 SDL2 |
| `mingw-w64-ucrt-x86_64-SDL2` | VM 窗口、输入、渲染依赖 |
| `mingw-w64-ucrt-x86_64-python` | 执行项目构建脚本 |
| `mingw-w64-ucrt-x86_64-python-pip` | Python 包管理器 |
| `mingw-w64-ucrt-x86_64-python-yaml` | 读取 `config/config.yaml` |
| `mingw-w64-ucrt-x86_64-python-pillow` | 资源/图片脚本可能需要 |
| `mingw-w64-ucrt-x86_64-python-pyserial` | 串口/烧录辅助脚本可能需要 |

---

## 4. 检查环境

在 UCRT64 中执行：

```bash
echo $MSYSTEM
which gcc
which cmake
which ninja
which python
which pkg-config
pkg-config --modversion sdl2
python - <<'PY'
import yaml
import PIL
import serial
print("Python deps OK")
PY
```

正确路径应类似：

```text
UCRT64
/ucrt64/bin/gcc
/ucrt64/bin/cmake
/ucrt64/bin/ninja
/ucrt64/bin/python
/ucrt64/bin/pkg-config
```
