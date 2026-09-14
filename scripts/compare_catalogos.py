#!/usr/bin/env python3
"""
Comparativa pareada entre dos catálogos con la MISMA estructura de celdas.

Pensado para medir el efecto de un cambio de parametrización (p. ej. v2 → v3, que
fija el coste energético del fracaso en 10 y baja σ a 0/1/3/5) sobre los resultados,
sin volver a discutir el diseño del catálogo: la estructura de bloques, las semillas
y por tanto la geometría y los plazos son idénticos, así que cada escenario del uno
tiene su homólogo exacto en el otro.

El emparejamiento se hace por el nombre del escenario SIN el prefijo del catálogo,
de modo que `cv2_A_r05_n030_bnd_E_est_q1_i1` se compara contra
`cv3_A_r05_n030_bnd_E_est_q1_i1`.

Unidad de análisis: el escenario (media de sus réplicas). Las Δ entre catálogos son
pareadas por escenario y su error estándar se calcula entre escenarios.

⚠️ Control interno: los escenarios DETERMINISTAS deben salir estadísticamente
   iguales en los dos catálogos (con ρ=1 no hay fracasos y σ=0 en ambos), así que su
   Δ mide el ruido de muestreo, no el efecto del cambio. Es la vara con la que juzgar
   si las diferencias de los regímenes estocásticos son sustanciales.

Uso:  python3 scripts/compare_catalogos.py [csv_antiguo] [csv_nuevo]
"""

import collections
import csv
import math
import statistics as st
import sys

CSV_A = sys.argv[1] if len(sys.argv) > 1 else "analisis/catalogo_v2.csv"
CSV_B = sys.argv[2] if len(sys.argv) > 2 else "analisis/catalogo_v3.csv"
ETQ_A, ETQ_B = "v2", "v3"

DEC = "dec-mcts-v4-g9999"
SOL = ["random", "greedy", "cbaa", "cbba", DEC]
SH = {"random": "rand", "greedy": "greedy", "cbaa": "cbaa", "cbba": "cbba", DEC: "decmcts"}
DERIV = ("tasa_exito", "tareas_intentadas", "failed_agents", "sum_robot_travel_distance")


def ee(xs):
    return st.stdev(xs) / math.sqrt(len(xs)) if len(xs) > 1 else 0.0


def slope(xs, ys):
    """Pendiente OLS y su estadístico t."""
    n = len(xs)
    mx, my = st.mean(xs), st.mean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    if not sxx or n < 3:
        return float("nan"), float("nan")
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    a = my - b * mx
    resid = [y - (a + b * x) for x, y in zip(xs, ys)]
    se = math.sqrt(sum(r * r for r in resid) / (n - 2) / sxx)
    return b, (b / se if se else float("nan"))


def load(path):
    """Devuelve (recompensa, métricas derivadas, factores) indexado por clave sin prefijo."""
    per = collections.defaultdict(lambda: collections.defaultdict(list))
    der = collections.defaultdict(lambda: collections.defaultdict(lambda: collections.defaultdict(list)))
    meta = {}
    for r in csv.DictReader(open(path)):
        if not r["bloque"]:
            continue
        clave = r["escenario"].split("_", 1)[1]        # quita `cv2_` / `cv3_`
        per[clave][r["solver"]].append(float(r["final_reward"]))
        for k in DERIV:
            if r.get(k) not in ("", "nan", None):
                der[clave][r["solver"]][k].append(float(r[k]))
        meta[clave] = dict(b=r["bloque"], R=int(r["robots"]), n=int(r["tareas"]),
                           win=r["ventana"], reg=r["regimen"], q=r["coalicion"])
    sc = {e: {s: st.mean(v) for s, v in d.items()} for e, d in per.items()}
    dd = {e: {s: {k: st.mean(v) for k, v in kk.items()} for s, kk in d.items()}
          for e, d in der.items()}
    return sc, dd, meta


A, DA, MA = load(CSV_A)
B, DB, MB = load(CSV_B)
COM = sorted(e for e in A if e in B
             and all(s in A[e] for s in SOL) and all(s in B[e] for s in SOL))
sel = lambda **kw: [e for e in COM if all(MA[e][k] == v for k, v in kw.items())]

W = 132
print("=" * W)
print(f"COMPARATIVA {ETQ_A} → {ETQ_B}   ·   {len(COM)} escenarios emparejados   ·   pareado por escenario")
print(f"  {ETQ_A}: {CSV_A}")
print(f"  {ETQ_B}: {CSV_B}")
print("  Cambio: coste de un intento fallido constante (10 en todos los regímenes)")
print("          y dispersión de la duración σ = 0/1/3/5  (antes 0/3/10/10)")
print("=" * W)

# ── 1 · control: el determinista no debería moverse ───────────────────────────
print("\n### 1 · CONTROL — los 162 escenarios DETERMINISTAS son equivalentes en ambos catálogos")
print("    (ρ=1 ⇒ no hay fracasos que cobrar, y σ=0 en los dos).  Su Δ es puro ruido de muestreo")
print("    y sirve de vara de medir para todo lo demás.\n")
det = sel(reg="det")
print(f"{'solver':<10}{ETQ_A:>9}{ETQ_B:>9}{'Δ pareada':>20}{'|Δ| máx':>10}")
ruido = {}
for s in SOL:
    d = [B[e][s] - A[e][s] for e in det]
    ruido[s] = ee(d)
    print(f"{SH[s]:<10}{st.mean(A[e][s] for e in det):>9.4f}{st.mean(B[e][s] for e in det):>9.4f}"
          f"{st.mean(d):>+14.4f} ±{ee(d):.4f}{max(abs(x) for x in d):>10.3f}")

# ── 2 · efecto global y por régimen ───────────────────────────────────────────
print("\n\n### 2 · EFECTO DEL CAMBIO SOBRE CADA SOLVER  (Δ = " + ETQ_B + " − " + ETQ_A + ", pareada)\n")
hdr = f"{'conjunto':<26}{'#':>4}" + "".join(f"{SH[s]:>17}" for s in SOL)
print(hdr)
for tag, E in [("catálogo completo", COM)] + [(f"  régimen {r}", sel(reg=r))
                                              for r in ("det", "lev", "est", "fue")]:
    if not E:
        continue
    cel = []
    for s in SOL:
        d = [B[e][s] - A[e][s] for e in E]
        marca = "*" if abs(st.mean(d)) > 2 * ee(d) else " "
        cel.append(f"{st.mean(d):>+10.4f}±{ee(d):.4f}{marca}")
    print(f"{tag:<26}{len(E):>4}" + "".join(f"{c:>17}" for c in cel))
print("\n  * la diferencia supera dos errores estándar")

print(f"\n\n### 3 · NIVELES ABSOLUTOS  ({ETQ_A} → {ETQ_B})\n")
print(f"{'conjunto':<26}{'#':>4}" + "".join(f"{SH[s]:>17}" for s in SOL))
for tag, E in [("catálogo completo", COM)] + [(f"  régimen {r}", sel(reg=r))
                                              for r in ("det", "lev", "est", "fue")]:
    if not E:
        continue
    cel = [f"{st.mean(A[e][s] for e in E):.3f}→{st.mean(B[e][s] for e in E):.3f}" for s in SOL]
    print(f"{tag:<26}{len(E):>4}" + "".join(f"{c:>17}" for c in cel))

# ── 4 · la cifra que sostiene el capítulo 6 ───────────────────────────────────
print("\n\n### 4 · LA Δ QUE IMPORTA:  dec-mcts − cbba,  en cada catálogo\n")
print(f"{'conjunto':<26}{'#':>4}{ETQ_A + ' Δ':>19}{ETQ_B + ' Δ':>19}"
      f"{'cambio de la Δ':>20}{ETQ_A + ' G/E/P':>13}{ETQ_B + ' G/E/P':>13}")


def wep(d):
    g = sum(1 for x in d if x > 1e-9)
    p = sum(1 for x in d if x < -1e-9)
    return f"{g}/{len(d)-g-p}/{p}"


def fila(tag, E):
    if not E:
        return
    da = [A[e][DEC] - A[e]["cbba"] for e in E]
    db = [B[e][DEC] - B[e]["cbba"] for e in E]
    dd = [x - y for x, y in zip(db, da)]
    marca = "*" if abs(st.mean(dd)) > 2 * ee(dd) else " "
    print(f"{tag:<26}{len(E):>4}"
          f"{st.mean(da):>+13.4f}±{ee(da):.4f}{st.mean(db):>+13.4f}±{ee(db):.4f}"
          f"{st.mean(dd):>+13.4f}±{ee(dd):.4f}{marca}{wep(da):>12}{wep(db):>13}")


fila("catálogo completo", COM)
for r in ("det", "lev", "est", "fue"):
    fila(f"  régimen {r}", sel(reg=r))
print()
nom = {"A": "A · equipo (control)", "B": "B · ventana", "C": "C · carga",
       "D": "D · coaliciones", "E": "E · incertidumbre"}
for b in "ABCDE":
    fila(nom[b], sel(b=b))
    for r in ("det", "est"):
        fila(f"    {r}", sel(b=b, reg=r))

# ── 5 · ¿cambia el ranking? ───────────────────────────────────────────────────
print("\n\n### 5 · ¿CAMBIA EL ORDEN DE LOS SOLVERS?\n")
print(f"{'conjunto':<26}{'#':>4}   {'mejor ' + ETQ_A:<14}{'mejor ' + ETQ_B:<14}  orden " + ETQ_B)
for tag, E in ([("catálogo completo", COM)]
               + [(f"  régimen {r}", sel(reg=r)) for r in ("det", "lev", "est", "fue")]
               + [(nom[b], sel(b=b)) for b in "ABCDE"]):
    if not E:
        continue
    ma = {s: st.mean(A[e][s] for e in E) for s in SOL}
    mb = {s: st.mean(B[e][s] for e in E) for s in SOL}
    ba, bb = max(SOL, key=lambda s: ma[s]), max(SOL, key=lambda s: mb[s])
    orden = " > ".join(SH[s] for s in sorted(SOL, key=lambda s: -mb[s]))
    print(f"{tag:<26}{len(E):>4}   {SH[ba]:<14}{SH[bb]:<14}{'' if ba == bb else '⚠ '}{orden}")

# ── 6 · bloque A: la pendiente frente al tamaño del equipo ────────────────────
print("\n\n### 6 · BLOQUE A — pendiente de la recompensa frente al nº de robots\n")
print("    Es la formulación más limpia de la tesis del trabajo: ¿convierte cada método")
print("    robots adicionales en rendimiento?\n")
print(f"{'régimen':<10}{'solver':<10}{ETQ_A + ' pendiente':>22}{ETQ_B + ' pendiente':>22}")
for reg in ("det", "est"):
    E = sel(b="A", reg=reg)
    Rs = [MA[e]["R"] for e in E]
    for s in ("cbba", DEC):
        ba, ta = slope(Rs, [A[e][s] for e in E])
        bb, tb = slope(Rs, [B[e][s] for e in E])
        print(f"{reg:<10}{SH[s]:<10}{ba:>+15.4f} (t={ta:>5.1f}){bb:>+15.4f} (t={tb:>5.1f})")

# ── 7 · mecanismo: intento, éxito, mortalidad, distancia ──────────────────────
print("\n\n### 7 · MÉTRICAS DERIVADAS  (cbba vs dec-mcts, " + ETQ_A + " → " + ETQ_B + ")\n")
for k in DERIV:
    print(f"  {k}")
    print(f"    {'régimen':<10}" + "".join(f"{SH[s]:>26}" for s in ("cbba", DEC)))
    for reg in ("det", "lev", "est", "fue"):
        E = [e for e in sel(reg=reg)
             if all(k in DA[e].get(s, {}) and k in DB[e].get(s, {}) for s in ("cbba", DEC))]
        if not E:
            continue
        cel = []
        for s in ("cbba", DEC):
            va = st.mean(DA[e][s][k] for e in E)
            vb = st.mean(DB[e][s][k] for e in E)
            cel.append(f"{va:8.3f}→{vb:8.3f}")
        print(f"    {reg:<10}" + "".join(f"{c:>26}" for c in cel))
    print()
