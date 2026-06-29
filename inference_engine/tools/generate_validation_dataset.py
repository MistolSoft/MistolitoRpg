import csv
import random
import os

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
DATASET_DIR = os.path.join(PROJECT_DIR, "data", "datasets")

HEADER = ["s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8"]

def generate_row():
    return [
        round(random.uniform(0.0, 1.0), 4),
        round(random.uniform(0.0, 1.0), 4),
        round(random.uniform(0.0, 1.0), 4),
        round(random.uniform(0.0, 1.0), 4),
        round(random.uniform(0.0, 4.0), 4),
        round(random.uniform(0.5, 2.0), 4),
        round(random.uniform(0.0, 2.0), 4),
        round(random.uniform(0.0, 2.0), 4),
        round(random.uniform(-1.0, 1.0), 4),
    ]

def generate_dataset(num_rows):
    rows = []
    for _ in range(num_rows):
        rows.append(generate_row())
    return rows

def write_csv(filename, rows):
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(HEADER)
        writer.writerows(rows)

if __name__ == "__main__":
    import sys
    n = 1000
    if len(sys.argv) > 1:
        n = int(sys.argv[1])
    out = os.path.join(DATASET_DIR, "combat_validation_inputs.csv")
    rows = generate_dataset(n)
    write_csv(out, rows)
    print(f"Generadas {n} filas de validación (s0-s8)")
    print(f"Guardado en {out}")
