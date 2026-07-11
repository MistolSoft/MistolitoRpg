import os
import sys
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset
from esp_ppq.api import espdl_quantize_onnx

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
DATASET_PATH = os.path.join(PROJECT_DIR, "data", "datasets", "combat_dataset.csv")
MODEL_DIR = os.path.join(PROJECT_DIR, "models")
CHECKPOINT_PATH = os.path.join(MODEL_DIR, "combat_ai_split.pth")

BACKBONE_ONNX = os.path.join(MODEL_DIR, "backbone.onnx")
BACKBONE_ESPDL = os.path.join(MODEL_DIR, "backbone.espdl")

INPUT_SHAPE = [1, 9]
BACKBONE_OUTPUT_SHAPE = [1, 16]
BATCHSIZE = 1
TARGET = "esp32s3"
CALIB_SAMPLES = 500


class Backbone(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(9, 32)
        self.relu1 = nn.ReLU()
        self.fc2 = nn.Linear(32, 32)
        self.relu2 = nn.ReLU()
        self.fc3 = nn.Linear(32, 16)
        self.relu3 = nn.ReLU()

    def forward(self, x):
        return self.relu3(self.fc3(self.relu2(self.fc2(self.relu1(self.fc1(x))))))


class PolicyHead(nn.Module):
    def __init__(self, num_actions=3):
        super().__init__()
        self.fc = nn.Linear(16, num_actions)

    def forward(self, x):
        return self.fc(x)


class CriticHead(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc = nn.Linear(16, 1)

    def forward(self, x):
        return self.fc(x)


class CombatModelSplit(nn.Module):
    def __init__(self, num_actions=3):
        super().__init__()
        self.backbone = Backbone()
        self.policy = PolicyHead(num_actions)
        self.critic = CriticHead()

    def forward(self, x):
        features = self.backbone(x)
        logits = self.policy(features)
        value = self.critic(features)
        return logits, value


def load_model():
    if not os.path.exists(CHECKPOINT_PATH):
        print(f"ERROR: No se encuentra {CHECKPOINT_PATH}")
        print("Ejecuta train_split_model.py primero")
        sys.exit(1)

    model = CombatModelSplit()
    checkpoint = torch.load(CHECKPOINT_PATH, map_location="cpu")
    model.load_state_dict(checkpoint)
    model.eval()
    return model


def export_onnx(model):
    dummy = torch.randn(1, 9)

    torch.onnx.export(
        model.backbone, dummy, BACKBONE_ONNX,
        input_names=["input"], output_names=["features"],
        opset_version=17,
    )
    print(f"Backbone ONNX: {BACKBONE_ONNX}")


def load_calib_data():
    import pandas as pd
    df = pd.read_csv(DATASET_PATH)
    X = df.iloc[:CALIB_SAMPLES, :9].values.astype(np.float32)
    calib_data = torch.from_numpy(X)
    calib_dataset = TensorDataset(calib_data)
    return DataLoader(calib_dataset, batch_size=BATCHSIZE, shuffle=False)


def quantize_backbone(calib_loader):
    print(f"\nQuantizing Backbone...")
    espdl_quantize_onnx(
        onnx_import_file=BACKBONE_ONNX,
        espdl_export_file=BACKBONE_ESPDL,
        calib_dataloader=calib_loader,
        calib_steps=CALIB_SAMPLES,
        input_shape=INPUT_SHAPE,
        target=TARGET,
        num_of_bits=8,
        device="cpu",
        verbose=1,
    )
    size_kb = os.path.getsize(BACKBONE_ESPDL) / 1024
    print(f"  -> {BACKBONE_ESPDL} ({size_kb:.1f} KB)")


def verify(model):
    x = torch.randn(1, 9)
    with torch.no_grad():
        features_ref = model.backbone(x)

    print(f"\nRoundtrip check:")
    print(f"  Input shape: {x.shape}")
    print(f"  Output shape: {features_ref.shape}")
    print(f"  Output range: [{features_ref.min():.3f}, {features_ref.max():.3f}]")


def export_actor_init(model, num_actions=3):
    import struct
    path = os.path.join(MODEL_DIR, "actor_init.bin")
    w = model.policy.fc.weight.data.numpy().astype(np.float32)
    b = model.policy.fc.bias.data.numpy().astype(np.float32)
    magic = 0x50484544
    version = 1
    epoch = 0
    meta = struct.pack("<IIBII", magic, version, num_actions, epoch, 0)
    with open(path, "wb") as f:
        f.write(meta)
        f.write(w.T.flatten().tobytes())
        f.write(b.tobytes())
    print(f"Actor init weights: {path} (actions={num_actions})")


def export_critic(model):
    import struct
    path = os.path.join(MODEL_DIR, "value_head.bin")
    w = model.critic.fc.weight.data.numpy().astype(np.float32)
    b = model.critic.fc.bias.data.numpy().astype(np.float32)
    meta = struct.pack("<III", 0x43524954, 1, 0)
    with open(path, "wb") as f:
        f.write(meta)
        f.write(w.tobytes())
        f.write(b.tobytes())
    print(f"Value head weights: {path}")


def main():
    print("=== Export + Quantize backbone ===\n")

    model = load_model()
    verify(model)

    print("\n--- Exporting ONNX ---")
    export_onnx(model)

    print("\n--- Loading calibration data ---")
    calib_loader = load_calib_data()
    print(f"  {CALIB_SAMPLES} samples loaded")

    quantize_backbone(calib_loader)

    print("\n--- Exporting Policy & Critic binary files ---")
    export_actor_init(model)
    export_critic(model)

    print("\n=== Done ===")
    print("Files for SD card:")
    print(f"  {BACKBONE_ESPDL}")
    print(f"  {os.path.join(MODEL_DIR, 'actor_init.bin')}")
    print(f"  {os.path.join(MODEL_DIR, 'value_head.bin')}")


if __name__ == "__main__":
    main()
