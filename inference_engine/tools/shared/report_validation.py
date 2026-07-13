import csv
import random
import os
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.append(TOOLS_DIR)

from heuristics import decide_action

PROJECT_DIR = os.path.dirname(TOOLS_DIR)
INPUT_FILE = os.path.join(PROJECT_DIR, "data", "datasets", "combat_validation_onnx_results.csv")
ACTIONS = ["ATACAR", "DEFENDER", "HUIR"]

print("Cargando resultados...")
with open(INPUT_FILE) as f:
    rows = list(csv.DictReader(f))

correct = 0
total = len(rows)
conf_matrix = {a: {b: 0 for b in ACTIONS} for a in ACTIONS}
disagreements = []

for r in rows:
    expected = decide_action(
        hit_prob=float(r["s2"]),
        defense_prob=float(r["s3"]),
        pet_hp_ratio=float(r["s0"]),
        enemy_hp_ratio=float(r["s4"]),
        threat_level=float(r["s7"]),
        dmg_efficiency=float(r["s6"]),
        quality_score=float(r["s8"]),
        last_action=0
    )
    predicted = int(r["pred_action"])

    conf_matrix[ACTIONS[expected]][ACTIONS[predicted]] += 1

    if expected == predicted:
        correct += 1
    elif len(disagreements) < 10:
        disagreements.append({
            "expected": ACTIONS[expected],
            "predicted": ACTIONS[predicted],
            "confidence": r["confidence"],
            "s0": r["s0"],
            "s1": r["s1"],
            "s2": r["s2"],
            "s3": r["s3"],
            "s4": r["s4"],
            "s6": r["s6"],
            "s7": r["s7"],
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
    print(f"  {a:>10}: {ok:4d}/{tot:4d} ({ok/tot*100:.1f}%)")
print()

print("=== Confianza promedio por acierto/desacierto ===")
conf_correct = []
conf_wrong = []
for r in rows:
    expected = decide_action(
        hit_prob=float(r["s2"]),
        defense_prob=float(r["s3"]),
        pet_hp_ratio=float(r["s0"]),
        enemy_hp_ratio=float(r["s4"]),
        threat_level=float(r["s7"]),
        dmg_efficiency=float(r["s6"]),
        quality_score=float(r["s8"]),
        last_action=0
    )
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
          f"pet(hp={d['s0']} en={d['s1']} atk={d['s2']} def={d['s3']}) | "
          f"enemy(hp={d['s4']} atk={d['s6']} def={d['s7']}) | "
          f"atk_adv={d['atk_adv']} def_disadv={d['def_disadv']}")
