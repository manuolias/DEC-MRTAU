#!/usr/bin/env python3
"""
Sonda exploratoria de VARIANTES DE DISEÑO DE ESCENARIO (sesión 6).

Objetivo
--------
Antes de rediseñar el catálogo definitivo en bloques equilibrados, medir qué ejes
de diseño favorecen a cada algoritmo. Varios de estos ejes NUNCA se han probado:
la descomposición de la ventana en «espera» y «plazo», el coste temporal del
fracaso, el coste de navegar frente al de ejecutar, la escasez de batería, la
heterogeneidad de duraciones/capacidades y la dispersión inicial de los robots.

Diseño
------
Una sola condición de referencia y UNA variación por variante:

    referencia:  geometría bnd (4 racimos) · L = 6 · T = 40 · q = 1
                 batería 100/100 · nav_rate 1 · velocidad 1
                 éxito N(10, σ) · fracaso instantáneo · robots homogéneos
                 todos los robots parten de la estación

    celdas:      R ∈ {4, 8}  ×  régimen ∈ {det, est}  ×  3 instancias
                 = 12 escenarios por variante (6 det + 6 est), equilibrado

Pareado entre variantes
-----------------------
La semilla depende solo de (R, régimen, instancia), NO de la variante, y todos los
sorteos por tarea (t0, ε, u1, u2) se extraen ANTES de aplicar la variante. Dos
variantes comparten por tanto la misma geometría y los mismos sorteos: la
comparación entre variantes es pareada tarea a tarea, no solo escenario a escenario.

Las 17 variantes
----------------
  VENTANA — factorial 2×2 sobre {apertura ∈ 0/t0} × {cierre ∈ t0+10/T+10}
    w_ancha     [0,  T+10+ε]      sin espera, sin plazo apretado   (celda 0-0)
    w_plazo     [0,  t0+10+ε]     sin espera, plazo aleatorio      (celda 0-t)
    w_espera    [t0, T+10+ε]      espera, sin plazo apretado       (celda t-0)
    w_base      [t0, t0+10+ε]     espera + plazo  = ventana E      (celda t-t)  ★REFERENCIA
    w_escalon   [0,  T(g+1)/4+ε]  plazo escalonado por racimo
    w_libre     [min,max] de dos instantes U(0,T+10) independientes
    w_dura      [t0, t0+5+ε/2]    ventana estrecha dura
  FRACASO (no-op en det: sirve de suelo de ruido)
    f_igual     fail_time = [10, σ]   el fracaso cuesta lo mismo que el éxito
    f_doble     fail_time = [20, σ]   el fracaso cuesta el doble
  BATERÍA
    b_navbarata nav_rate 0.25   navegar es 4× más barato que ejecutar
    b_navcara   nav_rate 4      navegar es 4× más caro que ejecutar
    b_escasa    capacidad 40    obliga a recargar
    b_navcara_escasa  nav_rate 3 + capacidad 40
  HETEROGENEIDAD
    h_duracion  duraciones 5/10/15 cíclicas (media 10: la carga no cambia)
    h_capacidad 3 clases de habilidad; cada robot cubre 2 de 3
    h_disperso  los robots arrancan repartidos por los racimos, no en la estación
  ANCLA
    c_q2        coalición de 2 trabajadores (resultado ya conocido: control)
"""

import math
import os
import random
import zlib

ROOT_DIR   = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(ROOT_DIR, "..", "scenarios", "probe")

T            = 40.0    # horizonte de referencia
WIDTH        = 10.0    # anchura nominal de la ventana estrecha
JITTER       = 3.0     # σ del ruido sobre el cierre
BASE_SEED    = 20260827

LOAD         = 6       # carga por robot (plazas / robot)
ROBOTS       = (4, 8)
REGIMES      = {"det": (1.00, 0.0, 0.0), "est": (0.75, 10.0, 10.0)}
N_INST       = 3
N_GROUPS     = 4

BATTERY      = 100.0
NAV_RATE     = 1.0
NAV_VELOCITY = 1.0
DURATION     = 10.0


# ── formato ───────────────────────────────────────────────────────────────────

def flow(items):
    return "[" + ",".join(str(v) for v in items) + "]"


def fmt(v):
    s = f"{float(v):.4f}".rstrip("0").rstrip(".")
    return s if "." in s else s + ".0"


def split_evenly(n, k):
    base, rem = divmod(n, k)
    return [base + (1 if i < rem else 0) for i in range(k)]


def ring(n, cx, cy, radius=1.0):
    pts = []
    for k in range(n):
        a = math.pi / 2.0 - 2.0 * math.pi * k / max(n, 1)
        pts.append((round(cx + radius * math.cos(a), 2),
                    round(cy + radius * math.sin(a), 2)))
    return pts


def geometry(n):
    """4 racimos equidistantes de la estación. Determinista dado n."""
    centres = [(0.0, 4.0), (4.0, 0.0), (0.0, -4.0), (-4.0, 0.0)]
    pos, grp, first = [], [], []
    for g, ((cx, cy), k) in enumerate(zip(centres, split_evenly(n, N_GROUPS))):
        first.append(len(pos))          # índice de la 1.ª tarea del racimo
        pos.extend(ring(k, cx, cy))
        grp.extend([g] * k)
    return pos, grp, first


# ── ventanas: todas se derivan de los MISMOS sorteos ──────────────────────────

def window(kind, d, group):
    """d = {'t0','eps','u1','u2'} sorteado antes de conocer la variante."""
    t0, eps, u1, u2 = d["t0"], d["eps"], d["u1"], d["u2"]
    if kind == "ancha":    return 0.0, T + WIDTH + eps
    if kind == "plazo":    return 0.0, t0 + WIDTH + eps
    if kind == "espera":   return t0,  T + WIDTH + eps
    if kind == "base":     return t0,  t0 + WIDTH + eps
    if kind == "escalon":  return 0.0, T * (group + 1) / N_GROUPS + eps
    if kind == "dura":     return t0,  t0 + WIDTH / 2 + eps / 2
    if kind == "libre":                       # apertura y cierre independientes
        a, b = (T + WIDTH) * u1, (T + WIDTH) * u2
        return min(a, b), max(a, b)
    raise ValueError(kind)


# ── catálogo de variantes ─────────────────────────────────────────────────────
# Cada variante es un dict de anulaciones sobre la referencia.

VARIANTS = {
    # ventana (factorial 2x2 + 3 formas extra)
    "w_ancha":          dict(win="ancha"),
    "w_plazo":          dict(win="plazo"),
    "w_espera":         dict(win="espera"),
    "w_base":           dict(),                       # ★ referencia
    "w_escalon":        dict(win="escalon"),
    "w_libre":          dict(win="libre"),
    "w_dura":           dict(win="dura"),
    # duración del fracaso
    "f_igual":          dict(fail_mu=10.0, fail_sd="sigma"),
    "f_doble":          dict(fail_mu=20.0, fail_sd="sigma"),
    # batería
    "b_navbarata":      dict(nav_rate=0.25),
    "b_navcara":        dict(nav_rate=4.0),
    "b_escasa":         dict(battery=40.0),
    "b_navcara_escasa": dict(nav_rate=3.0, battery=40.0),
    # heterogeneidad
    "h_duracion":       dict(durations=(5.0, 10.0, 15.0)),
    "h_capacidad":      dict(skills=True),
    "h_disperso":       dict(spread=True),
    # ancla conocida
    "c_q2":             dict(workers=2),
}


def build(name, variant, R, reg, inst):
    v = VARIANTS[variant]
    prob, sigma_t, fail_demand = REGIMES[reg]

    workers  = v.get("workers", 1)
    n_tasks  = max(N_GROUPS, int(round(LOAD * R / workers)))
    win      = v.get("win", "base")
    nav_rate = v.get("nav_rate", NAV_RATE)
    battery  = v.get("battery", BATTERY)
    fail_mu  = v.get("fail_mu", 0.0)
    fail_sd  = sigma_t if v.get("fail_sd") == "sigma" else 0.0
    durs     = v.get("durations", (DURATION,))

    positions, groups, first_of_group = geometry(n_tasks)

    # Sorteos independientes de la variante: misma semilla para toda la celda.
    rng = random.Random(BASE_SEED + zlib.crc32(f"{R}|{reg}|{inst}".encode()))
    draws = [dict(t0=float(rng.randint(0, int(T))),
                  eps=round(rng.gauss(0.0, JITTER), 2),
                  u1=rng.random(), u2=rng.random())
             for _ in range(n_tasks)]

    n_nodes   = n_tasks + 1
    all_nodes = list(range(1, n_nodes + 1))
    L = [f'name: "{name}"', "height: 15", "width: 15", "initial_time: 0", ""]

    L.append("graph:")
    L.append("  - node: 1")
    L.append('    description: "Node 1"')
    L.append("    coords: [0,0]")
    L.append(f"    neighbors: {flow(all_nodes)}")
    for idx, (x, y) in enumerate(positions):
        L += [f"  - node: {idx + 2}",
              f'    description: "Node {idx + 2}"',
              f"    coords: [{fmt(x)},{fmt(y)}]",
              f"    neighbors: {flow(all_nodes)}"]
    L.append("")

    L.append("tasks:")
    for idx, group in enumerate(groups):
        t_early, t_late = window(win, draws[idx], group)
        mu = durs[idx % len(durs)]
        L += [f"  - id: {idx + 1}",
              f"    node: {idx + 2}",
              f'    description: "Task {idx + 1}"',
              f"    time_window: [{fmt(t_early)},{fmt(t_late)}]",
              f"    success_prob: {fmt(prob)}",
              f"    success_time: [{fmt(mu)},{fmt(sigma_t)}]",
              f"    fail_time: [{fmt(fail_mu)},{fmt(fail_sd)}]",
              f"    demand: [{fmt(mu)},{fmt(fail_demand)}]",
              f"    required_workers: {workers}"]
    L.append("")

    # robots
    L.append("robots:")
    for r in range(1, R + 1):
        if v.get("skills"):
            # 3 clases; el robot r cubre las clases {r%3, (r+1)%3}
            mine = {(r - 1) % 3, r % 3}
            caps = [j + 1 for j in range(n_tasks) if j % 3 in mine]
        else:
            caps = list(range(1, n_tasks + 1))

        if v.get("spread"):
            node0 = 2 + first_of_group[(r - 1) % N_GROUPS]
        else:
            node0 = 1

        L += [f"  - id: {r}",
              f"    initial_node: {node0}",
              f'    description: "Robot {r}"',
              f"    initial_battery_level: {fmt(battery)}",
              f"    battery_capacity: {fmt(battery)}",
              f"    navigation_velocity: {fmt(NAV_VELOCITY)}",
              f"    battery_rate_while_navigating: {fmt(nav_rate)}",
              f"    capabilities: {flow(caps)}"]
    L.append("")

    L += ["recharging_stations:", "  - id: 1", "    node: 1",
          '    description: "Station 1"', ""]
    return "\n".join(L)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    written = 0
    for variant in VARIANTS:
        for R in ROBOTS:
            for reg in REGIMES:
                for inst in range(1, N_INST + 1):
                    w = VARIANTS[variant].get("workers", 1)
                    n = max(N_GROUPS, int(round(LOAD * R / w)))
                    name = f"prb_{variant}_r{R:02d}_n{n:03d}_{reg}_i{inst}"
                    with open(os.path.join(OUTPUT_DIR, name + ".yaml"), "w") as fh:
                        fh.write(build(name, variant, R, reg, inst))
                    written += 1

    cfg = os.path.join(OUTPUT_DIR, "experiment_config.yaml")
    with open(cfg, "w") as fh:
        fh.write("solvers: ['random', 'greedy', 'cbaa', 'cbba', 'dec-mcts-v4-g9999']\n"
                 "reward_functions: ['reward00']\n"
                 "replicas: 3\n")

    print(f"{written} escenarios en {os.path.normpath(OUTPUT_DIR)} "
          f"({len(VARIANTS)} variantes × {len(ROBOTS)} R × {len(REGIMES)} reg × {N_INST} inst)")


if __name__ == "__main__":
    main()
