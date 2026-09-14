#!/usr/bin/env python3
"""
Convierte los ficheros .log de una o varias tandas de experimentos en un CSV ordenado
(un registro por ejecución), listo para el notebook de análisis.

    python3 scripts/extract_metrics.py logs/eval_catalogo logs/eval_b4 -o resultados.csv

Solo usa la biblioteca estándar, así que funciona sin instalar nada. Sustituye a la
herramienta externa `mrtau metrics`, cuya salida no incluye las métricas derivadas de
eventos que necesita el análisis (tareas intentadas, instante de retirada).

Columnas
--------
Identificación : tanda, variante, escenario, solver, reward_function, replica
                 (`variante` = subdirectorio dentro de la tanda, vacío si los .log
                  cuelgan directamente de ella; distingue p. ej. r300/r3000/r9000
                  dentro de `logs/eval_rounds`)
Factores       : bloque, robots, tareas, geometria, ventana, regimen, coalicion, instancia
                 (se extraen del nombre del escenario; los catálogos antiguos, con nombres
                  del tipo `scenario_1E_12t_2r`, rellenan lo que pueden y dejan el resto vacío)
Métricas       : todas las líneas `metric:` del log
Derivadas      : tareas_intentadas   nº de tareas distintas que el equipo llegó a ejecutar
                 tasa_intento        tareas_intentadas / tareas
                 tasa_exito          completed_tasks / tareas_intentadas
                 t_retiro_medio      instante medio de `robot_finished`
"""

import argparse
import csv
import os
import re
import sys

# Catálogos v1, v2 y v3: {cat|cv2|cv3}_{bloque}_r{R}_n{n}_{geom}_{win}_{reg}_{q}_i{inst}
# El v2 añade la ventana `P` (solo plazo, sin espera) y baja la batería a 40.
# El v3 reparametriza los regímenes: coste de fracaso constante y σ = 0/1/3/5.
RE_NEW = re.compile(
    r'^(?:cat|cv2|cv3)_(?P<bloque>\w+?)_r(?P<robots>\d+)_n(?P<tareas>\d+)_(?P<geometria>\w{3})_'
    r'(?P<ventana>[A-Z])_(?P<regimen>\w{3})_(?P<coalicion>q\w+)_i(?P<instancia>\d+)$')
# Catálogos antiguos: scenario_{stype}{wtype}_{n}t_{r}r
RE_OLD = re.compile(
    r'^scenario_(?P<familia>\d[A-Z])_(?P<tareas>\d+)t_(?P<robots>\d+)r$')

RE_FILE = re.compile(r'^(?P<escenario>.+)_(?P<solver>[^_]+)_(?P<reward>reward\d+)_(?P<replica>\d+)\.log$')

# Régimen implícito de los catálogos antiguos: el primer dígito de la familia
OLD_REGIME = {'1': 'det', '2': 'est', '3': 'est', '4': 'est'}
OLD_WINDOW = {'A': 'A', 'B': 'E', 'C': 'C', 'D': 'C', 'E': 'E'}


def parse_scenario(name):
    """Extrae los factores del nombre del escenario. Devuelve un dict con claves fijas."""
    out = dict(bloque='', robots='', tareas='', geometria='', ventana='',
               regimen='', coalicion='', instancia='')
    m = RE_NEW.match(name)
    if m:
        out.update(m.groupdict())
        return out
    m = RE_OLD.match(name)
    if m:
        fam = m.group('familia')
        out.update(bloque=fam, robots=m.group('robots'), tareas=m.group('tareas'),
                   geometria='bnd', ventana=OLD_WINDOW.get(fam[1], ''),
                   regimen=OLD_REGIME.get(fam[0], ''),
                   coalicion='q2' if fam[1] == 'B' else 'q1', instancia='1')
    return out


def parse_log(path):
    """Lee un .log y devuelve (metricas, derivadas). None si está incompleto."""
    metrics, attempted, finish_times = {}, set(), []
    for line in open(path, errors='replace'):
        if line.startswith('metric: '):
            try:
                key, val = line[8:].split('; value: ')
                metrics[key.strip()] = float(val)
            except ValueError:
                continue
        elif line.startswith('event: task_execution'):
            attempted.add(line.split('task: ')[1].split(';')[0].strip())
        elif line.startswith('event: robot_finished'):
            finish_times.append(float(line.split('timestamp: ')[1].split(';')[0]))
    if 'computing_time' not in metrics:      # ejecución truncada
        return None
    derived = dict(
        tareas_intentadas=len(attempted),
        t_retiro_medio=round(sum(finish_times) / len(finish_times), 4) if finish_times else '')
    return metrics, derived


METRIC_COLS = ['final_reward', 'completed_tasks', 'failed_tasks', 'pending_tasks',
               'available_agents', 'failed_agents', 'finished_agents',
               'average_robot_makespan', 'sd_robot_makespan',
               'max_robot_makespan', 'min_robot_makespan',
               'average_robot_travel_distance', 'sum_robot_travel_distance',
               'computing_time']
FACTOR_COLS = ['bloque', 'robots', 'tareas', 'geometria', 'ventana',
               'regimen', 'coalicion', 'instancia']
DERIVED_COLS = ['tareas_intentadas', 'tasa_intento', 'tasa_exito', 't_retiro_medio']


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('dirs', nargs='+', help='directorios de logs (se recorren recursivamente)')
    ap.add_argument('-o', '--output', default='resultados.csv')
    args = ap.parse_args()

    rows, skipped = [], 0
    for d in args.dirs:
        tanda = os.path.basename(os.path.normpath(d))
        for root, _, files in os.walk(d):
            for fn in sorted(files):
                if not fn.endswith('.log'):
                    continue
                m = RE_FILE.match(fn)
                if not m:
                    skipped += 1
                    continue
                parsed = parse_log(os.path.join(root, fn))
                if parsed is None:
                    skipped += 1
                    continue
                metrics, derived = parsed

                rel = os.path.relpath(root, d)
                row = dict(tanda=tanda, variante='' if rel == '.' else rel,
                           escenario=m.group('escenario'),
                           solver=m.group('solver'), reward_function=m.group('reward'),
                           replica=int(m.group('replica')))
                row.update(parse_scenario(m.group('escenario')))
                row.update({k: metrics.get(k, '') for k in METRIC_COLS})
                row.update(derived)

                n_tasks = float(row['tareas']) if row['tareas'] else 0.0
                att = derived['tareas_intentadas']
                row['tasa_intento'] = round(att / n_tasks, 6) if n_tasks else ''
                row['tasa_exito'] = round(metrics.get('completed_tasks', 0) / att, 6) if att else ''
                rows.append(row)

    cols = (['tanda', 'variante', 'escenario', 'solver', 'reward_function', 'replica']
            + FACTOR_COLS + METRIC_COLS + DERIVED_COLS)
    with open(args.output, 'w', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)

    print(f"{len(rows)} ejecuciones -> {args.output}"
          + (f"   ({skipped} ficheros omitidos: incompletos o con nombre no reconocido)"
             if skipped else ""))
    if not rows:
        sys.exit(1)


if __name__ == '__main__':
    main()
