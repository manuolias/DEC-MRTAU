#!/usr/bin/env python3
"""
Generador del CATÁLOGO v3 del TFM (sesión 8).

Idéntico al v2 (`generate_catalog_v2.py`) SALVO en la parametrización de los
regímenes de incertidumbre. Dos cambios, ambos pedidos por el usuario:

  1. EL COSTE ENERGÉTICO DE UN INTENTO FALLIDO ES CONSTANTE (10) en todos los
     regímenes. En el v2 valía 0 / 3 / 10 / 10 según el régimen, lo que hacía que
     el coste de fracasar dependiese de la incertidumbre sin que hubiera ninguna
     razón de modelo para ello: no es una fuente de incertidumbre, es un dato del
     escenario. Con `fail_time = [0,0]`, el simulador cobra la demanda completa al
     fallar (`src/simulator.cpp:498`), así que un intento FALLIDO cuesta siempre 10.
     ⚠️ OJO: con ÉXITO el consumo NO es 10 fijo, sino `rate * execTime` con rate = 1
     (`averageSuccessDemand / averageSuccessTime` = 10/10), o sea proporcional a la
     duración real. Lo que coincide entre ambos desenlaces es la ESPERANZA (10), no
     el valor: el éxito es tan disperso como σ y el fracaso es determinista.
     ⚠️ En régimen determinista (ρ = 1) el campo no se usa **casi** nunca: sí se usa
     si un robot arranca una tarea sin batería suficiente, porque entonces
     `src/simulator.cpp:469-473` fuerza `success = false`. Ver la duda D-16.

  2. LA DISPERSIÓN DE LA DURACIÓN BAJA A VALORES RAZONABLES: σ = 0 / 1 / 3 / 5
     frente a 0 / 3 / 10 / 10 del v2. Con μ = 10, el v2 llegaba a un coeficiente de
     variación de 1.0 en `est` y `fue` (la duración de una tarea era prácticamente
     impredecible, y la normal truncada en 0 quedaba muy escorada). Los nuevos
     valores corresponden a CV = 0 / 0.1 / 0.3 / 0.5, un gradiente de incertidumbre
     temporal que sigue siendo exigente en `fue` pero deja de ser degenerado.

Todo lo demás —geometría, batería, iso-dificultad, bloques, ventanas, coaliciones,
semillas y nomenclatura— es exactamente el del v2, de manera que la comparación
entre ambos catálogos es pareada celda a celda. Los 162 escenarios DETERMINISTAS
son equivalentes en los dos catálogos (ρ = 1 y σ = 0): su único cambio es el valor
del campo `demand[1]`, que nunca se lee. Sirven como control del cambio.

Principio de diseño (heredado del v1 y revalidado con batería escasa)
---------------------------------------------------------------------
La dificultad se controla con la CARGA POR ROBOT  L = Σ_j q_j / R  y el horizonte
T = 40 se mantiene fijo. El pilotaje de 2026-08-31 (720 ejecuciones, capacidad 40)
confirma que L = 6 es el punto de ISO-DIFICULTAD.

Estructura: el bloque A es el CONTROL
-------------------------------------
El bloque A lleva la configuración de referencia (L=6, ventana E, q1) en los nueve
tamaños de equipo. Los bloques B-E varían UN SOLO FACTOR sobre R ∈ {2,5,10} y se
comparan contra las celdas de A a esos mismos R, así que ningún escenario se gasta
en replicar la referencia.

  A  equipo          R = 2…10                        9 R  × 2 reg × 4 inst = 72
  B  ventana         win ∈ {P, C, A}                 3 win× 3 R × 2 reg × 4 inst = 72
  C  carga           L ∈ {3, 4, 8}                   3 L  × 3 R × 2 reg × 4 inst = 72
  D  coaliciones     q ∈ {q2, qm}                    2 q  × 3 R × 2 reg × 6 inst = 72
  E  incertidumbre   ρ ∈ {1.0, 0.9, 0.75, 0.5}       4 reg× 3 R × 6 inst        = 72
                                                                        TOTAL  360

⚠️ EXCEPCIÓN DOCUMENTADA: el bloque E es el único que NO se divide en mitades
iguales (18 det / 54 est). No es un defecto del diseño sino una definición: el
régimen determinista *es* ρ = 1.0, o sea un extremo del propio gradiente que el
bloque barre; forzar la mitad exigiría replicar tres veces ese único punto.
La regla mitad/mitad se cumple en A, B, C y D — 288 de los 360 escenarios — y el
catálogo completo queda en 162 det / 198 est.

Pareado por semilla
-------------------
La semilla se deriva de (R, n, instancia) y NO del régimen, la ventana ni la
coalición. Dos escenarios con el mismo tamaño comparten por tanto geometría y los
mismos sorteos de ventana, y las comparaciones dentro de un bloque —y contra el
control A— son pareadas tarea a tarea, no solo escenario a escenario.
Los sorteos (t0, ε) se extraen SIEMPRE en el mismo orden, con independencia del
tipo de ventana, para que ese pareado no se rompa.
⚠️ La semilla NO depende del prefijo del catálogo, así que `cv3_A_r05_n030_..._i1`
   tiene exactamente la misma geometría y los mismos plazos que su homólogo `cv2_`.

Nomenclatura
------------
  cv3_{bloque}_r{R:02d}_n{n:03d}_{geom}_{win}_{reg}_{q}_i{i}.yaml
"""


import math
import os
import random
import zlib

ROOT_DIR   = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(ROOT_DIR, "..", "scenarios", "catalogo_v3")

# ── constantes globales del catálogo ──────────────────────────────────────────

MAX_TIME     = 40.0   # horizonte T: cota superior del inicio de las ventanas
WINDOW_WIDTH = 10.0   # anchura nominal de la ventana estrecha (tipo E)
WINDOW_JITTER= 3.0    # desviación típica del ruido que se suma al cierre
BASE_SEED    = 20260831

BATTERY      = 40.0   # batería escasa: la recarga es una decisión real
NAV_RATE     = 1.0    # ⚠️ NO subir: nav caro + batería escasa hunde a Dec-MCTS
NAV_VELOCITY = 1.0

N_GROUPS     = 4      # racimos
REPLICAS     = 3      # como en el v2

# Coste energético de un intento, CONSTANTE en todos los regímenes (cambio 1).
# ⚠️ El 2.º valor de `demand` NO es una desviación típica: el simulador lo lee como
#    `averageFailDemand`, la batería que cuesta un intento fallido
#    (src/scenario.cpp:63-64). Como `fail_time = [0,0]`, el simulador cobra la
#    demanda completa (src/simulator.cpp:498): intentar cuesta 10 salga bien o mal.
FAIL_DEMAND = 10.0

# Regímenes: (success_prob, σ_éxito).  σ da CV = σ/μ = 0 / 0.1 / 0.3 / 0.5 (cambio 2).
REGIMES = {
    "det": (1.00, 0.0),   # determinista
    "lev": (0.90, 1.0),   # incertidumbre leve
    "est": (0.75, 3.0),   # media
    "fue": (0.50, 5.0),   # fuerte
}

# Coaliciones: patrón cíclico de trabajadores requeridos y su media
COALITIONS = {"q1": ([1], 1.0), "q2": ([2], 2.0), "qm": ([1, 2, 3], 2.0)}


# ── utilidades de formato ─────────────────────────────────────────────────────

def flow(items) -> str:
    """Renderiza un iterable como secuencia YAML en línea: [a,b,c]."""
    return "[" + ",".join(str(v) for v in items) + "]"


def fmt(v: float) -> str:
    """Formatea un float dejando al menos un decimal."""
    s = f"{float(v):.4f}".rstrip("0").rstrip(".")
    return s if "." in s else s + ".0"


def split_evenly(n: int, k: int):
    """Reparte n elementos entre k grupos lo más uniformemente posible."""
    base, rem = divmod(n, k)
    return [base + (1 if i < rem else 0) for i in range(k)]


# ── geometría ─────────────────────────────────────────────────────────────────

def geom_bundles(n: int):
    """4 racimos equidistantes de la estación. Determinista dado n."""
    centres = [(0.0, 4.0), (4.0, 0.0), (0.0, -4.0), (-4.0, 0.0)]
    pos, grp = [], []
    for g, ((cx, cy), k) in enumerate(zip(centres, split_evenly(n, N_GROUPS))):
        for j in range(k):
            a = math.pi / 2.0 - 2.0 * math.pi * j / max(k, 1)
            pos.append((round(cx + math.cos(a), 2), round(cy + math.sin(a), 2)))
        grp.extend([g] * k)
    return pos, grp


# ── ventanas de ejecución ─────────────────────────────────────────────────────
#
# Las cuatro se derivan de los MISMOS sorteos (t0, ε), que se extraen siempre en
# el mismo orden. Así `E` y `P` comparten plazos y difieren solo en la espera.

def time_window(win: str, t0: float, eps: float, group: int):
    if win == "E":   # estrecha desplazada: ESPERA + PLAZO  (referencia)
        return t0, t0 + WINDOW_WIDTH + eps
    if win == "P":   # solo PLAZO: mismos cierres que E, sin espera obligatoria
        return 0.0, t0 + WINDOW_WIDTH + eps
    if win == "C":   # plazo escalonado por racimo: ¼, ½, ¾, 1 del horizonte
        return 0.0, MAX_TIME * (group + 1) / N_GROUPS + eps
    if win == "A":   # ancha: sin acoplamiento temporal
        return 0.0, MAX_TIME + WINDOW_WIDTH + eps
    raise ValueError(f"tipo de ventana desconocido: {win}")


# ── escritura del YAML ────────────────────────────────────────────────────────

def build_scenario(name, n_robots, n_tasks, win, reg, q) -> str:
    positions, groups = geom_bundles(n_tasks)
    prob, sigma_t = REGIMES[reg]
    pattern, _ = COALITIONS[q]

    n_nodes   = n_tasks + 1
    all_nodes = list(range(1, n_nodes + 1))
    L = [f'name: "{name}"', "height: 15", "width: 15", "initial_time: 0", ""]

    # --- grafo completo: nodo 1 = estación, nodos 2..n+1 = tareas ---
    L += ["graph:", "  - node: 1", '    description: "Node 1"',
          "    coords: [0,0]", f"    neighbors: {flow(all_nodes)}"]
    for idx, (x, y) in enumerate(positions):
        L += [f"  - node: {idx + 2}",
              f'    description: "Node {idx + 2}"',
              f"    coords: [{fmt(x)},{fmt(y)}]",
              f"    neighbors: {flow(all_nodes)}"]
    L.append("")

    # --- tareas ---
    # La semilla depende solo del tamaño y la instancia (ver cabecera).
    rng = random.Random(BASE_SEED + zlib.crc32(name_key(n_robots, n_tasks, name)))
    L.append("tasks:")
    for idx, group in enumerate(groups):
        t0  = float(rng.randint(0, int(MAX_TIME)))
        eps = round(rng.gauss(0.0, WINDOW_JITTER), 2)
        t_early, t_late = time_window(win, t0, eps, group)
        workers = pattern[idx % len(pattern)]
        L += [f"  - id: {idx + 1}",
              f"    node: {idx + 2}",
              f'    description: "Task {idx + 1}"',
              f"    time_window: [{fmt(t_early)},{fmt(t_late)}]",
              f"    success_prob: {fmt(prob)}",
              f"    success_time: [10,{fmt(sigma_t)}]",
              f"    fail_time: [0,0]",
              f"    demand: [10,{fmt(FAIL_DEMAND)}]",
              f"    required_workers: {workers}"]
    L.append("")

    # --- robots: homogéneos, todos capaces de toda tarea, todos en la estación ---
    caps = list(range(1, n_tasks + 1))
    L.append("robots:")
    for r in range(1, n_robots + 1):
        L += [f"  - id: {r}",
              "    initial_node: 1",
              f'    description: "Robot {r}"',
              f"    initial_battery_level: {fmt(BATTERY)}",
              f"    battery_capacity: {fmt(BATTERY)}",
              f"    navigation_velocity: {fmt(NAV_VELOCITY)}",
              f"    battery_rate_while_navigating: {fmt(NAV_RATE)}",
              f"    capabilities: {flow(caps)}"]
    L.append("")

    L += ["recharging_stations:", "  - id: 1", "    node: 1",
          '    description: "Station 1"', ""]
    return "\n".join(L)


def name_key(n_robots: int, n_tasks: int, name: str) -> bytes:
    """Clave de semilla: tamaño del problema + instancia, NADA más.

    Deja fuera régimen, ventana y coalición a propósito, para que escenarios del
    mismo tamaño compartan geometría y sorteos y las comparaciones sean pareadas.
    """
    inst = name.rsplit("_i", 1)[1]
    return f"{n_robots}|{n_tasks}|{inst}".encode()


# ── especificación del catálogo ───────────────────────────────────────────────

def catalog_cells():
    """(bloque, R, L, win, reg, q, n_instancias). El bloque A es el control."""
    # A — escalado con el equipo, a iso-dificultad. CONTROL del catálogo.
    for R in range(2, 11):
        for reg in ("det", "est"):
            yield ("A", R, 6, "E", reg, "q1", 4)
    # B — forma de la ventana. `E` (la referencia) vive en el bloque A.
    #     P aísla el PLAZO sin espera; C lo escalona; A lo suprime.
    for win in ("P", "C", "A"):
        for R in (2, 5, 10):
            for reg in ("det", "est"):
                yield ("B", R, 6, win, reg, "q1", 4)
    # C — carga por robot. L=6 (la referencia) vive en el bloque A.
    for Lc in (3, 4, 8):
        for R in (2, 5, 10):
            for reg in ("det", "est"):
                yield ("C", R, Lc, "E", reg, "q1", 4)
    # D — coaliciones. q1 (la referencia) vive en el bloque A.
    #     n se ajusta para que Σ q_j = L·R se mantenga.
    for q in ("q2", "qm"):
        for R in (3, 6, 10):
            for reg in ("det", "est"):
                yield ("D", R, 6, "E", reg, q, 6)
    # E — gradiente de incertidumbre. Único bloque sin mitad determinista:
    #     el régimen determinista es ρ=1.0, un extremo del propio gradiente.
    for reg in ("det", "lev", "est", "fue"):
        for R in (2, 5, 10):
            yield ("E", R, 6, "E", reg, "q1", 6)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    written, by_block, by_regime = 0, {}, {}

    for (block, R, Lc, win, reg, q, n_inst) in catalog_cells():
        _, mean_workers = COALITIONS[q]
        n_tasks = max(N_GROUPS, int(round(Lc * R / mean_workers)))

        for i in range(1, n_inst + 1):
            name = (f"cv3_{block}_r{R:02d}_n{n_tasks:03d}"
                    f"_bnd_{win}_{reg}_{q}_i{i}")
            path = os.path.join(OUTPUT_DIR, name + ".yaml")
            with open(path, "w") as fh:
                fh.write(build_scenario(name, R, n_tasks, win, reg, q))
            written += 1
            by_block[block] = by_block.get(block, 0) + 1
            key = (block, "det" if reg == "det" else "est")
            by_regime[key] = by_regime.get(key, 0) + 1

    with open(os.path.join(OUTPUT_DIR, "experiment_config.yaml"), "w") as fh:
        fh.write("solvers: ['random', 'greedy', 'cbaa', 'cbba', 'dec-mcts-v4-g9999']\n"
                 "reward_functions: ['reward00']\n"
                 f"replicas: {REPLICAS}\n")

    print(f"Catálogo v3 escrito en {os.path.normpath(OUTPUT_DIR)}")
    print(f"  {'bloque':<8}{'esc.':>6}{'det':>6}{'est':>6}")
    for b in sorted(by_block):
        print(f"  {b:<8}{by_block[b]:>6}{by_regime.get((b,'det'),0):>6}"
              f"{by_regime.get((b,'est'),0):>6}")
    print(f"  {'TOTAL':<8}{written:>6}"
          f"{sum(v for k, v in by_regime.items() if k[1]=='det'):>6}"
          f"{sum(v for k, v in by_regime.items() if k[1]=='est'):>6}")


if __name__ == "__main__":
    main()
