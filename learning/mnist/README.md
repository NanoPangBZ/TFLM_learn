# learning/mnist

放 MNIST 相关的学习脚本。

数据位置（仓库内，但被 `.gitignore` 忽略）：

```text
learning/mnist/data
```

## 下载训练集

```bat
.venv\Scripts\python.exe learning\mnist\download_mnist_train.py
```

- 只下载训练集：`train-images-idx3-ubyte.gz` + `train-labels-idx1-ubyte.gz`；
- 带进度条、MD5 校验；已存在且校验通过就跳过；
- 默认下载到 `learning/mnist/data`，该目录已被 `learning/.gitignore` 忽略；
- 如果 `--dest` 指向仓库内但未被忽略的目录，脚本会拒绝执行；
- `--force` 可强制重新下载。

## 简易 MLP 训练

```bat
.venv\Scripts\python.exe learning\mnist\train_mlp_baseline.py
```

流程：

1. 先调用 `download_mnist_train.py`（已下载则跳过）；
2. 读取 60 000 张训练图，随机切 50 000 训练 / 10 000 验证；
3. 训练 `784 → 64 → 10` MLP（Adam，batch 128，10 epochs）；
4. 输出验证准确率、混淆矩阵，模型保存到 `learning/out/mnist_mlp_baseline.keras`。

可选参数：

```bat
--epochs 5 --hidden 128 --batch-size 64 --val 10000 --skip-download
```

当前一次运行结果：验证集 **96.42%**，参数量 **50 890**。

> 必须用仓库虚拟环境运行。直接 `python train_mlp_baseline.py` 会找不到 TensorFlow；
> 脚本会提示正确的解释器路径并退出。
>
> 在 `learning\mnist` 目录下可以这样运行：
> ```bat
> ..\..\.venv\Scripts\python.exe .\train_mlp_baseline.py
> ```

建议从这个顺序开始：

1. `train_mlp_baseline.py`：28×28 float，784 → 64 → 10（已创建）；
2. `train_mlp_hidden.py`：改隐藏层 32/64/128，对比参数量和准确率；
3. `train_mlp_binary.py`：20×20 1-bit 输入，参考 [tools/train_mnist_1bit.py](../../tools/train_mnist_1bit.py)；
4. `quantize_ptq.py`：float 模型导出 int8 TFLite，比较准确率/体积；
5. 把每次结果记到 [../EXPERIMENTS.md](../EXPERIMENTS.md)。

参考脚本：

- [tools/train_mnist_mlp.py](../../tools/train_mnist_mlp.py)
- [tools/train_mnist_1bit.py](../../tools/train_mnist_1bit.py)
- [verify/01_tensorflow_smoke.py](../../verify/01_tensorflow_smoke.py)
