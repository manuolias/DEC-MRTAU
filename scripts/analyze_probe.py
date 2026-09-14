#!/usr/bin/env python3
"""
Análisis de la sonda de variantes de diseño (`logs/eval_probe`).

Responde a tres preguntas, por este orden:
  1. ¿En cuántas variantes GANA Dec-MCTS (mejor de los cinco / por encima de CBBA)?
  2. ¿Qué eje de diseño mueve la ventaja, y en qué dirección?
  3. ¿Cuál es el suelo de ruido, para saber qué diferencias son interpretables?

Uso:  python3 scripts/analyze_probe.py [csv]
"""

import collections
import csv
import math
import re
import statistics as st
import sys

CSV = sys.argv[1] if len(sys.argv) > 1 else "analisis/probe.csv"
DEC = "dec-mcts-v4-g9999"
SOL = ["random", "greedy", "cbaa", "cbba", DEC]
SHORT = {"random": "rand", "greedy": "greedy", "cbaa": "cbaa", "cbba": "cbba", DEC: "decmcts"}

ORDER = ["w_base", "w_ancha", "w_plazo", "w_espera", "w_escalon", "w_libre", "w_dura",
         "f_igual", "f_doble",
         "b_navbarata", "b_navcara", "b_escasa", "b_navcara_escasa",
         "h_duracion", "h_capacidad", "h_disperso", "c_q2"]
LABEL = {
    "w_base":  "ventana E (espera+plazo)  ★REF", "w_ancha": "ventana ancha [0,T]",
    "w_plazo": "solo plazo  [0,t0+10]",          "w_espera": "solo espera [t0,T]",
    "w_escalon": "plazo escalonado/racimo",      "w_libre": "apertura y cierre aleatorios",
    "w_dura":  "ventana dura (anchura 5)",
    "f_igual": "fracaso = éxito (10)",           "f_doble": "fracaso = 2x éxito (20)",
    "b_navbarata": "navegar barato (rate .25)",  "b_navcara": "navegar caro (rate 4)",
    "b_escasa": "batería escasa (40)",           "b_navcara_escasa": "nav caro + bat escasa",
    "h_duracion": "duraciones 5/10/15",          "h_capacidad": "capacidades heterogéneas",
    "h_disperso": "robots dispersos",            "c_q2": "coalición q=2  (control)",
}
RE_NAME = re.compile(r"prb_(?P<var>.+)_r(?P<R>\d+)_n(?P<n>\d+)_(?P<reg>det|est)_i(?P<i>\d)$")


def load():
    per = collections.defaultdict(lambda: collections.defaultdict(list))
    meta = {}
    for r in csv.DictReader(open(CSV)):
        m = RE_NAME.match(r["escenario"])
        if not m:
            continue
        e = r["escenario"]
        meta[e] = (m["var"], int(m["R"]), m["reg"], int(m["i"]))
        per[e][r["solver"]].append(float(r["final_reward"]))
    return {e: {s: st.mean(v) for s, v in d.items()} for e, d in per.items()}, meta


def ee(xs):
    return st.stdev(xs) / math.sqrt(len(xs)) if len(xs) > 1 else 0.0


def main():
    sc, meta = load()
    esc = sorted(sc)
    cells = lambda **kw: [e for e in esc if all(
        meta[e][i] == v for i, v in
        [(0, kw.get("var")), (1, kw.get("R")), (2, kw.get("reg"))] if v is not None)]

    missing = [e for e in esc if any(s not in sc[e] for s in SOL)]
    if missing:
        print(f"⚠️  {len(missing)} escenarios incompletos, se excluyen\n")
        esc = [e for e in esc if e not in missing]

    # ── 1. Tabla principal ────────────────────────────────────────────────────
    def block(title, reg):
        print(f"\n### {title}")
        print(f"{'variante':<20}{'descripción':<32}{'rand':>6}{'greedy':>7}{'cbaa':>6}"
              f"{'cbba':>6}{'decmcts':>8}{'Δ dec−cbba':>16}  {'W/E/P':>7}  mejor")
        rows = []
        for var in ORDER:
            E = [e for e in cells(var=var, reg=reg) if e in esc]
            if not E:
                continue
            m = {s: st.mean(sc[e][s] for e in E) for s in SOL}
            d = [sc[e][DEC] - sc[e]["cbba"] for e in E]
            w = sum(1 for x in d if x > 1e-9)
            p = sum(1 for x in d if x < -1e-9)
            best = max(SOL, key=lambda s: m[s])
            mark = "★" if best == DEC else " "
            print(f"{var:<20}{LABEL[var]:<32}{m['random']:>6.3f}{m['greedy']:>7.3f}"
                  f"{m['cbaa']:>6.3f}{m['cbba']:>6.3f}{m[DEC]:>8.3f}"
                  f"{st.mean(d):>+11.3f} ±{ee(d):.3f}{f'{w}/{len(d)-w-p}/{p}':>9}  {mark}{SHORT[best]}")
            rows.append((var, st.mean(d), ee(d), best, m))
        return rows

    print("=" * 132)
    print("SONDA DE VARIANTES DE DISEÑO — logs/eval_probe")
    print("12 escenarios por variante (2 R × 2 reg × 3 inst) × 3 réplicas. Δ pareada por escenario, EE entre escenarios.")
    print("=" * 132)
    det = block("RÉGIMEN DETERMINISTA (6 escenarios por variante)", "det")
    est = block("RÉGIMEN ESTOCÁSTICO (6 escenarios por variante)", "est")
    tot = block("AGREGADO det+est (12 escenarios por variante)", None)

    # ── 2. Recuento: ¿dónde gana Dec-MCTS? ────────────────────────────────────
    print("\n" + "=" * 132)
    print("RECUENTO — ¿en cuántas variantes gana Dec-MCTS?")
    print("=" * 132)
    for tag, rows in (("determinista", det), ("estocástico", est), ("agregado", tot)):
        best = [v for v, _, _, b, _ in rows if b == DEC]
        over = [v for v, d, _, _, _ in rows if d > 0]
        sig = [v for v, d, s, _, _ in rows if d > 2 * s and s > 0]
        print(f"\n{tag.upper():<14} (de {len(rows)} variantes)")
        print(f"  mejor de los 5 .......... {len(best):>2}/{len(rows)}   {', '.join(best) or '—'}")
        print(f"  por encima de CBBA ...... {len(over):>2}/{len(rows)}   {', '.join(over) or '—'}")
        print(f"  ... y con |Δ| > 2·EE .... {len(sig):>2}/{len(rows)}   {', '.join(sig) or '—'}")

    # ── 3. Efecto de cada variante SOBRE la ventaja (dif-en-dif vs w_base) ────
    print("\n" + "=" * 132)
    print("EFECTO DE CADA EJE SOBRE LA VENTAJA DE DEC-MCTS  (Δ_variante − Δ_referencia, pareado por celda)")
    print("Las celdas comparten semilla ⇒ misma geometría y mismos sorteos; el pareado es exacto.")
    print("=" * 132)
    print(f"{'variante':<20}{'descripción':<32}{'det':>18}{'est':>18}{'ambos':>18}")
    base = {meta[e][1:]: sc[e][DEC] - sc[e]["cbba"] for e in esc if meta[e][0] == "w_base"}
    for var in ORDER:
        if var == "w_base":
            continue
        out = []
        for reg in ("det", "est", None):
            E = [e for e in cells(var=var, reg=reg) if e in esc and meta[e][1:] in base]
            if not E:
                out.append("       —      ")
                continue
            dd = [(sc[e][DEC] - sc[e]["cbba"]) - base[meta[e][1:]] for e in E]
            star = "*" if abs(st.mean(dd)) > 2 * ee(dd) and ee(dd) > 0 else " "
            out.append(f"{st.mean(dd):>+9.3f} ±{ee(dd):.3f}{star}")
        print(f"{var:<20}{LABEL[var]:<32}" + "".join(f"{o:>18}" for o in out))
    print("\n(* = |efecto| > 2·EE. c_q2 cambia también el nº de tareas ⇒ su pareado no es exacto.)")

    # ── 4. Suelo de ruido ─────────────────────────────────────────────────────
    print("\n" + "=" * 132)
    print("SUELO DE RUIDO")
    print("=" * 132)
    print("En determinista p=1: f_igual y f_doble son configuraciones IDÉNTICAS a w_base.")
    print("Su discrepancia mide el ruido de réplica, no un efecto real.\n")
    for var in ("f_igual", "f_doble"):
        E = [e for e in cells(var=var, reg="det") if e in esc and meta[e][1:] in base]
        for s in SOL:
            ref = {meta[e][1:]: sc[e][s] for e in esc if meta[e][0] == "w_base" and meta[e][2] == "det"}
            d = [sc[e][s] - ref[meta[e][1:]] for e in E if meta[e][1:] in ref]
            print(f"  {var:<10} {SHORT[s]:<8} Δ = {st.mean(d):+.4f} ±{ee(d):.4f}   "
                  f"(máx |Δ| por escenario: {max(abs(x) for x in d):.4f})")
        print()

    # ── 5. Desglose por tamaño de equipo ──────────────────────────────────────
    print("=" * 132)
    print("Δ dec−cbba POR TAMAÑO DE EQUIPO  (¿la ventaja depende de R en cada variante?)")
    print("=" * 132)
    print(f"{'variante':<20}{'det R=4':>16}{'det R=8':>16}{'est R=4':>16}{'est R=8':>16}")
    for var in ORDER:
        out = []
        for reg in ("det", "est"):
            for R in (4, 8):
                E = [e for e in cells(var=var, reg=reg, R=R) if e in esc]
                d = [sc[e][DEC] - sc[e]["cbba"] for e in E]
                out.append(f"{st.mean(d):>+9.3f} ±{ee(d):.3f}" if d else "     —    ")
        print(f"{var:<20}" + "".join(f"{o:>16}" for o in out))


if __name__ == "__main__":
    main()
