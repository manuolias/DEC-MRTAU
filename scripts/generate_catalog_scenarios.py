#!/usr/bin/env python3
"""
Generador del catálogo definitivo de escenarios del TFM (objetivo 1.2).

Sustituye a `generate_bundle_scenarios.py`, que tenía dos limitaciones:
  - exigía que el nº de tareas fuese múltiplo de 4 (con n=10 escribía 8 en silencio);
  - ligaba el horizonte temporal al nº de tareas, de modo que "más robots" implicaba
    "menos carga por robot" y ambos factores quedaban confundidos.

Principio de diseño
-------------------
La dificultad se controla mediante la CARGA POR ROBOT  L = n_slots / R,  donde
n_slots = Σ_j q_j es el número total de plazas de trabajador que hay que cubrir.
El horizonte MAX_TIME se mantiene FIJO (T = 40) para todos los escenarios.

Pilotaje (2026-08-17, 3 réplicas, ventana E, régimen determinista): con L = 6 el
solver de referencia (CBBA) obtiene 0.667 / 0.667 / 0.683 con R = 2 / 5 / 10.
Es decir, L = 6 es el punto ISO-DIFICULTAD: el tamaño del equipo deja de estar
confundido con la dificultad del problema y puede estudiarse como factor aislado.

Ejes del catálogo
-----------------
  R    nº de robots                    2 … 10
  L    carga (plazas por robot)        3, 4, 5, 6, 8
  geom geometría                       bnd (4 racimos) · rnd (uniforme) · asy (asimétrica)
  win  ventana de ejecución            E (estrecha desplazada) · A (ancha) · C (escalonada)
  reg  régimen de incertidumbre        det · lev · est · fue
  q    coaliciones                     q1 (1 trab.) · q2 (2 trab.) · qm (mezcla 1-2-3)
  i    instancia (semilla)             1 … 5

Bloques experimentales
----------------------
  A1  presión de coordinación a iso-dificultad   R = 2…10, L = 6            90 esc.
  A2  rejilla robots × holgura                   R ∈ {2,5,10}, L ∈ {3,4,5,8} 72 esc.
  B   acoplamiento temporal                      win ∈ {A, C}                36 esc.
  C   coaliciones                                q ∈ {q2, qm}                36 esc.
  D   distribución espacial                      geom ∈ {rnd, asy}           36 esc.
  E   gradiente de incertidumbre                 reg ∈ {lev, fue}            18 esc.
                                                                     TOTAL  288 esc.

Nomenclatura
------------
  cat_{bloque}_r{R:02d}_n{n:03d}_{geom}_{win}_{reg}_{q}_i{i}.yaml

Reproducibilidad: la semilla de cada escenario se deriva por CRC32 de su propio
nombre, así que regenerar el catálogo reproduce byte a byte los mismos ficheros.
"""

import math
import os
import random
import zlib

ROOT_DIR   = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(ROOT_DIR, "..", "scenarios", "catalogo")

# ── constantes globales del catálogo ──────────────────────────────────────────

MAX_TIME     = 40.0   # horizonte T: cota superior del inicio de las ventanas
WINDOW_WIDTH = 10.0   # anchura nominal de la ventana estrecha (tipo E)
WINDOW_JITTER= 3.0    # desviación típica del ruido que se suma al cierre de ventana
BASE_SEED    = 20260817

BATTERY      = 100.0  # capacidad e inicio de batería de todos los robots
NAV_RATE     = 1.0    # consumo de batería por unidad de tiempo navegando
NAV_VELOCITY = 1.0

N_GROUPS     = 4      # racimos (o cuartiles, en la geometría uniforme)

# Regímenes de incertidumbre: (success_prob, σ_éxito, σ_demanda)
# La duración media de una tarea es 10 y el fracaso cuesta [0,0] en TODOS los
# regímenes, igual que en el catálogo anterior, para conservar la comparabilidad
# con las tandas eval_bundles / eval_versiones_fix2 / eval_1E.
REGIMES = {
    "det": (1.00,  0.0,  0.0),   # determinista
    "lev": (0.90,  3.0,  3.0),   # incertidumbre leve
    "est": (0.75, 10.0, 10.0),   # incertidumbre media (= stype 2 del catálogo anterior)
    "fue": (0.50, 10.0, 10.0),   # incertidumbre fuerte
}

# Coaliciones: patrón cíclico de trabajadores requeridos y su media
COALITIONS = {
    "q1": ([1],       1.0),
    "q2": ([2],       2.0),
    "qm": ([1, 2, 3], 2.0),
}


# ── utilidades de formato ─────────────────────────────────────────────────────

def flow(items) -> str:
    """Renderiza un iterable como secuencia YAML en línea: [a,b,c]."""
    return "[" + ",".join(str(v) for v in items) + "]"


def fmt(v: float) -> str:
    """Formatea un float dejando al menos un decimal."""
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return s if "." in s else s + ".0"


def split_evenly(n: int, k: int):
    """Reparte n elementos entre k grupos lo más uniformemente posible."""
    base, rem = divmod(n, k)
    return [base + (1 if i < rem else 0) for i in range(k)]


# ── geometrías ────────────────────────────────────────────────────────────────
#
# Todas devuelven (task_positions, task_groups) con el nodo 1 en (0,0) reservado
# para la estación de recarga, donde además arrancan todos los robots.

def _ring(n: int, cx: float, cy: float, radius: float):
    """n puntos equiespaciados sobre una circunferencia, desde arriba y en sentido horario."""
    pts = []
    for k in range(n):
        a = math.pi / 2.0 - 2.0 * math.pi * k / max(n, 1)
        pts.append((round(cx + radius * math.cos(a), 2),
                    round(cy + radius * math.sin(a), 2)))
    return pts


def geom_bundles(n: int, rng):
    """4 racimos equidistantes de la estación, con las tareas repartidas por igual."""
    centres = [(0.0, 4.0), (4.0, 0.0), (0.0, -4.0), (-4.0, 0.0)]
    pos, grp = [], []
    for g, ((cx, cy), k) in enumerate(zip(centres, split_evenly(n, N_GROUPS))):
        pos.extend(_ring(k, cx, cy, 1.0))
        grp.extend([g] * k)
    return pos, grp


def geom_asymmetric(n: int, rng):
    """4 racimos a distancias y con cargas desiguales: el más poblado es el más cercano."""
    dists  = [2.0, 4.0, 6.0, 8.0]
    angles = [math.pi / 2, 0.0, -math.pi / 2, math.pi]
    weights = [0.45, 0.25, 0.20, 0.10]
    counts = [max(1, int(round(w * n))) for w in weights]
    while sum(counts) > n:
        counts[counts.index(max(counts))] -= 1
    while sum(counts) < n:
        counts[counts.index(max(counts))] += 1

    pos, grp = [], []
    for g, (d, a, k) in enumerate(zip(dists, angles, counts)):
        pos.extend(_ring(k, d * math.cos(a), d * math.sin(a), 1.0))
        grp.extend([g] * k)
    return pos, grp


def geom_random(n: int, rng):
    """Tareas uniformes en [-5,5]²; distancia media a la estación comparable a la de bnd."""
    pos = [(round(rng.uniform(-5.0, 5.0), 2), round(rng.uniform(-5.0, 5.0), 2))
           for _ in range(n)]
    grp = [i % N_GROUPS for i in range(n)]   # cuartiles nominales, para la ventana C
    return pos, grp


GEOMETRIES = {"bnd": geom_bundles, "asy": geom_asymmetric, "rnd": geom_random}


# ── ventanas de ejecución ─────────────────────────────────────────────────────

def time_window(win: str, group: int, rng):
    """Devuelve (earliest_start, latest_start) según el tipo de ventana."""
    eps = round(rng.gauss(0.0, WINDOW_JITTER), 2)
    if win == "E":       # estrecha y desplazada al azar: obliga a planificar *cuándo*
        t0 = float(rng.randint(0, int(MAX_TIME)))
        return t0, t0 + WINDOW_WIDTH + eps
    if win == "A":       # ancha: todas las tareas disponibles desde el instante 0
        return 0.0, MAX_TIME + WINDOW_WIDTH + eps
    if win == "C":       # escalonada por racimo: ¼, ½, ¾ y 1 del horizonte
        return 0.0, MAX_TIME * (group + 1) / N_GROUPS + eps
    raise ValueError(f"tipo de ventana desconocido: {win}")


# ── escritura del YAML ────────────────────────────────────────────────────────

def build_scenario(name: str, n_robots: int, n_tasks: int,
                   geom: str, win: str, reg: str, q: str, rng) -> str:
    positions, groups = GEOMETRIES[geom](n_tasks, rng)
    prob, sigma_t, sigma_d = REGIMES[reg]
    pattern, _ = COALITIONS[q]

    n_nodes   = n_tasks + 1
    all_nodes = list(range(1, n_nodes + 1))
    L = []

    L.append(f'name: "{name}"')
    L.append("height: 15")
    L.append("width: 15")
    L.append("initial_time: 0")
    L.append("")

    # --- grafo completo: nodo 1 = estación, nodos 2..n+1 = tareas ---
    L.append("graph:")
    L.append("  - node: 1")
    L.append('    description: "Node 1"')
    L.append("    coords: [0,0]")
    L.append(f"    neighbors: {flow(all_nodes)}")
    for idx, (x, y) in enumerate(positions):
        L.append(f"  - node: {idx + 2}")
        L.append(f'    description: "Node {idx + 2}"')
        L.append(f"    coords: [{fmt(x)},{fmt(y)}]")
        L.append(f"    neighbors: {flow(all_nodes)}")
    L.append("")

    # --- tareas ---
    L.append("tasks:")
    for idx, group in enumerate(groups):
        t_early, t_late = time_window(win, group, rng)
        workers = pattern[idx % len(pattern)]
        L.append(f"  - id: {idx + 1}")
        L.append(f"    node: {idx + 2}")
        L.append(f'    description: "Task {idx + 1}"')
        L.append(f"    time_window: [{fmt(t_early)},{fmt(t_late)}]")
        L.append(f"    success_prob: {fmt(prob)}")
        L.append(f"    success_time: [10,{fmt(sigma_t)}]")
        L.append(f"    fail_time: [0,0]")
        L.append(f"    demand: [10,{fmt(sigma_d)}]")
        L.append(f"    required_workers: {workers}")
    L.append("")

    # --- robots: homogéneos, todos capaces de toda tarea, todos en la estación ---
    caps = list(range(1, n_tasks + 1))
    L.append("robots:")
    for r in range(1, n_robots + 1):
        L.append(f"  - id: {r}")
        L.append("    initial_node: 1")
        L.append(f'    description: "Robot {r}"')
        L.append(f"    initial_battery_level: {fmt(BATTERY)}")
        L.append(f"    battery_capacity: {fmt(BATTERY)}")
        L.append(f"    navigation_velocity: {fmt(NAV_VELOCITY)}")
        L.append(f"    battery_rate_while_navigating: {fmt(NAV_RATE)}")
        L.append(f"    capabilities: {flow(caps)}")
    L.append("")

    L.append("recharging_stations:")
    L.append("  - id: 1")
    L.append("    node: 1")
    L.append('    description: "Station 1"')
    L.append("")

    return "\n".join(L)


# ── especificación del catálogo ───────────────────────────────────────────────

def catalog_cells():
    """Genera las tuplas (bloque, R, L, geom, win, reg, q, n_instancias) del catálogo."""
    # A1 — presión de coordinación a iso-dificultad
    for R in range(2, 11):
        for reg in ("det", "est"):
            yield ("A1", R, 6, "bnd", "E", reg, "q1", 5)
    # A2 — rejilla robots × holgura (desacopla tamaño de equipo de dificultad)
    for R in (2, 5, 10):
        for Lc in (3, 4, 5, 8):
            for reg in ("det", "est"):
                yield ("A2", R, Lc, "bnd", "E", reg, "q1", 3)
    # B — acoplamiento temporal
    for R in (2, 5, 10):
        for win in ("A", "C"):
            for reg in ("det", "est"):
                yield ("B", R, 6, "bnd", win, reg, "q1", 3)
    # C — coaliciones (n se ajusta para que Σ q_j = L·R se mantenga)
    for R in (3, 6, 10):
        for q in ("q2", "qm"):
            for reg in ("det", "est"):
                yield ("C", R, 6, "bnd", "E", reg, q, 3)
    # D — distribución espacial
    for R in (2, 5, 10):
        for geom in ("rnd", "asy"):
            for reg in ("det", "est"):
                yield ("D", R, 6, geom, "E", reg, "q1", 3)
    # E — gradiente de incertidumbre
    for R in (2, 5, 10):
        for reg in ("lev", "fue"):
            yield ("E", R, 6, "bnd", "E", reg, "q1", 3)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    written, by_block = 0, {}

    for (block, R, L, geom, win, reg, q, n_inst) in catalog_cells():
        _, mean_workers = COALITIONS[q]
        n_tasks = max(N_GROUPS, int(round(L * R / mean_workers)))

        for i in range(1, n_inst + 1):
            name = (f"cat_{block}_r{R:02d}_n{n_tasks:03d}"
                    f"_{geom}_{win}_{reg}_{q}_i{i}")
            rng = random.Random(BASE_SEED + zlib.crc32(name.encode()))
            path = os.path.join(OUTPUT_DIR, name + ".yaml")
            with open(path, "w") as fh:
                fh.write(build_scenario(name, R, n_tasks, geom, win, reg, q, rng))
            written += 1
            by_block[block] = by_block.get(block, 0) + 1

    print(f"Catálogo escrito en {os.path.normpath(OUTPUT_DIR)}")
    for b in sorted(by_block):
        print(f"  bloque {b:<3} {by_block[b]:>4} escenarios")
    print(f"  {'TOTAL':<10} {written:>4} escenarios")


if __name__ == "__main__":
    main()
