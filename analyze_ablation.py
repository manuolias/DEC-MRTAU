#!/usr/bin/env python3
"""
Análisis de resultados de ablación Dec-MCTS.
Uso: python3 analyze_ablation.py <logs_dir>  (default: logs/prueba)
"""
import sys, os, csv, glob, statistics
from collections import defaultdict

LOGS_DIR = sys.argv[1] if len(sys.argv) > 1 else "logs/prueba"

# ── Parsear logs ──────────────────────────────────────────────────────────────
rows = []
for f in glob.glob(os.path.join(LOGS_DIR, "*.log")):
    info = {}
    with open(f) as fh:
        for line in fh:
            line = line.strip()
            # Lines are either "info: key; value: val" or "metric: key; value: val"
            for prefix in ("info: ", "metric: "):
                if line.startswith(prefix):
                    rest = line[len(prefix):]
                    if "; value: " in rest:
                        key, val = rest.split("; value: ", 1)
                        info[key.strip()] = val.strip()
    if "solver_name" in info and "final_reward" in info:
        rows.append(info)

if not rows:
    print("No logs found.")
    sys.exit(1)

# ── Agrupar por solver ────────────────────────────────────────────────────────
by_solver = defaultdict(list)
for r in rows:
    by_solver[r["solver_name"]].append(float(r["final_reward"]))

print("=" * 65)
print(f"{'Solver':<22} {'n':>4}  {'mean':>7}  {'median':>7}  {'stdev':>7}  {'min':>5}  {'max':>5}")
print("-" * 65)

solvers_ordered = ["random", "greedy", "cbaa", "cbba", "dec-mcts", "dec-mcts-v1", "dec-mcts-v2", "dec-mcts-v3"]
for s in solvers_ordered + [k for k in by_solver if k not in solvers_ordered]:
    if s not in by_solver:
        continue
    rewards = by_solver[s]
    n = len(rewards)
    mean = statistics.mean(rewards)
    med  = statistics.median(rewards)
    sd   = statistics.stdev(rewards) if n > 1 else 0.0
    mn   = min(rewards); mx = max(rewards)
    print(f"{s:<22} {n:>4}  {mean:>7.4f}  {med:>7.4f}  {sd:>7.4f}  {mn:>5.3f}  {mx:>5.3f}")

print("=" * 65)

# ── Comparación por escenario (señal de varianza vs sesgo) ───────────────────
print("\nPor escenario — mean reward:")
by_sc = defaultdict(lambda: defaultdict(list))
for r in rows:
    s = r["solver_name"]
    if s in solvers_ordered:
        by_sc[r["scenario_name"]][s].append(float(r["final_reward"]))

print(f"\n{'Scenario':<28} {'random':>10} {'greedy':>10} {'cbaa':>8} {'cbba':>8} {'dec-mcts':>10} {'dec-mcts-v1':>10} {'dec-mcts-v2':>10} {'dec-mcts-v3':>10}")
print("-" * 70)

wins = {"dec-mcts-v1": 0, "dec-mcts-v2": 0, "dec-mcts-v3": 0}
for sc in sorted(by_sc):
    d   = by_sc[sc]
    def m(k): return f"{statistics.mean(d[k]):.3f}" if k in d else "  -  "
    vals = {k: statistics.mean(d[k]) for k in d if d[k]}
    if vals:
        best = max(vals, key=vals.get)
        wins[best] = wins.get(best, 0) + 1
    print(f"{sc:<28} {m('dec-mcts-v1'):>10} "
          f"{m('dec-mcts-v2'):>10} {m('dec-mcts-v3'):>10}")

print("-" * 70)
print("Wins (mejor score por escenario):", wins)
"""
# ── H1 Diagnóstico: ¿f^r reduce o aumenta varianza? ─────────────────────────
print("\n── Diagnóstico H1 (f^r vs reward_direct) ──")
r1 = by_solver.get("dec-mcts", [])
r2 = by_solver.get("dec-mcts-nodiff", [])
if r1 and r2:
    diff = statistics.mean(r1) - statistics.mean(r2)
    print(f"  dec-mcts (con f^r):    mean={statistics.mean(r1):.4f}  sd={statistics.stdev(r1):.4f}")
    print(f"  dec-mcts-nodiff:       mean={statistics.mean(r2):.4f}  sd={statistics.stdev(r2):.4f}")
    print(f"  Diferencia:           {diff:+.4f}")
    if diff < -0.01:
        print("  → H1 CONFIRMADA: f^r perjudica. Baseline PASSIVE_ME introduce sesgo.")
    elif diff > 0.01:
        print("  → H1 refutada: f^r ayuda (+{diff:.4f}).")
    else:
        print("  → H1 no concluyente: diferencia pequeña (<0.01).")

# ── H4 Diagnóstico: ¿GAMMA=0.99 vs 0.999? ───────────────────────────────────
print("\n── Diagnóstico H4 (GAMMA=0.99 vs 0.999) ──")
r3 = by_solver.get("dec-mcts-gamma99", [])
if r1 and r3:
    diff4 = statistics.mean(r1) - statistics.mean(r3)
    print(f"  dec-mcts (γ=0.999):    mean={statistics.mean(r1):.4f}")
    print(f"  dec-mcts-gamma99:      mean={statistics.mean(r3):.4f}")
    print(f"  Diferencia:           {diff4:+.4f}  (+ = γ=0.999 mejor)")
    if diff4 > 0.01:
        print("  → H4 CONFIRMADA: γ=0.999 (más retención) es mejor.")
    elif diff4 < -0.01:
        print("  → H4 invertida: γ=0.99 funciona mejor.")
    else:
        print("  → H4 no concluyente.")
"""