#!/usr/bin/env python3
"""诊断：验证「宽高比失真」对识别准确率的影响。
训练与 num_discern 相同的 MLP(784->32->10)，对比：
  1) 原始 MNIST 测试集（基线）
  2) 横向压扁/拉伸后的 MNIST（模拟手绘数字宽高比与训练集不一致）"""
import numpy as np
import tensorflow as tf


def squeeze_width(imgs28, factor):
    """把 28x28 图像横向缩放到 factor 倍宽（居中回填/裁剪），模拟宽高比失真。"""
    n = imgs28.shape[0]
    imgs = imgs28.reshape(n, 28, 28, 1).astype(np.float32)
    new_w = max(4, int(28 * factor))
    resized = tf.image.resize(imgs, [28, new_w], method="bilinear").numpy()[:, :, :, 0]
    out = np.zeros((n, 28, 28), dtype=np.float32)
    if new_w <= 28:
        pad = (28 - new_w) // 2
        out[:, :, pad:pad + new_w] = resized
    else:
        crop = (new_w - 28) // 2
        out = resized[:, :, crop:crop + 28]
    return out.reshape(n, 784)


def main():
    (x_train, y_train), (x_test, y_test) = tf.keras.datasets.mnist.load_data()
    x_train = x_train.reshape(-1, 784).astype(np.float32) / 255.0
    x_test = x_test.reshape(-1, 784).astype(np.float32) / 255.0

    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(784,)),
        tf.keras.layers.Dense(32, activation="relu"),
        tf.keras.layers.Dense(10, activation="softmax"),
    ])
    model.compile(optimizer="adam", loss="sparse_categorical_crossentropy", metrics=["accuracy"])
    model.fit(x_train, y_train, epochs=10, batch_size=128, verbose=0)

    base = model.evaluate(x_test, y_test, verbose=0)[1]
    print(f"基线（原始 MNIST）        : {base:.4f}")

    x_test28 = x_test.reshape(-1, 28, 28)
    for factor in (0.5, 0.7, 1.3, 1.5):
        acc = model.evaluate(squeeze_width(x_test28, factor), y_test, verbose=0)[1]
        print(f"横向缩放 {factor:.1f} 倍（失真）: {acc:.4f}")


if __name__ == "__main__":
    main()
