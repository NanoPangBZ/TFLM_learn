# num_discern_uint8

纯 uint8 仿射量化版手写数字识别 demo。输入、权重、隐藏层激活、输出全部是 8-bit 整数，C 侧用 int32 累加、再量化回 uint8。

## 结果（MNIST 前 2 000 张测试集）

| 指标 | float 版 | uint8 版 |
|---|---:|---:|
| 准确率 | 95.15% | **95.25%** |
| argmax 一致率 | — | 99.70% |
| 权重存储 | 101 800 B | **25 576 B**（约 1/4） |
| 输入存储 | 3 136 B（float） | **784 B**（uint8） |

> 量化后准确率略高属于正常波动（量化误差带来轻微正则化），不代表 uint8 一定更好。

量化参数（由校准集统计）：

| 张量 | S | Z |
|---|---:|---:|
| 输入 | 1/255 = 0.00392156863 | 0 |
| W1 | 0.00569747289 | 135 |
| hidden | 0.0460112553 | 0 |
| W2 | 0.00803586245 | 143 |
| output | 0.195582992 | 147 |

校准：MNIST train 前 5 000 张；隐藏层 max 取 99.9 分位（11.7329）。

饱和比例：hidden `q==255` 0.067%；output `q==0/255` 均为 0。

## 文件

| 文件 | 说明 |
|---|---|
| `num_discern_uint8.c` | app：画布触发、预处理、28×28 uint8 输入、日志/预览 |
| `num_discern_uint8_model.c` | 纯 C 前向：两层 q_dense + ReLU + 再量化 |
| `num_discern_uint8_model.h` | 维度宏与 API |
| `num_discern_uint8_weights.h` | **自动生成**，勿手改 |
| `tools/quantize_mnist_mlp.py` | 解析 float 权重、校准、量化、NumPy 模拟、导出 |
| `tools/test_predict_uint8.c` | 在 2 000 张 MNIST 上验证 C 准确率 |
| `PLAN.md` | 完整实验计划 |

预处理暂时从 float 版 `num_discern.c` 复制，等实验稳定后再考虑抽公共模块。

## 构建与运行

```bat
build.bat debug
build\debug\tflm_framework.exe
```

当前 [CMakeLists.txt](../../../CMakeLists.txt) 里启用的是“uint8 版”两行；float 版三行处于注释状态。

切回 float 版：

1. 注释掉 `CMakeLists.txt` 中“uint8 版”下面的两行；
2. 取消注释“float 版”下面的三行（含 `canvas_service_test.c`）；
3. 重新构建：

```bat
build.bat debug
```

两版只能启用一组；同时启用会注册两个 trigger 回调，同一笔迹会触发两次、弹两套窗口。

## 重新生成权重

```bat
.venv\Scripts\python.exe tools\quantize_mnist_mlp.py --calib 5000 --eval 2000 --export-test-data
```

会：

1. 解析 `src/app/num_discern/num_discern_model_weights.h` 的 float 权重；
2. 用 MNIST train 前 5 000 张校准；
3. 导出 `num_discern_uint8_weights.h`；
4. 导出 `.tmp/test_imgs_u8.bin` / `.tmp/test_labels.bin`（2 000 张）。

## 准确率测试

```bat
gcc -O2 -Wall -Wextra -o .tmp\test_predict_uint8.exe ^
    tools\test_predict_uint8.c ^
    src\app\num_discern_uint8\num_discern_uint8_model.c ^
    -Isrc\app\num_discern_uint8

.tmp\test_predict_uint8.exe
```

预期：`uint8 accuracy = 1905 / 2000 = 95.25%`，与 Python 模拟一致。

## 量化要点

- 输入：`q = 像素 0..255`，`S=1/255, Z=0`，无损；预处理不再 `/255`。
- 权重：uint8 per-tensor；`Z≈135/143`，所以累加时要减 `Z_w * Σq_x`。
- 隐藏层：ReLU 后 `Z=0`，`S=max/255`；负值 clamp 到 0 即等价 ReLU。
- 输出：uint8 logits，单套 `S/Z`，argmax 保持单调。
- 偏置：int32，`q_b = round(b / (S_in * S_w))`。
- 累加：int32；最大 dot 约 784 × 255 × 255 ≈ 5.1×10⁷，安全。
- 再量化：先用 float 乘子 `M1/M2` 验证正确性；后续可换整数乘子 + 右移。

## 已知限制

- 纯 uint8 权重不是 TFLite 的标准形态（TFLite 内部多为 int8 对称权重）；这是学习实验。
- PC 上的耗时不能代表 MCU；速度结论要在 Cortex-M/DWT 上测。
- 预处理复制自 float 版，两份代码暂时会同步维护。
- 鼠标画出的粗线条与 MNIST 书写风格有差异，合成笔迹的识别结果不能当作模型准确率。

## 下一步

- 用整数乘子替换 float 再量化，做纯整数版本；
- 试 per-channel 权重量化；
- 试 int8 对称权重，与 TFLite 对照；
- 接入 TFLite/TFLM 解释器做交叉验证。
