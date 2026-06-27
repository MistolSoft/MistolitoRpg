import os
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
RESULTS_DIR = os.path.join(PROJECT_DIR, "experiments")

EPOCHS = 20
BATCH_SIZE = 32
LR = 1e-3
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

HIDDEN_SIZES = [2, 4, 8, 16]
INPUT_SIZE = 8


class CriticNet(nn.Module):
    def __init__(self, hidden_size=4):
        super().__init__()
        self.fc1 = nn.Linear(INPUT_SIZE, hidden_size)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(hidden_size, 1)
        self.tanh = nn.Tanh()

    def forward(self, x):
        x = self.relu(self.fc1(x))
        x = self.tanh(self.fc2(x))
        return x.squeeze(-1)


def calc_luck_hit(hit, p_success):
    if hit:
        return 1.0 - p_success
    return -p_success


def calc_luck_dmg(dmg_real, dmg_min, dmg_max):
    dmg_range = dmg_max - dmg_min
    if dmg_range <= 0:
        return 0.0
    normalized = (dmg_real - dmg_min) / dmg_range
    return normalized * 2.0 - 1.0


def calc_hit_prob(target_ac, attack_bonus):
    roll_needed = target_ac - attack_bonus
    if roll_needed < 2:
        roll_needed = 2
    if roll_needed > 20:
        roll_needed = 20
    return (21.0 - float(roll_needed)) / 20.0


def calc_defense_prob(pet_ac, enemy_attack):
    roll_needed = pet_ac - enemy_attack
    if roll_needed < 2:
        roll_needed = 2
    if roll_needed > 20:
        roll_needed = 20
    return (21.0 - float(roll_needed)) / 20.0


def calc_avg_pet_damage(dice_count, dice_size, str_mod):
    avg_roll = (dice_size + 1) / 2.0
    return max(1.0, dice_count * avg_roll + str_mod)


def calc_avg_enemy_damage(dice_size, damage_bonus):
    avg_roll = (dice_size + 1) / 2.0
    return max(1.0, avg_roll + damage_bonus)


def decide_action(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action):
    if pet_hp_ratio < 0.15:
        return 2
    if pet_hp_ratio < 0.3 and enemy_hp_ratio > 2.0:
        return 2

    if quality_score < -0.3:
        if last_action == 0:
            if threat_level > 0.5:
                return 1
            elif pet_hp_ratio < 0.3:
                return 2
        elif last_action == 1:
            if hit_prob > 0.5:
                return 0
        elif last_action == 2:
            if hit_prob > 0.4:
                return 0

    if quality_score > 0.3:
        if last_action == 0:
            return 0
        elif last_action == 1:
            if hit_prob > 0.6:
                return 0

    if hit_prob > 0.65 and threat_level < 0.4 and dmg_efficiency > 0.3:
        return 0
    if hit_prob > 0.5 and pet_hp_ratio > 0.6 and enemy_hp_ratio < 1.0:
        return 0
    if hit_prob > 0.45 and pet_hp_ratio > 0.5 and threat_level < 0.5:
        return 0

    if threat_level > 0.6 and pet_hp_ratio < 0.4:
        return 1
    if enemy_hp_ratio > 1.8 and pet_hp_ratio < 0.5:
        return 1
    if defense_prob > 0.5 and pet_hp_ratio < 0.6:
        return 1

    return 0


def gen_scenario(type_name):
    if type_name == "overwhelming_advantage":
        pet_hp_max = random.randint(60, 80)
        pet_hp = random.randint(int(pet_hp_max * 0.8), pet_hp_max)
        pet_dex_mod = random.randint(2, 4)
        pet_str_mod = random.randint(2, 4)
        pet_dice_count = 3
        pet_dice_size = random.choice([6, 8])
        enemy_hp_max = random.randint(15, 30)
        enemy_hp = random.randint(int(enemy_hp_max * 0.2), int(enemy_hp_max * 0.5))
        enemy_attack = random.randint(1, 4)
        enemy_ac = random.randint(7, 10)
        enemy_damage_dice = random.choice([4, 6])
        enemy_damage_bonus = random.randint(0, 1)

    elif type_name == "slight_advantage":
        pet_hp_max = random.randint(50, 70)
        pet_hp = random.randint(int(pet_hp_max * 0.6), pet_hp_max)
        pet_dex_mod = random.randint(1, 3)
        pet_str_mod = random.randint(1, 3)
        pet_dice_count = 3
        pet_dice_size = random.choice([4, 6])
        enemy_hp_max = random.randint(25, 45)
        enemy_hp = random.randint(int(enemy_hp_max * 0.4), int(enemy_hp_max * 0.7))
        enemy_attack = random.randint(2, 6)
        enemy_ac = random.randint(8, 12)
        enemy_damage_dice = random.choice([4, 6, 8])
        enemy_damage_bonus = random.randint(0, 2)

    elif type_name == "even_match":
        pet_hp_max = random.randint(40, 60)
        pet_hp = random.randint(int(pet_hp_max * 0.4), int(pet_hp_max * 0.7))
        pet_dex_mod = random.randint(1, 3)
        pet_str_mod = random.randint(1, 3)
        pet_dice_count = 3
        pet_dice_size = random.choice([4, 6])
        enemy_hp_max = random.randint(30, 50)
        enemy_hp = random.randint(int(enemy_hp_max * 0.4), int(enemy_hp_max * 0.7))
        enemy_attack = random.randint(3, 7)
        enemy_ac = random.randint(9, 13)
        enemy_damage_dice = random.choice([6, 8])
        enemy_damage_bonus = random.randint(1, 3)

    elif type_name == "slight_disadvantage":
        pet_hp_max = random.randint(35, 55)
        pet_hp = random.randint(int(pet_hp_max * 0.3), int(pet_hp_max * 0.5))
        pet_dex_mod = random.randint(0, 2)
        pet_str_mod = random.randint(0, 2)
        pet_dice_count = 2
        pet_dice_size = random.choice([4, 6])
        enemy_hp_max = random.randint(35, 55)
        enemy_hp = random.randint(int(enemy_hp_max * 0.5), int(enemy_hp_max * 0.8))
        enemy_attack = random.randint(5, 9)
        enemy_ac = random.randint(11, 14)
        enemy_damage_dice = random.choice([6, 8, 10])
        enemy_damage_bonus = random.randint(2, 4)

    elif type_name == "desperate":
        pet_hp_max = random.randint(30, 50)
        pet_hp = random.randint(1, int(pet_hp_max * 0.2))
        pet_dex_mod = random.randint(0, 2)
        pet_str_mod = random.randint(0, 2)
        pet_dice_count = 2
        pet_dice_size = random.choice([4, 6])
        enemy_hp_max = random.randint(40, 70)
        enemy_hp = random.randint(int(enemy_hp_max * 0.6), enemy_hp_max)
        enemy_attack = random.randint(6, 12)
        enemy_ac = random.randint(10, 14)
        enemy_damage_dice = random.choice([8, 10, 12])
        enemy_damage_bonus = random.randint(3, 6)

    elif type_name == "glass_cannon":
        pet_hp_max = random.randint(25, 40)
        pet_hp = random.randint(int(pet_hp_max * 0.3), int(pet_hp_max * 0.6))
        pet_dex_mod = random.randint(3, 4)
        pet_str_mod = random.randint(3, 4)
        pet_dice_count = 3
        pet_dice_size = random.choice([6, 8])
        enemy_hp_max = random.randint(20, 35)
        enemy_hp = random.randint(int(enemy_hp_max * 0.3), int(enemy_hp_max * 0.6))
        enemy_attack = random.randint(4, 8)
        enemy_ac = random.randint(8, 11)
        enemy_damage_dice = random.choice([8, 10])
        enemy_damage_bonus = random.randint(2, 5)

    else:
        pet_hp_max = random.randint(40, 80)
        pet_hp = random.randint(int(pet_hp_max * 0.4), pet_hp_max)
        pet_dex_mod = random.randint(1, 4)
        pet_str_mod = random.randint(1, 4)
        pet_dice_count = 3
        pet_dice_size = random.choice([4, 6])
        enemy_hp_max = random.randint(20, 60)
        enemy_hp = random.randint(int(enemy_hp_max * 0.3), enemy_hp_max)
        enemy_attack = random.randint(2, 8)
        enemy_ac = random.randint(8, 14)
        enemy_damage_dice = random.choice([4, 6, 8])
        enemy_damage_bonus = random.randint(0, 3)

    return {
        "pet_hp_max": pet_hp_max,
        "pet_hp": pet_hp,
        "pet_dex_mod": pet_dex_mod,
        "pet_ac": 10 + pet_dex_mod,
        "pet_str_mod": pet_str_mod,
        "pet_dice_count": pet_dice_count,
        "pet_dice_size": pet_dice_size,
        "enemy_hp_max": enemy_hp_max,
        "enemy_hp": enemy_hp,
        "enemy_attack": enemy_attack,
        "enemy_ac": enemy_ac,
        "enemy_damage_dice": enemy_damage_dice,
        "enemy_damage_bonus": enemy_damage_bonus,
    }


SCENARIOS = [
    "overwhelming_advantage",
    "slight_advantage",
    "even_match",
    "slight_disadvantage",
    "desperate",
    "glass_cannon",
]


def simulate_combat():
    scenario_type = random.choice(SCENARIOS)
    s = gen_scenario(scenario_type)

    pet_hp_max = s["pet_hp_max"]
    pet_hp = s["pet_hp"]
    pet_dex_mod = s["pet_dex_mod"]
    pet_ac = s["pet_ac"]
    pet_str_mod = s["pet_str_mod"]
    pet_dice_count = s["pet_dice_count"]
    pet_dice_size = s["pet_dice_size"]

    enemy_hp_max = s["enemy_hp_max"]
    enemy_hp = s["enemy_hp"]
    enemy_attack = s["enemy_attack"]
    enemy_ac = s["enemy_ac"]
    enemy_damage_dice = s["enemy_damage_dice"]
    enemy_damage_bonus = s["enemy_damage_bonus"]

    hit_prob = calc_hit_prob(enemy_ac, pet_dex_mod)
    defense_prob = calc_defense_prob(pet_ac, enemy_attack)
    avg_pet_dmg = calc_avg_pet_damage(pet_dice_count, pet_dice_size, pet_str_mod)
    avg_enemy_dmg = calc_avg_enemy_damage(enemy_damage_dice, enemy_damage_bonus)

    total_damage_dealt = 0.0
    total_damage_taken = 0.0
    hits = 0
    total_turns = 0

    samples = []

    max_turns = 10
    quality_score = 0.0
    last_action = 0

    for turn in range(max_turns):
        if pet_hp <= 0 or enemy_hp <= 0:
            break

        my_hp_ratio = pet_hp / pet_hp_max if pet_hp_max > 0 else 0.0
        enemy_hp_ratio_val = enemy_hp / enemy_hp_max if enemy_hp_max > 0 else 0.0
        dmg_eff = avg_pet_dmg / enemy_hp if enemy_hp > 0 else 1.0
        threat = avg_enemy_dmg / pet_hp if pet_hp > 0 else 1.0

        action = decide_action(hit_prob, defense_prob, my_hp_ratio, enemy_hp_ratio_val, threat, dmg_eff, quality_score, last_action)

        pet_hit = False
        damage_dealt = 0.0
        damage_taken = 0.0

        if action == 0:
            pet_roll = random.randint(1, 20) + pet_dex_mod
            if pet_roll >= enemy_ac:
                pet_hit = True
                damage_dealt = float(max(1, random.randint(1, pet_dice_size) * pet_dice_count + pet_str_mod))
                enemy_hp = max(0, enemy_hp - int(damage_dealt))
                total_damage_dealt += damage_dealt
                hits += 1

            enemy_roll = random.randint(1, 20) + enemy_attack
            if enemy_roll >= pet_ac:
                damage_taken = float(max(1, random.randint(1, enemy_damage_dice) + enemy_damage_bonus))
                pet_hp = max(0, pet_hp - int(damage_taken))
                total_damage_taken += damage_taken

        elif action == 1:
            enemy_roll = random.randint(1, 20) + enemy_attack
            if enemy_roll >= pet_ac - 3:
                raw = max(1, random.randint(1, enemy_damage_dice) + enemy_damage_bonus)
                damage_taken = float(max(0, raw - 3))
                pet_hp = max(0, pet_hp - int(damage_taken))
                total_damage_taken += damage_taken

        else:
            flee_roll = random.randint(1, 20) + pet_dex_mod
            if flee_roll >= 12:
                break
            else:
                enemy_roll = random.randint(1, 20) + enemy_attack
                if enemy_roll >= pet_ac:
                    damage_taken = float(max(1, random.randint(1, enemy_damage_dice) + enemy_damage_bonus))
                    pet_hp = max(0, pet_hp - int(damage_taken))
                    total_damage_taken += damage_taken

        total_turns += 1

        my_hp_ratio = pet_hp / pet_hp_max if pet_hp_max > 0 else 0.0
        enemy_hp_ratio_val = enemy_hp / enemy_hp_max if enemy_hp_max > 0 else 0.0
        p_success_used = hit_prob if action == 0 else 0.5

        f_luck_hit = calc_luck_hit(pet_hit, p_success_used)
        f_luck_dmg = calc_luck_dmg(damage_dealt, 1.0, float(pet_dice_count * pet_dice_size + pet_str_mod))
        f_hp_advantage = my_hp_ratio - enemy_hp_ratio_val
        f_dmg_momentum = (total_damage_dealt - total_damage_taken) / pet_hp_max if pet_hp_max > 0 else 0.0
        f_success_rate = hits / total_turns if total_turns > 0 else 0.0
        f_threat = avg_enemy_dmg / pet_hp if pet_hp > 0 else 1.0
        f_turns = total_turns / 5.0
        f_win_prob = my_hp_ratio / (my_hp_ratio + enemy_hp_ratio_val) if (my_hp_ratio + enemy_hp_ratio_val) > 0 else 0.5

        features = [
            max(-1.0, min(1.0, f_luck_hit)),
            max(-1.0, min(1.0, f_luck_dmg)),
            max(-1.0, min(1.0, f_hp_advantage)),
            max(-1.0, min(1.0, f_dmg_momentum)),
            max(-1.0, min(1.0, f_success_rate * 2 - 1)),
            max(-1.0, min(1.0, f_threat * 2 - 1)),
            max(-1.0, min(1.0, f_turns * 2 - 1)),
            max(-1.0, min(1.0, f_win_prob * 2 - 1)),
        ]

        action_effectiveness = hits / total_turns if total_turns > 0 else 0.5
        strategy_fit = 0.0
        if action == 0:
            strategy_fit = action_effectiveness * 2 - 1
        elif action == 1:
            damage_reduction = 1.0 - (damage_taken / avg_enemy_dmg) if avg_enemy_dmg > 0 else 0.5
            strategy_fit = damage_reduction * 2 - 1
        else:
            strategy_fit = -0.5

        hp_adv = my_hp_ratio - enemy_hp_ratio_val
        target_quality = 0.4 * hp_adv + 0.3 * (action_effectiveness * 2 - 1) + 0.3 * strategy_fit
        target_quality = max(-1.0, min(1.0, target_quality))

        samples.append({
            "features": features,
            "target": target_quality,
        })

        quality_score = target_quality
        last_action = action

    return samples


def generate_dataset(n_episodes=10000):
    all_samples = []
    for _ in range(n_episodes):
        samples = simulate_combat()
        all_samples.extend(samples)
    return all_samples


def train_model(hidden_size, X_train, X_test, y_train, y_test):
    train_dataset = TensorDataset(torch.from_numpy(X_train), torch.from_numpy(y_train))
    test_dataset = TensorDataset(torch.from_numpy(X_test), torch.from_numpy(y_test))
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=BATCH_SIZE)

    model = CriticNet(hidden_size=hidden_size).to(DEVICE)
    optimizer = optim.Adam(model.parameters(), lr=LR)
    criterion = nn.MSELoss()

    total_params = sum(p.numel() for p in model.parameters())
    history = {"train_loss": [], "val_loss": []}

    start_time = time.time()

    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0.0

        for Xb, yb in train_loader:
            Xb, yb = Xb.to(DEVICE), yb.to(DEVICE)
            optimizer.zero_grad()
            pred = model(Xb)
            loss = criterion(pred, yb)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * Xb.size(0)

        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for Xb, yb in test_loader:
                Xb, yb = Xb.to(DEVICE), yb.to(DEVICE)
                pred = model(Xb)
                loss = criterion(pred, yb)
                val_loss += loss.item() * Xb.size(0)

        train_loss /= len(X_train)
        val_loss /= len(X_test)
        history["train_loss"].append(train_loss)
        history["val_loss"].append(val_loss)

    elapsed = time.time() - start_time

    return {
        "hidden_size": hidden_size,
        "params": total_params,
        "final_train_loss": history["train_loss"][-1],
        "final_val_loss": history["val_loss"][-1],
        "best_val_loss": min(history["val_loss"]),
        "time_seconds": elapsed,
        "history": history,
        "model": model,
    }


def evaluate_model(model, X_eval):
    model.eval()
    with torch.no_grad():
        X_tensor = torch.from_numpy(X_eval).to(DEVICE)
        predictions = model(X_tensor).cpu().numpy()

    mean_pred = predictions.mean()
    std_pred = predictions.std()
    min_pred = predictions.min()
    max_pred = predictions.max()

    coverage = np.sum((predictions > -0.5) & (predictions < 0.5)) / len(predictions)

    return {
        "mean_pred": mean_pred,
        "std_pred": std_pred,
        "min_pred": min_pred,
        "max_pred": max_pred,
        "coverage_50pct": coverage,
    }


def save_results(results):
    os.makedirs(RESULTS_DIR, exist_ok=True)

    csv_path = os.path.join(RESULTS_DIR, "critic_arch_experiment.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "hidden_size", "params", "final_train_loss", "final_val_loss",
            "best_val_loss", "mean_pred", "std_pred", "min_pred", "max_pred",
            "coverage_50pct", "time_seconds"
        ])
        for r in results:
            writer.writerow([
                r["hidden_size"], r["params"], r["final_train_loss"],
                r["final_val_loss"], r["best_val_loss"], r["mean_pred"],
                r["std_pred"], r["min_pred"], r["max_pred"],
                r["coverage_50pct"], r["time_seconds"]
            ])

    print(f"\nResults saved: {csv_path}")


def main():
    print("=== Critic Architecture Experiment ===\n")
    print(f"Device: {DEVICE}")
    print(f"Hidden sizes to test: {HIDDEN_SIZES}")
    print(f"Epochs: {EPOCHS}\n")

    print("Generating combat dataset...")
    dataset = generate_dataset(n_episodes=10000)
    print(f"Total samples: {len(dataset)}")

    X = np.array([s["features"] for s in dataset], dtype=np.float32)
    y = np.array([s["target"] for s in dataset], dtype=np.float32)

    from sklearn.model_selection import train_test_split
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
    print(f"Train: {len(X_train)}  Test: {len(X_test)}\n")

    X_eval = X_test[:500]

    results = []

    for hs in HIDDEN_SIZES:
        print(f"--- Testing hidden_size={hs} ---")

        result = train_model(hs, X_train, X_test, y_train, y_test)

        print("  Evaluating predictions...")
        eval_metrics = evaluate_model(result["model"], X_eval)
        del result["model"]

        result.update(eval_metrics)
        results.append(result)

        print(f"  Params: {result['params']}")
        print(f"  Val Loss: {result['final_val_loss']:.4f}")
        print(f"  Mean Pred: {result['mean_pred']:.4f}")
        print(f"  Std Pred: {result['std_pred']:.4f}")
        print(f"  Coverage 50%: {result['coverage_50pct']:.4f}")
        print(f"  Time: {result['time_seconds']:.1f}s\n")

    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"{'Hidden':<10} {'Params':<8} {'ValLoss':<10} {'MeanPred':<10} {'StdPred':<10} {'Cov50%':<10} {'Time':<10}")
    print("-" * 80)
    for r in results:
        print(f"{r['hidden_size']:<10} {r['params']:<8} {r['final_val_loss']:<10.4f} {r['mean_pred']:<10.4f} {r['std_pred']:<10.4f} {r['coverage_50pct']:<10.4f} {r['time_seconds']:<10.1f}")

    best = min(results, key=lambda x: x["final_val_loss"])
    print(f"\nBest: hidden_size={best['hidden_size']} (val_loss={best['final_val_loss']:.4f})")

    save_results(results)


if __name__ == "__main__":
    main()
