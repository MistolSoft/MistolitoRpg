import numpy as np
import pandas as pd

def generate_rest_sample():
    hp = np.random.uniform(0.0, 0.5)
    energy = np.random.uniform(0.0, 0.3)
    exploration = np.random.uniform(0.0, 1.0)
    density = np.random.uniform(0.0, 1.0)
    difficulty = np.random.uniform(-1.0, 1.0)
    momentum = np.random.uniform(-1.0, 1.0)
    return [hp, energy, exploration, density, difficulty, momentum, 2]

def generate_fight_sample():
    hp = np.random.uniform(0.6, 1.0)
    energy = np.random.uniform(0.4, 1.0)
    exploration = np.random.uniform(0.0, 1.0)
    density = np.random.uniform(0.3, 1.0)
    difficulty = np.random.uniform(-0.3, 1.0)
    momentum = np.random.uniform(-1.0, 0.2)
    return [hp, energy, exploration, density, difficulty, momentum, 3]

def generate_move_sample():
    hp = np.random.uniform(0.5, 1.0)
    energy = np.random.uniform(0.3, 1.0)
    exploration = np.random.uniform(0.0, 1.0)
    
    choice = np.random.choice([0, 1])
    if choice == 0:
        density = np.random.uniform(0.0, 1.0)
        difficulty = np.random.uniform(-1.0, 1.0)
        momentum = np.random.uniform(0.3, 1.0)
    else:
        density = np.random.uniform(0.0, 0.1)
        difficulty = np.random.uniform(-1.0, 1.0)
        momentum = np.random.uniform(-1.0, 0.0)
        exploration = np.random.uniform(0.8, 1.0)
        
    return [hp, energy, exploration, density, difficulty, momentum, 1]

def generate_scan_sample():
    hp = np.random.uniform(0.5, 1.0)
    energy = np.random.uniform(0.3, 1.0)
    exploration = np.random.uniform(0.0, 0.6)
    density = np.random.uniform(0.0, 0.1)
    difficulty = np.random.uniform(-1.0, 1.0)
    momentum = np.random.uniform(-1.0, 0.1)
    return [hp, energy, exploration, density, difficulty, momentum, 0]

def main():
    import os
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    np.random.seed(42)
    samples_per_class = 4000
    
    data = []
    for _ in range(samples_per_class):
        data.append(generate_scan_sample())
        data.append(generate_move_sample())
        data.append(generate_rest_sample())
        data.append(generate_fight_sample())
        
    np.random.shuffle(data)
    
    df = pd.DataFrame(data, columns=[
        "hp_ratio", "energy_ratio", "exploration_ratio",
        "current_tile_density", "zone_difficulty_score", "movement_momentum",
        "action"
    ])
    
    output_path = os.path.join(project_dir, "search_dataset.csv")
    df.to_csv(output_path, index=False)

    total = len(df)
    action_counts = df["action"].value_counts().to_dict()
    action_names = {0: "SCAN", 1: "MOVE", 2: "REST", 3: "FIGHT"}

    print(f"\nGenerated samples: {total}")
    for act_id, name in action_names.items():
        count = action_counts.get(act_id, 0)
        pct = (count / total) * 100
        print(f"  {name:<6}: {count:5d} ({pct:.1f}%)")
    print(f"Saved to: {output_path}\n")

if __name__ == "__main__":
    main()


