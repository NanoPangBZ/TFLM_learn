#!/usr/bin/env python3
"""先下载 MNIST 训练集，再训练一个简易 MLP。

流程：
  1. 调用 learning/mnist/download_mnist_train.py（已存在则跳过下载）；
  2. 读取 60000 张 28x28 训练图，按 50k/10k 切分 train/val；
  3. 训练 784 -> hidden... -> 10 的 MLP；
  4. 在验证集上评估，保存模型到 learning/out/。

用法：
    .venv\\Scripts\\python.exe learning\\mnist\\train_mlp_baseline.py
    .venv\\Scripts\\python.exe learning\\mnist\\train_mlp_baseline.py --epochs 5 --hidden 64
    .venv\\Scripts\\python.exe learning\\mnist\\train_mlp_baseline.py --hidden 128 64
    .venv\\Scripts\\python.exe learning\\mnist\\train_mlp_baseline.py --skip-download

注意：
  - 数据在 learning/mnist/data，已被 .gitignore 忽略；
  - 模型保存到 learning/out，已被 .gitignore 忽略；
  - 只使用训练集，验证集从训练集切分；没有下载测试集。
"""
from __future__ import annotations

import argparse
import gzip
import importlib.util
import os
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]          # learning/mnist -> repo root
DATA_DIR = Path(__file__).resolve().parent / "data"
DOWNLOAD_SCRIPT = Path(__file__).with_name("download_mnist_train.py")
OUT_DIR = ROOT / "learning" / "out"


def ensure_tensorflow_available() -> None:
    """在下载前检查 TensorFlow，避免下载完才发现用错解释器。"""
    if importlib.util.find_spec("tensorflow") is not None:
        return

    venv_py = ROOT / ".venv" / "Scripts" / "python.exe"
    print("[error] 当前 Python 没有 TensorFlow。", file=sys.stderr)
    print(f"  当前解释器: {sys.executable}", file=sys.stderr)
    if venv_py.exists():
        print("  请用仓库虚拟环境运行：", file=sys.stderr)
        print(f"    {venv_py} {Path(__file__).resolve()}", file=sys.stderr)
    else:
        print("  未找到 .venv，请先在仓库根目录运行 setup.ps1", file=sys.stderr)
    raise SystemExit(1)


def ensure_download(skip: bool) -> None:
    if skip:
        print("[1/4] skip download (--skip-download)")
        return
    print("[1/4] ensure MNIST training set ...")
    subprocess.run(
        [sys.executable, str(DOWNLOAD_SCRIPT)],
        cwd=ROOT,
        check=True,
    )


def load_idx_images(path: Path) -> np.ndarray:
    with gzip.open(path, "rb") as f:
        magic, n, rows, cols = np.frombuffer(f.read(16), dtype=">u4")
        if int(magic) != 2051:
            raise ValueError(f"bad image magic: {magic}")
        data = np.frombuffer(f.read(), dtype=np.uint8)
        expected = int(n) * int(rows) * int(cols)
        if data.size != expected:
            raise ValueError(f"image data size {data.size} != {expected}")
        return data.reshape(int(n), int(rows), int(cols))


def load_idx_labels(path: Path) -> np.ndarray:
    with gzip.open(path, "rb") as f:
        magic, n = np.frombuffer(f.read(8), dtype=">u4")
        if int(magic) != 2049:
            raise ValueError(f"bad label magic: {magic}")
        data = np.frombuffer(f.read(), dtype=np.uint8)
        if data.size != int(n):
            raise ValueError(f"label data size {data.size} != {n}")
        return data


def main() -> int:
    # 控制台编码：避免 Windows 下中文提示乱码
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
            sys.stderr.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

    # 命令行参数：训练超参数 + 是否跳过下载
    parser = argparse.ArgumentParser(description="下载 MNIST 并训练一个简易 MLP。")
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument(
        "--hidden",
        type=int,
        nargs="+",
        default=[64],
        help="每层隐藏神经元数，可多值，例如 --hidden 128 64",
    )
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--val", type=int, default=10000, help="验证集张数")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--skip-download", action="store_true")
    args = parser.parse_args()

    # 运行环境：确认 TensorFlow 可用，并确保训练集已下载
    ensure_tensorflow_available()
    ensure_download(args.skip_download)

    # 数据读取：IDX -> (N, 28, 28) 图像 + (N,) 标签
    print("[2/4] load MNIST training set ...")
    images_path = DATA_DIR / "train-images-idx3-ubyte.gz"
    labels_path = DATA_DIR / "train-labels-idx1-ubyte.gz"
    if not images_path.exists() or not labels_path.exists():
        print(f"[error] 缺少数据文件：{images_path} / {labels_path}", file=sys.stderr)
        return 1

    images = load_idx_images(images_path)
    labels = load_idx_labels(labels_path)
    print(f"  images: {images.shape} {images.dtype}")
    print(f"  labels: {labels.shape} {labels.dtype}")
    print(f"  class counts: {np.bincount(labels, minlength=10).tolist()}")

    # 输入整理：展平成 784 维，并归一化到 [0,1]
    x = images.reshape(len(images), -1).astype(np.float32) / 255.0
    y = labels.astype(np.int64)
    n = len(x)
    val_n = min(args.val, n - 1)
    if val_n <= 0:
        print("[error] --val 必须大于 0", file=sys.stderr)
        return 1

    # 数据划分：固定种子随机打乱，切出训练集和验证集
    rng = np.random.default_rng(args.seed)
    perm = rng.permutation(n)
    val_idx = perm[:val_n]
    train_idx = perm[val_n:]

    x_train, y_train = x[train_idx], y[train_idx]
    x_val, y_val = x[val_idx], y[val_idx]
    print(f"  train: {len(x_train)}  val: {len(x_val)}")

    os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")
    import tensorflow as tf

    tf.keras.utils.set_random_seed(args.seed)

    # 模型构建：最小 MLP，输入 784，隐藏层可调，输出 10 类
    print("[3/4] build and train MLP ...")
    if any(h <= 0 for h in args.hidden):
        print("[error] --hidden 的每一层都必须大于 0", file=sys.stderr)
        return 1

    layers = [tf.keras.layers.Input(shape=(784,))]
    for h in args.hidden:
        layers.append(tf.keras.layers.Dense(h, activation="relu"))
    layers.append(tf.keras.layers.Dense(10, activation="softmax"))
    print("  hidden layers: " + " -> ".join(str(h) for h in args.hidden))

    model = tf.keras.Sequential(layers)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=1e-3),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    model.summary()

    # 训练：用验证集观察泛化，避免只看训练准确率
    model.fit(
        x_train, y_train,
        validation_data=(x_val, y_val),
        epochs=args.epochs,
        batch_size=args.batch_size,
        verbose=2,
    )

    # 评估与保存：验证准确率、混淆矩阵、.keras 模型文件
    print("[4/4] evaluate ...")
    val_loss, val_acc = model.evaluate(x_val, y_val, verbose=0)
    print(f"  val loss    : {val_loss:.4f}")
    print(f"  val accuracy: {val_acc * 100:.2f}%")

    pred = model.predict(x_val, verbose=0).argmax(axis=1)
    cm = tf.math.confusion_matrix(y_val, pred, num_classes=10).numpy()
    print("  confusion matrix:")
    print(cm)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    model_path = OUT_DIR / "mnist_mlp_baseline.keras"
    model.save(model_path)
    print(f"  model saved: {model_path}")
    print(f"  params: {model.count_params():,}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
