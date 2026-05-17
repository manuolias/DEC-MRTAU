#!/usr/bin/env python3
"""
Generate 192 MRTAU benchmark scenarios with 4-bundle layout.

Layout
------
  Node 1 at (0,0) — single recharging station, all robots start here.
  4 bundle centres: top (0,4), right (4,0), bottom (0,-4), left (-4,0).
  Task nodes on a unit circle around each centre, starting at angle 90°
  (top of circle) and going clockwise.  The complete graph connects every
  node to every other node.

Naming convention
-----------------
  scenario_{stype}{wtype}_{n_tasks}t_{n_robots}r.yaml
  stype  : 1=deterministic  2=stoch-fixed  3=stoch-var1  4=stoch-var2
  wtype  : A  B  C  D
  n_tasks: 12 | 16 | 20 | 24   (= 3 | 4 | 5 | 6 tasks per bundle)
  n_robots: 2 | 3 | 4 | 5

Scenario matrix  12 types × 4 task-counts × 4 robot-counts = 192 files
  (1A 1B 1C 1D) (2A 2B 2C 2D) (3A 3C) (4A 4D)

MAX_TIME definition
-------------------
  Generous estimate of the time for 4 robots (1 worker/task) to complete
  all tasks when assigned optimally (one bundle per robot).
  Basis: init_travel(~0.8) + n_per_bundle×10 + within_bundle_travel(~1.1)
  with ~30% buffer.  The same value is used for every scenario type with
  the same task count so comparisons remain fair across types.
"""

import math
import os
import random

random.seed(42)  # For reproducible results

ROOT_DIR   = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(ROOT_DIR, "..", "scenarios", "bundles")

# Bundle centres ordered: top, right, bottom, left
BUNDLE_CENTRES = [(0.0, 4.0), (4.0, 0.0), (0.0, -4.0), (-4.0, 0.0)]
BUNDLE_NAMES   = ["top", "right", "bottom", "left"]

TASK_COUNTS  = [12, 16, 20, 24]
ROBOT_COUNTS = [2, 3, 4, 5]

# Generous MAX_TIME: basis
#   12t (3/bnd) → ~32  →  50
#   16t (4/bnd) → ~42  →  60
#   20t (5/bnd) → ~52  →  70
#   24t (6/bnd) → ~62  →  80
MAX_TIME = {12: 20, 16: 30, 20: 40, 24: 50}

SCENARIO_TYPES = [
    (1, "A"), (1, "B"), (1, "C"), (1, "D"), (1, "E"),
    (2, "A"), (2, "B"), (2, "C"), (2, "D"),
    (3, "A"), (3, "C"),
    (4, "A"), (4, "D"),
]


# ── helpers ───────────────────────────────────────────────────────────────────

def flow(items) -> str:
    """Render a Python iterable as a YAML flow sequence: [a,b,c]."""
    return "[" + ",".join(str(v) for v in items) + "]"


def fmt_float(v: float) -> str:
    """Format a float cleanly, keeping at least one decimal place."""
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    if "." not in s:
        s += ".0"
    return s


def circle_pts(n: int, cx: float, cy: float):
    """n equally-spaced points on unit circle, starting at top, clockwise."""
    pts = []
    for k in range(n):
        a = math.pi / 2.0 - 2.0 * math.pi * k / n
        pts.append((round(cx + math.cos(a), 2),
                    round(cy + math.sin(a), 2)))
    return pts


# ── per-section builders ──────────────────────────────────────────────────────

def write_graph(lines: list, n_tasks: int):
    n_nodes   = n_tasks + 1
    all_nodes = list(range(1, n_nodes + 1))
    n_pb      = n_tasks // 4

    lines.append("graph:")
    lines.append("  - node: 1")
    lines.append('    description: "Node 1"')
    lines.append("    coords: [0,0]")
    lines.append(f"    neighbors: {flow(all_nodes)}")

    nid = 2
    for (cx, cy) in BUNDLE_CENTRES:
        for (x, y) in circle_pts(n_pb, cx, cy):
            lines.append(f"  - node: {nid}")
            lines.append(f'    description: "Node {nid}"')
            lines.append(f"    coords: [{fmt_float(x)},{fmt_float(y)}]")
            lines.append(f"    neighbors: {flow(all_nodes)}")
            nid += 1


def write_tasks(lines: list, n_tasks: int, stype: int, wtype: str):
    n_pb = n_tasks // 4
    mt   = MAX_TIME[n_tasks]

    lines.append("tasks:")
    tid = 1
    nid = 2

    for b_idx in range(4):
        for k in range(n_pb):

            rng = round(random.gauss(0, 3), 2) # mean=0, stddev=3
            # ── success properties ──────────────────────────────────────────
            if stype == 1:
                # Deterministic: prob=1, no variance
                prob   = "1.0"
                s_time = "[10,0]"
                f_time = "[0,0]"
                demand = "[10,0]"

            elif stype == 2:
                # Stochastic fixed: uniform 0.75 across all tasks
                prob   = "0.75"
                s_time = "[10,10]"
                f_time = "[0,0]"
                demand = "[10,10]"

            elif stype == 3:
                # Stochastic variable 1: prob varies by bundle
                #   top=1.0  right=0.75  bottom=0.50  left=0.25
                prob   = fmt_float([1.0, 0.75, 0.5, 0.25][b_idx])
                s_time = "[10,10]"
                f_time = "[0,0]"
                demand = "[10,10]"

            else:  # stype == 4
                # Stochastic variable 2: prob descends clockwise within each bundle
                #   pos 0 → 1   pos 1 → (n-1)/n  …  pos n-1 → 1/n
                prob   = fmt_float(round((n_pb - k) / n_pb, 4))
                s_time = "[10,10]"
                f_time = "[0,0]"
                demand = "[10,10]"

            # ── window / worker properties ──────────────────────────────────
            if wtype == "A":
                # All tasks: window [0, MAX_TIME], 1 worker
                tw = f"[0,{mt + rng}]"
                rw = 1

            elif wtype == "B":
                # All tasks: window [0, MAX_TIME]; top & bottom bundles need 2 workers
                tw = f"[0,{mt + rng}]"
                rw = 2 if b_idx in (0, 2) else 1

            elif wtype == "C":
                # Window scales by bundle: top→¼  right→½  bottom→¾  left→1
                w  = int(round(mt * (b_idx + 1) / 4))
                tw = f"[0,{w + rng}]"
                rw = 1

            elif wtype == "D":
                # Window ascends clockwise within bundle: pos k → [0, mt*(k+1)/n]
                w  = int(round(mt * (k + 1) / n_pb))
                tw = f"[0,{w + rng}]"
                rw = 1

            else:  # wtype == "E"
                # Window ascends clockwise within bundle: pos k → [0, mt*(k+1)/n]
                random_start = random.randint(0, mt)
                tw = f"[{random_start},{random_start + 10 + rng}]"
                rw = 1

            lines.append(f"  - id: {tid}")
            lines.append(f"    node: {nid}")
            lines.append(f'    description: "Task {tid}"')
            lines.append(f"    time_window: {tw}")
            lines.append(f"    success_prob: {prob}")
            lines.append(f"    success_time: {s_time}")
            lines.append(f"    fail_time: {f_time}")
            lines.append(f"    demand: {demand}")
            lines.append(f"    required_workers: {rw}")

            tid += 1
            nid += 1


def write_robots(lines: list, n_robots: int, n_tasks: int):
    caps = list(range(1, n_tasks + 1))
    lines.append("robots:")
    for r in range(1, n_robots + 1):
        lines.append(f"  - id: {r}")
        lines.append(f"    initial_node: 1")
        lines.append(f'    description: "Robot {r}"')
        lines.append(f"    initial_battery_level: 100")
        lines.append(f"    battery_capacity: 100")
        lines.append(f"    navigation_velocity: 1")
        lines.append(f"    battery_rate_while_navigating: 1")
        lines.append(f"    capabilities: {flow(caps)}")


def write_scenario(path: str, name: str, n_tasks: int,
                   stype: int, wtype: str, n_robots: int):
    lines = []
    lines.append(f'name: "{name}"')
    lines.append("height: 15")
    lines.append("width: 15")
    lines.append("initial_time: 0")
    lines.append("")

    write_graph(lines, n_tasks)
    lines.append("")

    write_tasks(lines, n_tasks, stype, wtype)
    lines.append("")

    write_robots(lines, n_robots, n_tasks)
    lines.append("")

    lines.append("recharging_stations:")
    lines.append("  - id: 1")
    lines.append("    node: 1")
    lines.append('    description: "Station 1"')
    lines.append("")

    with open(path, "w") as fh:
        fh.write("\n".join(lines))


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    count = 0

    for stype, wtype in SCENARIO_TYPES:
        for n_tasks in TASK_COUNTS:
            for n_robots in ROBOT_COUNTS:
                name = f"scenario_{stype}{wtype}_{n_tasks}t_{n_robots}r"
                path = os.path.join(OUTPUT_DIR, f"{name}.yaml")
                write_scenario(path, name, n_tasks, stype, wtype, n_robots)
                count += 1
                print(f"  {name}.yaml")

    print(f"\nDone — {count} scenarios written to {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
