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


class LinearProbe(nn.Module):
    def __init__(self, feature_size, num_classes=3):
        super().__init__()
        self.fc = nn.Linear(feature_size, num_classes)

    def forward(self, x):
        return self.fc(x)


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


EVAL_DATASET_PATH = os.path.join(PROJECT_DIR, "data", "datasets", "eval_dataset.csv")


def load_eval_dataset():
    df = pd.read_csv(EVAL_DATASET_PATH)
    X = df.iloc[:, :9].values.astype(np.float32)
    y = df["label"].values.astype(np.int64)
    return X, y


def extract_features(model, X):
    model.eval()
    with torch.no_grad():
        X_tensor = torch.from_numpy(X).to(DEVICE)
        features = model.backbone(X_tensor)
    return features.cpu().numpy()


def train_linear_probe(features, labels, feature_size, epochs=20):
    X_tensor = torch.from_numpy(features).to(DEVICE)
    y_tensor = torch.from_numpy(labels).to(DEVICE)

    probe = LinearProbe(feature_size).to(DEVICE)
    optimizer = optim.Adam(probe.parameters(), lr=0.001)
    criterion = nn.CrossEntropyLoss()

    dataset = TensorDataset(X_tensor, y_tensor)
    loader = DataLoader(dataset, batch_size=32, shuffle=True)

    for epoch in range(epochs):
        probe.train()
        for Xb, yb in loader:
            optimizer.zero_grad()
            logits = probe(Xb)
            loss = criterion(logits, yb)
            loss.backward()
            optimizer.step()

    probe.eval()
    with torch.no_grad():
        logits = probe(X_tensor)
        preds = logits.argmax(dim=1)
        accuracy = (preds == y_tensor).float().mean().item()

    return accuracy


def calc_feature_quality(features, labels):
    n_features = features.shape[1]
    n_classes = len(np.unique(labels))

    mean_per_class = []
    for c in range(n_classes):
        class_features = features[labels == c]
        mean_per_class.append(class_features.mean(axis=0))

    separability = 0.0
    for i in range(n_classes):
        for j in range(i + 1, n_classes):
            diff = np.linalg.norm(mean_per_class[i] - mean_per_class[j])
            separability += diff
    separability /= (n_classes * (n_classes - 1) / 2)

    variances = features.var(axis=0)
    feature_utility = variances.mean()

    return {
        "separability": separability,
        "feature_utility": feature_utility,
        "mean_variance": variances.mean(),
        "max_variance": variances.max(),
    }


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
        "model": model,
    }


def evaluate_feature_quality(model, X_eval, y_eval):
    features = extract_features(model, X_eval)

    probe_accuracy = train_linear_probe(features, y_eval, model.backbone.fc2.out_features)

    quality_metrics = calc_feature_quality(features, y_eval)

    model.eval()
    with torch.no_grad():
        X_tensor = torch.from_numpy(X_eval).to(DEVICE)
        logits, _ = model(X_tensor)
        probs = torch.softmax(logits, dim=1)
        confidence = probs.max(dim=1)[0].mean().item()
        entropy = -(probs * torch.log(probs + 1e-10)).sum(dim=1).mean().item()

    return {
        "probe_accuracy": probe_accuracy,
        "confidence": confidence,
        "entropy": entropy,
        **quality_metrics,
    }


def save_results(results):
    os.makedirs(RESULTS_DIR, exist_ok=True)

    csv_path = os.path.join(RESULTS_DIR, "feature_quality_experiment.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "feature_size", "params", "final_val_loss", "final_accuracy",
            "probe_accuracy", "confidence", "entropy", "separability",
            "feature_utility", "time_seconds"
        ])
        for r in results:
            writer.writerow([
                r["feature_size"], r["params"], r["final_val_loss"],
                r["final_accuracy"], r["probe_accuracy"], r["confidence"],
                r["entropy"], r["separability"], r["feature_utility"],
                r["time_seconds"]
            ])

    print(f"\nResults saved: {csv_path}")


def main():
    print("=== Feature Quality Experiment ===\n")
    print(f"Device: {DEVICE}")
    print(f"Feature sizes to test: {FEATURE_SIZES}")
    print(f"Epochs: {EPOCHS}\n")

    print("Loading training dataset...")
    X_train, X_test, a_train, a_test, r_train, r_test = load_data()
    print(f"Train: {len(X_train)}  Test: {len(X_test)}")

    print("\nLoading evaluation dataset...")
    X_eval, y_eval = load_eval_dataset()
    print(f"Evaluation: {len(X_eval)} samples (attack={np.sum(y_eval==0)}, defend={np.sum(y_eval==1)}, flee={np.sum(y_eval==2)})\n")

    results = []

    for fs in FEATURE_SIZES:
        print(f"--- Testing feature_size={fs} ---")

        result = train_model(fs, X_train, X_test, a_train, a_test, r_train, r_test)

        print("  Evaluating feature quality...")
        quality = evaluate_feature_quality(result["model"], X_eval, y_eval)
        del result["model"]

        result.update(quality)
        results.append(result)

        print(f"  Params: {result['params']}")
        print(f"  Val Loss: {result['final_val_loss']:.4f}")
        print(f"  Train Accuracy: {result['final_accuracy']:.4f}")
        print(f"  Probe Accuracy: {result['probe_accuracy']:.4f}")
        print(f"  Confidence: {result['confidence']:.4f}")
        print(f"  Separability: {result['separability']:.4f}")
        print(f"  Feature Utility: {result['feature_utility']:.4f}")
        print(f"  Time: {result['time_seconds']:.1f}s\n")

    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"{'Features':<10} {'Params':<8} {'ValLoss':<10} {'TrainAcc':<10} {'ProbeAcc':<10} {'Conf':<8} {'Sep':<8} {'Util':<8}")
    print("-" * 80)
    for r in results:
        print(f"{r['feature_size']:<10} {r['params']:<8} {r['final_val_loss']:<10.4f} {r['final_accuracy']:<10.4f} {r['probe_accuracy']:<10.4f} {r['confidence']:<8.4f} {r['separability']:<8.4f} {r['feature_utility']:<8.4f}")

    best_probe = max(results, key=lambda x: x["probe_accuracy"])
    best_sep = max(results, key=lambda x: x["separability"])

    print(f"\nBest probe accuracy: feature_size={best_probe['feature_size']} ({best_probe['probe_accuracy']:.4f})")
    print(f"Best separability: feature_size={best_sep['feature_size']} ({best_sep['separability']:.4f})")

    save_results(results)


if __name__ == "__main__":
    main()
