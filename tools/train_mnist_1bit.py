#!/usr/bin/env python3
"""训练 20x20 1-bit 输入 MLP，量化成 uint8，并导出 C 头文件。

输入约定（与 app 一致）：
  - 对 MNIST 28x28 先按墨迹求包围盒，再等比缩放到 20x20 以内并居中；
  - 像素 > 128 记为 1，否则 0；
  - 展平顺序 row-major：index = y*20 + x；
  - 打包：每 8 bit 一个字节，MSB 在前；20x20 = 400 bit = 50 字节。

输出：
  src/app/num_discern_1bit/num_discern_1bit_weights.h
  .tmp/test_imgs_1bit.bin / .tmp/test_labels.bin（可选）

用法：
    .venv\\Scripts\\python.exe tools\\train_mnist_1bit.py
    .venv\\Scripts\\python.exe tools\\train_mnist_1bit.py --hidden 24 --epochs 10
"""
import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT_HEADER = ROOT / "src" / "app" / "num_discern_1bit" / "num_discern_1bit_weights.h"

GRID = 20
BITS = GRID * GRID
PACKED = BITS // 8


def qround(x):
    x = np.asarray(x)
    return np.where(x >= 0, np.floor(x + 0.5), np.ceil(x - 0.5)).astype(np.int64)


def preprocess_bbox(images, grid=GRID):
    """模拟 app 的预处理：按 >16 求包围盒，等比缩放到 grid 以内，居中。"""
    out = np.zeros((len(images), grid, grid), dtype=np.uint8)
    for i, img in enumerate(images):
        mask = img > 16
        ys, xs = np.where(mask)
        if len(xs) == 0:
            continue
        y0, y1 = int(ys.min()), int(ys.max())
        x0, x1 = int(xs.min()), int(xs.max())
        crop = np.ascontiguousarray(img[y0:y1 + 1, x0:x1 + 1])
        ch, cw = crop.shape
        scale = grid / float(max(cw, ch))
        sw = max(1, int(cw * scale + 0.5))
        sh = max(1, int(ch * scale + 0.5))
        resized = np.asarray(
            Image.fromarray(crop).resize((sw, sh), Image.Resampling.BILINEAR)
        )
        ox = (grid - sw) // 2
        oy = (grid - sh) // 2
        out[i, oy:oy + sh, ox:ox + sw] = resized
    return out


def quantize_weight(w):
    mn, mx = float(w.min()), float(w.max())
    if mx == mn:
        return np.zeros(w.shape, dtype=np.uint8), 1.0, 0
    s = (mx - mn) / 255.0
    z = int(round(-mn / s))
    q = np.clip(qround(w.astype(np.float64) / s) + z, 0, 255).astype(np.uint8)
    return q, s, z


def quantize_bias(b, s_scale):
    return qround(b.astype(np.float64) / s_scale).astype(np.int32)


def fmt_float(v):
    s = format(float(v), ".9g")
    if "e" not in s and "E" not in s and "." not in s:
        s += ".0"
    return s + "f"


def dump_u8(lines, name, arr, per_line=24):
    flat = arr.reshape(-1)
    lines.append(f"static const uint8_t {name}[{len(flat)}] = {{")
    for i in range(0, len(flat), per_line):
        chunk = ", ".join(str(int(x)) for x in flat[i:i + per_line])
        lines.append("    " + chunk + ("," if i + per_line < len(flat) else ""))
    lines.append("};")
    lines.append("")


def dump_i32(lines, name, arr, per_line=10):
    flat = arr.reshape(-1)
    lines.append(f"static const int32_t {name}[{len(flat)}] = {{")
    for i in range(0, len(flat), per_line):
        chunk = ", ".join(str(int(x)) for x in flat[i:i + per_line])
        lines.append("    " + chunk + ("," if i + per_line < len(flat) else ""))
    lines.append("};")
    lines.append("")


def export_header(qw1, qb1, qw2, qb2, p, stats):
    lines = []
    lines.append("/* 本文件由 tools/train_mnist_1bit.py 自动生成，勿手改。 */")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("/* 20x20 1-bit 输入，纯 uint8 权重/激活的仿射量化 */")
    lines.append(f"/* float 准确率：{stats['float_acc']*100:.2f}% ({stats['n']} 张) */")
    lines.append(f"/* uint8 准确率：{stats['u8_acc']*100:.2f}% ({stats['n']} 张) */")
    lines.append(f"/* argmax 一致率：{stats['agree']*100:.2f}% */")
    lines.append("")
    lines.append(f"#define ND1_HIDDEN_SIZE           {p['hidden']}")
    lines.append(f"#define ND1_INPUT_SCALE           {fmt_float(p['s_x'])}")
    lines.append(f"#define ND1_INPUT_ZERO_POINT      {p['z_x']}")
    lines.append(f"#define ND1_W1_SCALE              {fmt_float(p['s_w1'])}")
    lines.append(f"#define ND1_W1_ZERO_POINT         {p['z_w1']}")
    lines.append(f"#define ND1_HIDDEN_SCALE          {fmt_float(p['s_h'])}")
    lines.append(f"#define ND1_HIDDEN_ZERO_POINT     {p['z_h']}")
    lines.append(f"#define ND1_W2_SCALE              {fmt_float(p['s_w2'])}")
    lines.append(f"#define ND1_W2_ZERO_POINT         {p['z_w2']}")
    lines.append(f"#define ND1_OUTPUT_SCALE          {fmt_float(p['s_o'])}")
    lines.append(f"#define ND1_OUTPUT_ZERO_POINT     {p['z_o']}")
    lines.append("")
    lines.append("#define ND1_M1 ((ND1_INPUT_SCALE * ND1_W1_SCALE) / ND1_HIDDEN_SCALE)")
    lines.append("#define ND1_M2 ((ND1_HIDDEN_SCALE * ND1_W2_SCALE) / ND1_OUTPUT_SCALE)")
    lines.append("")
    dump_u8(lines, "nd1_w1", qw1)
    dump_i32(lines, "nd1_b1", qb1)
    dump_u8(lines, "nd1_w2", qw2)
    dump_i32(lines, "nd1_b2", qb2)

    OUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    OUT_HEADER.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"weights exported -> {OUT_HEADER}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hidden", type=int, default=24)
    ap.add_argument("--epochs", type=int, default=10)
    ap.add_argument("--calib", type=int, default=5000)
    ap.add_argument("--eval", type=int, default=2000)
    ap.add_argument("--percentile", type=float, default=99.9)
    ap.add_argument("--export-test-data", action="store_true")
    args = ap.parse_args()

    import tensorflow as tf
    tf.random.set_seed(0)
    np.random.seed(0)
    print("TensorFlow", tf.__version__, "| NumPy", np.__version__)

    (x_train, y_train), (x_test, y_test) = tf.keras.datasets.mnist.load_data()

    # 按 app 的方式把 28x28 归一到 20x20，再二值化
    x_train_20 = preprocess_bbox(x_train)
    x_test_20 = preprocess_bbox(x_test)
    x_train_bits = (x_train_20 > 128).astype(np.uint8).reshape(-1, BITS)
    x_test_bits = (x_test_20 > 128).astype(np.uint8).reshape(-1, BITS)
    x_train_f = x_train_bits.astype(np.float32)
    x_test_f = x_test_bits.astype(np.float32)
    print(f"train={x_train_bits.shape} test={x_test_bits.shape}")

    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(BITS,)),
        tf.keras.layers.Dense(args.hidden, activation="relu"),
        tf.keras.layers.Dense(10, activation="softmax"),
    ])
    model.compile(optimizer="adam",
                  loss="sparse_categorical_crossentropy",
                  metrics=["accuracy"])
    model.fit(x_train_f, y_train, epochs=args.epochs, batch_size=128, verbose=1)

    w1, b1 = model.layers[0].get_weights()
    w2, b2 = model.layers[1].get_weights()

    n_eval = min(args.eval, len(x_test_f))
    xe = x_test_f[:n_eval]
    ye = y_test[:n_eval]
    pred_f = model.predict(xe, verbose=0).argmax(axis=1)
    float_acc = float((pred_f == ye).mean())

    # 校准
    n_cal = min(args.calib, len(x_train_f))
    xc = x_train_f[:n_cal]
    hc = np.maximum(xc @ w1 + b1, 0.0)
    logits_c = hc @ w2 + b2
    hmax = float(np.percentile(hc, args.percentile))
    if hmax <= 0:
        hmax = 1.0
    s_h = hmax / 255.0
    o_min, o_max = float(logits_c.min()), float(logits_c.max())
    if o_max == o_min:
        s_o, z_o = 1.0, 0
    else:
        s_o = (o_max - o_min) / 255.0
        z_o = int(round(-o_min / s_o))

    # 量化
    s_x, z_x = 1.0, 0
    qw1, s_w1, z_w1 = quantize_weight(w1)
    qw2, s_w2, z_w2 = quantize_weight(w2)
    qb1 = quantize_bias(b1, s_x * s_w1)
    qb2 = quantize_bias(b2, s_h * s_w2)
    m1 = (s_x * s_w1) / s_h
    m2 = (s_h * s_w2) / s_o

    # NumPy 模拟 uint8 推理
    xb = x_test_bits[:n_eval].astype(np.int64)
    sum_x = xb.sum(axis=1)
    acc1 = xb @ qw1.astype(np.int64) + qb1.astype(np.int64) - z_w1 * sum_x[:, None]
    hq = np.clip(qround(acc1 * m1), 0, 255).astype(np.uint8)
    sum_h = hq.astype(np.int64).sum(axis=1)
    acc2 = hq.astype(np.int64) @ qw2.astype(np.int64) + qb2.astype(np.int64) - z_w2 * sum_h[:, None]
    oq = np.clip(qround(acc2 * m2) + z_o, 0, 255).astype(np.uint8)
    pred_q = oq.argmax(axis=1)

    u8_acc = float((pred_q == ye).mean())
    agree = float((pred_f == pred_q).mean())
    print("\n===== 量化参数 =====")
    print(f"input : S={s_x} Z={z_x} (0/1 bit)")
    print(f"W1    : S={s_w1:.9g} Z={z_w1}")
    print(f"hidden: S={s_h:.9g} Z=0 (max={hmax:.6g})")
    print(f"W2    : S={s_w2:.9g} Z={z_w2}")
    print(f"output: S={s_o:.9g} Z={z_o} (range=[{o_min:.6g}, {o_max:.6g}])")
    print(f"M1={m1:.9g} M2={m2:.9g}")
    print("\n===== 结果 =====")
    print(f"eval        : {n_eval}")
    print(f"float acc   : {float_acc*100:.2f}%")
    print(f"uint8 acc   : {u8_acc*100:.2f}%")
    print(f"argmax agree: {agree*100:.2f}%")
    print(f"hidden q=255: {(hq == 255).mean()*100:.3f}%")

    p = {
        "hidden": args.hidden,
        "s_x": s_x, "z_x": z_x,
        "s_w1": s_w1, "z_w1": z_w1,
        "s_h": s_h, "z_h": 0,
        "s_w2": s_w2, "z_w2": z_w2,
        "s_o": s_o, "z_o": z_o,
    }
    stats = {"float_acc": float_acc, "u8_acc": u8_acc, "agree": agree, "n": n_eval}
    export_header(qw1, qb1, qw2, qb2, p, stats)

    if args.export_test_data:
        out = ROOT / ".tmp"
        out.mkdir(exist_ok=True)
        packed = np.packbits(x_test_bits[:n_eval], axis=1, bitorder="big")
        packed.tofile(out / "test_imgs_1bit.bin")
        ye.astype(np.uint8).tofile(out / "test_labels.bin")
        print(f"test data -> {out / 'test_imgs_1bit.bin'} ({n_eval} x {PACKED}), {out / 'test_labels.bin'}")

    if u8_acc < 0.90:
        print("\n[警告] uint8 准确率低于 90%，建议增加 hidden 或检查二值化阈值。")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
