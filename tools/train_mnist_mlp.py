#!/usr/bin/env python3
"""训练 MNIST 小 MLP（784 -> 32 -> 10），导出权重为 C 头文件。

输出：src/app/num_discern/num_discern_model_weights.h

用法（仓库根目录）：
    .venv\\Scripts\\python.exe tools\\train_mnist_mlp.py
"""
import os
import sys

import numpy as np


def c_float(v):
    """把 float 转成可精确回读的 C 字面量（float32 只需 ~9 位有效数字）。"""
    s = format(float(v), ".9g")
    if "e" not in s and "E" not in s and "." not in s:
        s += ".0"
    return s + "f"


def export_header(w1, b1, w2, b2, acc):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "src", "app", "num_discern", "num_discern_model_weights.h")

    lines = []
    lines.append("/* 本文件由 tools/train_mnist_mlp.py 自动生成，勿手改。 */")
    lines.append("#pragma once")
    lines.append(f"/* MNIST MLP 784->{w1.shape[1]}->{w2.shape[1]}，test accuracy = {acc:.4f} */")
    lines.append("")

    def dump(name, arr):
        flat = arr.reshape(-1)
        lines.append(f"static const float {name}[{len(flat)}] = {{")
        for i in range(0, len(flat), 8):
            chunk = ", ".join(c_float(x) for x in flat[i:i + 8])
            lines.append("    " + chunk + ("," if i + 8 < len(flat) else ""))
        lines.append("};")
        lines.append("")

    # w1: [784, hidden] 行主序，w1[j*hidden+i] 为 input[j] -> hidden[i]
    dump("nd_w1", w1)
    dump("nd_b1", b1)
    dump("nd_w2", w2)
    dump("nd_b2", b2)

    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print(f"weights exported -> {out}")


def main():
    import tensorflow as tf

    # 1. 数据（keras 内置 MNIST，首次自动下载 ~11MB）
    (x_train, y_train), (x_test, y_test) = tf.keras.datasets.mnist.load_data()
    x_train = x_train.reshape(-1, 784).astype(np.float32) / 255.0
    x_test = x_test.reshape(-1, 784).astype(np.float32) / 255.0
    print(f"train={x_train.shape[0]}  test={x_test.shape[0]}")

    # 2. 模型
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(784,)),
        tf.keras.layers.Dense(32, activation="relu"),
        tf.keras.layers.Dense(10, activation="softmax"),
    ])
    model.compile(optimizer="adam",
                  loss="sparse_categorical_crossentropy",
                  metrics=["accuracy"])
    model.fit(x_train, y_train, epochs=10, batch_size=128, verbose=1)

    loss, acc = model.evaluate(x_test, y_test, verbose=0)
    print(f"test accuracy = {acc:.4f}")

    # 3. 导出
    w1, b1 = model.layers[0].get_weights()
    w2, b2 = model.layers[1].get_weights()
    export_header(w1, b1, w2, b2, acc)
    return 0


if __name__ == "__main__":
    sys.exit(main())
