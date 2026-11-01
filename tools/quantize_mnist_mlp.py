#!/usr/bin/env python3
"""把现有 float MLP 量化成纯 uint8 版本，并导出 C 头文件。

流程：
  1. 解析 src/app/num_discern/num_discern_model_weights.h 里的 float 权重；
  2. 用 MNIST 训练集子集校准隐藏层/输出层范围；
  3. per-tensor 量化 W1/W2 为 uint8，bias 为 int32；
  4. NumPy 模拟 uint8 推理，对比 float 的准确率和 argmax 一致率；
  5. 导出 src/app/num_discern_uint8/num_discern_uint8_weights.h；
  6. 可选导出 C 测试用的 uint8 MNIST 测试集到 .tmp/。

用法（仓库根目录）：
    .venv\\Scripts\\python.exe tools\\quantize_mnist_mlp.py
    .venv\\Scripts\\python.exe tools\\quantize_mnist_mlp.py --calib 5000 --eval 2000
"""
import argparse
import os
import re
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
FLOAT_HEADER = ROOT / "src" / "app" / "num_discern" / "num_discern_model_weights.h"
OUT_HEADER = ROOT / "src" / "app" / "num_discern_uint8" / "num_discern_uint8_weights.h"

FLOAT_RE = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?f")


def parse_float_array(text, name, count):
    m = re.search(
        r"static\s+const\s+float\s+" + re.escape(name) + r"\s*\[\s*(\d+)\s*\]\s*=\s*\{(.*?)\};",
        text,
        re.S,
    )
    if not m:
        raise RuntimeError(f"找不到数组 {name}")
    declared = int(m.group(1))
    if declared != count:
        raise RuntimeError(f"{name}: 声明长度 {declared} != 期望 {count}")
    vals = FLOAT_RE.findall(m.group(2))
    if len(vals) != count:
        raise RuntimeError(f"{name}: 解析到 {len(vals)} 个值，期望 {count}")
    return np.array([float(v[:-1]) for v in vals], dtype=np.float32)


def load_float_weights():
    text = FLOAT_HEADER.read_text(encoding="utf-8")
    w1 = parse_float_array(text, "nd_w1", 784 * 32).reshape(784, 32)
    b1 = parse_float_array(text, "nd_b1", 32)
    w2 = parse_float_array(text, "nd_w2", 32 * 10).reshape(32, 10)
    b2 = parse_float_array(text, "nd_b2", 10)
    return w1, b1, w2, b2


def qround(x):
    """四舍五入、远离 0；与 C 侧 qround 保持一致。"""
    x = np.asarray(x)
    return np.where(x >= 0, np.floor(x + 0.5), np.ceil(x - 0.5)).astype(np.int64)


def quantize_weight(w):
    mn = float(w.min())
    mx = float(w.max())
    if mx == mn:
        return np.zeros(w.shape, dtype=np.uint8), 1.0, 0
    s = (mx - mn) / 255.0
    z = int(round(-mn / s))
    q = np.clip(qround(w.astype(np.float64) / s) + z, 0, 255).astype(np.uint8)
    return q, s, z


def quantize_bias(b, s_x, s_w):
    s_b = s_x * s_w
    q = qround(b.astype(np.float64) / s_b).astype(np.int32)
    return q, s_b


def float_forward(x, w1, b1, w2, b2):
    h = np.maximum(x @ w1 + b1, 0.0)
    logits = h @ w2 + b2
    return h, logits


def uint8_forward(x_u8, qw1, qb1, zw1, qw2, qb2, zw2, m1, m2, z_out):
    x64 = x_u8.astype(np.int64)
    sum_x = x64.sum(axis=1)
    acc1 = x64 @ qw1.astype(np.int64) + qb1.astype(np.int64) - zw1 * sum_x[:, None]
    hq = np.clip(qround(acc1 * m1), 0, 255).astype(np.uint8)

    h64 = hq.astype(np.int64)
    sum_h = h64.sum(axis=1)
    acc2 = h64 @ qw2.astype(np.int64) + qb2.astype(np.int64) - zw2 * sum_h[:, None]
    oq = np.clip(qround(acc2 * m2) + z_out, 0, 255).astype(np.uint8)
    return hq, oq


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


def export_header(w1q, b1q, w2q, b2q, params, stats, calib, percentile):
    lines = []
    lines.append("/* 本文件由 tools/quantize_mnist_mlp.py 自动生成，勿手改。 */")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("/* 纯 uint8 仿射量化：x ~= S * (q - Z) */")
    lines.append(f"/* 校准集：MNIST train 前 {calib} 张；隐藏层 max 用 {percentile:g} 分位 */")
    lines.append(f"/* float 测试准确率：{stats['float_acc']*100:.2f}% ({stats['eval_n']} 张) */")
    lines.append(f"/* uint8 测试准确率：{stats['u8_acc']*100:.2f}% ({stats['eval_n']} 张) */")
    lines.append(f"/* argmax 一致率：{stats['agree']*100:.2f}% */")
    lines.append("")
    lines.append(f"#define Q8_INPUT_SCALE            {fmt_float(params['s_x'])}")
    lines.append(f"#define Q8_INPUT_ZERO_POINT       {params['z_x']}")
    lines.append(f"#define Q8_W1_SCALE               {fmt_float(params['s_w1'])}")
    lines.append(f"#define Q8_W1_ZERO_POINT          {params['z_w1']}")
    lines.append(f"#define Q8_HIDDEN_SCALE           {fmt_float(params['s_h'])}")
    lines.append(f"#define Q8_HIDDEN_ZERO_POINT      {params['z_h']}")
    lines.append(f"#define Q8_W2_SCALE               {fmt_float(params['s_w2'])}")
    lines.append(f"#define Q8_W2_ZERO_POINT          {params['z_w2']}")
    lines.append(f"#define Q8_OUTPUT_SCALE           {fmt_float(params['s_o'])}")
    lines.append(f"#define Q8_OUTPUT_ZERO_POINT      {params['z_o']}")
    lines.append("")
    lines.append("/* 再量化乘子：acc 是 int32 累加器，real = S_in * S_w * acc */")
    lines.append("#define Q8_M1 ((Q8_INPUT_SCALE * Q8_W1_SCALE) / Q8_HIDDEN_SCALE)")
    lines.append("#define Q8_M2 ((Q8_HIDDEN_SCALE * Q8_W2_SCALE) / Q8_OUTPUT_SCALE)")
    lines.append("")

    dump_u8(lines, "q8_w1", w1q)
    dump_i32(lines, "q8_b1", b1q)
    dump_u8(lines, "q8_w2", w2q)
    dump_i32(lines, "q8_b2", b2q)

    OUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    OUT_HEADER.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"weights exported -> {OUT_HEADER}")


def export_test_data(x_test_u8, y_test, n):
    out = ROOT / ".tmp"
    out.mkdir(exist_ok=True)
    x = x_test_u8[:n].reshape(-1)
    y = y_test[:n].astype(np.uint8)
    x.tofile(out / "test_imgs_u8.bin")
    y.tofile(out / "test_labels.bin")
    print(f"test data exported -> {out / 'test_imgs_u8.bin'} ({n} x 784), {out / 'test_labels.bin'}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--calib", type=int, default=5000, help="校准集张数（train）")
    ap.add_argument("--eval", type=int, default=2000, help="测试集张数（test）")
    ap.add_argument("--percentile", type=float, default=99.9, help="隐藏层 max 分位")
    ap.add_argument("--export-test-data", action="store_true", help="导出 .tmp/test_imgs_u8.bin")
    args = ap.parse_args()

    import tensorflow as tf
    print("TensorFlow", tf.__version__, "| NumPy", np.__version__)

    w1, b1, w2, b2 = load_float_weights()
    print(f"loaded float weights: w1{w1.shape} b1{b1.shape} w2{w2.shape} b2{b2.shape}")

    (x_train, y_train), (x_test, y_test) = tf.keras.datasets.mnist.load_data()
    x_train_u8 = x_train.reshape(-1, 784).astype(np.uint8)
    x_test_u8 = x_test.reshape(-1, 784).astype(np.uint8)

    n_cal = min(args.calib, len(x_train_u8))
    x_cal = x_train_u8[:n_cal].astype(np.float32) / 255.0
    h_cal, logits_cal = float_forward(x_cal, w1, b1, w2, b2)

    s_x, z_x = 1.0 / 255.0, 0
    s_w1, z_w1 = None, None
    w1q, s_w1, z_w1 = quantize_weight(w1)
    w2q, s_w2, z_w2 = quantize_weight(w2)
    b1q, s_b1 = quantize_bias(b1, s_x, s_w1)
    # b2 的 scale 依赖 hidden scale，下面校准后再算

    hmax = float(np.percentile(h_cal, args.percentile))
    if hmax <= 0:
        hmax = 1.0
    s_h, z_h = hmax / 255.0, 0

    o_min = float(logits_cal.min())
    o_max = float(logits_cal.max())
    if o_max == o_min:
        s_o, z_o = 1.0, 0
    else:
        s_o = (o_max - o_min) / 255.0
        z_o = int(round(-o_min / s_o))

    b2q, s_b2 = quantize_bias(b2, s_h, s_w2)

    m1 = (s_x * s_w1) / s_h
    m2 = (s_h * s_w2) / s_o

    n_eval = min(args.eval, len(x_test_u8))
    x_eval_u8 = x_test_u8[:n_eval]
    y_eval = y_test[:n_eval]
    x_eval_f = x_eval_u8.astype(np.float32) / 255.0

    _, logits_f = float_forward(x_eval_f, w1, b1, w2, b2)
    pred_f = logits_f.argmax(axis=1)

    hq, oq = uint8_forward(x_eval_u8, w1q, b1q, z_w1, w2q, b2q, z_w2, m1, m2, z_o)
    pred_q = oq.argmax(axis=1)

    float_acc = float((pred_f == y_eval).mean())
    u8_acc = float((pred_q == y_eval).mean())
    agree = float((pred_f == pred_q).mean())

    # 反量化 logits 误差
    logits_dq = (oq.astype(np.float32) - z_o) * s_o
    mae = float(np.abs(logits_dq - logits_f).mean())
    maxerr = float(np.abs(logits_dq - logits_f).max())

    h_q255 = float((hq == 255).mean())
    o_q0 = float((oq == 0).mean())
    o_q255 = float((oq == 255).mean())

    print("\n===== 量化参数 =====")
    print(f"input : S={s_x:.9g} Z={z_x}")
    print(f"W1    : S={s_w1:.9g} Z={z_w1}")
    print(f"hidden: S={s_h:.9g} Z={z_h}  (max={hmax:.6g})")
    print(f"W2    : S={s_w2:.9g} Z={z_w2}")
    print(f"output: S={s_o:.9g} Z={z_o}  (range=[{o_min:.6g}, {o_max:.6g}])")
    print(f"b1    : S={s_b1:.9g}  b2: S={s_b2:.9g}")
    print(f"M1={m1:.9g}  M2={m2:.9g}")

    print("\n===== 结果 =====")
    print(f"eval samples : {n_eval}")
    print(f"float acc    : {float_acc*100:.2f}%")
    print(f"uint8 acc    : {u8_acc*100:.2f}%")
    print(f"argmax agree : {agree*100:.2f}%")
    print(f"logits MAE   : {mae:.4f}  max: {maxerr:.4f}")
    print(f"saturation   : hidden==255 {h_q255*100:.3f}% | out==0 {o_q0*100:.3f}% | out==255 {o_q255*100:.3f}%")

    stats = {
        "float_acc": float_acc,
        "u8_acc": u8_acc,
        "agree": agree,
        "eval_n": n_eval,
    }
    params = {
        "s_x": s_x, "z_x": z_x,
        "s_w1": s_w1, "z_w1": z_w1,
        "s_h": s_h, "z_h": z_h,
        "s_w2": s_w2, "z_w2": z_w2,
        "s_o": s_o, "z_o": z_o,
    }
    export_header(w1q, b1q, w2q, b2q, params, stats, n_cal, args.percentile)

    if args.export_test_data:
        export_test_data(x_test_u8, y_test, n_eval)

    if u8_acc < 0.90:
        print("\n[警告] uint8 准确率低于 90%，建议检查校准范围或改用 per-channel。")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
