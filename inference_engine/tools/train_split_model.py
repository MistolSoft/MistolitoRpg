import os
import sys
import numpy as np
import pandas as pd
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
DATASET_PATH = os.path.join(PROJECT_DIR, "data", "datasets", "combat_dataset.csv")
MODEL_DIR = os.path.join(PROJECT_DIR, "models")

EPOCHS = 200
BATCH_SIZE = 64
LR = 1e-3
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"


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


def main():
    print("Loading dataset...")
    df = pd.read_csv(DATASET_PATH)

    X = df.iloc[:, :9].values.astype(np.float32)
    y_actions = df["action"].values.astype(np.int64)
    y_returns = df["ret"].values.astype(np.float32)

    from sklearn.model_selection import train_test_split
    X_train, X_test, a_train, a_test, r_train, r_test = train_test_split(
        X, y_actions, y_returns, test_size=0.2, random_state=42
    )

    print(f"Train: {len(X_train)}  Test: {len(X_test)}")

    train_dataset = TensorDataset(
        torch.from_numpy(X_train),
        torch.from_numpy(a_train),
        torch.from_numpy(r_train)
    )
    test_dataset = TensorDataset(
        torch.from_numpy(X_test),
        torch.from_numpy(a_test),
        torch.from_numpy(r_test)
    )
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=BATCH_SIZE)

    model = CombatModelSplit().to(DEVICE)
    optimizer = optim.Adam(model.parameters(), lr=LR)
    criterion_policy = nn.CrossEntropyLoss()
    criterion_value = nn.MSELoss()

    total_params = sum(p.numel() for p in model.parameters())
    print(f"Parameters: {total_params}")

    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0.0

        for Xb, ab, rb in train_loader:
            Xb, ab, rb = Xb.to(DEVICE), ab.to(DEVICE), rb.to(DEVICE)

            optimizer.zero_grad()
            logits, value = model(Xb)

            loss_policy = criterion_policy(logits, ab)
            loss_value = criterion_value(value.squeeze(-1), rb)
            loss = loss_policy + 0.5 * loss_value

            loss.backward()
            optimizer.step()
            train_loss += loss.item() * Xb.size(0)

        model.eval()
        correct = 0
        total = 0
        val_loss = 0.0

        with torch.no_grad():
            for Xb, ab, rb in test_loader:
                Xb, ab, rb = Xb.to(DEVICE), ab.to(DEVICE), rb.to(DEVICE)
                logits, value = model(Xb)

                loss_policy = criterion_policy(logits, ab)
                loss_value = criterion_value(value.squeeze(-1), rb)
                loss = loss_policy + 0.5 * loss_value

                val_loss += loss.item() * Xb.size(0)
                preds = logits.argmax(dim=1)
                correct += (preds == ab).sum().item()
                total += ab.size(0)

        train_loss /= len(X_train)
        val_loss /= len(X_test)
        acc = correct / total
        print(f"Epoch {epoch+1:3d}/{EPOCHS}  loss: {train_loss:.4f}  val_loss: {val_loss:.4f}  acc: {acc:.4f}")

    print(f"\nTest accuracy: {acc:.4f}")

    os.makedirs(MODEL_DIR, exist_ok=True)
    checkpoint_path = os.path.join(MODEL_DIR, "combat_ai_split.pth")
    torch.save(model.state_dict(), checkpoint_path)
    print(f"Model saved: {checkpoint_path}")


if __name__ == "__main__":
    main()
