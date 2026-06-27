import os
import sys
import csv
import random
import numpy as np
import pandas as pd
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
DATASET_PATH = os.path.join(PROJECT_DIR, "data", "datasets", "combat_dataset.csv")
RESULTS_DIR = os.path.join(PROJECT_DIR, "experiments")

EPOCHS = 15
BATCH_SIZE = 64
LR = 1e-3
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

FEATURE_SIZES = [4, 8, 16, 32, 64]


class Backbone(nn.Module):
    def __init__(self, input_size=9, feature_size=16):
        super().__init__()
        self.fc1 = nn.Linear(input_size, feature_size)
        self.relu1 = nn.ReLU()
        self.fc2 = nn.Linear(feature_size, feature_size)
        self.relu2 = nn.ReLU()

    def forward(self, x):
        return self.relu2(self.fc2(self.relu1(self.fc1(x))))


class PolicyHead(nn.Module):
    def __init__(self, feature_size=16, num_actions=3):
        super().__init__()
        self.fc = nn.Linear(feature_size, num_actions)

    def forward(self, x):
        return self.fc(x)


class CriticHead(nn.Module):
    def __init__(self, feature_size=16):
        super().__init__()
        self.fc = nn.Linear(feature_size, 1)

    def forward(self, x):
        return self.fc(x)


class CombatModel(nn.Module):
    def __init__(self, feature_size=16, num_actions=3):
        super().__init__()
        self.backbone = Backbone(9, feature_size)
        self.policy = PolicyHead(feature_size, num_actions)
        self.critic = CriticHead(feature_size)

    def forward(self, x):
        features = self.backbone(x)
        logits = self.policy(features)
        value = self.critic(features)
        return logits, value


def load_data():
    df = pd.read_csv(DATASET_PATH)
    X = df.iloc[:, :9].values.astype(np.float32)
    y_actions = df["action"].values.astype(np.int64)
    y_returns = df["ret"].values.astype(np.float32)

    from sklearn.model_selection import train_test_split
    X_train, X_test, a_train, a_test, r_train, r_test = train_test_split(
        X, y_actions, y_returns, test_size=0.2, random_state=42
    )

    return X_train, X_test, a_train, a_test, r_train, r_test


def train_model(feature_size, X_train, X_test, a_train, a_test, r_train, r_test):
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

    model = CombatModel(feature_size=feature_size).to(DEVICE)
    optimizer = optim.Adam(model.parameters(), lr=LR)
    criterion_policy = nn.CrossEntropyLoss()
    criterion_value = nn.MSELoss()

    total_params = sum(p.numel() for p in model.parameters())
    history = {"train_loss": [], "val_loss": [], "accuracy": []}

    start_time = time.time()

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

        history["train_loss"].append(train_loss)
        history["val_loss"].append(val_loss)
        history["accuracy"].append(acc)

    elapsed = time.time() - start_time

    return {
        "feature_size": feature_size,
        "params": total_params,
        "final_train_loss": history["train_loss"][-1],
        "final_val_loss": history["val_loss"][-1],
        "final_accuracy": history["accuracy"][-1],
        "best_val_loss": min(history["val_loss"]),
        "best_accuracy": max(history["accuracy"]),
        "time_seconds": elapsed,
        "history": history,
    }


def save_results(results):
    os.makedirs(RESULTS_DIR, exist_ok=True)

    csv_path = os.path.join(RESULTS_DIR, "feature_size_experiment.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "feature_size", "params", "final_train_loss", "final_val_loss",
            "final_accuracy", "best_val_loss", "best_accuracy", "time_seconds"
        ])
        for r in results:
            writer.writerow([
                r["feature_size"], r["params"], r["final_train_loss"],
                r["final_val_loss"], r["final_accuracy"], r["best_val_loss"],
                r["best_accuracy"], r["time_seconds"]
            ])

    print(f"\nResults saved: {csv_path}")

    try:
        import matplotlib.pyplot as plt

        fig, axes = plt.subplots(1, 3, figsize=(15, 5))

        feature_sizes = [r["feature_size"] for r in results]

        axes[0].plot(feature_sizes, [r["final_val_loss"] for r in results], "bo-")
        axes[0].set_xlabel("Feature Size")
        axes[0].set_ylabel("Validation Loss")
        axes[0].set_title("Val Loss vs Feature Size")
        axes[0].grid(True)

        axes[1].plot(feature_sizes, [r["final_accuracy"] for r in results], "ro-")
        axes[1].set_xlabel("Feature Size")
        axes[1].set_ylabel("Accuracy")
        axes[1].set_title("Accuracy vs Feature Size")
        axes[1].grid(True)

        axes[2].plot(feature_sizes, [r["params"] for r in results], "go-")
        axes[2].set_xlabel("Feature Size")
        axes[2].set_ylabel("Parameters")
        axes[2].set_title("Parameters vs Feature Size")
        axes[2].grid(True)

        plt.tight_layout()
        plot_path = os.path.join(RESULTS_DIR, "feature_size_experiment.png")
        plt.savefig(plot_path, dpi=150)
        print(f"Plot saved: {plot_path}")

    except ImportError:
        print("matplotlib not installed, skipping plot")


def main():
    print("=== Feature Size Experiment ===\n")
    print(f"Device: {DEVICE}")
    print(f"Feature sizes to test: {FEATURE_SIZES}")
    print(f"Epochs: {EPOCHS}\n")

    print("Loading dataset...")
    X_train, X_test, a_train, a_test, r_train, r_test = load_data()
    print(f"Train: {len(X_train)}  Test: {len(X_test)}\n")

    results = []

    for fs in FEATURE_SIZES:
        print(f"--- Testing feature_size={fs} ---")
        result = train_model(fs, X_train, X_test, a_train, a_test, r_train, r_test)
        results.append(result)
        print(f"  Params: {result['params']}")
        print(f"  Val Loss: {result['final_val_loss']:.4f}")
        print(f"  Accuracy: {result['final_accuracy']:.4f}")
        print(f"  Time: {result['time_seconds']:.1f}s\n")

    print("=" * 50)
    print("SUMMARY")
    print("=" * 50)
    print(f"{'Features':<10} {'Params':<10} {'Val Loss':<12} {'Accuracy':<10} {'Time':<10}")
    print("-" * 50)
    for r in results:
        print(f"{r['feature_size']:<10} {r['params']:<10} {r['final_val_loss']:<12.4f} {r['final_accuracy']:<10.4f} {r['time_seconds']:<10.1f}")

    best = min(results, key=lambda x: x["final_val_loss"])
    print(f"\nBest: feature_size={best['feature_size']} (val_loss={best['final_val_loss']:.4f})")

    save_results(results)


if __name__ == "__main__":
    main()
