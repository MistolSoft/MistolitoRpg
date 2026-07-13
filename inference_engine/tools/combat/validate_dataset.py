import csv
import os

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(TOOLS_DIR)
DATASET_PATH = os.path.join(PROJECT_DIR, "data", "datasets", "combat_dataset.csv")

with open(DATASET_PATH) as f:
    rows = list(csv.DictReader(f))

ATACAR = "0"
DEFENDER = "1"
HUIR = "2"

print("=== Primeras 5 filas ===")
for r in rows[:5]:
    print(f"  pet(hp_ratio={r['s0']} en_ratio={r['s1']} hit_prob={r['s2']} def_prob={r['s3']}) | "
          f"enemy(hp_ratio={r['s4']} threat={r['s7']}) "
          f"-> action={r['action']}")

print()
print("=== HUIR con pet_hp > 0.5 ===")
samples = [r for r in rows if r["action"] == HUIR and float(r["s0"]) > 0.5][:5]
for s in samples:
    print(f"  pet(hp_ratio={s['s0']} en_ratio={s['s1']} hit_prob={s['s2']} def_prob={s['s3']}) | "
          f"enemy(hp_ratio={s['s4']} threat={s['s7']}) -> HUIR")

print()
print("=== ATACAR con pet_hp < 0.3 ===")
samples = [r for r in rows if r["action"] == ATACAR and float(r["s0"]) < 0.3][:5]
for s in samples:
    print(f"  pet(hp_ratio={s['s0']} hit_prob={s['s2']}) | "
          f"enemy(hp_ratio={s['s4']} threat={s['s7']}) | "
          f"dmg_eff={s['s6']} -> ATACAR")

print()
print("=== DEFENDER con pet_hp < 0.4 ===")
samples = [r for r in rows if r["action"] == DEFENDER and float(r["s0"]) < 0.4][:5]
for s in samples:
    print(f"  pet(hp_ratio={s['s0']} hit_prob={s['s2']} def_prob={s['s3']}) | "
          f"enemy(hp_ratio={s['s4']} threat={s['s7']}) -> DEFENDER")

print()
total = len(rows)
counts = {ATACAR: 0, DEFENDER: 0, HUIR: 0}
for r in rows:
    counts[r["action"]] += 1
print(f"Total: {total}")
print(f"  ATACAR:  {counts[ATACAR]:5d} ({counts[ATACAR]/total*100:.1f}%)")
print(f"  DEFENDER:{counts[DEFENDER]:5d} ({counts[DEFENDER]/total*100:.1f}%)")
print(f"  HUIR:    {counts[HUIR]:5d} ({counts[HUIR]/total*100:.1f}%)")
