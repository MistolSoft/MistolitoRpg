import csv
import random

HEADER = [
    "pet_hp", "pet_energy", "pet_attack", "pet_defense",
    "enemy_hp", "enemy_energy", "enemy_attack", "enemy_defense",
]

def generate_row():
    return [
        round(random.randint(0, 100) / 100.0, 4),
        round(random.randint(0, 100) / 100.0, 4),
        round(random.randint(1, 20) / 20.0, 4),
        round(random.randint(1, 20) / 20.0, 4),
        round(random.randint(0, 100) / 100.0, 4),
        round(random.randint(0, 100) / 100.0, 4),
        round(random.randint(1, 20) / 20.0, 4),
        round(random.randint(1, 20) / 20.0, 4),
    ]

def generate_dataset(num_rows):
    rows = []
    for _ in range(num_rows):
        rows.append(generate_row())
    return rows

def write_csv(filename, rows):
    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(HEADER)
        writer.writerows(rows)

if __name__ == "__main__":
    import sys
    n = 1000
    if len(sys.argv) > 1:
        n = int(sys.argv[1])
    out = "data/datasets/combat_validation_inputs.csv"
    rows = generate_dataset(n)
    write_csv(out, rows)
    print(f"Generadas {n} filas de validación (solo inputs)")
    print(f"Guardado en {out}")
