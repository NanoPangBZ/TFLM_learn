# num_discern_uint8 实验计划（草案）

> 状态：草案，待确认后执行
> 目标目录：`src/app/num_discern_uint8/`
> 关联代码：[num_discern.c](../num_discern/num_discern.c)、[num_discern_model.c](../num_discern/num_discern_model.c)、[num_discern_model_weights.h](../num_discern/num_discern_model_weights.h)
> 关联笔记：[note.md](../num_discern/note.md)、[note_layers.md](../num_discern/note_layers.md)
> 默认路线：**先纯 uint8 手写推理 → 再 TFLite 风格 int8 对照 → 最后可选 TFLM**

---

## 0. 一句话目标

在 `num_discern_uint8` 里做一个 **8-bit 整数版的手写数字识别 app**：用同一套画布输入，理解 `scale / zero_point / int32 累加 / 再量化 / ReLU / 饱和` 的完整数学，并在 MNIST 上量化对比 float 模型的准确率、体积和耗时。

---

## 1. 目标

1. **理解** affine quantization（仿射量化）的每一步，而不是只会调 TFLite API。
2. **实现** 纯 C 的 uint8 全连接前向：输入、权重、隐藏层激活、输出都用 8-bit 整数。
3. **验证** C 推理与 Python/NumPy 模拟结果一致，且在 MNIST 上准确率接近 float 基线。
4. **测量** 量化前后的模型体积、运行时内存、单次推理耗时、argmax 一致率。
5. **对比** 纯 uint8 与 TFLite 风格 int8 的差异，为后续 TFLM 做准备。
6. **沉淀** 一份可复现的实验记录和可运行 app，供后续 CNN/TFLM 复用。

---

## 2. 非目标（本阶段不做）

- 不做 CNN 量化（先把 MLP 的 8-bit 数学吃透）。
- 不直接集成 TFLM 源码（Phase 4 只做 TFLite 解释器对照，TFLM 单独评估）。
- 不做 QAT（Phase 5 可选实验）。
- 不追求 PC 上的极致速度（PC 浮点很强，速度结论以 MCU 为准）。
- 不改动现有 float app 的行为（通过配置开关切换，不破坏基线）。

---

## 3. 关键认知：uint8 的两种含义

在动手前必须先定清楚“uint8 量化”指哪一种，否则实现和 TFLite 对不上。

| 方案 | 输入 | 权重 | 激活 | 特点 |
|---|---|---|---|---|
| **A. 纯 uint8** | uint8 | uint8 | uint8 | 学习价值最高；权重 zero_point 不为 0，需要零点修正 |
| **B. TFLite 风格** | uint8 | int8（对称） | int8 或 uint8 | 工程标准；权重 zero_point=0，累加更简单 |
| **C. 仅 uint8 输入** | uint8 | float32 | float32 | **不是量化**，只是存储格式 |

**本计划默认先做 A（纯 uint8）**，因为它能让你亲手处理：

```
acc = Σ (q_x - Z_x) * (q_w - Z_w) + q_b
```

这个展开式里的零点修正项。做完 A 之后，再把权重换成 int8 对称（B），就能体会 TFLite 为什么这么设计。

> 重要：现代 TFLite 的 full-integer 模型内部通常是 **int8** 权重/激活，`uint8` 只用于输入/输出边界。纯 uint8 全网络不是 TFLite 的标准形态，但作为学习实验完全成立。

---

## 4. 量化方案设计

### 4.1 符号约定

对任意实数 `x`，仿射量化：

```
q = round(x / S) + Z          // 量化
x ≈ S * (q - Z)               // 反量化
```

- `S`：scale，实数；
- `Z`：zero_point，整数；
- `q`：量化后的整数。

纯 uint8 时 `q ∈ [0, 255]`。

### 4.2 输入层

输入是 28×28 的灰度图，像素本来就是 `0..255`：

```
S_x = 1/255, Z_x = 0
q_x = 像素值（0..255）
```

**这是无损的**：像素 200 就是 `q=200`，反量化 `200/255=0.7843`，和 float 模型完全一致。

因此 uint8 app 的预处理**不需要 `/255`**，直接输出 `uint8_t input_q[784]` 即可。这是它比 float app 更自然的地方。

### 4.3 权重

第一层 `W1`（784×32）和第二层 `W2`（32×10）分别做 per-tensor 量化（Phase 1）：

```
min_w = min(W), max_w = max(W)
S_w = (max_w - min_w) / 255
Z_w = round(-min_w / S_w)
q_w = clamp(round(W / S_w) + Z_w, 0, 255)
```

- 如果 `max_w == min_w`，取 `S_w = 1, Z_w = 0`；
- 权重的 `Z_w` 通常接近 128（因为权重围绕 0 分布）；
- 这个零点会让累加多出修正项，见 4.7。

Phase 5 再尝试 **per-output-channel**：每个输出神经元一套 `S_w[i], Z_w[i]`，通常更准，但 C 结构体和循环要改。

### 4.4 隐藏层激活

隐藏层是 ReLU 输出，最小值天然是 0：

```
h_real = max(0, z_real)
```

因此取：

```
Z_h = 0
S_h = max_h / 255
```

`max_h` 用校准集统计，建议先取 **99.9 分位**而不是绝对最大值，避免离群值把 scale 拉爆；同时统计被截断的比例。如果 `max_h == 0`（校准集全黑），取 `S_h = 1`。

```
q_h = clamp(round(h_real / S_h), 0, 255)
```

因为 `Z_h=0`，**负的 `h_real` 经过 round 后 clamp 到 0，天然实现了 ReLU**，不需要单独写 `if (z < 0) z = 0`。

### 4.5 输出层

输出是 10 个 logits，有正有负：

```
min_o = min(logits), max_o = max(logits)
S_o = (max_o - min_o) / 255
Z_o = round(-min_o / S_o)
q_o = clamp(round(logits / S_o) + Z_o, 0, 255)
```

对单套 `S_o/Z_o`，仿射映射是单调的，所以 **argmax(q_o) = argmax(logits)**；但如果饱和严重，多个类别可能被截到同一个值，导致并列。要统计输出端 `q==0` 和 `q==255` 的比例。如果 `max_o == min_o`，取 `S_o = 1, Z_o = 0`。

> 如果以后做 per-channel 输出权重，各类的 `S_w` 不同，**不能直接比较原始 q 值**，必须先反量化再 argmax，或者统一用同一个输出 scale。第一版保持输出层 per-tensor，避免这个坑。

### 4.6 偏置

偏置不量化成 uint8，而是 **int32**，因为它要直接加进累加器：

```
S_b = S_x * S_w
q_b = round(b / S_b)
```

第二层：

```
S_b2 = S_h * S_w2
q_b2 = round(b2 / S_b2)
```

`q_b` 可能超出 int16，必须用 int32 存储。

### 4.7 累加与零点修正（核心）

把 `x = S_x(q_x - Z_x)`、`w = S_w(q_w - Z_w)`、`b = S_x S_w q_b` 代入：

```
z = Σ x*w + b
  = S_x S_w [ Σ (q_x - Z_x)(q_w - Z_w) + q_b ]
```

令：

```
acc = Σ (q_x - Z_x)(q_w - Z_w) + q_b
```

展开：

```
acc = Σ q_x q_w
      - Z_x Σ q_w
      - Z_w Σ q_x
      + N Z_x Z_w
      + q_b
```

其中 `N = n_in`。对每个输出神经元，可以预计算一个常数：

```
c[i] = -Z_x * Σ_j q_w[j][i] + N * Z_x * Z_w + q_b[i]
```

推理时只需：

```
sum_x = Σ_j q_x[j]                 // 每次推理算一次
acc[i] = dot(q_x, q_w[:, i]) + c[i] - Z_w * sum_x
```

这就是纯 uint8 权重比 int8 权重多出来的开销：一个 `sum_x` 和一次乘法。它仍然很便宜，而且非常适合教学。

**当输入 `Z_x=0`（本方案）时**，公式简化为：

```
c[i] = q_b[i]
acc[i] = dot + q_b[i] - Z_w * sum_x
```

第二层输入是 `q_h`，且 `Z_h=0`，所以同样简化：

```
acc2[i] = dot(q_h, q_w2[:, i]) + q_b2[i] - Z_w2 * sum_h
```

### 4.8 再量化（requantize）

第一层累加器的实数值：

```
z_real = S_x * S_w * acc
```

隐藏层：

```
q_h = clamp(qround(z_real / S_h), 0, 255)
    = clamp(qround(acc * M1), 0, 255)
M1 = (S_x * S_w) / S_h
```

第二层：

```
o_real = S_h * S_w2 * acc2
q_o = clamp(qround(o_real / S_o) + Z_o, 0, 255)
    = clamp(qround(acc2 * M2) + Z_o, 0, 255)
M2 = (S_h * S_w2) / S_o
```

**Phase 1/2 用 float 乘子 `M` 做再量化**，先保证数学正确、和 Python 对得上。
**Phase 3 以后**再换成 TFLM 风格的整数乘子 + 右移：

```
q = MultiplyByQuantizedMultiplier(acc, multiplier, shift)
```

这样才是纯整数、无浮点的推理。

### 4.9 舍入、饱和、溢出

- **舍入**：定义统一的 `qround`，四舍五入、远离 0：

  ```c
  static inline int32_t qround(float x) {
      return (int32_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
  }
  ```

  Python 端用等价的 `floor(x+0.5)` / `ceil(x-0.5)`，不要一边用 `np.rint`、一边用 `lrintf`，否则边界样本会对不上。

- **饱和**：`q` 必须 clamp 到 `[0,255]`；统计隐藏层和输出层的截断比例。

- **溢出**：纯 uint8 权重下，最坏 dot：

  ```
  784 * 255 * 255 = 50 979 840
  ```

  加上零点修正和 bias 仍在 int32（约 21 亿）范围内；但**不要用 int16**。C 里用 `int32_t`，Python 模拟用 `int64` 再断言落在 int32 内。

### 4.10 纯 uint8 vs TFLite 风格 int8

| 项目 | 纯 uint8 | TFLite 风格 |
|---|---|---|
| 权重 | uint8，`Z_w≈128` | int8 对称，`Z_w=0` |
| 输入 | uint8，`Z_x=0` | uint8 或 int8 |
| 隐藏激活 | uint8，`Z_h=0` | int8 或 uint8 |
| 累加修正 | 需要 `-Z_w*sum_x` | `acc = dot + q_b` |
| 教学价值 | 高，能看到完整零点数学 | 中，接近工程实现 |
| 与 TFLite 对齐 | 否 | 是（大致） |

建议：**Phase 1-3 做纯 uint8；Phase 4 用 TFLite 转换做对照，Phase 5 再把权重切到 int8 对称，比较两者的准确率和代码复杂度。**

---

## 5. 目录与文件规划

```
src/app/num_discern_uint8/
├── PLAN.md                          # 本文件
├── README.md                        # 实验记录、结论、复现命令（Phase 5 写）
├── num_discern_uint8.c              # app：触发回调、预处理、日志/预览
├── num_discern_uint8_model.c        # uint8 前向：两层 q_dense + requant
├── num_discern_uint8_model.h        # 层结构体、API、维度宏
├── num_discern_uint8_weights.h      # 自动生成：q_w/q_b/q_c + scales/zps
└── (可选) num_discern_uint8_test.c  # 若不想污染 tools/，单元测试放这里

tools/
├── quantize_mnist_mlp.py            # 校准、量化、NumPy 模拟、导出头文件
├── test_predict_uint8.c             # 读 MNIST uint8 测试集，统计 C 准确率
└── export_mnist_test_u8.py          # 导出 uint8 版 test_imgs_u8.bin（可选）
```

命名约定：

- 头文件保护、`TAG`、日志宏沿用现有 app 风格；
- 生成的权重头文件顶部注明“自动生成、勿手改”，并写清量化配置和准确率；
- 所有量化参数（`S_x, Z_x, S_w, Z_w, S_h, S_o, Z_o`）都要在头文件里以注释或常量形式可查。

---

## 6. App 集成与共存

### 6.1 触发与预处理

新 app 仍然注册 `canvas_service` 的 trigger 回调：

```c
static void num_discern_uint8_trigger_cb(const canvas_service_trigger_t* trigger, void* ctx)
{
    // 1. RGB565 -> 墨迹 0..255
    // 2. 包围盒、20x20、质心居中（与 float app 相同）
    // 3. 直接写 uint8_t input_q[784]，不做 /255
    // 4. num_discern_uint8_predict(input_q) -> 0..9
    // 5. 日志：recognized digit: N
}
```

关键差异：

- float app：`float input[784] = v / 255.0f`；
- uint8 app：`uint8_t input_q[784] = (uint8_t)v`。

**同一张 28×28 灰度图，uint8 版就是它的量化输入，`S_x=1/255, Z_x=0`，完全无损。**

### 6.2 配置开关（避免两个 app 同时抢触发）

当前 CMake 会把所有 app 编进同一个可执行文件；如果两个 app 都注册回调，同一个笔迹会触发两次、弹两套窗口。建议加一个配置头 `src/app/app_cfg.h`：

```c
#define APP_NUM_DISCERN_FLOAT_ENABLE     1
#define APP_NUM_DISCERN_UINT8_ENABLE     0
#define APP_NUM_DISCERN_COMPARE_MODE     0
```

用法：

| 模式 | FLOAT | UINT8 | COMPARE | 行为 |
|---|---:|---:|---:|---|
| float 基线 | 1 | 0 | 0 | 当前行为 |
| uint8 实验 | 0 | 1 | 0 | 只跑 uint8 app |
| 并排对比 | 1 | 1 | 1 | 两个回调都跑；日志加 `[float]`/`[uint8]` 前缀，只让一个 app 建预览窗 |

对应地在现有 `num_discern.c` 和新 app 里用 `#if` 包住 `REGEDIT_APP_INIT_FUNCTION` 的注册。改动很小，但能避免“两个 app 同时画、同时清屏”的混乱。

> 更彻底的做法是把预处理抽到 `src/app/common/digit_preprocess.c/.h`，两个 app 共用。第一版建议**先复制**，等 uint8 验证通过再重构，减少对 float 基线的扰动。

### 6.3 CMake

在根 `CMakeLists.txt` 的 app 区块加：

```cmake
# ----- uint8 数字识别实验
src/app/num_discern_uint8/num_discern_uint8.c
src/app/num_discern_uint8/num_discern_uint8_model.c
```

如果希望用 CMake 选项控制，而不是改头文件，可以加：

```cmake
option(NUM_DISCERN_UINT8 "Build uint8 experiment app" OFF)
if(NUM_DISCERN_UINT8)
    target_sources(tflm_framework PRIVATE
        src/app/num_discern_uint8/num_discern_uint8.c
        src/app/num_discern_uint8/num_discern_uint8_model.c)
    target_compile_definitions(tflm_framework PRIVATE APP_NUM_DISCERN_UINT8_ENABLE=1)
endif()
```

第一版建议**直接加源文件 + 配置头**，简单直观；等实验稳定再考虑 CMake 选项。

### 6.4 预览与日志

- 第一版只建 **一个** 预览窗：`uint8 28x28 input`，显示 `input_q`（灰度 0..255）；
- 可选再建一个 `uint8 hidden` 窗，把 32 个隐藏激活画成 32 个柱状/色块，直观看到量化后的特征；
- 日志至少包含：
  ```
  [uint8] recognized digit: N
  [uint8] logits q: 12 34 ... (可选)
  ```
- 并排模式：
  ```
  [float] digit: 7
  [uint8] digit: 7
  ```
  统计两者一致率。

### 6.5 内存注意

- `e_heap_free` 是空操作，回调里**不要**反复 `e_heap_malloc`；
- `input_q[784]`、`hidden_q[32]`、`out_q[10]` 用静态缓冲或栈上数组；
- 预览窗的 `malloc` 只在首次创建时发生，之后复用。

---

## 7. 分阶段计划

### Phase 0：准备与基线（约 0.5 天）

- [ ] 创建目录和本计划（已完成）；
- [ ] 记录 float 基线：2 000 张 MNIST 准确率（当前 95.15%）、权重字节数（101 800 B）、单次推理耗时（PC，1 000 次平均）；
- [ ] 确定输入/权重/激活/输出的量化约定（本文件第 4 节）；
- [ ] 确定 app 配置模式（先 standalone uint8）。

**产出**：基线数据表；配置决策。

### Phase 1：Python 量化实验室（约 1.5 天）

目标：**不写 C**，先在 NumPy 里把量化数学跑通。

- [ ] 写 `tools/quantize_mnist_mlp.py`：
  - 读取现有 float 权重（解析 [num_discern_model_weights.h](../num_discern/num_discern_model_weights.h)，或从训练脚本导出 `.npz`）；
  - 加载 MNIST，取校准集（如训练集前 2 000 张）和测试集（2 000 / 10 000 张）；
  - 统计隐藏层 `max_h`（99.9 分位）和输出层 `min_o/max_o`；
  - 量化 `W1/b1/W2/b2`；
  - 用 NumPy 做 int32 累加 + 再量化，得到 uint8 预测；
  - 输出：float vs uint8 准确率、argmax 一致率、logit 误差、饱和比例、每层 `S/Z` 表；
- [ ] 扫几组校准策略：`max` vs `99.9%` vs `99%`；
- [ ] 导出 `num_discern_uint8_weights.h`（含 `q_w`、`q_b`、`q_c`、`S/Z`、注释）；
- [ ] 保存一组小测试向量（输入、隐藏、输出）供 C 对照。

**验收**：

- NumPy uint8 在 2 000 张上准确率 ≥ 93%（float 95.15%，允许约 2 个点下降）；
- 无 NaN、无溢出；
- 能打印每层 scale/zero_point 和饱和比例。

**产出**：量化脚本 + 权重头文件 + 量化报告。

### Phase 2：C 模型与单元测试（约 1.5 天）

- [ ] `num_discern_uint8_model.h`：定义 `q8_layer_t`、维度宏、API；
- [ ] `num_discern_uint8_model.c`：
  - `q8_dense_u8()`：int32 累加、零点修正、ReLU、requant、clamp；
  - 第一层 `relu=1, Z_h=0`；第二层 `relu=0, Z_o!=0`；
  - Phase 2 用 float `M` 做再量化；
- [ ] `tools/test_predict_uint8.c`：
  - 读 `test_imgs_u8.bin` / `test_labels.bin`；
  - 统计 2 000 张准确率；
  - 对 5~10 个固定样本，打印隐藏/输出，和 Python 结果逐项对比；
- [ ] 手算一个小例子（2 输入、1 输出）验证零点修正公式。

**验收**：

- C 与 NumPy 在测试向量上 **逐元素一致**（允许 0 误差；若用同一 `qround`，应完全一致）；
- C 在 2 000 张上的准确率与 NumPy 完全相同；
- `-Wall -Wextra` 无新增警告；
- 用 `int64` 断言中间值不越 int32。

**产出**：可独立编译的 uint8 模型 + 准确率测试。

### Phase 3：接入 app（约 1 天）

- [ ] `num_discern_uint8.c`：复制 float app 的预处理，改成输出 `uint8_t input_q[784]`；
- [ ] 注册 trigger 回调，日志输出识别结果；
- [ ] 加一个 28×28 预览窗（可选 hidden 柱状图）；
- [ ] 配置开关：先 standalone（float 关闭 / uint8 开启）；
- [ ] CMake 加源文件，`build.bat debug` 通过；
- [ ] 模拟鼠标画数字，确认触发、推理、日志正常；连续 100 次触发不崩、内存不涨。

**验收**：

- app 启动正常，窗口正常；
- 画数字后 1 s 触发，日志输出 `[uint8] digit: N`；
- 与 float app 对同一张图的结果可对比（并排模式或测试脚本）；
- 没有重复触发/重复窗口。

**产出**：可运行的 uint8 app。

### Phase 4：TFLite 对照（约 2 天，可选 TFLM）

- [ ] 把 float 模型转成 full-integer TFLite：
  ```python
  conv.optimizations = [tf.lite.Optimize.DEFAULT]
  conv.representative_dataset = ...
  conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
  conv.inference_input_type = tf.uint8
  conv.inference_output_type = tf.uint8
  ```
- [ ] 用 `tf.lite.Interpreter` 检查输入/输出张量的 `scale/zero_point`；
- [ ] 在 2 000 张测试集上比较 TFLite vs 手写 uint8 的 argmax 一致率和准确率；
- [ ] 记录差异原因（TFLite 内部 int8、per-channel、校准策略不同等）；
- [ ] （可选）TFLM：生成 C 数组、接 `MicroInterpreter`。这一步单独评估构建成本，不阻塞前三个阶段。

**验收**：

- TFLite 与手写 uint8 的 argmax 一致率 ≥ 98%；
- 差异有书面解释；
- 如果做 TFLM，能跑通一张 MNIST 图并输出正确类别。

### Phase 5：实验矩阵与总结（约 1.5 天）

- [ ] 跑实验矩阵（见第 8 节）；
- [ ] 画准确率/体积/耗时对比表；
- [ ] 试 per-channel 权重量化；
- [ ] 试 int8 对称权重（TFLite 风格），比较代码复杂度和准确率；
- [ ] （可选）试 QAT，看能否把掉点拉回来；
- [ ] 写 `README.md`：结论、复现命令、踩坑记录；
- [ ] 更新 `AGENTS.md` 的“当前进度”（如果确认完成）。

---

## 8. 实验矩阵

| 编号 | 配置 | 权重 | 激活 | 校准 | 目的 |
|---|---|---|---|---|---|
| E0 | float 基线 | float32 | float32 | — | 对照 |
| E1 | uint8 per-tensor, max | uint8 | uint8 | 绝对最大 | 最直观 |
| E2 | uint8 per-tensor, 99.9% | uint8 | uint8 | 99.9 分位 | 抗离群 |
| E3 | uint8 per-channel | uint8 | uint8 | 99.9 分位 | 提升精度 |
| E4 | int8 对称权重 | int8 | uint8 | 99.9 分位 | 接近 TFLite |
| E5 | 隐藏层 64 | uint8 | uint8 | 99.9 分位 | 容量换精度 |
| E6 | QAT（可选） | int8 模拟 | int8 模拟 | — | 恢复精度 |

每个配置记录：

- 2 000 / 10 000 张准确率；
- 与 float 的 argmax 一致率；
- 反量化 logits 的 MAE / max error；
- 权重字节数、头文件大小；
- 隐藏/输出饱和比例；
- PC 单次推理耗时（1 000 次平均）；
- 典型错误样本（可选）。

---

## 9. 验收标准

**正确性**

- [ ] C 与 NumPy 在固定测试向量上逐元素一致；
- [ ] C 与 NumPy 在 2 000 张上的准确率完全一致；
- [ ] C 与 float 的 argmax 一致率 ≥ 98%；
- [ ] 无 int32 溢出、无未定义行为（如工具链支持，开 UBSan 跑测试）。

**精度**

- [ ] uint8 在 2 000 张上 ≥ 93%（float 95.15%，允许约 2 个点下降）；
- [ ] 争取 ≥ 94.5%（per-channel 或 QAT 后）。

**体积**

- [ ] 权重从 101 800 B 降到约 25 450 B（4×）；
- [ ] 加上 `q_b/q_c` 后总静态数据 < 27 KB；
- [ ] 头文件体积明显小于 float 版（约 1/4）。

**运行**

- [ ] debug/release 都能构建；
- [ ] app 连续 100 次触发不崩、不持续吃内存；
- [ ] 日志能区分 float / uint8 结果。

**文档**

- [ ] `README.md` 记录量化参数、实验结果、复现命令；
- [ ] 权重头文件顶部写明量化配置和准确率；
- [ ] 本计划中的决策有最终结论。

---

## 10. 风险与对策

| 风险 | 表现 | 对策 |
|---|---|---|
| 零点公式符号错 | 准确率接近随机，C 与 Python 不一致 | 手算 2×1 小例子；单元测试覆盖 `Z_x/Z_w` 非零 |
| 舍入不一致 | 个别样本 C/Python 不同 | 统一定义 `qround`；Python 用同一规则 |
| 校准离群值 | scale 过大，分辨率损失 | 用 99.9 分位；统计截断比例；可视化直方图 |
| 溢出 | 结果乱跳/负数 | int32 累加；Python 用 int64 断言；边界测试 |
| ReLU 与 zero_point 混用 | 负值被量化成非零 | 本方案隐藏层 `Z_h=0`；若改 `Z_h≠0`，先 clamp 实数再量化 |
| uint8 权重偏移 | 忘记 `-Z_w*sum_x` | 预计算 `c`；小例子验证 |
| 两个 app 同时触发 | 重复窗口/重复清屏 | `app_cfg.h` 开关；先 standalone |
| 预览窗线程问题 | 偶发卡死/黑屏 | 沿用现有 app 的创建方式；第一版只建一个窗 |
| `e_heap` 耗尽 | 第 N 次触发后失败 | 静态缓冲；不在回调里 malloc |
| PC 速度误导 | 以为 int8 没加速 | PC 只测正确性；速度结论留给 MCU/DWT |
| TFLite 对不上 | 逐元素不同 | 比 argmax/反量化 logits；接受容差并解释 |
| 头文件太大 | 编译变慢 | uint8 已比 float 小 4×；可拆成多个数组文件 |

---

## 11. 时间表（按业余时间估算）

| 阶段 | 内容 | 预计 |
|---|---|---|
| Phase 0 | 准备、基线 | 0.5 天 |
| Phase 1 | Python 量化实验室 | 1.5 天 |
| Phase 2 | C 模型 + 测试 | 1.5 天 |
| Phase 3 | app 接入 | 1 天 |
| Phase 4 | TFLite 对照（可选 TFLM） | 2 天 |
| Phase 5 | 实验矩阵 + 总结 | 1.5 天 |
| **合计** | | **约 6–8 天** |

建议节奏：**Phase 1 结束先给你看准确率和 scale 表，确认后再写 C**，避免方向错了返工。

---

## 12. 第一批任务清单

- [x] 创建 `src/app/num_discern_uint8/`；
- [x] 写本计划；
- [ ] 记录 float 基线（准确率、体积、耗时）；
- [ ] 写 `tools/quantize_mnist_mlp.py` 骨架：解析权重、校准、量化、NumPy 模拟；
- [ ] 跑 E1/E2，得到第一张准确率/scale 表；
- [ ] 确认纯 uint8 路线，再进入 C 实现。

---

## 13. 需要确认的决策

1. **先做纯 uint8 还是 TFLite 风格 int8？**
   推荐：先纯 uint8（学习零点数学），Phase 4 再对照 int8。
2. **standalone 还是并排对比？**
   推荐：先 standalone（float 关闭）；准确率对比用离线测试脚本，稳定后再做并排。
3. **预处理复制还是抽公共模块？**
   推荐：先复制，uint8 验证通过后再重构，避免影响 float 基线。
4. **TFLM 现在接还是以后？**
   推荐：Phase 4 只做 TFLite 解释器对照；TFLM 作为独立任务，不阻塞当前实验。
5. **准确率目标？**
   推荐：2 000 张 ≥ 93% 为及格，≥ 94.5% 为争取目标。

---

## 14. 一句话

**先把“像素就是 uint8、权重也是 uint8、累加用 int32、再量化回 uint8”这条链路在 Python 和 C 里各跑一遍，再用 MNIST 数字证明它没有算错；TFLite/TFLM 是之后的对照，不是第一天的起点。**
