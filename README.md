# TFLM_learn — 神经网络学习（TensorFlow + SDL3）

在 Windows 上训练神经网络，量化成 int8 TFLite，再用 **SDL3 模拟单片机外设**，
在没有真实硬件的情况下走完「训练 → 量化 → 嵌入式部署」的完整闭环。

## 为什么是 TensorFlow + SDL3

- **TensorFlow / Keras**：在 Windows 上训练网络，用内置的 TFLite Converter 导出模型并做 int8 量化。
- **SDL3**：扮演那台"单片机"——帧缓冲显示、鼠标当触摸屏、声卡当麦克风，写的是真·嵌入式风格的 C/C++ 推理代码。
- 最终形态对准 **TFLM（TensorFlow Lite for Microcontrollers）**：几十 KB 内存、int8、无动态内存（arena 分配器）。
  你的 C/C++ / ARM / 编译器功底，正好能看懂甚至改写它的算子实现。

## 环境要求

| 工具 | 版本 | 说明 |
|------|------|------|
| Git | 任意 | 拉取仓库 |
| CMake | ≥ 3.16 | 编译 C 工程 |
| C 编译器 | MinGW-w64 或 MSVC | 编译 SDL3 示例 |
| Python | 3.10 – 3.13 | 推荐 3.13（`py` 或 `python` 在 PATH） |

## 快速开始（一条命令）

```powershell
# Windows（PowerShell）
powershell -ExecutionPolicy Bypass -File .\setup.ps1
```

```bash
# Linux / macOS
bash setup.sh
```

`setup` 脚本**只在本仓库内安装，不污染系统**：

- `.venv/` — Python 虚拟环境（TensorFlow、NumPy、Matplotlib），系统 Python 本身不动。
- `third_party/` — SDL3、SDL3_ttf（内置 FreeType）和字体，由 `scripts/fetch_deps.py` 自动下载。

两者都在 `.gitignore` 里，不随仓库提交；换一台机器 `git clone` 后重新跑脚本即可复现。

## 验证安装

```powershell
# 1. TensorFlow：训练 → float/int8 导出 → 解释器推理
.venv\Scripts\python.exe verify\01_tensorflow_smoke.py

# 2. SDL3：用 MinGW + CMake 编译并运行一个窗口
cmake -S verify\02_sdl3_smoke -B verify\02_sdl3_smoke\build -G "MinGW Makefiles"
cmake --build verify\02_sdl3_smoke\build
verify\02_sdl3_smoke\build\sdl3_smoke.exe
```

预期输出：Python 冒烟测试末尾打印 `SMOKE OK`；SDL3 程序弹出一个窗口并在控制台打印
`SDL3 3.4.18 OK`。

## 目录结构

```
TFLM_learn/
├── setup.ps1 / setup.sh   # 一键安装环境（首选）
├── build.bat              # 构建入口（须选 target + debug/release）
├── CMakeLists.txt         # 根：统一编 framework；要求 -DTFLM_TARGET=
├── requirements.txt       # Python 依赖（已锁定版本）
├── cmake/
│   └── sdl3.cmake         # 共享的 SDL3 定位 / 链接 helper
├── scripts/
│   ├── fetch_deps.py      # 下载 SDL3 / SDL3_ttf / 字体到 third_party/
│   └── build.py           # 交互/CLI 选 target + 配置
├── .vscode/               # tasks.json + launch.json（F5 构建·调试）
├── src/
│   ├── main.c             # 统一入口
│   ├── framework/         # 框架接口 + private/sdl（根 CMake 统一包含）
│   ├── lib/               # com_host / symbol_table
│   ├── app/               # 应用（由 target 私有 CMakeLists 勾选）
│   └── target/
│       ├── TFLM_LEARN/    # cfg（无 encoder）+ 私有 CMakeLists
│       └── platform_develop/  # cfg（含 encoder）+ BSP 测试
├── verify/                # 安装验证冒烟测试
├── .venv/                 # （不提交）本地虚拟环境
└── third_party/           # （不提交）SDL3 / SDL3_ttf / 字体
```

## 构建

```bat
build.bat                              rem 交互菜单选 target + 配置
build.bat TFLM_LEARN debug             rem TFLM 学习目标 Debug
build.bat platform_develop release     rem 平台开发目标 Release
build.bat --clean TFLM_LEARN debug     rem 先清理再构建
```

产物输出到 `build/<target>/<debug|release>/`，可执行文件名与目标名一致（如 `TFLM_LEARN.exe`）。
脚本自动选择生成器（优先 MinGW，否则交给 CMake 自动选 MSVC/Ninja）。

## VS Code / Cursor 调试

已内置 `.vscode/tasks.json` 和 `.vscode/launch.json`：

- **Ctrl+Shift+B** —— 构建 `TFLM_LEARN` debug（默认构建任务）
- **F5** —— 可选 `Debug TFLM_LEARN` / `Debug platform_develop`

Debug 目标以 `-g -O0` 编译，带调试符号可正常打断点。
需要 Microsoft C/C++ 扩展（ms-vscode.cpptools），gdb 路径为 `C:\mingw64\bin\gdb.exe`。

## 学习路线（下一步）

1. **正弦波 Hello World** —— 密集网络拟合 sin，int8 量化后 SDL 实时画「预测 vs 真值」曲线。
   学到：前向/反向传播、激活、损失、量化、flatbuffer 模型格式。
2. **MNIST 手写识别** —— CNN + SDL 鼠标画 28×28 数字、实时识别。
   学到：卷积/池化、图像预处理、量化误差。
3. （可选）关键词唤醒、手势识别 —— MFCC、深度可分离卷积、时序数据。

C++ 侧推理有两条路径，建议都体验：

- **TFLite C API**：桌面版，几十行代码，最快打通闭环。
- **TFLM 源码**：真正接近单片机的形态（arena 分配、算子精简），构建更繁琐但最有价值。

## 已锁定版本

- Python 3.13（wheels 覆盖 3.10–3.13）
- TensorFlow 2.21.0 · NumPy 2.5.4 · Matplotlib 3.11.2
- SDL3 3.4.18 · SDL3_ttf 3.2.2（内置 FreeType 2.13.2）
