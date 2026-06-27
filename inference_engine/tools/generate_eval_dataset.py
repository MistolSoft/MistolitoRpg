import os
import csv
import random

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
DATASET_DIR = os.path.join(PROJECT_DIR, "data", "datasets")


def clamp(value, min_val, max_val):
    return max(min_val, min(max_val, value))


def gen_attack_situation():
    pet_hp_ratio = random.uniform(0.6, 1.0)
    pet_energy_ratio = random.uniform(0.5, 1.0)
    hit_prob = random.uniform(0.55, 0.95)
    defense_prob = random.uniform(0.2, 0.5)
    enemy_hp_ratio = random.uniform(0.3, 0.9)
    level_ratio = random.uniform(0.7, 1.1)
    dmg_efficiency = random.uniform(0.25, 0.8)
    threat_level = random.uniform(0.1, 0.4)
    quality_score = random.uniform(-0.1, 0.3)

    return {
        "inputs": [
            round(pet_hp_ratio, 4),
            round(pet_energy_ratio, 4),
            round(hit_prob, 4),
            round(defense_prob, 4),
            round(enemy_hp_ratio, 4),
            round(level_ratio, 4),
            round(dmg_efficiency, 4),
            round(threat_level, 4),
            round(quality_score, 4),
        ],
        "label": 0,
        "situation": "attack",
    }


def gen_defend_situation():
    pet_hp_ratio = random.uniform(0.3, 0.65)
    pet_energy_ratio = random.uniform(0.3, 0.7)
    hit_prob = random.uniform(0.3, 0.55)
    defense_prob = random.uniform(0.4, 0.65)
    enemy_hp_ratio = random.uniform(1.0, 2.0)
    level_ratio = random.uniform(1.0, 1.5)
    dmg_efficiency = random.uniform(0.1, 0.3)
    threat_level = random.uniform(0.35, 0.65)
    quality_score = random.uniform(-0.2, 0.1)

    return {
        "inputs": [
            round(pet_hp_ratio, 4),
            round(pet_energy_ratio, 4),
            round(hit_prob, 4),
            round(defense_prob, 4),
            round(enemy_hp_ratio, 4),
            round(level_ratio, 4),
            round(dmg_efficiency, 4),
            round(threat_level, 4),
            round(quality_score, 4),
        ],
        "label": 1,
        "situation": "defend",
    }


def gen_flee_situation():
    pet_hp_ratio = random.uniform(0.05, 0.25)
    pet_energy_ratio = random.uniform(0.1, 0.4)
    hit_prob = random.uniform(0.15, 0.35)
    defense_prob = random.uniform(0.15, 0.4)
    enemy_hp_ratio = random.uniform(2.0, 4.0)
    level_ratio = random.uniform(1.3, 2.0)
    dmg_efficiency = random.uniform(0.03, 0.15)
    threat_level = random.uniform(0.65, 1.0)
    quality_score = random.uniform(-0.5, -0.1)

    return {
        "inputs": [
            round(pet_hp_ratio, 4),
            round(pet_energy_ratio, 4),
            round(hit_prob, 4),
            round(defense_prob, 4),
            round(enemy_hp_ratio, 4),
            round(level_ratio, 4),
            round(dmg_efficiency, 4),
            round(threat_level, 4),
            round(quality_score, 4),
        ],
        "label": 2,
        "situation": "flee",
    }


def main():
    import sys
    n = 1500
    if len(sys.argv) > 1:
        n = int(sys.argv[1])

    per_class = n // 3
    all_samples = []

    generators = [gen_attack_situation, gen_defend_situation, gen_flee_situation]

    for gen_func in generators:
        for _ in range(per_class):
            all_samples.append(gen_func())

    random.shuffle(all_samples)

    os.makedirs(DATASET_DIR, exist_ok=True)
    output_path = os.path.join(DATASET_DIR, "eval_dataset.csv")

    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "pet_hp_ratio", "pet_energy_ratio", "hit_prob", "defense_prob",
            "enemy_hp_ratio", "level_ratio", "dmg_efficiency", "threat_level",
            "quality_score", "label", "situation"
        ])
        for sample in all_samples:
            writer.writerow(sample["inputs"] + [sample["label"], sample["situation"]])

    counts = {"attack": 0, "defend": 0, "flee": 0}
    for sample in all_samples:
        counts[sample["situation"]] += 1

    print(f"Generated: {len(all_samples)} samples")
    print(f"  Attack: {counts['attack']}")
    print(f"  Defend: {counts['defend']}")
    print(f"  Flee:   {counts['flee']}")
    print(f"Saved to: {output_path}")


if __name__ == "__main__":
    main()
