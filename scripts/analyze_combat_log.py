import sys
import os
import re

def analyze_log(filepath):
    if not os.path.exists(filepath):
        print(f"Error: {filepath} not found.")
        return

    try:
        with open(filepath, "r", encoding="utf-16") as f:
            lines = f.read().splitlines()
    except Exception:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

    current_combat = None
    combats = []

    for line in lines:
        if "STORAGE: Enemy" in line and "loaded:" in line:
            if current_combat:
                combats.append(current_combat)
            m = re.search(r"Enemy \d+ loaded: (\w+) HP=(\d+) AC=(\d+) dmg=.* exp=(\d+)", line)
            if m:
                current_combat = {
                    "enemy_name": m.group(1),
                    "enemy_hp": int(m.group(2)),
                    "enemy_ac": int(m.group(3)),
                    "enemy_exp": int(m.group(4)),
                    "turns": []
                }
        elif "COMBAT: Initiative:" in line:
            if current_combat:
                m = re.search(r"Initiative: Pet=(-?\d+) .* vs Enemy=(\d+) .*", line)
                if m:
                    current_combat["pet_init"] = int(m.group(1))
                    current_combat["enemy_init"] = int(m.group(2))
        elif "NN Inputs: [" in line:
            if current_combat:
                m = re.search(r"NN Inputs: \[(.*)\]", line)
                if m:
                    current_combat["turns"].append({
                        "inputs": [float(x.strip()) for x in m.group(1).split(",")],
                        "scores": {}
                    })
        elif "Backbone Features: [" in line:
            if current_combat and current_combat["turns"]:
                m = re.search(r"Backbone Features: \[(.*)\]", line)
                if m:
                    current_combat["turns"][-1]["features"] = [float(x.strip()) for x in m.group(1).split(",")]
        elif "Inference run: best_idx=" in line:
            if current_combat and current_combat["turns"]:
                m = re.search(r"best_idx=(\d+) scores=\[ATK=(.*), DEF=(.*), FLEE=(.*), HEAL=(.*), MAG=(.*), SUP=(.*)\]", line)
                if m:
                    current_combat["turns"][-1]["best_idx"] = int(m.group(1))
                    current_combat["turns"][-1]["scores"] = {
                        "ATK": float(m.group(2)),
                        "DEF": float(m.group(3)),
                        "FLEE": float(m.group(4)),
                        "HEAL": float(m.group(5)),
                        "MAG": float(m.group(6)),
                        "SUP": float(m.group(7))
                    }
        elif "REWARD:   Action:" in line:
            if current_combat and current_combat["turns"]:
                m = re.search(r"Action: (\w+) \| Turn reward: (-?\d+\.\d+)", line)
                if m:
                    current_combat["turns"][-1]["action"] = m.group(1)
                    current_combat["turns"][-1]["reward"] = float(m.group(2))
        elif "REWARD:   Damage:" in line:
            if current_combat and current_combat["turns"]:
                m = re.search(r"Damage: dealt=(\d+) taken=(\d+)", line)
                if m:
                    current_combat["turns"][-1]["dealt"] = int(m.group(1))
                    current_combat["turns"][-1]["taken"] = int(m.group(2))
        elif "REWARD:   Pet HP:" in line:
            if current_combat and current_combat["turns"]:
                m = re.search(r"Pet HP: (\d+) -> (\d+) \(", line)
                if m:
                    current_combat["turns"][-1]["pet_hp_before"] = int(m.group(1))
                    current_combat["turns"][-1]["pet_hp_after"] = int(m.group(2))
        elif "REWARD:   Enemy HP:" in line:
            if current_combat and current_combat["turns"]:
                m = re.search(r"Enemy HP: (\d+) -> (\d+) \(", line)
                if m:
                    current_combat["turns"][-1]["enemy_hp_before"] = int(m.group(1))
                    current_combat["turns"][-1]["enemy_hp_after"] = int(m.group(2))
        elif "COMBAT SUMMARY" in line:
            if current_combat:
                m = re.search(r"Enemy: (\w+) HP \d+->\d+ \((\w+)\) \| Pet: HP \d+->(\d+)", line)
                if m:
                    current_combat["outcome"] = m.group(2)
                    current_combat["pet_hp_final"] = int(m.group(3))
        elif "COORD: Turns:" in line:
            if current_combat:
                m = re.search(r"Turns: (\d+) \| ATK:(\d+) DEF:(\d+) FLEE:(\d+)", line)
                if m:
                    current_combat["total_turns"] = int(m.group(1))
                    current_combat["action_counts"] = {
                        "ATK": int(m.group(2)),
                        "DEF": int(m.group(3)),
                        "FLEE": int(m.group(4))
                    }

    if current_combat:
        combats.append(current_combat)

    print(f"=== ANALYZING COMBAT LOGS ({len(combats)} encounters found) ===\n")
    for idx, c in enumerate(combats, 1):
        print(f"Encounter {idx}: vs {c.get('enemy_name', 'UNKNOWN')} (HP: {c.get('enemy_hp', 'UNKNOWN')}, AC: {c.get('enemy_ac', 'UNKNOWN')})")
        if "pet_init" in c:
            print(f"  Initiative: Pet {c['pet_init']} vs Enemy {c['enemy_init']}")
        print(f"  Outcome: {c.get('outcome', 'UNKNOWN')} | Final Pet HP: {c.get('pet_hp_final', 'UNKNOWN')}")
        print(f"  Total Turns: {c.get('total_turns', len(c['turns']))}")
        if "action_counts" in c:
            print(f"  Actions taken: {c['action_counts']}")
        
        print("  Turn Details:")
        for t_idx, t in enumerate(c["turns"], 1):
            act = t.get("action", "UNKNOWN")
            scores_str = ", ".join(f"{k}: {v:.3f}" for k, v in t["scores"].items())
            inputs_str = ", ".join(f"{x:.3f}" for x in t.get("inputs", []))
            features_str = ", ".join(f"{x:.3f}" for x in t.get("features", []))
            dealt = t.get("dealt", 0)
            taken = t.get("taken", 0)
            p_hp_b = t.get("pet_hp_before", "?")
            p_hp_a = t.get("pet_hp_after", "?")
            e_hp_b = t.get("enemy_hp_before", "?")
            e_hp_a = t.get("enemy_hp_after", "?")
            print(f"    Turn {t_idx:2d}: NN Inputs: [{inputs_str}]")
            print(f"             Backbone Features: [{features_str}]")
            print(f"             NN Output Index: {t.get('best_idx', '?')} | scores: [{scores_str}]")
            print(f"             Chosen: {act} | Damage: dealt={dealt} taken={taken} | Pet HP: {p_hp_b}->{p_hp_a} | Enemy HP: {e_hp_b}->{e_hp_a}")
        print("-" * 80)

if __name__ == "__main__":
    path = "firmware/combat_log.txt"
    if len(sys.argv) > 1:
        path = sys.argv[1]
    analyze_log(path)
