"""Smoke test: TensorFlow trains a tiny model and exports a quantized .tflite.

This exercises the exact pipeline the whole learning path relies on:
    train (Keras) -> TFLite convert -> int8 quantize -> run interpreter
Run from the repo root with:
    .venv\\Scripts\\python.exe verify\\01_tensorflow_smoke.py
"""
import numpy as np
import tensorflow as tf

print("TensorFlow", tf.__version__)
print("NumPy", np.__version__)

# --- Tiny dataset: y = sin(x) ---
x = np.linspace(0.0, 2.0 * np.pi, 1000, dtype=np.float32)
y = np.sin(x).astype(np.float32)

# --- Tiny model ---
model = tf.keras.Sequential(
    [
        tf.keras.layers.Input(shape=(1,)),
        tf.keras.layers.Dense(16, activation="relu"),
        tf.keras.layers.Dense(16, activation="relu"),
        tf.keras.layers.Dense(1),
    ]
)
model.compile(optimizer="adam", loss="mse")
model.fit(x, y, epochs=5, batch_size=32, verbose=0)
print("train MSE:", round(float(model.evaluate(x, y, verbose=0)), 6))

# --- Export float TFLite ---
conv = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_float = conv.convert()
with open("smoke_float.tflite", "wb") as f:
    f.write(tflite_float)
print("float tflite bytes:", len(tflite_float))

# --- Quantize to int8 (the MCU-friendly format) ---
def representative_dataset():
    # TF 2.16+ requires each yielded value to be a list/tuple/dict of inputs.
    for i in range(0, len(x), 32):
        yield [x[i : i + 32].reshape(-1, 1)]

conv.optimizations = [tf.lite.Optimize.DEFAULT]
conv.representative_dataset = representative_dataset
conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
conv.inference_input_type = tf.int8
conv.inference_output_type = tf.int8
tflite_int8 = conv.convert()
with open("smoke_int8.tflite", "wb") as f:
    f.write(tflite_int8)
print("int8 tflite bytes:", len(tflite_int8))

# --- Run the int8 model through the interpreter ---
interp = tf.lite.Interpreter(model_content=tflite_int8)
interp.allocate_tensors()
inp = interp.get_input_details()[0]
out = interp.get_output_details()[0]

in_scale, in_zero = inp["quantization"]
out_scale, out_zero = out["quantization"]

xi = np.float32(1.0)
xi_q = np.clip(xi / in_scale + in_zero, -128, 127).astype(np.int8)
interp.set_tensor(inp["index"], np.array([[xi_q]], dtype=np.int8))
interp.invoke()
y_q = interp.get_tensor(out["index"])[0][0]
y_pred = (float(y_q) - out_zero) * out_scale

print(f"sin(1.0)  true      = {np.sin(1.0):.4f}")
print(f"sin(1.0)  int8 model= {y_pred:.4f}")
print("SMOKE OK")
