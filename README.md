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
├── setup.ps1              # Windows 一键安装（首选）
├── setup.sh               # Linux/macOS 一键安装
├── requirements.txt       # Python 依赖（已锁定版本）
├── scripts/
│   └── fetch_sdl3.py      # 下载并解压 SDL3 到 third_party/（跨平台，stdlib only）
├── verify/
│   ├── 01_tensorflow_smoke.py   # TF 训练 + int8 量化 + 推理冒烟测试
│   └── 02_sdl3_smoke/           # SDL3 窗口冒烟测试（C + CMake）
├── .venv/                 # （不提交）本地虚拟环境
└── third_party/           # （不提交）SDL3 开发包
```

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
