import os
import sys
import numpy as np
import pandas as pd
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
from sklearn.model_selection import train_test_split

EPOCHS = 15
BATCH_SIZE = 64
LR = 1e-3
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

class Backbone(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(6, 32)
        self.relu1 = nn.ReLU()
        self.fc2 = nn.Linear(32, 32)
        self.relu2 = nn.ReLU()
        self.fc3 = nn.Linear(32, 16)
        self.relu3 = nn.ReLU()

    def forward(self, x):
        return self.relu3(self.fc3(self.relu2(self.fc2(self.relu1(self.fc1(x))))))

class PolicyHead(nn.Module):
    def __init__(self, num_actions=4):
        super().__init__()
        self.fc = nn.Linear(16, num_actions)

    def forward(self, x):
        return self.fc(x)

class CoreModelSplit(nn.Module):
    def __init__(self, num_actions=4):
        super().__init__()
        self.backbone = Backbone()
        self.policy = PolicyHead(num_actions)

    def forward(self, x):
        features = self.backbone(x)
        logits = self.policy(features)
        return logits

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    print("Loading dataset...")
    dataset_path = os.path.join(script_dir, "core_dataset.csv")
    if not os.path.exists(dataset_path):
        dataset_path = os.path.join(project_dir, "core_dataset.csv")
    if not os.path.exists(dataset_path):
        dataset_path = "core_dataset.csv"

    df = pd.read_csv(dataset_path)

    X = df.iloc[:, :6].values.astype(np.float32)
    y = df["action"].values.astype(np.int64)

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )

    print(f"Train: {len(X_train)}  Test: {len(X_test)}")

    train_dataset = TensorDataset(
        torch.from_numpy(X_train),
        torch.from_numpy(y_train)
    )
    test_dataset = TensorDataset(
        torch.from_numpy(X_test),
        torch.from_numpy(y_test)
    )
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=BATCH_SIZE)

    model = CoreModelSplit().to(DEVICE)
    optimizer = optim.Adam(model.parameters(), lr=LR)
    criterion = nn.CrossEntropyLoss()

    total_params = sum(p.numel() for p in model.parameters())
    print(f"Parameters: {total_params}")

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

        train_loss /= len(X_train)
        val_loss /= len(X_test)
        acc = correct / total
        print(f"Epoch {epoch+1:3d}/{EPOCHS}  loss: {train_loss:.4f}  val_loss: {val_loss:.4f}  acc: {acc:.4f}")

    print(f"\nTest accuracy: {acc:.4f}")

    model_dir = os.path.join(project_dir, "models")
    os.makedirs(model_dir, exist_ok=True)
    checkpoint_path = os.path.join(model_dir, "core_model.pth")
    torch.save(model.state_dict(), checkpoint_path)
    print(f"Model saved: {checkpoint_path}")

if __name__ == "__main__":
    main()
