#!/usr/bin/env python3
"""
Comparación por escenario de los solvers Dec-MCTS (incluida la nueva v4).
Uso: python3 compare_v4.py <logs_dir>  (default: logs/prueba)
"""
import sys, os, glob, statistics
from collections import defaultdict

LOGS_DIR = sys.argv[1] if len(sys.argv) > 1 else "logs/prueba"
SOLVERS = ["cbba", "dec-mcts-v1", "dec-mcts-v2", "dec-mcts-v3", "dec-mcts-v4"]

rows = []
for f in glob.glob(os.path.join(LOGS_DIR, "*.log")):
    info = {}
    with open(f) as fh:
        for line in fh:
            line = line.strip()
            for prefix in ("info: ", "metric: "):
                if line.startswith(prefix):
                    rest = line[len(prefix):]
                    if "; value: " in rest:
                        key, val = rest.split("; value: ", 1)
                        info[key.strip()] = val.strip()
    if "solver_name" in info and "final_reward" in info:
        rows.append(info)

by_sc = defaultdict(lambda: defaultdict(list))
for r in rows:
    if r["solver_name"] in SOLVERS:
        by_sc[r["scenario_name"]][r["solver_name"]].append(float(r["final_reward"]))

header = f"{'Scenario':<26}" + "".join(f"{s:>13}" for s in SOLVERS)
print(header)
print("-" * len(header))

wins = defaultdict(int)
totals = defaultdict(list)
for sc in sorted(by_sc):
    d = by_sc[sc]
    vals = {k: statistics.mean(v) for k, v in d.items() if v}
    if vals:
        best = max(vals.values())
        for k, v in vals.items():
            if abs(v - best) < 1e-9:
                wins[k] += 1
    cells = ""
    for s in SOLVERS:
        cells += f"{vals.get(s, float('nan')):>13.3f}" if s in vals else f"{'-':>13}"
        if s in vals:
            totals[s].append(vals[s])
    print(f"{sc:<26}{cells}")

print("-" * len(header))
mean_row = f"{'MEAN':<26}"
for s in SOLVERS:
    mean_row += f"{statistics.mean(totals[s]):>13.4f}" if totals[s] else f"{'-':>13}"
print(mean_row)
print("\nWins (mejor media por escenario, con empates):",
      dict(sorted(wins.items(), key=lambda kv: -kv[1])))

# Diferencia v4 - v2 emparejada por escenario
diffs = []
for sc in sorted(by_sc):
    d = by_sc[sc]
    if d.get("dec-mcts-v2") and d.get("dec-mcts-v4"):
        diffs.append(statistics.mean(d["dec-mcts-v4"]) - statistics.mean(d["dec-mcts-v2"]))
if diffs:
    n_pos = sum(1 for x in diffs if x > 1e-9)
    n_neg = sum(1 for x in diffs if x < -1e-9)
    print(f"\nΔ(v4-v2) por escenario: media={statistics.mean(diffs):+.4f}  "
          f"mediana={statistics.median(diffs):+.4f}  gana v4 en {n_pos}, pierde en {n_neg} "
          f"de {len(diffs)} escenarios")
