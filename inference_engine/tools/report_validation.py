import csv
import random

from tools.generate_dataset import decide_action

INPUT_FILE = "data/datasets/combat_validation_onnx_results.csv"
ACTIONS = ["ATACAR", "DEFENDER", "HUIR"]

print("Cargando resultados...")
with open(INPUT_FILE) as f:
    rows = list(csv.DictReader(f))

correct = 0
total = len(rows)
conf_matrix = {a: {b: 0 for b in ACTIONS} for a in ACTIONS}
disagreements = []

for r in rows:
    pet_hp = int(round(float(r["pet_hp"]) * 100))
    pet_en = int(round(float(r["pet_energy"]) * 100))
    pet_atk = int(round(float(r["pet_attack"]) * 20))
    pet_def = int(round(float(r["pet_defense"]) * 20))
    en_hp = int(round(float(r["enemy_hp"]) * 100))
    en_en = int(round(float(r["enemy_energy"]) * 100))
    en_atk = int(round(float(r["enemy_attack"]) * 20))
    en_def = int(round(float(r["enemy_defense"]) * 20))

    expected = decide_action(pet_hp, pet_en, pet_atk, pet_def, en_hp, en_en, en_atk, en_def)
    predicted = int(r["pred_action"])

    conf_matrix[ACTIONS[expected]][ACTIONS[predicted]] += 1

    if expected == predicted:
        correct += 1
    elif len(disagreements) < 10:
        disagreements.append({
            "expected": ACTIONS[expected],
            "predicted": ACTIONS[predicted],
            "confidence": r["confidence"],
            "pet_hp": r["pet_hp"],
            "pet_en": r["pet_energy"],
            "pet_atk": r["pet_attack"],
            "pet_def": r["pet_defense"],
            "en_hp": r["enemy_hp"],
            "en_atk": r["enemy_attack"],
            "en_def": r["enemy_defense"],
            "atk_adv": r["atk_adv"],
            "def_disadv": r["def_disadv"],
        })

print(f"Total muestras: {total}")
print(f"Aciertos:       {correct} ({correct/total*100:.1f}%)")
print(f"Desaciertos:    {total - correct} ({(total-correct)/total*100:.1f}%)")
print()

print("=== Matriz de confusion ===")
print(f"{'':>12}", end="")
for a in ACTIONS:
    print(f"{a:>10}", end="")
print()
for expected in ACTIONS:
    print(f"{expected:>12}", end="")
    for predicted in ACTIONS:
        v = conf_matrix[expected][predicted]
        print(f"{v:>10}", end="")
    print()
print()

print("=== Exactitud por accion esperada ===")
for a in ACTIONS:
    tot = sum(conf_matrix[a].values())
    ok = conf_matrix[a][a]
    print(f"  {a:>10}: {ok:4d}/{tot:4d} ({ok/tot*100:.1f}%%)")
print()

print("=== Confianza promedio por acierto/desacierto ===")
conf_correct = []
conf_wrong = []
for r in rows:
    pet_hp = int(round(float(r["pet_hp"]) * 100))
    pet_en = int(round(float(r["pet_energy"]) * 100))
    pet_atk = int(round(float(r["pet_attack"]) * 20))
    pet_def = int(round(float(r["pet_defense"]) * 20))
    en_hp = int(round(float(r["enemy_hp"]) * 100))
    en_en = int(round(float(r["enemy_energy"]) * 100))
    en_atk = int(round(float(r["enemy_attack"]) * 20))
    en_def = int(round(float(r["enemy_defense"]) * 20))

    expected = decide_action(pet_hp, pet_en, pet_atk, pet_def, en_hp, en_en, en_atk, en_def)
    predicted = int(r["pred_action"])
    c = float(r["confidence"])
    if expected == predicted:
        conf_correct.append(c)
    else:
        conf_wrong.append(c)
if conf_correct:
    print(f"  Aciertos:     {sum(conf_correct)/len(conf_correct):.4f}")
if conf_wrong:
    print(f"  Desaciertos:  {sum(conf_wrong)/len(conf_wrong):.4f}")
print()

print("=== 10 desaciertos con mayor confianza ===")
disagreements.sort(key=lambda x: -float(x["confidence"]))
for d in disagreements[:10]:
    print(f"  esperado={d['expected']} predicho={d['predicted']} "
          f"(conf={d['confidence']}) | "
          f"pet(hp={d['pet_hp']} en={d['pet_en']} atk={d['pet_atk']} def={d['pet_def']}) | "
          f"enemy(hp={d['en_hp']} atk={d['en_atk']} def={d['en_def']}) | "
          f"atk_adv={d['atk_adv']} def_disadv={d['def_disadv']}")
