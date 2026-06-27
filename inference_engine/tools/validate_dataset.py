import csv

with open("data/datasets/combat_dataset.csv") as f:
    rows = list(csv.DictReader(f))

ATACAR = "0"
DEFENDER = "1"
HUIR = "2"

print("=== Primeras 5 filas ===")
for r in rows[:5]:
    print(f"  pet(hp={r['pet_hp']} en={r['pet_energy']} atk={r['pet_attack']} def={r['pet_defense']}) | "
          f"enemy(hp={r['enemy_hp']} en={r['enemy_energy']} atk={r['enemy_attack']} def={r['enemy_defense']}) "
          f"-> action={r['action']}")

print()
print("=== HUIR con pet_hp > 0.5 ===")
samples = [r for r in rows if r["action"] == HUIR and float(r["pet_hp"]) > 0.5][:5]
for s in samples:
    print(f"  pet(hp={s['pet_hp']} en={s['pet_energy']} atk={s['pet_attack']} def={s['pet_defense']}) | "
          f"enemy(hp={s['enemy_hp']} atk={s['enemy_attack']} def={s['enemy_defense']}) -> HUIR")

print()
print("=== ATACAR con pet_hp < 0.3 ===")
samples = [r for r in rows if r["action"] == ATACAR and float(r["pet_hp"]) < 0.3][:5]
for s in samples:
    pa, ed = float(s["pet_attack"]), float(s["enemy_defense"])
    print(f"  pet(hp={s['pet_hp']} atk={s['pet_attack']} def={s['pet_defense']}) | "
          f"enemy(hp={s['enemy_hp']} atk={s['enemy_attack']} def={s['enemy_defense']}) | "
          f"atk_adv={pa - ed:.2f} -> ATACAR")

print()
print("=== DEFENDER con pet_hp < 0.4 ===")
samples = [r for r in rows if r["action"] == DEFENDER and float(r["pet_hp"]) < 0.4][:5]
for s in samples:
    pa, ed = float(s["pet_attack"]), float(s["enemy_defense"])
    ea, pd = float(s["enemy_attack"]), float(s["pet_defense"])
    print(f"  pet(hp={s['pet_hp']} atk={s['pet_attack']} def={s['pet_defense']}) | "
          f"enemy(hp={s['enemy_hp']} atk={s['enemy_attack']} def={s['enemy_defense']}) | "
          f"atk_adv={pa - ed:.2f} def_disadv={ea - pd:.2f} -> DEFENDER")

print()
total = len(rows)
counts = {ATACAR: 0, DEFENDER: 0, HUIR: 0}
for r in rows:
    counts[r["action"]] += 1
print(f"Total: {total}")
print(f"  ATACAR:  {counts[ATACAR]:5d} ({counts[ATACAR]/total*100:.1f}%)")
print(f"  DEFENDER:{counts[DEFENDER]:5d} ({counts[DEFENDER]/total*100:.1f}%)")
print(f"  HUIR:    {counts[HUIR]:5d} ({counts[HUIR]/total*100:.1f}%)")
