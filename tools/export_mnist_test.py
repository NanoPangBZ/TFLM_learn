#!/usr/bin/env python3
"""导出 MNIST 测试集前 N 张图（28x28 -> 784 float，归一化 [0,1]）与标签，
供 tools/test_predict.c 验证 C 前向传播的准确率。"""
import numpy as np
import tensorflow as tf

N = 2000
(_, _), (x_test, y_test) = tf.keras.datasets.mnist.load_data()
x = x_test[:N].reshape(N, 784).astype(np.float32) / 255.0
y = y_test[:N].astype(np.uint8)

x.tofile("test_imgs.bin")
y.tofile("test_labels.bin")
print(f"exported {N} test images")
