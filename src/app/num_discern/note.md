# num_discern 学习笔记

> 记录时间：2026-10-24
> 代码位置：`src/app/num_discern/`
> 相关文件：[train_mnist_mlp.py](../../../tools/train_mnist_mlp.py)、[01_tensorflow_smoke.py](../../../verify/01_tensorflow_smoke.py)、[AGENTS.md](../../../AGENTS.md)
> 本机环境：Windows + MinGW-w64 GCC 16.2.0 + CMake 3.27 + Python 3.12 + TensorFlow 2.21.0

## 0. 六句话总结

1. `num_discern` 是“SDL 画布手写 → 预处理成 28×28 → 纯 C MLP 推理 → 日志输出”的手写数字识别 app。
2. 模型是 **784 → 32 → 10** 的全连接网络（MLP），25 450 个 float32 参数，MNIST 测试集准确率 96.14%。
3. 权重不是运行时加载的模型文件，而是训练脚本生成的 C 头文件 [num_discern_model_weights.h](num_discern_model_weights.h)，直接编进固件。
4. 模型输入是一维 `float[784]`：28×28 灰度图按行展开，值域 `[0,1]`，**白字黑底**。
5. **“输入用 uint8”不等于量化**；量化指权重和激活都变成 int8，并用整数乘加、int32 累加。
6. 把输入改成 32×32 的 1-bit 位图**不会减小模型、也不会加速**：权重才是大头，32×32 还会让首层参数和计算量增加约 30%。

---

## 1. app 概览

### 1.1 数据流

```
320×240 RGB565 画布
   │  触摸轮询 60 Hz，抬起后空闲 1 s
   ▼
canvas_service 触发，拷贝画布快照
   │  RGB565 → 亮度 → 反相
   ▼
墨迹图 320×240（0..255）
   │  阈值 16 去噪 + 求包围盒
   ▼
裁剪区域
   │  等比缩放，长边缩到 20 px
   ▼
20×20 数字，居中放置
   │  按质心平移到 28×28 中央
   ▼
float input[784] ∈ [0,1]
   │  MLP 784→32→10
   ▼
argmax → 日志 "recognized digit: N"
```

### 1.2 关键文件

| 文件 | 作用 |
|---|---|
| [num_discern.c](num_discern.c) | 注册触发回调；预处理；调用推理；三个预处理预览窗 |
| [num_discern_model.c](num_discern_model.c) | 纯 C 前向：两层全连接 + ReLU + argmax |
| [num_discern_model.h](num_discern_model.h) | `ND_INPUT_SIZE=784`、`ND_HIDDEN_SIZE=32`、`ND_OUTPUT_SIZE=10` |
| [num_discern_model_weights.h](num_discern_model_weights.h) | 自动生成的权重/偏置，25 450 个 float |
| [canvas_service.c](../../service/canvas/canvas_service.c) | 画布、触摸、空闲触发、回调列表 |
| [bsp_display_sdl.c](../../bsp/private/sdl/bsp_display_sdl.c) | RGB565 帧缓冲 + SDL 窗口 |
| [bsp_touch_sdl.c](../../bsp/private/sdl/bsp_touch_sdl.c) | 鼠标左键映射为单点触摸 |

### 1.3 在框架中的启动顺序

1. 构造函数把各模块注册进函数表（[c_function_regedit.h](../../framework/c_function_regedit.h)）。
2. `enter_framework()` 按组调用初始化：
   - framework：调度器、轮询池、日志、KV…
   - service：`canvas_service_init()` 打开 display/touch、分配快照缓冲、创建 60 Hz 轮询任务；
   - app：`num_discern_init()` 注册 trigger 回调。
3. `e_sched_start()` 释放所有等待中的任务线程。
4. `arch_run()` 在主线程泵 SDL 事件，直到收到退出事件。

`num_discern` 自己不做轮询，只被动等 `canvas_service` 的回调，因此 app 层很薄。

---

## 2. 预处理细节

入口是 [num_discern.c](num_discern.c) 的 `num_discern_trigger_cb()`：

1. **格式检查**：只接受 `RGB565`，尺寸不能超过 `BSP_DISPLAY_WIDTH × BSP_DISPLAY_HEIGHT`。
2. **反相**：`ink = 255 - luminance`。画布是白底黑笔，MNIST 是黑底白字，所以必须反相，否则模型看到的极性完全相反。
3. **包围盒**：跳过 `<= 16` 的像素，求 min/max x/y；没有墨迹就跳过。
4. **等比缩放**：长边缩到 20 px，短边按比例，居中放进 20×20；缩放采用区域平均，避免锯齿。
5. **质心居中**：在 20×20 上算质心，平移到 28×28 中央，空出来的地方填 0。
6. **归一化**：`input[y*28+x] = v / 255.0f`，得到 `[0,1]` 的 float。

三个预览窗对应的就是这三步的中间结果：

- `1. ink + bbox`：反相墨迹 + 红色包围盒；
- `2. 20x20 digit`：等比缩放后的数字；
- `3. 28x28 input`：模型真正看到的张量（放大 8 倍显示）。

调试时优先看第 3 个窗：它和 `input[784]` 是同一份数据。

### 2.1 容易踩的坑

- **极性**：训练用白字黑底，推理也要白字黑底；反相漏了准确率会崩。
- **归一化**：训练 `/255`，推理也要 `/255`；不要额外减均值/除方差。
- **展平顺序**：训练和推理都必须是行优先 `y*28+x`；转置后形状仍匹配，但准确率会变成随机猜。
- **居中**：MLP 没有平移不变性，包围盒 + 质心居中是在帮它消除位置差异。
- **热路径内存**：`e_heap_free` 是空操作（bump allocator），回调里不要反复 `e_heap_malloc`；代码里用静态缓冲 `s_ink`、`s_gray` 是对的。

---

## 3. MNIST 是什么

| 项目 | 内容 |
|---|---|
| 全称 | Modified National Institute of Standards and Technology database |
| 样本 | 70 000 张手写数字 |
| 划分 | 60 000 训练 + 10 000 测试 |
| 尺寸 | 28×28，单通道灰度 |
| 像素值 | 0–255，**白字黑底** |
| 标签 | 0–9，10 类 |
| 来源 | 美国人口普查局员工 + 高中生手写，做过尺寸归一化和居中 |

训练脚本里就是一行：

```python
(x_train, y_train), (x_test, y_test) = tf.keras.datasets.mnist.load_data()
x_train = x_train.reshape(-1, 784).astype(np.float32) / 255.0
```

MNIST 是机器学习里的 “Hello World”：数据小、任务直观，但包含真实的手写差异（粗细、倾斜、连笔），足够用来验证整条训练/部署链路。

数据集首次使用会下载到 `%USERPROFILE%\.keras\datasets\`。

---

## 4. MLP 是什么

**MLP = Multi-Layer Perceptron，多层感知机**：若干层神经元，每层与上一层全连接，信号只向前传。

本仓库的网络：

```
输入层 784            隐藏层 32              输出层 10
(28×28 像素)   →   ReLU(全连接)   →   softmax(全连接)
```

严格说这是 **1 个隐藏层的 MLP**（输入层不算计算层）。

### 4.1 前向公式

```
h[i]   = ReLU( Σ_j W1[j][i] * x[j] + b1[i] )     j=0..783, i=0..31
out[k] = Σ_i W2[i][k] * h[i] + b2[k]             k=0..9
```

`ReLU(v) = max(0, v)`。没有非线性激活，两层线性变换叠起来仍等价于一层线性变换。

C 代码见 [num_discern_model.c](num_discern_model.c)：

```c
s = nd_b1[i];
for (j = 0; j < 784; j++)
    s += input[j] * nd_w1[j * 32 + i];
hidden[i] = (s > 0.0f) ? s : 0.0f;

s = nd_b2[k];
for (i = 0; i < 32; i++)
    s += hidden[i] * nd_w2[i * 10 + k];
```

### 4.2 参数量

| 层 | 权重 | 偏置 |
|---|---:|---:|
| 784 → 32 | 784×32 = 25 088 | 32 |
| 32 → 10 | 32×10 = 320 | 10 |
| **合计** | | **25 450 个 float32 ≈ 99.4 KiB** |

这个规模是刻意选的：小到能塞进单片机，又足够在 MNIST 上到 96% 左右。

### 4.3 训练与推理

- **训练**（TensorFlow/Keras）：前向 → softmax → 交叉熵 → 反向传播 → Adam 更新；10 epochs、batch 128。
- **推理**（C）：只做前向；softmax 单调，分类只需 **argmax**，所以 C 里没有 softmax。

### 4.4 MLP 的局限

- 每个隐藏神经元都连到全部 784 个像素，**不知道二维相邻关系**；
- 数字平移几个像素，对 MLP 就是完全不同的输入向量；
- 笔画粗细/倾斜的泛化不如 CNN；
- MNIST 上 MLP 约 96%，CNN 通常 99% 以上。

所以预处理里的“包围盒 + 质心居中”不是可选项，而是在替 MLP 消除平移差异。

---

## 5. 权重头文件是什么、怎么来的

### 5.1 它是什么

[num_discern_model_weights.h](num_discern_model_weights.h) 是**训练脚本自动生成的 C 头文件**，不是手写代码：

```c
/* 本文件由 tools/train_mnist_mlp.py 自动生成，勿手改。 */
/* MNIST MLP 784->32->10，test accuracy = 0.9614 */

static const float nd_w1[25088] = { ... };  // 784×32
static const float nd_b1[32]     = { ... };
static const float nd_w2[320]    = { ... }; // 32×10
static const float nd_b2[10]     = { ... };
```

- `nd_w1` 按 **行主序** 存 `[输入][隐藏]`，`nd_w1[j*32+i]` 表示 `input[j] → hidden[i]`；
- 每行 8 个数，用 `.9g` 格式输出：9 位有效数字刚好能无损回读 float32；
- 文件约 398 KB 源码，编译后占 `.rodata` 约 99.4 KiB；
- 数组是 `static const`，只被 `num_discern_model.c` 包含，不污染其它编译单元。

### 5.2 怎么生成

脚本：[train_mnist_mlp.py](../../../tools/train_mnist_mlp.py)

1. 加载 MNIST，`reshape(-1,784) / 255`；
2. 建 `Input(784) → Dense(32, relu) → Dense(10, softmax)`；
3. Adam + `sparse_categorical_crossentropy`，10 epochs，batch 128；
4. 在 10 000 张测试集上评估，把 accuracy 写进文件头；
5. `get_weights()` 取出 `(w1,b1)`、`(w2,b2)`，展平后写成 C 字面量。

重新生成（会覆盖当前文件）：

```bat
.venv\Scripts\python.exe tools\train_mnist_mlp.py
```

注意：

- 训练有随机性，重跑后权重和准确率会变化；
- 不要把矩阵转置后再导出，否则 C 侧下标全部错位，准确率会崩；
- 这个文件提交进仓库是有意的：没装 TensorFlow 的机器也能直接编译、复现模型。

### 5.3 它和 TFLite 模型文件的关系

- 现在：权重直接变成 C 数组，**没有 flatbuffer、没有运行时解析、没有量化参数**；
- TFLite/TFLM：权重存在 `.tflite` flatbuffer 里，带 shape、dtype、量化 scale/zero_point，由解释器读取；
- 两者本质相同：都是“按固定顺序排列的权重 + 一份前向计算逻辑”。

这一步可以看作“手工版 TFLite”：理解了这个头文件，再去看 flatbuffer 就不会觉得神秘。

### 5.4 验证结果

本机用 [export_mnist_test.py](../../../tools/export_mnist_test.py) 导出前 2 000 张 MNIST 测试图，再用 [test_predict.c](../../../tools/test_predict.c) 编译 C 前向：

```
accuracy = 1903 / 2000 = 95.15%
```

文件头写的是 10 000 张整体 96.14%，前 2 000 张 95.15% 属于正常波动，说明**导出的权重和 C 推理是一致的**。

---

## 6. 输入张量如何描述图片

### 6.1 形状与类型

| 属性 | 值 |
|---|---|
| 逻辑形状 | 单样本 `(784,)`；带 batch 时 `(N, 784)` |
| 张量阶 | 1（单样本）/ 2（batch） |
| 数据类型 | `float32` |
| 每样本字节 | 784 × 4 = **3 136 B** |
| 值域 | `[0.0, 1.0]` |
| 通道 | 1（灰度，无 RGB） |
| 布局 | 行优先、连续内存 |

C 侧函数只收到裸指针：

```c
int num_discern_predict(const float* input);
```

**函数不知道这是 28×28**，也不知道图片长什么样；它只知道从 `input[0]` 开始有 784 个 float，顺序必须和训练时一致。

### 6.2 索引映射

规则只有一条：

```
index = y * 28 + x      // y 从上到下，x 从左到右
```

| 像素位置 | 下标 |
|---|---|
| 左上 `(0,0)` | 0 |
| 右上 `(27,0)` | 27 |
| 第二行最左 `(0,1)` | 28 |
| 左下 `(0,27)` | 756 |
| 右下 `(27,27)` | 783 |

小例子（把 28 换成 3）：

```
1 2 3
4 5 6    →  [1,2,3,4,5,6,7,8,9]
7 8 9
```

空间关系不写在张量里，而是隐含在权重下标中：`input[0]` 和 `input[1]` 是相邻像素这件事，模型只能通过训练自己学出来。

### 6.3 每个数字代表什么

- 0.0 = 纯白背景；
- 1.0 = 实心黑笔迹（反相后为 255，再除以 255）；
- 中间值 = 抗锯齿边缘，例如 0.4、0.7。

MNIST 原始像素 0–255 本身就是 8-bit 量化后的光强；`/255` 是把它**反量化**回 `[0,1]` 的实数。所以“输入用 uint8 存储”并不引入额外量化误差，真正损失精度的是权重/激活量化。

### 6.4 训练/推理之间的“契约”

| 契约 | 本仓库做法 | 违反后果 |
|---|---|---|
| 展平顺序 | 行优先 `y*28+x` | 形状仍匹配，准确率接近随机 |
| 极性 | 白字黑底（画布反相） | 准确率崩 |
| 归一化 | `/255`，`[0,1]` | 分布偏移，准确率下降 |
| 尺寸 | 固定 28×28 | 维度不匹配或信息丢失 |
| 位置 | 包围盒 + 质心居中 | 平移敏感 |
| 通道 | 单通道灰度 | 维度/语义错 |
| 类型 | float32 | C 侧读错内存 |

### 6.5 可视化/调试

ASCII 打印：

```c
for (int y = 0; y < 28; y++) {
    for (int x = 0; x < 28; x++)
        putchar(input[y*28+x] > 0.5f ? '#' : '.');
    putchar('\n');
}
```

Python 读回：

```python
import numpy as np
v = np.fromfile("input.bin", dtype=np.float32)
img = v.reshape(28, 28)   # 行优先，直接 reshape
```

### 6.6 与 TFLite/TFLM 张量的区别

| | 当前 C 数组 | TFLite/TFLM |
|---|---|---|
| 形状 | 代码约定，运行时无元数据 | `dims` 里带 `[1,784]` 或 `[1,28,28,1]` |
| 类型 | 固定 `float` | `float32/int8/uint8/...` |
| 字节数 | 调用方自己保证 | `bytes` 字段 |
| 量化参数 | 无 | `scale`、`zero_point` |
| 内存 | 调用方给 | arena 分配 |

换成 int8 模型后，输入要量化：

```c
int8_t q = (int8_t)lrintf(x / scale) + zero_point;
```

---

## 7. “输入用 uint8 是不是量化？”

### 7.1 结论

**不是。** 判断标准：**乘加运算本身是不是也变成了整数。**

| 做法 | 输入 | 权重 | 实际计算 | 算量化吗 |
|---|---|---|---|---|
| uint8 图像 → `/255` → float | uint8→float32 | float32 | 浮点 | ❌ 只是输入管线 |
| 输入层声明 uint8，后面 Cast/Rescale | uint8 | float32 | 浮点 | ⚠️ 只量化输入，hybrid |
| 输入 int8 + 权重 int8 + 激活 int8，int32 累加 | int8 | int8 | 整数 | ✅ 标准全整数量化 |
| QAT：训练时插 fake-quant | float（模拟） | float（模拟） | 浮点训练 | ✅ 量化感知训练 |

### 7.2 量化的数学

```
q = round(x / S) + Z          // 量化
x ≈ S * (q - Z)               // 反量化
```

MNIST 输入的理想参数：

- uint8：`S = 1/255, Z = 0`，像素 `p` 直接就是 `q = p`；
- int8：`S = 1/255, Z = -128`，`q = p - 128`。

例子：`p = 200`

```
x = 200/255 = 0.7843137
uint8: q = 200            → x = 200/255          = 0.7843137
int8 : q = 200 - 128 = 72 → x = (72+128)/255     = 0.7843137
```

**对 MNIST 输入来说，8-bit 量化是无损的**，因为原始数据本来只有 256 个可能值。精度损失主要来自权重和激活的量化。

### 7.3 只量化输入为什么没收益

| 项目 | float32 | int8 | 节省 |
|---|---:|---:|---:|
| 输入（784） | 3 136 B | 784 B | 2.3 KB |
| 权重（25 450） | 101 800 B | 25 450 B | **76 KB** |

- 输入只占 2.3 KB，权重才是大头；
- 权重仍是 float，矩阵乘仍是浮点，用不上 int8 SIMD / CMSIS-NN；
- 标准 TFLM 算子要求权重和激活都是量化类型，只量化输入容易被插入 `Dequantize`，变成 hybrid。

### 7.4 真正的量化路线

1. **PTQ（训练后量化）**：训练 float 模型 → representative dataset 校准 → 导出 int8 `.tflite`；
2. **QAT（量化感知训练）**：训练时插 fake-quant，让权重提前适应量化误差，小模型更稳；
3. **手写 int8 推理**：把权重压成 int8，自己写 int32 累加 + requantize，适合理解 TFLM 算子。

仓库里现成的 PTQ 例子是 [01_tensorflow_smoke.py](../../../verify/01_tensorflow_smoke.py)：

```python
conv.optimizations = [tf.lite.Optimize.DEFAULT]
conv.representative_dataset = representative_dataset
conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
conv.inference_input_type = tf.int8
conv.inference_output_type = tf.int8
tflite_int8 = conv.convert()
```

TFLite 的量化模式不止一种：

| 模式 | 权重 | 激活 | 计算 | 主要收益 |
|---|---|---|---|---|
| Float16 | float16 | float32 | 浮点 | 体积减半 |
| Dynamic range | int8 | 运行时动态量化 | 浮点为主 | 体积约 1/4 |
| **Full integer** | int8 | int8 | **整数** | 体积、算力、TFLM 目标 |
| QAT | int8（模拟） | int8（模拟） | 浮点训练 | 精度更好 |

### 7.5 常见误区

- **`uint8` 位图 ≠ 1-bit 位图**：1 bit 是二值化，只有两个等级，比 8-bit 激进得多。
- **归一化 ≠ 量化**：`/255` 是反量化/归一化；量化是反向映射到整数格点。
- **训练 dtype ≠ 部署 dtype**：训练时用 uint8 输入，不会自动让模型变成 int8；决定权在 converter 的 `optimizations` / `representative_dataset` / `inference_input_type`。
- **输入量化 ≠ 模型量化**：前者只是入口，后者要求权重、激活、算子全部整数化。

---

## 8. 32×32、1-bit 输入能减小模型、加快推理吗？

### 8.1 结论

**不能。** 32×32 1-bit 只压缩输入缓冲（3 136 B → 128 B），但：

- 权重没变（甚至因为输入变大而增加）；
- 乘加还是 float，不会变成整数；
- 还要额外拆位，通常更慢；
- 1-bit 输入在标准 TFLM 里没有对应的张量类型，必须先解包。

### 8.2 算账

当前：`784 → 32 → 10`，25 450 个 float。

| 方案 | 输入存储 | 首层权重数 | 权重存储 | 首层乘加 | 相对当前 |
|---|---:|---:|---:|---:|---:|
| 当前 28×28 float32 | 3 136 B | 25 088 | ≈ 99.4 KiB | 25 088 | 基准 |
| 32×32 1bpp + float32 权重 | 128 B | 32 768 | ≈ 129.4 KiB | 32 768 | 权重 +30%，算力 +30% |
| 28×28 1bpp + float32 权重 | 98 B | 25 088 | ≈ 99.4 KiB | 25 088（+拆位） | 静态省 3 KB，算力不变/更慢 |
| 28×28 全 int8 | 784 B | 25 088 | ≈ 24.9 KiB | 25 088 int8 | 体积 -75%，可加速 |
| 28×28 BNN（权重也 1-bit） | 98 B | 25 088 bit | ≈ 3.1 KiB | 约 800 次字操作 | 体积 -97%，数量级加速 |

关键数字：

- 第一层占全部乘加的 **98.7%**（25 088 / 25 408）；
- 32×32 首层权重从 `784×32` 变成 `1024×32`，多 7 680 个 float，约 **+30 KB**；
- 输入省 3 KB，但权重多 30 KB，**总静态反而 +26%**。

### 8.3 为什么只压输入不加速

1. **权重是大头**：99.4 KiB 权重 vs 3.1 KiB 输入，输入只是零头。
2. **MAC 次数由权重决定**：输入 0/1 不会让编译器自动生成整数指令，`W` 仍是 float。
3. **拆位有开销**：每个像素 `shift + and`，1 024 个像素几千条额外指令。
4. **标准 TFLM 没有 1-bit 张量**：模型看到的仍是 `float[1024]` 或 `int8[1024]`，128 B 只是外部存储格式，arena 里仍要 1–4 KB。
5. **精度代价**：二值化丢抗锯齿灰度，细笔画/易混数字（3/8、4/9）更容易错。

### 8.4 什么情况下 1-bit 有价值

- **BNN（权重也二值化）**：`XNOR + popcount` 代替乘加；28×28 首层约 800 次字操作，权重约 3.1 KiB。但需要 BNN 训练（STE）、缩放因子和自定义 kernel，TFLM 不内置。
- **稀疏跳过**：只遍历为 1 的位，`acc += w[j]`；数字笔迹通常只占 10–20% 像素，理论省 80% 加法，但分支/间接寻址可能抵消，需实测。
- **通信/存储**：128 B 比 3 KB 好传，但这优化的是传输，不是推理。

### 8.5 真正减小模型/加速的路线

1. **全 int8 量化（PTQ/QAT）**：权重 4× 压缩，整数乘加，Cortex-M 上通常 2–4× 加速；首选。
2. **减小隐藏层 / 剪枝**：32 → 16，首层权重减半，但可能掉点。
3. **降低输入分辨率**：28×28 → 14×14，首层权重 1/4，但丢细节。
4. **BNN**：极致体积/速度，训练复杂、算子要自己写，适合进阶。
5. **只压输入**：收益几 KB，精度有风险，计算不变，不推荐单独做。

### 8.6 给本项目的建议

- 先测 float 基线（准确率 + 单次推理耗时）；
- 再做 int8 PTQ，比较模型字节数、准确率、耗时；
- 若掉点明显，试 QAT 或把隐藏层加到 64；
- 不要用 32×32 1-bit 直接喂未修改的 28×28 模型（维度不匹配）；
- PC 上的速度不代表 MCU：x86 浮点强，Cortex-M0/M3 无 FPU 时 float 是软件模拟，int8 收益会非常明显。

---

## 9. 本机验证记录（2026-10-24）

### 9.1 环境

- 已执行 `setup.ps1`：`.venv` + `third_party` 就绪；
- Python 3.12.0；TensorFlow 2.21.0；NumPy 2.5.4；Matplotlib 3.11.2；
- SDL3 3.4.18、SDL3_ttf 3.2.2、DejaVuSans.ttf；
- CMake 3.27.0-rc3；MinGW-w64 GCC 16.2.0；git 2.38.1。

### 9.2 构建与启动

```bat
build.bat debug
```

- 构建成功，产物 `build/debug/tflm_framework.exe`，SDL3.dll 自动拷贝；
- 启动日志正常：调度器、KV、canvas_service、num_discern、框架版本都打印；
- app 窗口标题 `bsp_display (SDL3)`，尺寸 320×240。

### 9.3 TensorFlow 冒烟

```bat
.venv\Scripts\python.exe verify\01_tensorflow_smoke.py
```

- 输出 `SMOKE OK`；
- float TFLite 3 252 B，int8 3 464 B；
- int8 对 `sin(1.0)` 预测 0.3824（真值 0.8415）——这是 5 epochs 的极小模型 + 量化误差，冒烟脚本只验证管线，不作为精度基准。
- 备注：PowerShell 会把 TF 的 stderr 日志当成错误，出现 `NativeCommandError`/退出码 1，但脚本本身成功。

### 9.4 C 推理准确率

- 导出 2 000 张 MNIST 测试图，编译 `test_predict.c`；
- 结果 `1903 / 2000 = 95.15%`；
- 权重头文件标注 96.14%（10 000 张整体），一致。

### 9.5 端到端触发

用 Win32 `PostMessage` 模拟鼠标在 `bsp_display (SDL3)` 窗口画了一条竖线和一个椭圆：

- 1 s 空闲后成功触发 `canvas_service`；
- 三个预处理窗口 + `canvas_service_test` 预览窗口正常创建；
- 推理执行，日志输出 `recognized digit: 8`。

**链路（触摸 → 触发 → 预处理 → 推理 → 日志）验证通过。** 但合成笔迹（粗直线/椭圆）与 MNIST 书写风格差异较大，识别结果不能当作模型准确率；后续可用真实手写样本对比，或检查预处理后的 28×28 输入是否与预期一致。

### 9.6 仓库状态

- 分支 `develop`，工作区干净；
- 最近提交：`a9b9b87 feat: ✨优化数字识别应用的预处理流程，提高准确率`；
- `.venv` 约 1.7 GB，`third_party` 约 26 MB，`build` 约 7 MB（都在 `.gitignore` 中）。

---

## 10. 下一步 / 待办

- [ ] **P1 正弦波**：AGENTS.md 里仍未勾选；密集网络拟合 `sin`，int8 量化后 SDL 实时画“预测 vs 真值”。
- [ ] **num_discern 的 int8 PTQ**：导出 `.tflite`，对比 float vs int8 的准确率/体积/耗时。
- [ ] **QAT**：如果 PTQ 掉点明显，用 fake-quant 微调。
- [ ] **测耗时**：用 `SDL_GetPerformanceCounter()` 包住 `num_discern_predict()`，循环 1 000 次；PC 数据只作参考。
- [ ] **输入调试**：把 `input[784]` 打印成 ASCII 或 dump 成 bin，确认预处理与 MNIST 分布一致。
- [ ] **真实手写验证**：用鼠标写 0–9 各若干次，统计识别结果，观察域差异。
- [ ] **清理测试窗口**：当前 `canvas_service_test` + 3 个预处理窗共 4 个窗口；正式演示可只在需要时注册。
- [ ] **文档同步**：README 的目录树还写着 `app_demo.c`/`symbol_table` 等旧结构；`git_info.h` 也还是 `main`/init commit，启动日志里的 Git 信息是旧的。

---

## 11. 一句话回顾

**输入只是入口，模型大小和算力由权重与乘加次数决定。** 先把 float 基线跑稳，再做 int8 全量化；1-bit/32×32 输入是“看起来很美”的微优化，除非连权重一起二值化（BNN），否则省不了模型、也快不了推理。
