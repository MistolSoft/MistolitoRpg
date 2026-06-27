"""
Convierte combat_ai.onnx -> combat_ai.espdl (int8) usando esp_ppq.
El archivo .espdl va directo a la SD del ESP32-S3 para usarlo con esp-dl.
"""

import os
import sys
import pandas as pd
import numpy as np
import torch
from torch.utils.data import DataLoader, TensorDataset
from esp_ppq.api import espdl_quantize_onnx

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)

DATASET_PATH = os.path.join(PROJECT_DIR, "data", "datasets", "combat_dataset.csv")
ONNX_PATH = os.path.join(PROJECT_DIR, "models", "combat_ai.onnx")
ESPDL_PATH = os.path.join(PROJECT_DIR, "models", "combat_ai.espdl")

INPUT_SHAPE = [1, 8]
BATCHSIZE = 1
TARGET = "esp32s3"
CALIB_SAMPLES = 500


def main():
    if not os.path.exists(ONNX_PATH):
        print(f"ERROR: No se encuentra {ONNX_PATH}")
        sys.exit(1)

    if not os.path.exists(DATASET_PATH):
        print(f"ERROR: No se encuentra {DATASET_PATH}")
        sys.exit(1)

    df = pd.read_csv(DATASET_PATH)
    X = df.iloc[:CALIB_SAMPLES, :8].values.astype(np.float32)
    calib_data = torch.from_numpy(X)
    calib_dataset = TensorDataset(calib_data)
    calib_dataloader = DataLoader(calib_dataset, batch_size=BATCHSIZE, shuffle=False)

    print(f"Calibrando con {len(calib_data)} muestras...")
    print(f"Exportando: {ONNX_PATH} -> {ESPDL_PATH} (int8)...")

    espdl_quantize_onnx(
        onnx_import_file=ONNX_PATH,
        espdl_export_file=ESPDL_PATH,
        calib_dataloader=calib_dataloader,
        calib_steps=len(calib_data),
        input_shape=INPUT_SHAPE,
        target=TARGET,
        num_of_bits=8,
        device="cpu",
        verbose=1,
    )

    size_kb = os.path.getsize(ESPDL_PATH) / 1024
    print(f"Modelo exportado: {ESPDL_PATH} ({size_kb:.1f} KB)")
    print("Listo para copiar a la SD del ESP32-S3.")


if __name__ == "__main__":
    main()
