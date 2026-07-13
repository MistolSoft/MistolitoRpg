import os
import sys
import argparse
import pandas as pd
import numpy as np
import torch
from torch.utils.data import DataLoader, TensorDataset
from esp_ppq.api import espdl_quantize_onnx

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(TOOLS_DIR)))

TARGET = "esp32s3"
CALIB_SAMPLES = 500

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=str, default="combat", choices=["combat", "core"])
    args = parser.parse_args()

    if args.model == "combat":
        dataset_path = os.path.join(PROJECT_DIR, "data", "datasets", "combat_dataset.csv")
        onnx_path = os.path.join(PROJECT_DIR, "models", "combat", "backbone.onnx")
        espdl_path = os.path.join(PROJECT_DIR, "models", "combat", "backbone.espdl")
        input_shape = [1, 9]
        features_end = 9
    else:
        dataset_path = os.path.join(PROJECT_DIR, "core_dataset.csv")
        onnx_path = os.path.join(PROJECT_DIR, "models", "core", "backbone.onnx")
        espdl_path = os.path.join(PROJECT_DIR, "models", "core", "backbone.espdl")
        input_shape = [1, 6]
        features_end = 6

    if not os.path.exists(onnx_path):
        print(f"ERROR: No se encuentra {onnx_path}")
        sys.exit(1)

    if not os.path.exists(dataset_path):
        print(f"ERROR: No se encuentra {dataset_path}")
        sys.exit(1)

    df = pd.read_csv(dataset_path)
    X = df.iloc[:CALIB_SAMPLES, :features_end].values.astype(np.float32)
    calib_data = torch.from_numpy(X)
    calib_dataset = TensorDataset(calib_data)
    calib_dataloader = DataLoader(calib_dataset, batch_size=1, shuffle=False)

    print(f"Calibrando modelo {args.model} con {len(calib_data)} muestras...")
    print(f"Exportando: {onnx_path} -> {espdl_path} (int8)...")

    espdl_quantize_onnx(
        onnx_import_file=onnx_path,
        espdl_export_file=espdl_path,
        calib_dataloader=calib_dataloader,
        calib_steps=len(calib_data),
        input_shape=input_shape,
        target=TARGET,
        num_of_bits=8,
        device="cpu",
        verbose=1,
    )

    size_kb = os.path.getsize(espdl_path) / 1024
    print(f"Modelo exportado: {espdl_path} ({size_kb:.1f} KB)")

if __name__ == "__main__":
    main()
