# num_discern_1bit

20×20、1 bit/像素的量化手写数字识别 app。命名与实现统一为：

- 目录/文件：`num_discern_1bit`
- 宏/函数前缀：`ND1_` / `num_discern_1bit_`
- 权重数组：`nd1_w1`、`nd1_b1`、`nd1_w2`、`nd1_b2`

当前结构：`400 → 24 → 10`。

## 输入格式

- 20×20 = 400 个像素 = **400 bit = 50 字节**；
- 位序：`index = y*20 + x`，`byte = index/8`，`bit = 7 - (index%8)`，**MSB 在前**；
- 排列：row-major，从左到右、从上到下；
- 二值化：像素值 `> 128` 记为 1，否则 0；
- 模型输入就是这 50 字节，推理时解包成 400 个 0/1。

```python
packed = np.packbits(bits.reshape(-1, 400), axis=1, bitorder="big")  # (N, 50)
```

```c
bit = (packed[idx >> 3] >> (7 - (idx & 7))) & 1;
```

## 预处理

1. RGB565 → 亮度 → 反相墨迹；
2. 阈值 16 求包围盒；
3. 等比缩放到 20×20 以内；
4. 居中到 20×20；
5. 阈值 >128 二值化，按 MSB 打包成 50 字节。

数字占满 20×20；因为 `ND_GRID_SIZE == ND_DIGIT_SIZE`，代码会跳过质心平移，避免把数字移出边界。训练数据采用同样的包围盒 + 缩放 + 二值化流程。

## 结果（MNIST 前 2 000 张测试集）

| 指标 | 数值 |
|---|---:|
| float 模型 | 93.45% |
| uint8 量化模型 | **93.45%** |
| C 实测 | **1869 / 2000 = 93.45%** |
| argmax 一致率 | 99.40% |
| 权重+偏置 | **9 976 B** |
| 输入 | **50 B** |
| 总静态数据 | **10 026 B** |

> 训练有随机性，重跑时准确率会有 ±0.2 个点左右的小幅波动。

对比：

| 版本 | hidden | 权重 | 输入 | 合计 | 准确率 |
|---|---:|---:|---:|---:|---:|
| uint8 28×28 | 32 | 25 576 B | 784 B | 26 360 B | 95.25% |
| 1-bit 28×28 | 24 | 19 192 B | 98 B | 19 290 B | 93.60% |
| **1-bit 20×20** | **24** | **9 976 B** | **50 B** | **10 026 B** | **93.45%** |
| 1-bit 32×32 | 24 | 24 952 B | 128 B | 25 080 B | 93.50% |

结论：

- 20×20 与 28×28 的 H=24 准确率几乎相同（-0.15 个点）；
- 权重从 19 192 B 降到 9 976 B（约 -48%），输入从 98 B 降到 50 B；
- 比 28×28 uint8 小约 62%，准确率低约 1.8 个点；
- 首层权重 `400×24 = 9 600`，比 28×28 的 `784×24 = 18 816` 少 9 216 个。

## 量化参数

| 张量 | S | Z |
|---|---:|---:|
| 输入 | 1.0 | 0 |
| W1 | 0.00521743134 | 153 |
| hidden | 0.0560824151 | 0 |
| W2 | 0.00816548385 | 132 |
| output | 0.268981762 | 177 |

- 隐藏层 `Z=0`，`qround` 后 clamp 到 0 等价 ReLU；
- 输入 `Z=0`，累加时只需减 `Z_w * Σbit`；
- 校准：MNIST train 前 5 000 张；隐藏层 max 取 99.9 分位（14.301）。

## 文件

| 文件 | 说明 |
|---|---|
| `num_discern_1bit.c` | app：画布触发、预处理、20×20 二值化、50 B 打包、预览 |
| `num_discern_1bit_model.c` | 解包 + 两层 uint8 量化前向 |
| `num_discern_1bit_model.h` | 输入/输出尺寸、API |
| `num_discern_1bit_weights.h` | **自动生成**，勿手改；`ND1_HIDDEN_SIZE=24` |
| `note.md` | 输入层/隐藏层/神经元/像素/权重单位的概念说明 |
| `tools/train_mnist_1bit.py` | 包围盒预处理、训练、校准、量化、导出 |
| `tools/test_predict_1bit.c` | 在 2 000 张 packed 测试集上验证 C 准确率 |

## 训练与重新生成

```bat
.venv\Scripts\python.exe tools\train_mnist_1bit.py --hidden 24 --epochs 10 --export-test-data
```

会：

1. MNIST 28×28 → 包围盒 → 缩放到 20×20 → 居中 → `>128` 二值化；
2. 训练 `400 → 24 → 10` MLP；
3. 校准并量化成 uint8；
4. 导出 `num_discern_1bit_weights.h`；
5. 导出 `.tmp/test_imgs_1bit.bin`（2 000×50）和 `.tmp/test_labels.bin`。

换隐藏层大小只需改 `--hidden`，C 侧通过 `ND1_HIDDEN_SIZE` 自动适配。

## 构建与运行

```bat
build.bat debug
build\debug\tflm_framework.exe
```

启动日志：

```text
[num_discern_1bit]: model weights: 9976 bytes, input: 50 bytes
[num_discern_1bit]: ready (20x20 1-bit), waiting for canvas trigger
```

[CMakeLists.txt](../../../CMakeLists.txt) 当前启用的是“1-bit 20x20 版”。

切回 28×28 uint8：注释“1-bit 20x20 版”两行，取消注释“uint8 28x28 版”两行。
切回 28×28 float：注释当前版，取消注释“float 28x28 版”三行。

只能启用一组；同时启用会注册多个 trigger 回调。

## 准确率测试

```bat
gcc -O2 -Wall -Wextra -o .tmp\test_predict_1bit.exe ^
    tools\test_predict_1bit.c ^
    src\app\num_discern_1bit\num_discern_1bit_model.c ^
    -Isrc\app\num_discern_1bit

.tmp\test_predict_1bit.exe
```

预期：

```text
1-bit uint8 accuracy = 1869 / 2000 = 93.45%
model weights = 9976 bytes
```

## 说明与限制

- 20×20 是当前体积最小的版本：约 10 KB，准确率与 28×28 H=24 基本持平。
- 1-bit 输入会丢灰度/抗锯齿信息；训练和推理都按 `>128` 二值化。
- 数字占满 20×20，没有额外边距；质心平移在 `grid == digit` 时被跳过。
- 模型使用 uint8 权重 + int32 累加 + float 再量化乘子；下一步可换整数乘子/右移。
- 合成鼠标笔迹与 MNIST 书写风格有域差异，端到端识别结果不能当作模型准确率；以 MNIST 测试集为准。

## 下一步

- 试 `--hidden 16` 或 32，比较体积/精度；
- per-channel 权重量化；
- 用 popcount/位运算优化首层；
- 整数再量化，做纯整数推理；
- 与 TFLite/TFLM 的 int8 模型对照。
