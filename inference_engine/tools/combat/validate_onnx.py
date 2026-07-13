import numpy as np
import pandas as pd
import onnxruntime as ort
import os

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
MODEL_FILE = os.path.join(PROJECT_DIR, "models", "combat_ai.onnx")
INPUT_FILE = os.path.join(PROJECT_DIR, "data", "datasets", "combat_validation_inputs.csv")
OUTPUT_FILE = os.path.join(PROJECT_DIR, "data", "datasets", "combat_validation_onnx_results.csv")

ACTIONS = ["ATACAR", "DEFENDER", "HUIR"]

print("Cargando modelo ONNX...")
sess = ort.InferenceSession(MODEL_FILE)
input_name = sess.get_inputs()[0].name
expected_input_shape = sess.get_inputs()[0].shape

print("Cargando inputs de validacion...")
df = pd.read_csv(INPUT_FILE)
X = df.values.astype(np.float32)

num_features = expected_input_shape[1]
X_input = X[:, :num_features]

print(f"Ejecutando inferencia en {len(X_input)} muestras con {num_features} features...")
preds = sess.run(None, {input_name: X_input})[0]
pred_classes = np.argmax(preds, axis=1)
confidences = np.max(preds, axis=1)

df["pred_action"] = pred_classes
df["pred_label"] = [ACTIONS[c] for c in pred_classes]
df["confidence"] = np.round(confidences, 4)
df["atk_adv"] = np.round(X[:, 2] - X[:, 7], 4)
df["def_disadv"] = np.round(X[:, 6] - X[:, 3], 4)

df.to_csv(OUTPUT_FILE, index=False)
print(f"Resultados guardados en {OUTPUT_FILE}")

print("\n=== Distribucion de predicciones ===")
counts = df["pred_label"].value_counts()
for label in ACTIONS:
    c = counts.get(label, 0)
    print(f"  {label}: {c:4d} ({c/len(df)*100:.1f}%)")

print(f"\n=== Confianza promedio ===")
for label in ACTIONS:
    subset = df[df["pred_label"] == label]
    if len(subset) > 0:
        print(f"  {label}: {subset['confidence'].mean():.4f}")

print(f"\n=== 10 muestras al azar ===")
samples = df.sample(10, random_state=42)
for _, row in samples.iterrows():
    print(f"  hp={row['s0']:.2f} en={row['s1']:.2f} atk={row['s2']:.2f} def={row['s3']:.2f} | "
          f"enemy(hp={row['s4']:.2f} atk={row['s6']:.2f} def={row['s7']:.2f}) | "
          f"atk_adv={row['atk_adv']:.2f} def_disadv={row['def_disadv']:.2f} | "
          f"-> {row['pred_label']} (conf={row['confidence']:.2f})")
