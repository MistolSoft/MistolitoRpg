import csv
import random
import os
import torch
import torch.nn as nn
import numpy as np
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(TOOLS_DIR)
SHARED_DIR = os.path.join(PARENT_DIR, "shared")
sys.path.insert(0, TOOLS_DIR)
sys.path.insert(0, SHARED_DIR)

from heuristics import (
    calc_hit_prob,
    calc_defense_prob,
    calc_avg_pet_damage,
    calc_avg_enemy_damage,
    decide_action
)
PROJECT_DIR = os.path.dirname(os.path.dirname(PARENT_DIR))
DATASET_DIR = os.path.join(PROJECT_DIR, "data", "datasets")
MODEL_DIR = os.path.join(PROJECT_DIR, "models")

CRITIC_FEATURE_COUNT = 8
CRITIC_HIDDEN_SIZE = 4
CRITIC_MODEL_PATH = os.path.join(MODEL_DIR, "critic.pth")

GAMMA = 0.95

REWARD_KILL_BONUS = 0.5
REWARD_DEATH_PENALTY = -1.0
REWARD_FLEE_SAFE = 0.2
REWARD_FLEE_PENALTY = -0.3
REWARD_HP_DAMAGE_SCALE = 0.3
REWARD_DEFEND_BONUS = 0.1
REWARD_ENEMY_ALMOST_DEAD = 0.15
REWARD_LOW_HP_COMEBACK = 0.1

HEADER = [
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8",
    "action", "reward",
    "ns0", "ns1", "ns2", "ns3", "ns4", "ns5", "ns6", "ns7", "ns8",
    "done", "ret"
]


class CriticNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(CRITIC_FEATURE_COUNT, CRITIC_HIDDEN_SIZE)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(CRITIC_HIDDEN_SIZE, 1)
        self.tanh = nn.Tanh()

    def forward(self, x):
        x = self.relu(self.fc1(x))
        x = self.tanh(self.fc2(x))
        return x.squeeze(-1)


def load_critic():
    if not os.path.exists(CRITIC_MODEL_PATH):
        print(f"WARNING: Critic not found at {CRITIC_MODEL_PATH}")
        print("Using default quality_score = 0.0")
        return None

    model = CriticNet()
    checkpoint = torch.load(CRITIC_MODEL_PATH, map_location="cpu")
    model.load_state_dict(checkpoint)
    model.eval()
    print(f"Critic loaded from {CRITIC_MODEL_PATH}")
    return model


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


def calc_critic_features(history):
    if not history:
        return [0.0] * CRITIC_FEATURE_COUNT

    sum_luck_hit = 0.0
    sum_luck_dmg = 0.0
    sum_hp_adv = 0.0
    sum_dmg_momentum = 0.0
    sum_success_rate = 0.0
    sum_threat = 0.0
    sum_turns = 0.0
    sum_win_prob = 0.0

    total_damage_dealt = sum(t["damage_dealt"] for t in history)
    total_damage_taken = sum(t["damage_taken"] for t in history)
    hits = sum(1 for t in history if t["hit"])
    total_turns = len(history)
    my_hp_max = history[0]["my_hp_max"]
    enemy_hp_max = history[0]["enemy_hp_max"]

    for t in history:
        sum_luck_hit += calc_luck_hit(t["hit"], t["p_success"])
        sum_luck_dmg += calc_luck_dmg(t["damage_dealt"], t["dmg_min"], t["dmg_max"])

        my_hp_ratio = t["my_hp"] / my_hp_max if my_hp_max > 0 else 0.0
        enemy_hp_ratio = t["enemy_hp"] / enemy_hp_max if enemy_hp_max > 0 else 0.0
        sum_hp_adv += my_hp_ratio - enemy_hp_ratio

        max_hp = my_hp_max if my_hp_max > 0 else 1.0
        sum_dmg_momentum += (total_damage_dealt - total_damage_taken) / max_hp

        sum_success_rate += hits / total_turns if total_turns > 0 else 0.5

        enemy_avg_dmg = total_damage_taken / total_turns if total_turns > 0 else 1.0
        sum_threat += enemy_avg_dmg / t["my_hp"] if t["my_hp"] > 0 else 1.0

        sum_turns += total_turns / 5.0

        total = my_hp_ratio + enemy_hp_ratio
        sum_win_prob += my_hp_ratio / total if total > 0 else 0.5

    n = float(total_turns)
    features = [
        max(-1.0, min(1.0, sum_luck_hit / n)),
        max(-1.0, min(1.0, sum_luck_dmg / n)),
        max(-1.0, min(1.0, sum_hp_adv / n)),
        max(-1.0, min(1.0, sum_dmg_momentum / n)),
        max(-1.0, min(1.0, sum_success_rate / n * 2 - 1)),
        max(-1.0, min(1.0, sum_threat / n * 2 - 1)),
        max(-1.0, min(1.0, sum_turns / n * 2 - 1)),
        max(-1.0, min(1.0, sum_win_prob / n * 2 - 1)),
    ]
    return features


def predict_quality(critic, features):
    if critic is None:
        return 0.0

    with torch.no_grad():
        x = torch.tensor(features, dtype=torch.float32).unsqueeze(0)
        score = critic(x).item()
    return score


def d20():
    return random.randint(1, 20)




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

    elif type_name == "tank_vs_tank":
        pet_hp_max = random.randint(40, 60)
        pet_hp = random.randint(int(pet_hp_max * 0.6), pet_hp_max)
        pet_dex_mod = random.randint(0, 2)
        pet_str_mod = random.randint(0, 2)
        pet_dice_count = 2
        pet_dice_size = 4
        enemy_hp_max = random.randint(40, 60)
        enemy_hp = random.randint(int(enemy_hp_max * 0.6), enemy_hp_max)
        enemy_attack = random.randint(2, 5)
        enemy_ac = random.randint(13, 16)
        enemy_damage_dice = random.choice([4])
        enemy_damage_bonus = random.randint(0, 2)
        
    elif type_name == "high_evasion":
        pet_hp_max = random.randint(30, 50)
        pet_hp = random.randint(int(pet_hp_max * 0.5), pet_hp_max)
        pet_dex_mod = random.randint(1, 3)
        pet_str_mod = random.randint(1, 3)
        pet_dice_count = 2
        pet_dice_size = 6
        enemy_hp_max = random.randint(15, 25)
        enemy_hp = enemy_hp_max
        enemy_attack = random.randint(3, 6)
        enemy_ac = random.randint(15, 18)
        enemy_damage_dice = 6
        enemy_damage_bonus = random.randint(1, 3)

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
        "pet_min_damage": 0,
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
    "tank_vs_tank",
    "high_evasion",
]


def calc_reward(action, pet_hp, pet_hp_max, pet_hp_before,
                enemy_hp, enemy_hp_max, enemy_hp_before,
                damage_dealt, damage_taken, enemy_killed, pet_died, fled):
    reward = 0.0
    damage_ratio = (damage_dealt - damage_taken) / pet_hp_max if pet_hp_max > 0 else 0
    reward += damage_ratio * REWARD_HP_DAMAGE_SCALE
    if enemy_killed:
        reward += REWARD_KILL_BONUS
    if pet_died:
        reward += REWARD_DEATH_PENALTY
    if fled:
        pet_hp_ratio = pet_hp_before / pet_hp_max if pet_hp_max > 0 else 0
        if pet_hp_ratio < 0.3:
            reward += REWARD_FLEE_SAFE
        elif pet_hp_ratio > 0.7:
            reward += REWARD_FLEE_PENALTY
    if action == 1 and not fled and not pet_died:
        pet_hp_ratio = pet_hp_before / pet_hp_max if pet_hp_max > 0 else 0
        # Reduce the defense bonus to avoid infinite defend hacking
        reward += (REWARD_DEFEND_BONUS * pet_hp_ratio * 0.5)
    enemy_hp_ratio_before = enemy_hp_before / enemy_hp_max if enemy_hp_max > 0 else 0
    enemy_hp_ratio_after = enemy_hp / enemy_hp_max if enemy_hp_max > 0 else 0
    if enemy_hp_ratio_before > 0.5 and enemy_hp_ratio_after < 0.25:
        reward += REWARD_ENEMY_ALMOST_DEAD
    pet_hp_ratio = pet_hp_before / pet_hp_max if pet_hp_max > 0 else 0
    if pet_hp_ratio < 0.3 and damage_dealt > damage_taken:
        reward += REWARD_LOW_HP_COMEBACK
    return round(reward, 4)


def make_state(pet_hp, pet_hp_max, pet_energy, pet_energy_max,
               pet_dex_mod, pet_ac, pet_str_mod, pet_min_damage,
               pet_dice_count, pet_dice_size,
               enemy_hp, enemy_hp_max, enemy_attack, enemy_ac,
               enemy_damage_dice, enemy_damage_bonus,
               enemy_level, pet_level,
               quality_score):
    hit_prob = calc_hit_prob(enemy_ac, pet_dex_mod)
    defense_prob = calc_defense_prob(pet_ac, enemy_attack)
    enemy_hp_ratio = enemy_hp / pet_hp if pet_hp > 0 else 1.0
    level_ratio = float(enemy_level) / float(pet_level) if pet_level > 0 else 1.0
    avg_pet_dmg = calc_avg_pet_damage(pet_dice_count, pet_dice_size, pet_str_mod, pet_min_damage)
    avg_enemy_dmg = calc_avg_enemy_damage(enemy_damage_dice, enemy_damage_bonus)
    dmg_efficiency = avg_pet_dmg / enemy_hp if enemy_hp > 0 else 1.0
    threat_level = avg_enemy_dmg / pet_hp if pet_hp > 0 else 1.0

    return [
        pet_hp / pet_hp_max,
        pet_energy / pet_energy_max,
        hit_prob,
        defense_prob,
        enemy_hp_ratio,
        level_ratio,
        dmg_efficiency,
        threat_level,
        quality_score,
    ]


def simulate_turn(state, action, pet_hp, pet_hp_max, pet_attack_bonus,
                  pet_ac, enemy_hp, enemy_hp_max, enemy_attack, enemy_ac,
                  quality_score):
    pet_hp_before = pet_hp
    enemy_hp_before = enemy_hp
    damage_dealt = 0
    damage_taken = 0
    enemy_killed = False
    pet_died = False
    fled = False

    if action == 0:
        pet_roll = d20() + pet_attack_bonus
        if pet_roll >= enemy_ac:
            damage_dealt = max(1, d20() + (pet_attack_bonus + 2) - 8)
            enemy_hp = max(0, enemy_hp - damage_dealt)
            enemy_killed = enemy_hp <= 0
        enemy_roll = d20() + enemy_attack
        if enemy_roll >= pet_ac:
            damage_taken = max(1, d20() + enemy_attack - 8)
            pet_hp = max(0, pet_hp - damage_taken)
            pet_died = pet_hp <= 0

    elif action == 1:
        enemy_roll = d20() + enemy_attack
        if enemy_roll >= pet_ac - 3:
            raw = max(1, d20() + enemy_attack - 8)
            damage_taken = max(0, raw - 3)
            pet_hp = max(0, pet_hp - damage_taken)
            pet_died = pet_hp <= 0

    else:
        flee_roll = d20() + pet_attack_bonus
        if flee_roll >= 12:
            fled = True
        else:
            enemy_roll = d20() + enemy_attack
            if enemy_roll >= pet_ac:
                damage_taken = max(1, d20() + enemy_attack - 8)
                pet_hp = max(0, pet_hp - damage_taken)
                pet_died = pet_hp <= 0

    reward = calc_reward(action, pet_hp, pet_hp_max, pet_hp_before,
                       enemy_hp, enemy_hp_max, enemy_hp_before,
                       damage_dealt, damage_taken, enemy_killed, pet_died, fled)

    done = pet_died or enemy_killed or fled

    return pet_hp, enemy_hp, damage_dealt, damage_taken, reward, done


def simulate_episode(critic):
    scenario_type = random.choice(SCENARIOS)
    s = gen_scenario(scenario_type)

    pet_hp = s["pet_hp"]
    pet_hp_max = s["pet_hp_max"]
    pet_dex_mod = s["pet_dex_mod"]
    pet_ac = s["pet_ac"]
    pet_str_mod = s["pet_str_mod"]
    pet_min_damage = s["pet_min_damage"]
    pet_dice_count = s["pet_dice_count"]
    pet_dice_size = s["pet_dice_size"]
    pet_attack_bonus = pet_dex_mod
    pet_level = random.randint(1, 10)
    pet_energy = random.randint(20, 50)
    pet_energy_max = 50

    enemy_hp = s["enemy_hp"]
    enemy_hp_max = s["enemy_hp_max"]
    enemy_attack = s["enemy_attack"]
    enemy_ac = s["enemy_ac"]
    enemy_damage_dice = s["enemy_damage_dice"]
    enemy_damage_bonus = s["enemy_damage_bonus"]
    if random.random() < 0.25:
        enemy_level = random.randint(pet_level + 3, pet_level + 8)
    else:
        enemy_level = random.randint(max(1, pet_level - 2), pet_level + 2)

    if enemy_level > pet_level:
        level_diff = enemy_level - pet_level
        enemy_hp_max = int(enemy_hp_max * (1.0 + 0.25 * level_diff))
        enemy_hp = int(enemy_hp * (1.0 + 0.25 * level_diff))
        enemy_attack += int(0.8 * level_diff)
        enemy_ac += int(0.5 * level_diff)
        enemy_damage_bonus += int(0.5 * level_diff)

    hit_prob = calc_hit_prob(enemy_ac, pet_dex_mod)
    defense_prob = calc_defense_prob(pet_ac, enemy_attack)
    avg_pet_dmg = calc_avg_pet_damage(pet_dice_count, pet_dice_size, pet_str_mod, pet_min_damage)
    avg_enemy_dmg = calc_avg_enemy_damage(enemy_damage_dice, enemy_damage_bonus)

    rows = []
    combat_history = []
    quality_score = 0.0
    last_action = 0
    
    profile = random.choice(["balanced", "aggressive", "cautious"])
    epsilon = 0.15

    for turn in range(random.randint(2, 8)):
        if pet_hp <= 0 or enemy_hp <= 0:
            break

        my_hp_ratio = pet_hp / pet_hp_max if pet_hp_max > 0 else 0.0
        enemy_hp_ratio_val = enemy_hp / enemy_hp_max if enemy_hp_max > 0 else 0.0
        dmg_eff = avg_pet_dmg / enemy_hp if enemy_hp > 0 else 1.0
        threat = avg_enemy_dmg / pet_hp if pet_hp > 0 else 1.0

        if random.random() < epsilon:
            action = random.choice([0, 1, 2])
        else:
            action = decide_action(hit_prob, defense_prob, my_hp_ratio, enemy_hp_ratio_val, threat, dmg_eff, quality_score, last_action, profile=profile)

        state = make_state(pet_hp, pet_hp_max, pet_energy, pet_energy_max,
                           pet_dex_mod, pet_ac, pet_str_mod, pet_min_damage,
                           pet_dice_count, pet_dice_size,
                           enemy_hp, enemy_hp_max, enemy_attack, enemy_ac,
                           enemy_damage_dice, enemy_damage_bonus,
                           enemy_level, pet_level, quality_score)

        p_success = hit_prob if action == 0 else 0.5

        pet_hp2, enemy_hp2, dmg_d, dmg_t, reward, done = simulate_turn(
            state, action, pet_hp, pet_hp_max, pet_attack_bonus,
            pet_ac, enemy_hp, enemy_hp_max, enemy_attack, enemy_ac,
            quality_score)

        hit = dmg_d > 0

        combat_history.append({
            "hit": hit,
            "p_success": p_success,
            "damage_dealt": dmg_d,
            "damage_taken": dmg_t,
            "dmg_min": 1.0,
            "dmg_max": float(pet_dice_count * pet_dice_size + pet_str_mod),
            "my_hp": pet_hp2,
            "enemy_hp": enemy_hp2,
            "my_hp_max": pet_hp_max,
            "enemy_hp_max": enemy_hp_max,
        })

        features = calc_critic_features(combat_history)
        quality_score = predict_quality(critic, features)

        next_state = make_state(pet_hp2, pet_hp_max, pet_energy, pet_energy_max,
                                pet_dex_mod, pet_ac, pet_str_mod, pet_min_damage,
                                pet_dice_count, pet_dice_size,
                                enemy_hp2, enemy_hp_max, enemy_attack, enemy_ac,
                                enemy_damage_dice, enemy_damage_bonus,
                                enemy_level, pet_level, quality_score)

        rows.append({
            "state": state,
            "action": action,
            "reward": reward,
            "next_state": next_state,
            "done": done,
        })

        last_action = action
        pet_hp = pet_hp2
        enemy_hp = enemy_hp2
        pet_energy = max(0, pet_energy - random.randint(3, 8))

        if done:
            break

    G = 0.0
    for row in reversed(rows):
        G = row["reward"] + GAMMA * G * (1.0 - row["done"])
        row["return"] = round(G, 4)

    return rows


def main():
    import sys
    n = 10000
    if len(sys.argv) > 1:
        n = int(sys.argv[1])

    critic = load_critic()

    per_scenario = n // len(SCENARIOS)
    all_rows = []

    for scenario in SCENARIOS:
        for _ in range(per_scenario):
            all_rows.extend(simulate_episode(critic))

    random.shuffle(all_rows)

    os.makedirs(DATASET_DIR, exist_ok=True)
    output_path = os.path.join(DATASET_DIR, "combat_dataset.csv")

    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(HEADER)
        for row in all_rows:
            writer.writerow(
                [round(v, 4) for v in row["state"]]
                + [row["action"], row["reward"]]
                + [round(v, 4) for v in row["next_state"]]
                + [row["done"], row["return"]]
            )

    actions = {0: 0, 1: 0, 2: 0}
    for row in all_rows:
        actions[row["action"]] += 1
    total = len(all_rows)
    rewards = [r["reward"] for r in all_rows]
    returns = [r["return"] for r in all_rows]

    print(f"\nEpisodes: {n}")
    print(f"Total steps: {total}")
    print(f"  ATACAR:   {actions[0]:5d}  ({actions[0]/total*100:.1f}%)")
    print(f"  DEFENDER: {actions[1]:5d}  ({actions[1]/total*100:.1f}%)")
    print(f"  HUIR:     {actions[2]:5d}  ({actions[2]/total*100:.1f}%)")
    print(f"Reward: min={min(rewards):.3f} max={max(rewards):.3f} avg={sum(rewards)/len(rewards):.3f}")
    print(f"Return: min={min(returns):.3f} max={max(returns):.3f} avg={sum(returns)/len(returns):.3f}")
    print(f"Saved to: {output_path}")


if __name__ == "__main__":
    main()
