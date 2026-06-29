import os
import sys
import random
import torch
import torch.nn as nn

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.append(TOOLS_DIR)

from heuristics import decide_action
from generate_dataset import SCENARIOS, gen_scenario, make_state, simulate_turn, calc_hit_prob, calc_defense_prob, calc_avg_pet_damage, calc_avg_enemy_damage
from train_split_model import CombatModelSplit

PROJECT_DIR = os.path.dirname(TOOLS_DIR)
MODEL_PATH = os.path.join(PROJECT_DIR, "models", "combat_ai_split.pth")

class Agent:
    def __init__(self, name):
        self.name = name

    def get_action(self, state_dict):
        raise NotImplementedError

class RandomAgent(Agent):
    def get_action(self, state_dict):
        return random.choice([0, 1, 2])

class HeuristicAgent(Agent):
    def __init__(self, name, profile):
        super().__init__(name)
        self.profile = profile

    def get_action(self, state_dict):
        return decide_action(
            state_dict["hit_prob"], state_dict["defense_prob"],
            state_dict["my_hp_ratio"], state_dict["enemy_hp_ratio"],
            state_dict["threat"], state_dict["dmg_eff"],
            state_dict["quality_score"], state_dict["last_action"],
            profile=self.profile
        )

class ModelAgent(Agent):
    def __init__(self, name, model_path):
        super().__init__(name)
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.model = CombatModelSplit(num_actions=6).to(self.device)
        if os.path.exists(model_path):
            self.model.load_state_dict(torch.load(model_path, map_location=self.device))
            self.model.eval()
            print(f"[{name}] Model loaded successfully from {model_path}.")
        else:
            print(f"[{name}] WARNING: Model not found at {model_path}. Using uninitialized weights.")

    def get_action(self, state_dict):
        s = make_state(
            state_dict["pet_hp"], state_dict["pet_hp_max"], state_dict["pet_energy"], state_dict["pet_energy_max"],
            state_dict["pet_dex_mod"], state_dict["pet_ac"], state_dict["pet_str_mod"], state_dict["pet_min_damage"],
            state_dict["pet_dice_count"], state_dict["pet_dice_size"],
            state_dict["enemy_hp"], state_dict["enemy_hp_max"], state_dict["enemy_attack"], state_dict["enemy_ac"],
            state_dict["enemy_damage_dice"], state_dict["enemy_damage_bonus"],
            state_dict["enemy_level"], state_dict["pet_level"], state_dict["quality_score"]
        )
        x = torch.tensor(s, dtype=torch.float32).unsqueeze(0).to(self.device)
        with torch.no_grad():
            logits, _ = self.model(x)
            # We only have 3 actions currently valid (0, 1, 2). The model has 6 outputs.
            # Mask the invalid actions (3, 4, 5) with -inf
            logits[0, 3:] = -float('inf')
            action = logits.argmax(dim=1).item()
        return action

def run_benchmark(agent, num_episodes=1000):
    wins = 0
    survivals = 0
    flees = 0
    total_turns = 0
    total_hp_remaining = 0.0

    for _ in range(num_episodes):
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
        enemy_level = random.randint(max(1, pet_level - 2), pet_level + 2)

        quality_score = 0.0
        last_action = 0
        turns = 0

        while pet_hp > 0 and enemy_hp > 0:
            turns += 1
            if turns > 20: # Timeout
                break

            hit_prob = calc_hit_prob(enemy_ac, pet_dex_mod)
            defense_prob = calc_defense_prob(pet_ac, enemy_attack)
            avg_pet_dmg = calc_avg_pet_damage(pet_dice_count, pet_dice_size, pet_str_mod, pet_min_damage)
            avg_enemy_dmg = calc_avg_enemy_damage(enemy_damage_dice, enemy_damage_bonus)
            
            my_hp_ratio = pet_hp / pet_hp_max if pet_hp_max > 0 else 0.0
            enemy_hp_ratio_val = enemy_hp / enemy_hp_max if enemy_hp_max > 0 else 0.0
            dmg_eff = avg_pet_dmg / enemy_hp if enemy_hp > 0 else 1.0
            threat = avg_enemy_dmg / pet_hp if pet_hp > 0 else 1.0

            state_dict = {
                "pet_hp": pet_hp, "pet_hp_max": pet_hp_max, "pet_energy": pet_energy, "pet_energy_max": pet_energy_max,
                "pet_dex_mod": pet_dex_mod, "pet_ac": pet_ac, "pet_str_mod": pet_str_mod, "pet_min_damage": pet_min_damage,
                "pet_dice_count": pet_dice_count, "pet_dice_size": pet_dice_size,
                "enemy_hp": enemy_hp, "enemy_hp_max": enemy_hp_max, "enemy_attack": enemy_attack, "enemy_ac": enemy_ac,
                "enemy_damage_dice": enemy_damage_dice, "enemy_damage_bonus": enemy_damage_bonus,
                "enemy_level": enemy_level, "pet_level": pet_level, "quality_score": quality_score,
                "hit_prob": hit_prob, "defense_prob": defense_prob, "my_hp_ratio": my_hp_ratio,
                "enemy_hp_ratio": enemy_hp_ratio_val, "threat": threat, "dmg_eff": dmg_eff, "last_action": last_action
            }

            action = agent.get_action(state_dict)
            
            state_vector = make_state(
                pet_hp, pet_hp_max, pet_energy, pet_energy_max,
                pet_dex_mod, pet_ac, pet_str_mod, pet_min_damage,
                pet_dice_count, pet_dice_size,
                enemy_hp, enemy_hp_max, enemy_attack, enemy_ac,
                enemy_damage_dice, enemy_damage_bonus,
                enemy_level, pet_level, quality_score
            )
            
            pet_hp2, enemy_hp2, dmg_d, dmg_t, reward, done = simulate_turn(
                state_vector, action, pet_hp, pet_hp_max, pet_attack_bonus,
                pet_ac, enemy_hp, enemy_hp_max, enemy_attack, enemy_ac,
                quality_score)

            last_action = action
            pet_hp = pet_hp2
            enemy_hp = enemy_hp2
            pet_energy = max(0, pet_energy - random.randint(3, 8))
            
            if done:
                # Fled
                if action == 2 and pet_hp > 0 and enemy_hp > 0:
                    flees += 1
                break

        total_turns += turns
        if pet_hp > 0:
            survivals += 1
            total_hp_remaining += (pet_hp / pet_hp_max)
        if enemy_hp <= 0 and pet_hp > 0:
            wins += 1

    return {
        "win_rate": (wins / num_episodes) * 100,
        "survival_rate": (survivals / num_episodes) * 100,
        "flee_rate": (flees / num_episodes) * 100,
        "avg_turns": total_turns / num_episodes,
        "avg_hp_rem": (total_hp_remaining / survivals * 100) if survivals > 0 else 0.0
    }

def main():
    print("=" * 60)
    print("MISTOLITO RPG - COMBAT BENCHMARK")
    print("=" * 60)
    
    agents = [
        ModelAgent("Neural Network AI", MODEL_PATH),
        HeuristicAgent("Heuristic (Balanced)", "balanced"),
        HeuristicAgent("Heuristic (Aggressive)", "aggressive"),
        HeuristicAgent("Heuristic (Cautious)", "cautious"),
        RandomAgent("Random Action Base")
    ]
    
    num_episodes = 2000
    print(f"\nRunning {num_episodes} simulated combat episodes per agent...\n")
    
    print(f"{'AGENT NAME':<25} | {'WIN %':<7} | {'SURVIVAL %':<10} | {'FLEE %':<7} | {'AVG TURNS':<9} | {'AVG HP% REM'}")
    print("-" * 85)
    
    for agent in agents:
        metrics = run_benchmark(agent, num_episodes)
        print(f"{agent.name:<25} | {metrics['win_rate']:>6.2f}% | {metrics['survival_rate']:>9.2f}% | {metrics['flee_rate']:>6.2f}% | {metrics['avg_turns']:>9.2f} | {metrics['avg_hp_rem']:>6.2f}%")

if __name__ == "__main__":
    main()
