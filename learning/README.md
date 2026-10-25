# learning — 个人训练/学习脚本

这个目录放**自己的训练实验脚本**，和 `tools/`、`src/app/` 的生产代码分开，方便随便改、随便试，不影响 app 构建。

## 约定

- 用仓库的虚拟环境运行：
  ```bat
  .venv\Scripts\python.exe learning\mnist\train_xxx.py
  ```
- 学习用数据放在：
  ```text
  learning/mnist/data
  ```
  该目录已被 `learning/.gitignore` 忽略，不会进 Git；不要把大文件提交。
- 训练产物写到 `learning/out/` 或 `.tmp/`，已在 `.gitignore` 中忽略。
- 每个脚本开头写清楚：
  - 实验目的；
  - 输入格式/预处理；
  - 模型结构；
  - 关键超参数；
  - 预期结果。
- 每次实验在 [EXPERIMENTS.md](EXPERIMENTS.md) 里记一行，避免“跑过但忘了参数”。
- 固定随机种子，便于复现：
  ```python
  tf.keras.utils.set_random_seed(0)
  ```
- 这个目录不会被 CMake 编译；脚本尽量独立，不直接改 `src/`。
  如果实验成熟，再考虑移到 `tools/` 或 app 目录。

## 建议结构

```text
learning/
├── README.md
├── EXPERIMENTS.md
├── .gitignore
├── mnist/
│   ├── train_mlp_baseline.py
│   ├── train_mlp_binary.py
│   └── ...
└── out/                  # 训练产物，不提交
```

可以按主题继续建子目录，例如 `mnist/`、`cnn/`、`quantization/`。

## 推荐起步

1. 运行 `learning/mnist/train_mlp_baseline.py`；
2. 它会先调用下载脚本，再训练 28×28 float、784→64→10 的基线；
3. 再改隐藏层、epochs、学习率、dropout；
4. 最后试 1-bit / int8 量化和部署。

## 不要做的事

- 不要把 `train-images-idx3-ubyte` 这类数据文件放进仓库；
- 不要用测试集调参；
- 不要在同一个脚本里同时改多个变量，否则无法判断收益来自哪里；
- 不要把临时权重、`.npz`、`.tflite` 提交进 Git。
