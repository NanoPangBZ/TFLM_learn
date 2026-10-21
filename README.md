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
- `third_party/SDL3/` — SDL3 开发包，由 `scripts/fetch_sdl3.py` 自动下载解压。

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
├── build.bat              # 构建 demo/ 下的目标（交互菜单，委托 scripts/build.py）
├── requirements.txt       # Python 依赖（已锁定版本）
├── cmake/
│   └── sdl3.cmake         # 共享的 SDL3 定位 / 链接 helper
├── scripts/
│   ├── fetch_sdl3.py      # 下载并解压 SDL3 到 third_party/
│   └── build.py           # build.bat 背后的交互式构建菜单
├── .vscode/               # VS Code 任务与调试配置（tasks.json / launch.json）
├── demo/                  # 学习项目
│   └── 0_sdl_setup/       # SDL3 画布（28×28 网格，含示例 pre_build.py）
├── verify/                # 安装验证冒烟测试
│   ├── 01_tensorflow_smoke.py
│   └── 02_sdl3_smoke/
├── .venv/                 # （不提交）本地虚拟环境
└── third_party/           # （不提交）SDL3 开发包
```

## 构建 demo

```bat
build.bat                 rem 方向键菜单选择目标
build.bat 0_sdl_setup     rem 直接构建指定目标
```

交互菜单用 **↑/↓** 移动高亮、**Enter** 构建、**Esc/Q** 退出（数字键可快速跳转）。
脚本会自动选择生成器（优先 MinGW，否则交给 CMake 自动选 MSVC/Ninja），
构建产物输出到 `demo/<目标>/build/`。

### 编译前脚本（可选）

每个 demo 可在自身目录放一个「编译前脚本」，`build.py` 会在 cmake 之前自动运行：

- `pre_build.py` —— 跨平台（首选，用当前 .venv 的 Python 运行）
- `pre_build.ps1` —— 仅 Windows
- `pre_build.sh` —— 仅 Linux/macOS

典型用途：训练模型 → 导出 `.tflite` → 转成 C 头文件数组，供 C++ demo 编译进去。
`demo/0_sdl_setup/pre_build.py` 是一个最小示例（仅打印，演示钩子）。

## VS Code 调试

已内置 `.vscode/tasks.json` 和 `.vscode/launch.json`：

- **Ctrl+Shift+B** —— 构建（选择 demo 目标）
- **F5** —— 用 gdb 调试 `demo/<目标>/build/<目标>.exe`

调试前请先 Ctrl+Shift+B 构建（F5 不再自动构建，避免目标重复弹两次选择）。
各 demo 默认以 `-g -O0` 编译（见 CMakeLists），带调试符号，可正常打断点。

约定：**可执行文件名 = demo 目录名**（见各 demo 的 CMakeLists），这样调试路径能通用。
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
- SDL3 3.4.18
