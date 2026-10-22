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
build.bat               rem 默认构建 debug
build.bat debug         rem Debug（-g -O0）
build.bat release       rem Release（-O2 -DNDEBUG）
build.bat --clean debug rem 先清理再构建
```

- 编译整个 `src/` 框架，产物在 `build/debug/` / `build/release/`，可执行文件 `tflm_framework.exe`（SDL3.dll 自动拷贝到产物目录）。
- VS Code/Cursor：**Ctrl+Shift+B** 构建 debug、**F5** 自动构建后用 gdb 调试（`C:\mingw64\bin\gdb.exe`）。
- 需要 Microsoft C/C++ 扩展（`ms-vscode.cpptools`）。

## 目录结构与约定

```
CMakeLists.txt       根构建脚本（编译整个 src/）
build.bat            构建入口（debug / release，委托 scripts/build.py）
scripts/build.py     构建脚本（debug / release / --clean / TMP 重定向）
scripts/fetch_deps.py 下载第三方依赖到 third_party/
cmake/sdl3.cmake     共享 SDL3 定位，提供 sdl3_attach() / sdl3_ttf_attach()
src/                 框架源码（镜像参考工程结构，见 README）
.vscode/             tasks.json + launch.json（F5 构建·调试）
```

**约定：**
1. 框架源码在 `src/`，接口头在 `src/framework/*.h`，SDL 后端实现在 `src/framework/private/sdl/*.c`。
2. 构建目标只有 `debug` / `release` 两个，可执行文件名固定为 `tflm_framework`（launch.json 按此定位）。
3. Debug 默认 `-g -O0`（调试符号 + 关优化），保证断点可用。
4. 在 `src/` 下新增源文件后，需在根 `CMakeLists.txt` 的 `FRAMEWORK_SOURCES` 中登记。

## 当前进度

- [x] 环境搭建（TensorFlow 2.21.0 + SDL3 + SDL3_ttf，setup 一键复现）
- [x] 基础软件框架（src/，镜像参考工程：16 个框架接口 + SDL3 后端实现）
- [x] 构建系统（build.bat → build.py：debug / release 两个目标）
- [x] VS Code/Cursor F5 构建 + gdb 调试配置
- [ ] P1 正弦波：密集网络拟合 sin，int8 量化后 SDL 实时画「预测 vs 真值」
- [ ] P2 MNIST：CNN + 画板实时识别手写数字

## 关键决策与踩坑记录（别重蹈覆辙）

1. **RGBA8888 字节序**：SDL 的 8888 格式名按「高字节→低字节」排列，即 uint32 = `0xRRGGBBAA`（R 在高字节、A 在低字节）。写反会导致颜色错乱（曾把深蓝灰背景显示成红色）。
2. **.bat 必须纯 ASCII**：中文在 cmd 里按 GBK 逐字节解析会切碎命令，`chcp 65001` 在第二行来不及生效。构建入口 `build.bat` 因此做成 ASCII 薄壳，交互逻辑交给 Python（`build.py`）。
3. **PowerShell 不会从当前目录执行 .bat**（需要 `.\` 前缀）：VS Code 任务要用 `cmd /c build.bat ...` 而非 `build.bat`。
4. **鼠标连画断触**：`MOUSE_MOTION` 是离散采样，快速拖动会跳像素；要在相邻两点间插值盖章（`fb_stroke`）。
5. **SDL3_ttf 静态内置 FreeType**：无需单独下载 freetype；`TTF_GetFreeTypeVersion()` 可验证。
6. **调试符号**：不加 `-g` 的 exe 无法打断点。
7. **MinGW PE 链接段注册不可用**：参考工程的 `__attribute__((section("__function_regedit")))` 段注册在 MinGW 下失效（`#pragma pack(1)`+位域导致不同编译单元 `sizeof(function_item_t)` 不一致、段按 16 字节对齐插入填充），框架改用 GCC 构造函数 + 静态注册表（`c_function_regedit_add`）等价实现，接口宏/结构体保持逐字一致。
8. **SDL3 3.4.18 API 命名**：原子指针是 `void**` + `SDL_Get/SetAtomicPointer`（非 `SDL_AtomicPtr`）；线程优先级用 `SDL_SetCurrentThreadPriority`（非 `SDL_SetThreadPriority`）；优先级枚举只有 LOW/NORMAL/HIGH/TIME_CRITICAL。
9. **e_mutex 用 SDL_Semaphore(1) 而非 SDL_Mutex**：SDL3 的 `SDL_Mutex` 是递归锁，为对齐参考的二值信号量（非递归）语义改用信号量。

## 已锁定版本

- Python 3.13（wheels 覆盖 3.10–3.13）
- TensorFlow 2.21.0 · NumPy 2.5.4 · Matplotlib 3.11.2
- SDL3 3.4.18 · SDL3_ttf 3.2.2（FreeType 2.13.2）

## 提交约定

- 不主动 commit/push，除非用户明确要求。
- 提交信息用中文，按 `type: 描述` 格式（build/demo/feat/chore 等）。
