# 03 — Escenarios, tandas de experimentos y diagnóstico

Métrica reportada en todo el proyecto: **`final_reward` = fracción de tareas completadas**
(`reward00` con $k_1=1$).

## Familias de escenarios

### `scenarios/bundles/` (208 ficheros) — generador `scripts/generate_bundle_scenarios.py`

Geometría fija: nodo 1 en (0,0) con la única estación (todos los robots parten de ahí) y
4 "bundles" (racimos) en (0,4), (4,0), (0,−4), (−4,0), con las tareas repartidas en un
círculo unidad alrededor de cada centro. Grafo completo. Batería 100, velocidad 1,
consumo de navegación 1. Nombre: `scenario_{stype}{wtype}_{n}t_{r}r`.

**`stype` — incertidumbre:**
- `1` determinista: `success_prob=1`, `success_time=[10,0]`, `demand=[10,0]`.
- `2` estocástico fijo: `prob=0.75`, `success_time=[10,10]`, `fail_time=[0,0]`, `demand=[10,10]`.
- `3` prob. variable por racimo: 1.0 / 0.75 / 0.50 / 0.25.
- `4` prob. descendente dentro de cada racimo.

**`wtype` — ventanas y coaliciones:**
- `A`: ventana `[0, MAX_TIME]`, 1 worker.
- `B`: igual que A pero los racimos superior e inferior requieren **2 workers**.
- `C`: ventana escalada por racimo (¼, ½, ¾, 1 de MAX_TIME).
- `D`: ventana creciente dentro del racimo.
- `E`: ventana **desplazada aleatoriamente**: `[t0, t0+10]` con `t0 ~ U(0, MAX_TIME)`.

Combinaciones generadas: `1A 1B 1C 1D 1E 2A 2B 2C 2D 3A 3C 4A 4D` × {12,16,20,24} tareas ×
{2,3,4,5} robots = 208. `MAX_TIME = {12:20, 16:30, 20:40, 24:50}`.

### `scenarios/random/` (45) — `scripts/generate_random_scenarios.py`
15 escenarios base (5×10t, 5×15t, 5×20t) × {3,4,5} robots. Mapa 200×200, coordenadas
aleatorias en [−100,100]², semilla 2025.

### `scenarios/salomon/` (56)
Instancias derivadas de los benchmarks de Solomon (c101…, 10 robots, 100 tareas).
**No hay script generador en `scripts/` ni logs asociados** — origen y estado sin confirmar.

### `data/` (17 ficheros)
Copia de trabajo del ejecutor: los **16 escenarios `scenario_1E_*`** + `experiment_config.yaml`.
Configuración actual: `solvers: [random, greedy, cbaa, cbba, dec-mcts-v4-g9999]`,
`reward_functions: [reward00]`, `replicas: 3`.

## Validez de las tandas de logs

⚠️ **Bug corregido**: hasta el `2026-06-10` (código), commit `466190a` del `2026-06-23`, una
tarea `ASSIGNED` **no caducaba**: el compromiso de los robots la protegía y permitía completar
tareas iniciadas fuera de su ventana. Todo log anterior a la corrección está **inflado**.

| Directorio | Fecha | Runs | Válido | Contenido |
|---|---|---|---|---|
| `logs/prueba` | 06-04 → 06-10 | 321 | ❌ **NO** | Mezcla pre/post-fix. Origen de `results*.csv` y `resultado*.csv` |
| `logs/ablacion_blockprob` | 06-04 | 96 | ❌ NO | Ablación `blockingProb` (variantes `bp-*`, ya borradas de `main.cpp`) |
| `logs/ablacion_bpcap` | 06-04 | 288 | ❌ NO | Ablación del cap de `blockingProb` (0.5…0.9 vs successProb) |
| `logs/fixed_sim` | 06-10 | 448 | ✅ SÍ | 16 escenarios `1E` × 4 réplicas. Comparativa de versiones v1–v4 |
| `logs/eval_bundles` | 06-22 | 1040 | ✅ SÍ | **208 escenarios bundles × 1 réplica × 5 solvers** |
| `logs/eval_random` | 06-22 | 675 | ✅ SÍ | 45 escenarios random × 3 réplicas × 5 solvers |
| `logs/eval_tricky` | 06-22 | 25 | ✅ SÍ | 1 escenario `scenario-tricky.yaml` × 5 réplicas |
| `logs/eval_hc` | 06-23 | 60 | ✅ SÍ | Alto cómputo (1200/12000 iters) en 12 escenarios |
| `logs/eval_rounds` | 06-23 | 108 | ✅ SÍ | Efecto de las rondas de comunicación: `r300`/`r3000`/`r9000` |

Los `.csv` de `logs/` (`results*.csv`, `resultado*.csv`) proceden **todos de `logs/prueba`** ⇒
**no usar**. Los `.mp4` son visualizaciones sueltas (borrar en la limpieza).

## Resultados (solo logs válidos)

### `logs/eval_1E` (sesión 3) — familia 1E completa, código ya corregido

16 escenarios (`1E_{12,16,20,24}t_{2,3,4,5}r`) × 9 solvers × **10 réplicas** = 1440 runs.
Familia determinista con **ventanas estrechas y desplazadas al azar** (`[t0, t0+10]`,
`t0 ~ U(0,MAX_TIME)`), 1 worker por tarea.

| Solver | media | EE |
|---|---|---|
| **dec-mcts-v4** | **0.6184** | 0.015 |
| dec-mcts-v4-g9999 | 0.6179 | 0.015 |
| dec-mcts-v1 | 0.6124 | 0.015 |
| cbba | 0.6089 | 0.014 |
| dec-mcts-v2 | 0.6074 | 0.015 |
| dec-mcts-v3 | 0.5817 | 0.014 |
| cbaa | 0.5349 | 0.012 |
| greedy | 0.3812 | 0.012 |
| random | 0.3524 | 0.011 |

Comparación **pareada por escenario** de `v4-g9999`: vs cbba **+0.009 ± 0.010** (gana en 8,
pierde en 4 de 16 → **empate estadístico**); vs cbaa **+0.083 ± 0.034**; vs greedy
**+0.237 ± 0.031** (gana en 15 de 16).

**El hallazgo importante — la ventaja escala con el número de robots:**

| robots | Δ vs cbba | Δ vs cbaa | Δ vs greedy |
|---|---|---|---|
| 2r | −0.007 ± 0.026 | −0.018 ± 0.024 | +0.121 ± 0.038 |
| 3r | −0.001 ± 0.002 | +0.012 ± 0.013 | +0.229 ± 0.021 |
| 4r | **+0.020 ± 0.023** | +0.096 ± 0.058 | +0.316 ± 0.025 |
| 5r | **+0.024 ± 0.021** | +0.242 ± 0.069 | +0.281 ± 0.097 |

Tendencia monótona y limpia: con 2 robots Dec-MCTS no aporta nada sobre las pujas; a partir
de 4-5 robots despega. Es la firma de que **la presión de coordinación es el eje que
discrimina**, y sugiere que el catálogo debería llegar a 6-10 robots (hoy el generador se
detiene en 5). Ver objetivo 1.2.

**Validación cruzada del pipeline**: comparando con `logs/fixed_sim` (misma familia, código
anterior a las correcciones), los solvers **no modificados** reproducen su valor con ±0.007
(random −0.007, greedy +0.000, cbaa +0.002, cbba +0.000), mientras que v1 +0.071, v2 +0.043
y v3 +0.057. v4 (+0.011) y v4-g9999 (+0.001) apenas cambian, coherente con que el retiro
prematuro era un fenómeno del **régimen estocástico** y 1E es determinista.

### `logs/eval_versiones_fix2` (sesión 3) — **ESTADO ACTUAL DE REFERENCIA**

Mismo diseño que `eval_versiones` (21 escenarios × 9 solvers × 5 réplicas), con los
defectos A1–A4 corregidos en los cuatro solvers Dec-MCTS.

| Solver | global | DET (1A,1B,1E) | EST (2A,2B,3C,4D) | escenarios ganados /21 |
|---|---|---|---|---|
| **cbba** | **0.483** | **0.557** | **0.427** | 11 |
| dec-mcts-v4-g9999 | 0.463 | 0.551 | 0.396 | 6 |
| dec-mcts-v1 | 0.461 | 0.545 | 0.398 | 5 |
| dec-mcts-v4 | 0.458 | 0.552 | 0.388 | — |
| dec-mcts-v3 | 0.454 | 0.544 | 0.386 | 4 |
| dec-mcts-v2 | 0.451 | 0.547 | 0.379 | 4 |
| cbaa | 0.433 | 0.489 | 0.392 | 8 |
| greedy | 0.406 | 0.444 | 0.377 | 4 |
| random | 0.294 | 0.331 | 0.267 | 0 |

Por familia (mejor marcado con `*`):

| Familia | greedy | cbaa | cbba | v1 | v2 | v3 | v4-g9999 |
|---|---|---|---|---|---|---|---|
| 1A | 0.618 | 0.571 | 0.604 | 0.596 | 0.599 | 0.601 | **0.624\*** |
| 1B | 0.403 | 0.431 | 0.492 | 0.493 | 0.497 | **0.499\*** | 0.496 |
| 1E | 0.312 | 0.465 | **0.576\*** | 0.547 | 0.546 | 0.533 | 0.535 |
| 2A | 0.501 | 0.472 | **0.542\*** | 0.479 | 0.483 | 0.497 | 0.468 |
| 2B | 0.368 | 0.378 | **0.411\*** | 0.403 | 0.372 | 0.379 | 0.392 |
| 3C | 0.317 | 0.356 | **0.394\*** | 0.372 | 0.332 | 0.336 | 0.357 |
| 4D | 0.322 | 0.361 | 0.360 | 0.336 | 0.329 | 0.333 | **0.368\*** |

**Lecturas:**
1. El hundimiento en régimen estocástico **era un bug**, no una limitación del método: tras
   corregirlo, Dec-MCTS pasa de 0.314 a 0.396 en EST y supera a cbaa y greedy.
2. **Las cuatro versiones quedan estadísticamente empatadas** (0.451–0.463). Los defectos
   pesaban más que las diferencias de diseño entre ellas ⇒ para cerrar el solver, elegir por
   criterios cualitativos (v4 es la más refinada y la mejor en determinista).
3. **cbba sigue siendo el mejor solver global.** La brecha restante (−0.020 global, −0.031 en
   EST) ya no es atribuible a un fallo de implementación.
4. Ruido de referencia medido sobre los solvers **no modificados** entre dos tandas idénticas:
   ≈ ±0.016. Cualquier diferencia menor no es interpretable.

### Tandas intermedias
- `logs/eval_versiones` — línea base **antes** de las correcciones (sesión 2).
- `logs/eval_versiones_fix` — ⚠️ **descartada**: primera versión de A1, con poda demasiado
  agresiva que destruía subárboles (regresión en determinista). Conservada solo como
  evidencia de esa lección.

### `logs/eval_versiones` (sesión 2, 2026-08-06) — **la tanda de referencia para el objetivo 1.1**

21 escenarios: familias `1A 1B 1E 2A 2B 3C 4D` × `{12t_2r, 16t_3r, 24t_5r}`, 9 solvers,
**5 réplicas** = 945 runs. Compilado con `-O3`.

| Solver | global | **DET (1A,1B,1E)** | **EST (2A,2B,3C,4D)** | s/run |
|---|---|---|---|---|
| cbba | 0.4716 | **0.559** | 0.406 | 0.00 |
| cbaa | 0.4466 | 0.488 | **0.416** | 0.00 |
| dec-mcts-v2 | 0.4315 | 0.513 | 0.370 | 1.69 |
| dec-mcts-v4-g9999 | 0.4165 | 0.553 | 0.314 | 1.92 |
| dec-mcts-v1 | 0.4149 | 0.503 | 0.349 | 1.54 |
| dec-mcts-v4 | 0.4133 | 0.551 | 0.310 | 1.94 |
| greedy | 0.4089 | 0.444 | 0.382 | 0.00 |
| dec-mcts-v3 | 0.3970 | 0.517 | 0.307 | 1.87 |
| random | 0.3105 | 0.358 | 0.275 | 0.00 |

**Lecturas clave:**
1. El ranking **se invierte según el régimen**. En determinista v4 empata con cbba y gana a
   cbaa/greedy por 6–11 puntos. En estocástico las **cuatro** versiones pierden incluso
   contra `greedy`.
2. **v4 no es la mejor versión**: su supremacía anterior era un artefacto de medirla solo en
   la familia `1E`, que es determinista. En estocástico el orden es v2 > v1 > v4g > v4 > v3.
3. Diagnóstico conductual (medido sobre los logs, ver `05_dudas.md` D-BUG-01):

| Régimen | solver | tareas libres al retirarse | batería % | t_retiro | % tareas intentadas |
|---|---|---|---|---|---|
| DET | v2 / v4g | 0.25 / 0.15 | 59 / 54 | 50.3 / 51.1 | 0.51 / 0.55 |
| EST | cbba | 1.03 | 46 | 42.4 | 0.61 |
| EST | v2 | 0.45 | 59 | 45.5 | 0.52 |
| EST | **v4-g9999** | **5.12** | **61** | **35.2** | **0.44** |

v3/v4 **abandonan la misión con un tercio de las tareas todavía disponibles y dos tercios de
la batería intacta**. Es un defecto de la regla de decisión, no un problema de calibración.


### `logs/fixed_sim` — 16 escenarios `1E`, 4 réplicas

| Solver | n | media | sd |
|---|---|---|---|
| **dec-mcts-v4-g9999** | 64 | **0.6171** | 0.194 |
| cbba | 64 | 0.6089 | 0.181 |
| dec-mcts-v4 (γ=0.999) | 64 | 0.6069 | 0.189 |
| dec-mcts-v4-c035 | 32 | 0.6023 | 0.183 |
| dec-mcts-v2 | 64 | 0.5641 | 0.193 |
| dec-mcts-v1 | 32 | 0.5418 | 0.173 |
| cbaa | 32 | 0.5333 | 0.150 |
| dec-mcts-v3 | 32 | 0.5245 | 0.159 |
| greedy | 32 | 0.3812 | 0.156 |
| random | 32 | 0.3596 | 0.113 |

Conclusiones: v4 > v2 > v1 > v3; γ=0.9999 > γ=0.999; C=0.7 > C=0.35. La ventaja de
`v4-g9999` sobre `cbba` es de **+0.008** (nada concluyente).

### `logs/eval_random` — 45 escenarios aleatorios, 3 réplicas — **Dec-MCTS gana claramente**

| Solver | media |
|---|---|
| **dec-mcts-v4-g9999** | **0.5033** |
| cbaa | 0.4498 |
| cbba | 0.4030 |
| greedy | 0.3967 |
| random | 0.2251 |

### `logs/eval_bundles` — 208 escenarios, 1 réplica — **Dec-MCTS pierde en el agregado**

| Solver | media |
|---|---|
| cbba | 0.5080 |
| cbaa | 0.4866 |
| greedy | 0.4585 |
| dec-mcts-v4-g9999 | 0.4465 |
| random | 0.3588 |

### 🔑 DIAGNÓSTICO CLAVE — desglose de `eval_bundles` por familia

| Familia | random | greedy | cbaa | cbba | **dec-mcts** |
|---|---|---|---|---|---|
| 1A | 0.523 | 0.652 | 0.617 | 0.652 | **0.653** |
| 1B | 0.208 | 0.389 | 0.483 | 0.521 | **0.530** |
| 1C | 0.424 | 0.514 | 0.493 | 0.526 | **0.557** |
| 1D | 0.436 | 0.535 | 0.548 | 0.550 | **0.590** |
| 1E | 0.358 | 0.381 | 0.533 | 0.609 | **0.619** |
| 2A | 0.412 | 0.532 | 0.556 | **0.576** | 0.385 |
| 2B | 0.154 | 0.333 | **0.445** | 0.430 | 0.358 |
| 2C | 0.365 | 0.473 | 0.458 | **0.485** | 0.318 |
| 2D | 0.386 | 0.472 | 0.449 | **0.472** | 0.368 |
| 3A | 0.441 | 0.478 | **0.495** | 0.463 | 0.402 |
| 3C | 0.277 | 0.335 | 0.410 | **0.433** | 0.304 |
| 4A | 0.396 | 0.480 | 0.437 | **0.511** | 0.409 |
| 4D | 0.284 | 0.387 | 0.402 | **0.403** | 0.311 |

**Dec-MCTS gana en las 5 familias deterministas (1x) y pierde en las 8 estocásticas (2x/3x/4x),
sin una sola excepción.** Este es el patrón más informativo de todo el proyecto y el punto de
ataque prioritario del objetivo 1.1. Ver hipótesis H-01…H-05 en `05_dudas.md`.

Comprobación descartada: **no es mortalidad de robots** (0% de robots muertos en ambos
regímenes). Es puramente que completa menos tareas (0.357 vs 0.468 de cbba en estocástico).

Desgloses secundarios (`eval_bundles`): Dec-MCTS mejora al crecer nº de tareas (0.404 con 12t
→ 0.475 con 24t) y nº de robots (0.297 con 2r → 0.587 con 5r), pero cbba también, y mantiene
la ventaja en todos los cortes agregados.

### `logs/eval_tricky` — `scenario-tricky.yaml`, 5 réplicas
cbaa 0.9375 > cbba 0.750 > **dec-mcts 0.650** > greedy 0.5625 > random 0.200.
Escenario diseñado como "trampa"; Dec-MCTS no lo resuelve.

### `logs/eval_hc` — alto cómputo (1200 iters/latido, 12000 de emergencia), 12 escenarios
cbba 0.5056 > cbaa 0.4979 > **dec-mcts-v4-g9999-hc 0.4688** > greedy 0.4597 > random 0.3660.
**Multiplicar por 40 el cómputo NO cierra la brecha** ⇒ el problema no es presupuesto de
simulación, es sesgo del modelo o de la heurística.

### `logs/eval_rounds` — rondas de comunicación (12 escenarios, 3 réplicas)
`r300` 0.4122 · `r3000` 0.4257 · `r9000` 0.4117. **Más rondas de comunicación tampoco ayudan.**

### Ablaciones sobre `blockingProb` (⚠️ logs pre-fix, no válidos)
`bp-sp` (=successProb) 0.6600 vs `bp-1` 0.6491; con cap 0.5–0.9 todo entre 0.6547 y 0.6613.
Conclusión provisional: **el valor de $p^{blq}_{MAX}$ es casi irrelevante**. Conviene repetir
la ablación con el simulador corregido si se quiere citar en la memoria (el cap. 2 lo presenta
como "hiperparámetro que conviene optimizar").

### `logs/eval_c1_mix` y `logs/eval_c1_1E` (sesión 3) — **ablación C1: ¿aporta el canal de comunicación?**

`dec-mcts-v4-nocomm` es idéntico a `dec-mcts-v4-g9999` salvo que **ignora las distribuciones
comunicadas** por los vecinos: en los rollouts, los demás robots dejan de seguir su bundle
muestreado y se rigen por la política de rollout, y `blockingProb` queda a 0 (sin
de-conflicción). Todo lo demás —árbol, D-UCT, física, publicación de la propia distribución—
es idéntico. 10 réplicas en ambos conjuntos.

| Conjunto | con comm | sin comm | Δ pareada | gana comm |
|---|---|---|---|---|
| MIX 21 esc. (global) | 0.455 | 0.441 | +0.013 ± 0.012 | 9/21 |
| ‣ MIX determinista (1x) | 0.552 | 0.514 | **+0.038 ± 0.019** | 5/9 |
| ‣ MIX estocástico (2-4x) | 0.382 | 0.387 | **−0.005 ± 0.013** | 4/12 |
| **1E completa (16 esc.)** | **0.619** | **0.553** | **+0.066 ± 0.019** | 10/16 |

Efecto por nº de robots en 1E: 2r **+0.004** · 3r **+0.019** · 4r **+0.174** · 5r **+0.065**.
En MIX: 2r +0.005 · 3r +0.005 · 5r +0.030.

**Conclusiones (importantes):**
1. **El canal de comunicación SÍ aporta**, y es lo que coloca a Dec-MCTS por encima de cbba
   en 1E: sin él cae a 0.553, por debajo de cbba (0.609) y casi al nivel de cbaa (0.535).
   Queda **refutada** la sospecha de que la coordinación fuese decorativa.
2. **Su valor depende del régimen**: +0.038 en determinista, **−0.005 (nulo) en estocástico**.
   Interpretación: un plan comunicado solo vale lo que valga su predictibilidad. Bajo
   incertidumbre el plan del vecino se desvía en cuanto una tarea falla, así que condicionar
   sobre él aporta sesgo en lugar de información. Ahí es donde vive la brecha que queda
   frente a cbba.
3. **Su valor depende del tamaño del equipo**: nulo con 2-3 robots, máximo con 4-5.
4. **Por eso el conjunto MIX subestimaba el efecto**: 2 de sus 3 tamaños tienen 2-3 robots y
   4 de sus 7 familias son estocásticas. La medición sobre 1E completa (con los 4 tamaños de
   equipo) lo revela con claridad. Lección metodológica: el diseño del conjunto de prueba
   determinó la conclusión.

## 🎯 Criterios para el diseño del catálogo definitivo (objetivo 1.2)

> Evidencia acumulada sobre **qué hace que un escenario discrimine** entre solvers. Consultar
> este apartado antes de generar o descartar escenarios. Ver también D-02 en `05_dudas.md`.

### C-1 · El eje que más discrimina es la PRESIÓN DE COORDINACIÓN (nº de robots)

Medido en `logs/eval_1E` (16 escenarios × 10 réplicas), Δ pareada de `dec-mcts-v4-g9999`:

| robots | Δ vs cbba | Δ vs cbaa | Δ vs greedy |
|---|---|---|---|
| 2r | −0.007 ± 0.026 | −0.018 ± 0.024 | +0.121 ± 0.038 |
| 3r | −0.001 ± 0.002 | +0.012 ± 0.013 | +0.229 ± 0.021 |
| 4r | +0.020 ± 0.023 | +0.096 ± 0.058 | +0.316 ± 0.025 |
| 5r | +0.024 ± 0.021 | +0.242 ± 0.069 | +0.281 ± 0.097 |

Tendencia monótona: **con 2 robots Dec-MCTS no aporta nada** sobre las pujas (gana cbaa); a
partir de 4-5 robots despega. cbaa se desmorona al crecer el equipo (0.576 con 5r frente a
0.794 de cbba), lo esperable de una subasta de tarea única sin anticipación.
⇒ **El generador se detiene en 5 robots, justo donde empieza a separarse. Extenderlo a
6-10 robots es la acción de mayor valor esperado para el catálogo definitivo.**

### C-2 · El nº de tareas apenas discrimina
En `logs/eval_1E`, el ranking es prácticamente el mismo con 12, 16, 20 y 24 tareas. Sirve para
variar la dificultad absoluta, no para separar algoritmos. No merece la pena gastar muchas
celdas del catálogo en este eje.

### C-3 · Las ventanas estrechas y desplazadas (tipo 1E) favorecen la anticipación
1E (`[t0, t0+10]` con `t0 ~ U(0,MAX_TIME)`) es la familia donde Dec-MCTS lidera y donde
greedy se hunde (0.381 frente a 0.618): obliga a planificar *cuándo*, no solo *qué*. Las
ventanas anchas (tipo A) igualan a todos por arriba y las escaladas por racimo (C/D) quedan
en medio.

### C-4 · El régimen estocástico sigue siendo terreno de las pujas
Tras corregir los defectos, Dec-MCTS ya supera a cbaa y greedy en estocástico, pero **cbba
mantiene ventaja** (0.427 vs 0.396). Un catálogo compuesto solo de familias estocásticas
enterraría la hipótesis del trabajo; uno compuesto solo de deterministas sería poco honesto.
Hay que representar ambos regímenes y **reportarlos por separado**: el mensaje real es que
el régimen cambia el ranking.

### C-7 · La comunicación solo rinde con equipos grandes y planes predecibles
Ablación C1 (arriba): el aporte del canal es **nulo con 2-3 robots y en régimen estocástico**,
y grande con 4-5 robots en régimen determinista (+0.174 con 4r en 1E). Como la comunicación
es *la* aportación conceptual de Dec-MCTS frente a las pujas, **un catálogo cargado de
escenarios de 2-3 robots o mayoritariamente estocásticos hace estructuralmente imposible que
Dec-MCTS destaque**. Es la explicación de por qué las tandas anteriores no discriminaban.

### C-5 · Cuidado con los escenarios de 2 robots
Con 2 robots todos los solvers rinden mal (0.19–0.35) y las diferencias son ruido. Aportan
poca información y mucha varianza.

### C-6 · Potencia estadística
Con 5 réplicas el ruido entre dos tandas idénticas es ≈ ±0.016, del orden de las diferencias
que queremos detectar. **Con 10 réplicas los solvers no modificados reproducen su valor con
±0.007.** Usar ≥10 réplicas en la tanda definitiva.

## Herramientas de análisis

- `analyze_ablation.py <logs_dir>` — tabla media/mediana/sd por solver (tiene una sección
  comentada al final con diagnósticos H1/H4 y la tabla por escenario está a medias).
- `compare_v4.py <logs_dir>` — comparación por escenario entre versiones de Dec-MCTS.
- `mrtau metrics -i logs/ -o results.csv` — herramienta externa del proyecto; parsea `.log` a CSV.
- `mrtau video -i test.log -o simulacion.mp4` — vídeo de una ejecución.

**Pendiente (objetivo 1.3)**: sustituir estos scripts sueltos por un notebook de Jupyter con
el análisis por criterios (nº robots, nº tareas, dificultad, distribución).
