import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from sklearn.model_selection import train_test_split

INPUT_FILE = "data/datasets/combat_dataset.csv"
MODEL_DIR = "models"

EPOCHS = 100
BATCH_SIZE = 64
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"


class CombatModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(8, 16),
            nn.ReLU(),
            nn.Linear(16, 8),
            nn.ReLU(),
            nn.Linear(8, 3),
        )

    def forward(self, x):
        return self.net(x)


def main():
    print("Cargando dataset...")
    df = pd.read_csv(INPUT_FILE)

    X = df.iloc[:, :8].values.astype(np.float32)
    y = df["action"].values.astype(np.int64)

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )

    print(f"Train: {len(X_train)}  Test: {len(X_test)}")

    X_train_t = torch.from_numpy(X_train)
    y_train_t = torch.from_numpy(y_train)
    X_test_t = torch.from_numpy(X_test)
    y_test_t = torch.from_numpy(y_test)

    train_dataset = torch.utils.data.TensorDataset(X_train_t, y_train_t)
    test_dataset = torch.utils.data.TensorDataset(X_test_t, y_test_t)
    train_loader = torch.utils.data.DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    test_loader = torch.utils.data.DataLoader(test_dataset, batch_size=BATCH_SIZE)

    model = CombatModel().to(DEVICE)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters())

    total_params = sum(p.numel() for p in model.parameters())
    print(f"Parametros: {total_params}")

    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0.0
        for Xb, yb in train_loader:
            Xb, yb = Xb.to(DEVICE), yb.to(DEVICE)
            optimizer.zero_grad()
            logits = model(Xb)
            loss = criterion(logits, yb)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * Xb.size(0)

        model.eval()
        correct = 0
        total = 0
        val_loss = 0.0
        with torch.no_grad():
            for Xb, yb in test_loader:
                Xb, yb = Xb.to(DEVICE), yb.to(DEVICE)
                logits = model(Xb)
                loss = criterion(logits, yb)
                val_loss += loss.item() * Xb.size(0)
                preds = logits.argmax(dim=1)
                correct += (preds == yb).sum().item()
                total += yb.size(0)

        train_loss /= len(X_train_t)
        val_loss /= len(X_test_t)
        acc = correct / total
        print(f"Epoch {epoch+1:3d}/{EPOCHS}  loss: {train_loss:.4f}  val_loss: {val_loss:.4f}  acc: {acc:.4f}")

    print(f"\nTest accuracy: {acc:.4f}")

    os.makedirs(MODEL_DIR, exist_ok=True)
    onnx_path = f"{MODEL_DIR}/combat_ai.onnx"

    dummy = torch.randn(1, 8).to(DEVICE)
    torch.onnx.export(
        model,
        dummy,
        onnx_path,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={"input": {0: "batch_size"}, "output": {0: "batch_size"}},
        opset_version=17,
        dynamo=False,
    )
    print(f"Modelo exportado: {onnx_path}")


if __name__ == "__main__":
    import os
    main()
