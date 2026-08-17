# CLAUDE.md — TFM: MRTA Distribuido bajo Incertidumbre (DEC-MRTAU)

## Antes de nada

Lee `.claude-notes/README.md` y sigue el protocolo de sesión que describe. En resumen:
al empezar, leer `.claude-notes/01_contexto.md`, las últimas entradas de
`.claude-notes/04_bitacora.md` y `.claude-notes/05_dudas.md`; al terminar, actualizarlos.

`.claude-notes/` es el cuaderno de trabajo del asistente y **no forma parte de la entrega**.

## Qué es este proyecto

TFM de Manuel Olías (tutor: Ignacio Pérez-Hurtado). Resuelve la **asignación de tareas
multi-robot bajo incertidumbre** con tiempos continuos, coaliciones, ventanas de ejecución y
restricciones de batería, pasando del modelo **centralizado** del tutor a un paradigma
**puramente distribuido** (DEC-POGSMDP: cada robot decide con información local + comunicación).

Cinco solvers a comparar: `random`, `greedy`, `cbaa`, `cbba` y **`dec-mcts`** (v1–v4).
**La hipótesis del trabajo es que Dec-MCTS debe ser el mejor.** Se cumple
**condicionalmente**: gana cuando hay presión de coordinación (≥4 robots) y los planes son
predecibles (régimen determinista, ventanas estrechas), y la ventaja procede efectivamente
del canal de comunicación. No gana con equipos de 2-3 robots ni en régimen estocástico.
El trabajo actual es **diseñar un catálogo de escenarios que represente honestamente el
problema y deje que esa ventaja se manifieste** (ver `.claude-notes/03_experimentos.md`,
sección de criterios de diseño).

## Cómo se ejecuta

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make      # ejecutable: simulador
./simulador <dirEscenarios> <dirLogs> [configExperimento]    # todos los args son opcionales
```

`experiment_config.yaml` define `solvers`, `reward_functions` y `replicas`. El ejecutor es
**idempotente**: salta los `.log` ya completos, así que se puede reanudar una tanda.
Compilar en Release importa (×3 de velocidad); el número de iteraciones de MCTS es fijo, así
que optimizar **no altera los resultados**, solo el `computing_time`.

Métrica reportada: `final_reward` = **fracción de tareas completadas** (`reward00` con k1=1).

Análisis: `analyze_ablation.py <dirLogs>` y `compare_v4.py <dirLogs>`; la herramienta externa
`mrtau metrics -i logs/ -o results.csv` convierte logs a CSV.

## Mapa del repositorio

| Ruta | Contenido |
|---|---|
| `src/` | `main.cpp` (ejecutor), `simulator.cpp` (motor de eventos), `scenario/state/logger` |
| `include/tau/` | Cabeceras y **todos los solvers** (header-only) |
| `data/` | Escenarios de la tanda actual + `experiment_config.yaml` |
| `scenarios/` | Catálogos: `bundles/` (208), `random/` (45), `salomon/` (56) |
| `scripts/` | Generadores de escenarios en Python |
| `logs/` | Tandas de experimentos (⚠️ no todas son válidas, ver notas) |
| `memoria/` | Proyecto LaTeX. Caps. 2 y 3 completos; 1, 4, 5, 6, 7 pendientes |

## Reglas de trabajo (impuestas por el usuario)

1. **Identificar el propósito** de un fichero, clase o variable **antes** de tocarlo. Ningún
   cambio puede perder información relevante.
2. **Preguntar, no dar nada por sentado** sobre intención, arquitectura o requisitos.
3. **Señalar explícitamente las dudas** antes de seguir adelante. Admitir lo que no se sabe.
4. Se agradecen las sugerencias de mejora, especialmente las de impacto duradero.
5. **Ceñirse a los objetivos y peticiones**: nada de trabajo de más sin preguntar antes.

### Sobre la memoria

- **Caps. 2 (`02_marco_teórico.tex`) y 3 (`03_estado_del_arte.tex`) están terminados: NO
  modificarlos sin preguntar antes.**
- Redacción en **LaTeX y español**, registro académico formal y riguroso, reutilizando los
  nombres de variables ya definidos, con `\ref{}` a las secciones previas y citas adecuadas.
- El usuario no compila LaTeX localmente: importa el contenido, no que compile.
- Reparto: cap. 4 = materiales/arquitectura/framework · cap. 5 = escenarios · cap. 6 =
  resultados. Hay un briefing detallado para el cap. 4 en
  `memoria/notas_cap4_materiales_y_metodos.md`.

## Avisos importantes

- ⚠️ **Logs inválidos**: `logs/prueba`, `logs/ablacion_blockprob` y `logs/ablacion_bpcap` son
  anteriores a la corrección del bug de caducidad (commit `466190a`) y están inflados. Los
  `*.csv` de `logs/` proceden todos de `logs/prueba` ⇒ **no usarlos**.
- ⚠️ **Dec-MCTS lleva dentro una réplica del simulador** (`makeState`/`makeEventQueue`/
  `physics*`). Si se toca la física de `simulator.cpp`, hay que replicar el cambio en los
  cuatro solvers o las estimaciones quedan sesgadas. **Divergencia viva**: v1, v2 y v3 aún
  modelan la semántica *antigua* de caducidad (una tarea `ASSIGNED` no caducaba); solo v4
  replica la corregida. Ver `.claude-notes/02_codigo.md` y la duda D-06.
- ⚠️ **Al medir, usar ≥10 réplicas**: con 5, el ruido entre dos tandas idénticas (±0.016) es
  del orden de las diferencias que se quieren detectar. Validar siempre con los solvers no
  modificados, que deben reproducir su valor.
- ⚠️ Los resultados dependen mucho del **régimen del escenario** (determinista vs estocástico).
  No sacar conclusiones sobre un solver midiendo en una sola familia.
- No hay semilla configurable (`std::random_device` en todas partes): promediar réplicas.
