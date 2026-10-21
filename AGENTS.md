# AGENTS.md — 项目上下文

> 本文件供 AI 代理 / 新会话 / 新设备快速接手本仓库。开始工作前先读这里。

## 项目是什么

在 Windows 上训练神经网络，量化成 int8 TFLite，再用 **SDL3 模拟单片机外设**，
在没有真实硬件的情况下走完「训练 → 量化 → 嵌入式部署」的完整闭环。
最终形态对准 **TFLM（TensorFlow Lite for Microcontrollers）**。

## 环境（一条命令，自包含、不污染系统）

```bat
powershell -ExecutionPolicy Bypass -File .\setup.ps1   rem Windows
bash setup.sh                                            rem Linux/macOS
```

- Python 虚拟环境在 `.venv/`，SDL3/SDL3_ttf/字体在 `third_party/`，都 gitignore。
- 依赖由 `scripts/fetch_deps.py` 下载（SDL3 3.4.18、SDL3_ttf 3.2.2 内置 FreeType 2.13.2、DejaVuSans.ttf）。
- 工具链：MinGW-w64 + CMake（`build.py` 会自动检测生成器）。

## 构建 / 调试

```bat
build.bat                rem 方向键菜单选 demo
build.bat <目标>          rem 直接构建，如 build.bat 0_sdl_setup
build.bat --clean <目标>  rem 先清理再构建
```

- VS Code/Cursor：**Ctrl+Shift+B** 构建、**F5** 用 gdb 调试（`C:\mingw64\bin\gdb.exe`）。
- 需要 Microsoft C/C++ 扩展（`ms-vscode.cpptools`）。

## 目录结构与约定

```
scripts/
  build.py        交互式构建（方向键菜单 / 预构建钩子 / --clean / TMP 重定向）
  fetch_deps.py   下载第三方依赖到 third_party/
cmake/sdl3.cmake  共享 SDL3 / SDL3_ttf 定位，提供 sdl3_attach() / sdl3_ttf_attach()
demo/<name>/      每个 demo 一个目录（含 CMakeLists.txt 才被 build.py 识别）
.vscode/          tasks.json + launch.json（共享的构建·调试配置）
```

**约定（新增 demo 必须遵守）：**
1. **可执行文件名 = demo 目录名**（见各 demo 的 CMakeLists），这样 launch.json 调试路径能通用。
2. 每个 demo 可放 `pre_build.py` / `pre_build.ps1` / `pre_build.sh`，`build.py` 会在 cmake 前自动运行（用途：训练模型→导出 .tflite→转 C 头文件）。
3. C 工程默认 `-g -O0`（调试符号 + 关优化），保证断点可用。

## 当前进度

- [x] 环境搭建（TensorFlow 2.21.0 + SDL3 + SDL3_ttf，setup 一键复现）
- [x] 构建系统（build.bat → build.py：方向键菜单、预构建钩子、--clean）
- [x] VS Code/Cursor 构建 + gdb 调试配置
- [x] demo 0：SDL3 全像素画板（RGBA8888 帧缓冲、左键画/右键擦、SDL3_ttf 文字提示）
- [ ] P1 正弦波：密集网络拟合 sin，int8 量化后 SDL 实时画「预测 vs 真值」
- [ ] P2 MNIST：CNN + 画板实时识别手写数字

## 关键决策与踩坑记录（别重蹈覆辙）

1. **RGBA8888 字节序**：SDL 的 8888 格式名按「高字节→低字节」排列，即 uint32 = `0xRRGGBBAA`（R 在高字节、A 在低字节）。写反会导致颜色错乱（曾把深蓝灰背景显示成红色）。
2. **.bat 必须纯 ASCII**：中文在 cmd 里按 GBK 逐字节解析会切碎命令，`chcp 65001` 在第二行来不及生效。构建入口 `build.bat` 因此做成 ASCII 薄壳，交互逻辑交给 Python（`build.py`）。
3. **PowerShell 不会从当前目录执行 .bat**（需要 `.\` 前缀）：VS Code 任务要用 `cmd /c build.bat ...` 而非 `build.bat`。
4. **鼠标连画断触**：`MOUSE_MOTION` 是离散采样，快速拖动会跳像素；要在相邻两点间插值盖章（`fb_stroke`）。
5. **SDL3_ttf 静态内置 FreeType**：无需单独下载 freetype；`TTF_GetFreeTypeVersion()` 可验证。
6. **调试符号**：不加 `-g` 的 exe 无法打断点。

## 已锁定版本

- Python 3.13（wheels 覆盖 3.10–3.13）
- TensorFlow 2.21.0 · NumPy 2.5.4 · Matplotlib 3.11.2
- SDL3 3.4.18 · SDL3_ttf 3.2.2（FreeType 2.13.2）

## 提交约定

- 不主动 commit/push，除非用户明确要求。
- 提交信息用中文，按 `type: 描述` 格式（build/demo/feat/chore 等）。
