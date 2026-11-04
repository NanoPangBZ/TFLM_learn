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

- Python 虚拟环境在 `.venv/`，SDL3 静态库/字体在 `third_party/`，都 gitignore。
- 依赖由 `scripts/fetch_deps.py` → `build_sdl_static.py` 编出静态 SDL3/SDL3_ttf（3.4.18 / 3.2.2）到 `third_party/sdl_static/`；字体 DejaVuSans.ttf。
- 工具链：MinGW-w64 + CMake（`build.py` 会自动检测生成器）；链接加 `-static`，exe 不依赖 SDL3.dll / MinGW 运行时 DLL。

## 构建 / 调试

必须手动选择编译目标（`TFLM_LEARN` / `platform_develop`）与配置（`debug` / `release`）：

```bat
build.bat                              rem 交互菜单选 target + 配置
build.bat TFLM_LEARN debug             rem TFLM 学习目标 Debug
build.bat platform_develop release     rem 平台开发目标 Release
build.bat --clean TFLM_LEARN debug     rem 先清理再构建
```

- 产物在 `build/<target>/<debug|release>/`，可执行文件名与目标名一致（如 `TFLM_LEARN.exe`；SDL/字库均静态链入，可单文件运行）。
- VS Code/Cursor：**F5** → QuickPick 选 target → 自动构建 debug 并用 gdb 启动（`C:\mingw64\bin\gdb.exe`）；**Ctrl+Shift+B** 同样先选 target 再构建 debug。
- 新增/删除 `src/target/<name>` 后跑一次 `build.bat --sync-vscode`（或任意构建）可刷新 F5 下拉列表。
- 需要 Microsoft C/C++ 扩展（`ms-vscode.cpptools`）。

## 目录结构与约定

```
CMakeLists.txt            根构建：统一编 framework/arch/公共bsp/lib；要求 -DTFLM_TARGET=
build.bat                 构建入口（委托 scripts/build.py）
scripts/build.py          交互/CLI 选 target + debug|release；产物 build/<target>/<config>/
scripts/fetch_deps.py     下载第三方依赖到 third_party/
cmake/sdl3.cmake          sdl3_attach() / sdl3_ttf_attach()
src/
  main.c                  统一入口
  framework/              框架接口 + private/sdl 实现（根 CMake 统一包含）
  arch/                   arch 层实现
  bsp/                    BSP 实现（encoder 仅由需要它的 target 追加）
  app/                    应用（由 target 私有 CMakeLists 勾选）
  service/                服务（由 target 私有 CMakeLists 勾选）
  target/
    TFLM_LEARN/           cfg（含 version.h，无 encoder）+ 私有 CMakeLists
    platform_develop/     cfg（含 version.h + encoder）+ 私有 CMakeLists
tools/                    Python/C 工具
.vscode/                  tasks.json + launch.json
```

**约定：**
1. 框架源码在 `src/`，接口头在 `src/framework/*.h`，SDL 后端在 `src/framework/private/sdl/*.c`；由**根** `CMakeLists.txt` 的 `COMMON_SOURCES` 统一登记。
2. per-target 配置头（`arch_cfg.h` / `bsp_cfg.h` / `framework_cfg.h` / `service_cfg.h` / `version.h`）只放在 `src/target/<name>/`；include 顺序为该目录优先于 `src/`。
3. app / service / 专属 test / 可选 bsp（如 encoder）由 `src/target/<name>/CMakeLists.txt` 用 `target_sources(${TFLM_APP_TARGET} ...)` 追加。
4. 可执行文件名与 target 目录名一致（`build/<name>/<config>/<name>.exe`）。
5. Debug 默认 `-g -O0`，保证断点可用。

## 当前进度

- [x] 环境搭建（TensorFlow 2.21.0 + SDL3 + SDL3_ttf，setup 一键复现）
- [x] 基础软件框架（src/，镜像参考工程：16 个框架接口 + SDL3 后端实现）
- [x] 多目标构建（`TFLM_LEARN` / `platform_develop`；cfg 按 target 分离）
- [x] VS Code/Cursor F5 构建 + gdb 调试配置
- [ ] P1 正弦波：密集网络拟合 sin，int8 量化后 SDL 实时画「预测 vs 真值」
- [x] num_discern：手写数字识别 app（MLP 784→32→10 float32，MNIST 95%；canvas 空闲触发 → 预处理 28×28 → 推理 → 日志输出）

## 关键决策与踩坑记录（别重蹈覆辙）

1. **RGBA8888 字节序**：SDL 的 8888 格式名按「高字节→低字节」排列，即 uint32 = `0xRRGGBBAA`（R 在高字节、A 在低字节）。写反会导致颜色错乱（曾把深蓝灰背景显示成红色）。
2. **.bat 必须纯 ASCII**：中文在 cmd 里按 GBK 逐字节解析会切碎命令，`chcp 65001` 在第二行来不及生效。构建入口 `build.bat` 因此做成 ASCII 薄壳，交互逻辑交给 Python（`build.py`）。
3. **PowerShell 不会从当前目录执行 .bat**（需要 `.\` 前缀）：VS Code 任务要用 `cmd /c build.bat ...` 而非 `build.bat`。
4. **鼠标连画断触**：`MOUSE_MOTION` 是离散采样，快速拖动会跳像素；要在相邻两点间插值盖章（`fb_stroke`）。
5. **SDL / 字库静态链接**：`third_party/sdl_static` 由源码编出；`cmake/sdl3.cmake` 链静态 SDL，MinGW 加 `-static`；需要 TTF 时用 `embed_bin.py` 把 DejaVuSans.ttf 编进 exe（`embedded_font_open_dejavu`），不再旁路拷贝 .ttf / DLL。
6. **调试符号**：不加 `-g` 的 exe 无法打断点。
7. **MinGW PE 链接段注册不可用**：参考工程的 `__attribute__((section("__function_regedit")))` 段注册在 MinGW 下失效（`#pragma pack(1)`+位域导致不同编译单元 `sizeof(function_item_t)` 不一致、段按 16 字节对齐插入填充），框架改用 GCC 构造函数 + 静态注册表（`c_function_regedit_add`）等价实现，接口宏/结构体保持逐字一致。
8. **SDL3 3.4.18 API 命名**：原子指针是 `void**` + `SDL_Get/SetAtomicPointer`（非 `SDL_AtomicPtr`）；线程优先级用 `SDL_SetCurrentThreadPriority`（非 `SDL_SetThreadPriority`）；优先级枚举只有 LOW/NORMAL/HIGH/TIME_CRITICAL。
9. **e_mutex 用 SDL_Semaphore(1) 而非 SDL_Mutex**：SDL3 的 `SDL_Mutex` 是递归锁，为对齐参考的二值信号量（非递归）语义改用信号量。
10. **e_heap 是 bump 分配器、`e_heap_free` 为空操作**：只「分配一次、永不释放」（同 TFLM arena 模式）。热路径/回调里不能反复 `e_heap_malloc`（每次永久吃堆，几次即耗尽），应改用静态缓冲或一次性分配。
11. **arch 统一主窗口，bsp_display/bsp_touch 按 map table 多实例**：窗口/渲染器由 `arch/private/sdl/arch_display.c` 创建，`RenderClear` / `RenderPresent` 全部在 arch 主线程完成；`bsp_cfg.h` 用 `BSP_DISPLAY_MAP_TABLE` / `BSP_TOUCH_MAP_TABLE` 把 `type+idx` 映射到主窗口内的矩形（1:1、超出裁剪），每个 idx 是独立实例。`bsp_display_flush` 只更新内存 staging 并请求重绘，禁止在轮询线程直接调用 SDL 渲染。app 的中间图像通过 `src/app/common/debug_display.c` 写到 display 1/2/3，不再开独立窗口。RGBX8888 的 uint32 是 `0xRRGGBBXX`（R 在高字节），不是 ARGB。draw_buffer 为 **RGB565_SWAPPED**（与真机/LVGL 一致）；`open` 后默认 `is_on=1`；`set_draw_buffer` 允许局部窗口缓冲；flush 对＜整屏缓冲按紧密 stride=width，对≥整屏缓冲按全屏 stride（兼容 canvas）。
12. **bsp_storage 用固定大小 bin 模拟 NOR flash**：`BSP_STORAGE_MAP_TABLE` 每项按 `type+index` 映射到 `flash_data/<filename>.bin`，文件大小必须等于 `total_size`，不一致就重建并填 `erase_value`（0xFF）。读/写允许任意地址和长度（写跨 `program_unit` 页会自动读改写，未擦除区域按 `old & data` 生效，只能 1→0）；只有擦除要求地址/长度按 `erase_unit` 对齐并填 `erase_value`。实现走 SDL_IOStream，不直接用 stdio。
13. **bsp_encoder / bsp_knob / bsp_led 仅 platform_develop**：映射表在该 target 的 `bsp_cfg.h`；SDL 源由该 target 私有 CMakeLists 追加；encoder/knob 需 `sdl3_ttf_attach`。`TFLM_LEARN` 不含三者。encoder 按 name 打开、全周连续；knob 按 name `bsp_knob_get`，有限行程（270°）+ 限位挡块 + 红点标位，`get_value` 返回 [0,1]。滚轮用 `arch_input_peek_wheel` / `take_wheel`，谁命中谁消费。`bsp_led`：`BSP_LED_MAP_TABLE` 配圆心 `(x,y)` 与 `color`(0xRRGGBB)，直径固定 16px；API 与 YouWei 一致 `set/get_value(idx)`（0 灭 / 非 0 亮）。
14. **bsp_audio_system（SDL）**：`bsp_cfg` 配采样率 / 帧长 / 通道数，以及右侧模拟 UI（`UI_X/Y`、`PANEL_W`、`METER_H/GAP`；频率表与配置页同宽；可选 `RX/TX_NAMES`）。每路启动 `NONE`；RX：`SYS SPK`/`SYS MIC`/FILE/DEMO/SINE，TX：FILE/SYS。主视图为通道**频率表**（频谱 + 右上角能量条 + rms/峰值频率），点击进配置页；配置页布局常量 `AUDIO_CFG_*` 与命中矩形共用。TX **一帧双缓冲**。公共头与 YouWei 一致。RX DEMO 播内嵌 `demo.wav`（`embed_bin_attach` → `g_embedded_demo_wav`）；RX FILE 仍配磁盘路径；TX FILE 写磁盘。

## 已锁定版本

- Python 3.13（wheels 覆盖 3.10–3.13）
- TensorFlow 2.21.0 · NumPy 2.5.4 · Matplotlib 3.11.2
- SDL3 3.4.18 · SDL3_ttf 3.2.2（FreeType 2.13.2）

## 提交约定

- 不主动 commit/push，除非用户明确要求。
- 提交信息用中文，按 `type: 描述` 格式（build/demo/feat/chore 等）。
- 提交时只提交暂存区已有内容（直接 `git commit`），**不自动 `git add -A`**，由用户自行决定暂存哪些文件。
