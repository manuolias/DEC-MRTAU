#!/usr/bin/env python3
"""
Análisis del CATÁLOGO v2 (`logs/eval_catalogo_v2`).

Estructura del catálogo (ver `scripts/generate_catalog_v2.py`):
  A  control · escalado con el equipo   R = 2…10, L=6, ventana E, q1
  B  ventana                            win ∈ {P, C, A}   (E vive en A)
  C  carga por robot                    L ∈ {3, 4, 8}     (L=6 vive en A)
  D  coaliciones                        q ∈ {q2, qm}      (q1 vive en A)
  E  gradiente de incertidumbre         ρ ∈ {1.0, .9, .75, .5}

Unidad de análisis: el ESCENARIO (media de sus 3 réplicas). Las Δ son pareadas y el
error estándar se calcula ENTRE ESCENARIOS, nunca entre réplicas.

Uso:  python3 scripts/analyze_catalog_v2.py [csv]
"""

import collections
import csv
import math
import statistics as st
import sys

CSV = sys.argv[1] if len(sys.argv) > 1 else "analisis/catalogo_v2.csv"
DEC = "dec-mcts-v4-g9999"
SOL = ["random", "greedy", "cbaa", "cbba", DEC]
SH = {"random": "rand", "greedy": "greedy", "cbaa": "cbaa", "cbba": "cbba", DEC: "decmcts"}
WIN = {"E": "estrecha (espera+plazo)", "P": "solo plazo (sin espera)",
       "C": "plazo escalonado", "A": "ancha"}
REGN = {"det": "ρ=1.00 σ=0", "lev": "ρ=0.90 σ=3", "est": "ρ=0.75 σ=10", "fue": "ρ=0.50 σ=10"}


def ee(xs):
    return st.stdev(xs) / math.sqrt(len(xs)) if len(xs) > 1 else 0.0


def slope(xs, ys):
    """Pendiente OLS y su estadístico t."""
    n = len(xs)
    mx, my = st.mean(xs), st.mean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    a = my - b * mx
    resid = [y - (a + b * x) for x, y in zip(xs, ys)]
    se = math.sqrt(sum(r * r for r in resid) / (n - 2) / sxx) if n > 2 else float("nan")
    return b, (b / se if se else float("nan"))


def load():
    per = collections.defaultdict(lambda: collections.defaultdict(list))
    extra = collections.defaultdict(lambda: collections.defaultdict(dict))
    meta = {}
    for r in csv.DictReader(open(CSV)):
        e = r["escenario"]
        if not r["bloque"]:
            continue
        per[e][r["solver"]].append(float(r["final_reward"]))
        d = extra[e][r["solver"]]
        for k in ("failed_agents", "tareas_intentadas", "tasa_exito",
                  "sum_robot_travel_distance", "computing_time", "max_robot_makespan"):
            if r[k] not in ("", "nan"):
                d.setdefault(k, []).append(float(r[k]))
        meta[e] = dict(b=r["bloque"], R=int(r["robots"]), n=int(r["tareas"]),
                       win=r["ventana"], reg=r["regimen"], q=r["coalicion"],
                       i=int(r["instancia"]))
    sc = {e: {s: st.mean(v) for s, v in d.items()} for e, d in per.items()}
    ex = {e: {s: {k: st.mean(v) for k, v in kk.items()} for s, kk in d.items()}
          for e, d in extra.items()}
    return sc, ex, meta


def main():
    sc, ex, meta = load()
    esc = sorted(e for e in sc if all(s in sc[e] for s in SOL))
    skipped = len(sc) - len(esc)
    load_of = lambda e: round(sum_workers(e) / meta[e]["R"])
    sum_workers = lambda e: meta[e]["n"] * (1 if meta[e]["q"] == "q1" else 2)

    hdr = (f"{'celda':<34}{'#':>4}  {'rand':>6}{'greedy':>7}{'cbaa':>6}{'cbba':>6}"
           f"{'decmcts':>8}{'Δ dec−cbba':>15}  {'W/E/P':>8}  mejor")

    def line(tag, E, w=34):
        if not E:
            return None
        m = {s: st.mean(sc[e][s] for e in E) for s in SOL}
        d = [sc[e][DEC] - sc[e]["cbba"] for e in E]
        wn = sum(1 for x in d if x > 1e-9)
        p = sum(1 for x in d if x < -1e-9)
        best = max(SOL, key=lambda s: m[s])
        print(f"{tag:<{w}}{len(E):>4}  {m['random']:>6.3f}{m['greedy']:>7.3f}{m['cbaa']:>6.3f}"
              f"{m['cbba']:>6.3f}{m[DEC]:>8.3f}{st.mean(d):>+10.3f} ±{ee(d):.3f}"
              f"{f'{wn}/{len(d)-wn-p}/{p}':>10}  {'★' if best == DEC else ' '}{SH[best]}")
        return st.mean(d), ee(d), best

    sel = lambda **kw: [e for e in esc if all(meta[e][k] == v for k, v in kw.items())]

    print("=" * 130)
    print("CATÁLOGO v2 — 360 escenarios × 3 réplicas × 5 solvers.  Batería 40 (recarga obligatoria) en TODOS.")
    print(f"Escenarios completos: {len(esc)}" + (f"  ⚠️ {skipped} incompletos, excluidos" if skipped else ""))
    print("=" * 130)

    print("\n### PANORAMA\n" + hdr)
    line("catálogo completo", esc)
    for reg in ("det", "lev", "est", "fue"):
        line(f"  régimen {reg}  ({REGN[reg]})", sel(reg=reg))

    print("\n### POR BLOQUE\n" + hdr)
    names = {"A": "A · equipo (control)", "B": "B · ventana", "C": "C · carga",
             "D": "D · coaliciones", "E": "E · incertidumbre"}
    for b in "ABCDE":
        line(names[b], sel(b=b))
        for reg in ("det", "lev", "est", "fue"):
            E = sel(b=b, reg=reg)
            if E:
                line(f"    {reg}", E)

    # ── A: la curva frente a R ────────────────────────────────────────────────
    print("\n" + "=" * 130)
    print("BLOQUE A — escalado con el equipo a iso-dificultad (L=6, ventana E, q1)")
    print("=" * 130)
    for reg in ("det", "est"):
        print(f"\n{reg}:")
        print(f"{'R':>4}" + "".join(f"{R:>8}" for R in range(2, 11)))
        for s in SOL:
            print(f"{SH[s]:>4}" + "".join(
                f"{st.mean(sc[e][s] for e in sel(b='A', reg=reg, R=R)):>8.3f}"
                for R in range(2, 11)))
        ds = [(R, st.mean(sc[e][DEC] - sc[e]["cbba"] for e in sel(b="A", reg=reg, R=R)))
              for R in range(2, 11)]
        print(f"{'Δ':>4}" + "".join(f"{d:>+8.3f}" for _, d in ds))
        print("  pendiente por robot: " + "  ".join(
            f"{SH[s]} {slope([meta[e]['R'] for e in sel(b='A', reg=reg)], [sc[e][s] for e in sel(b='A', reg=reg)])[0]:+.4f}"
            f" (t={slope([meta[e]['R'] for e in sel(b='A', reg=reg)], [sc[e][s] for e in sel(b='A', reg=reg)])[1]:.1f})"
            for s in ("cbba", DEC)))
        b_, t_ = slope([meta[e]["R"] for e in sel(b="A", reg=reg)],
                       [sc[e][DEC] - sc[e]["cbba"] for e in sel(b="A", reg=reg)])
        print(f"  pendiente de la Δ:   {b_:+.4f} por robot (t = {t_:.1f})")

    # ── B: ventana, pareado contra el control A ───────────────────────────────
    print("\n" + "=" * 130)
    print("BLOQUE B — forma de la ventana.  `E` es el control (bloque A, R ∈ {2,5,10}).")
    print("Pareado exacto: mismas semillas ⇒ misma geometría y mismos sorteos que el control.")
    print("=" * 130)
    ctrl = {(meta[e]["R"], meta[e]["reg"], meta[e]["i"]): e
            for e in sel(b="A") if meta[e]["R"] in (2, 5, 10) and meta[e]["i"] <= 4}
    for reg in ("det", "est"):
        print(f"\n### ventana · {reg}\n" + hdr)
        line("  E · estrecha (control)", [e for e in ctrl.values() if meta[e]["reg"] == reg])
        for w in ("P", "C", "A"):
            E = sel(b="B", win=w, reg=reg)
            line(f"  {w} · {WIN[w]}", E)
            dd = [(sc[e][DEC] - sc[e]["cbba"]) -
                  (sc[c][DEC] - sc[c]["cbba"])
                  for e in E if (c := ctrl.get((meta[e]["R"], reg, meta[e]["i"])))]
            if dd:
                star = "*" if abs(st.mean(dd)) > 2 * ee(dd) else " "
                print(f"{'      efecto vs control:':<34}{st.mean(dd):>+42.3f} ±{ee(dd):.3f}{star}")
        for w in ("P", "C", "A"):
            for R in (2, 5, 10):
                line(f"      {w} · R={R}", sel(b="B", win=w, reg=reg, R=R))

    # ── C: carga ──────────────────────────────────────────────────────────────
    print("\n" + "=" * 130)
    print("BLOQUE C — carga por robot.  L=6 es el control (bloque A).  ⚠️ n cambia ⇒ no hay pareado exacto.")
    print("=" * 130)
    for reg in ("det", "est"):
        print(f"\n{reg} — Δ dec−cbba:")
        print(f"{'R \\ L':>7}" + "".join(f"{L:>16}" for L in (3, 4, 6, 8)))
        for R in (2, 5, 10):
            row = []
            for L in (3, 4, 6, 8):
                E = ([e for e in sel(b="A", reg=reg, R=R) if meta[e]["i"] <= 4] if L == 6
                     else [e for e in sel(b="C", reg=reg, R=R) if meta[e]["n"] == L * R])
                d = [sc[e][DEC] - sc[e]["cbba"] for e in E]
                row.append(f"{st.mean(d):>+9.3f} ±{ee(d):.3f}" if d else " " * 16)
            print(f"{R:>7}" + "".join(f"{c:>16}" for c in row))

    # ── D: coaliciones ────────────────────────────────────────────────────────
    print("\n" + "=" * 130)
    print("BLOQUE D — coaliciones.  q1 es el control (bloque A, R ∈ {3,6,10}).")
    print("=" * 130)
    for reg in ("det", "est"):
        print(f"\n### coaliciones · {reg}\n" + hdr)
        line("  q1 · 1 trabajador (control)",
             [e for e in sel(b="A", reg=reg) if meta[e]["R"] in (3, 6, 10)])
        for q in ("q2", "qm"):
            line(f"  {q} · " + ("2 trabajadores" if q == "q2" else "mezcla 1-2-3"),
                 sel(b="D", q=q, reg=reg))
            for R in (3, 6, 10):
                line(f"      R={R}", sel(b="D", q=q, reg=reg, R=R))

    # ── E: gradiente ──────────────────────────────────────────────────────────
    print("\n" + "=" * 130)
    print("BLOQUE E — gradiente de incertidumbre (L=6, ventana E, q1, R ∈ {2,5,10}).")
    print("⚠️ Único bloque sin mitad determinista: `det` ES ρ=1.0, un extremo del propio gradiente.")
    print("=" * 130)
    print("\n" + hdr)
    for reg in ("det", "lev", "est", "fue"):
        line(f"  {REGN[reg]}", sel(b="E", reg=reg))
        for R in (2, 5, 10):
            line(f"      R={R}", sel(b="E", reg=reg, R=R))

    # ── diagnóstico de la batería ─────────────────────────────────────────────
    print("\n" + "=" * 130)
    print("DIAGNÓSTICO DE LA RESTRICCIÓN DE BATERÍA (nueva en v2)")
    print("=" * 130)
    print("⚠️ La mortalidad de robots es un modo de fallo distinto de la mala asignación:")
    print("   al comparar con random/greedy hay que decir cuánto de su desventaja es esto.\n")
    print(f"{'solver':<10}{'robots muertos/run':>20}{'% runs con bajas':>18}"
          f"{'tasa de éxito':>15}{'distancia':>11}{'s/run':>9}")
    for s in SOL:
        fa = [ex[e][s].get("failed_agents", 0) for e in esc]
        print(f"{SH[s]:<10}{st.mean(fa):>20.3f}{100*sum(1 for x in fa if x>0)/len(fa):>17.1f}%"
              f"{st.mean(ex[e][s].get('tasa_exito', float('nan')) for e in esc if 'tasa_exito' in ex[e][s]):>15.3f}"
              f"{st.mean(ex[e][s].get('sum_robot_travel_distance', 0) for e in esc):>11.1f}"
              f"{st.mean(ex[e][s].get('computing_time', 0) for e in esc):>9.2f}")

    # ── descomposición intento × éxito ────────────────────────────────────────
    print("\n" + "=" * 130)
    print("MECANISMO — recompensa = tasa de intento × tasa de éxito  (cbba vs dec-mcts)")
    print("=" * 130)
    print(f"{'corte':<24}{'int.cbba':>10}{'int.dec':>9}{'Δint':>8}   "
          f"{'éx.cbba':>9}{'éx.dec':>8}{'Δéx':>8}   {'Δreward':>9}")
    def mech(tag, E):
        if not E:
            return
        f = lambda s, k: st.mean(ex[e][s][k] for e in E if k in ex[e][s])
        ic = f("cbba", "tareas_intentadas") / st.mean(meta[e]["n"] for e in E)
        idc = f(DEC, "tareas_intentadas") / st.mean(meta[e]["n"] for e in E)
        ec, ed = f("cbba", "tasa_exito"), f(DEC, "tasa_exito")
        dr = st.mean(sc[e][DEC] - sc[e]["cbba"] for e in E)
        print(f"{tag:<24}{ic:>10.3f}{idc:>9.3f}{idc-ic:>+8.3f}   "
              f"{ec:>9.3f}{ed:>8.3f}{ed-ec:>+8.3f}   {dr:>+9.3f}")
    for reg in ("det", "lev", "est", "fue"):
        mech(f"régimen {reg}", sel(reg=reg))
    for b in "ABCDE":
        mech(f"bloque {b}", sel(b=b))
    mech("ventana P (sin espera)", sel(b="B", win="P"))
    mech("ventana E (control)", [e for e in sel(b="A") if meta[e]["R"] in (2, 5, 10)])


if __name__ == "__main__":
    main()
