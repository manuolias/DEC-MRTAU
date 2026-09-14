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

### `scenarios/salomon/` (56) — ❌ **DESCARTADOS (sesión 4)**
Instancias derivadas de los benchmarks de Solomon (c101…, 10 robots, 100 tareas, horizonte
≈1200). **No hay script generador en `scripts/`.** Se planteó usarlos como validación externa
sobre un benchmark reconocido (bloque F) y el usuario lo **descartó**: coste medido de
**425 s/run** con Dec-MCTS (≈3.5 h la tanda mínima) para confirmar un rendimiento bajo ya
conocido — sonda de 1 réplica sobre `c101`: cbba 0.41 vs dec-mcts 0.34.

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

**El "hallazgo" que resultó ser un ARTEFACTO** — ⚠️ **REFUTADO en la sesión 4, ver D-07.**
En esta familia el nº de tareas es fijo y `MAX_TIME` crece con él, así que *más robots*
significa *menos carga por robot*: lo que se lee abajo como "presión de coordinación" era en
realidad **holgura**. A iso-dificultad la tendencia no existe. Se conserva como documentación
del error, que es material para la lección metodológica del cap. 6.

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

## ⚠️ CATÁLOGO v1 — `scenarios/catalogo/` — **OBSOLETO desde la sesión 6**

> **Sustituido por el catálogo v2** (sección anterior). Se conserva porque la comparación
> v1↔v2 **es** la lección metodológica del cap. 6: dos catálogos defendibles que dan
> conclusiones opuestas sobre el mismo código. Pero **ninguna cifra de esta sección puede
> presentarse como resultado del trabajo**: ni el ranking global, ni la Δ −0.0355, ni la
> pendiente −0.0130/robot, ni la tabla por familias.
>
> Diferencias con el v2: batería 100 (la restricción **nunca se activaba**: 15 robots muertos
> en 14 400 ejecuciones), 10 réplicas, bloques de tamaño muy desigual (90/72/36/36/36/18) y
> sin el eje espera/plazo.

### Texto original (sesión 4, objetivo 1.2)

Generador: `scripts/generate_catalog_scenarios.py`. Lanzador paralelo: `scripts/run_catalog.sh`.
Logs: `logs/eval_catalogo`. Config: `solvers: [random, greedy, cbaa, cbba, dec-mcts-v4-g9999]`,
`replicas: 10`. **288 escenarios × 5 solvers × 10 réplicas = 14 400 runs.**

### El principio de diseño: la carga por robot, no el nº de tareas

La dificultad se controla con **L = Σ_j q_j / R** (plazas de trabajador por robot) y el
horizonte **T = MAX_TIME se mantiene fijo en 40** para todos los escenarios. Esto corrige el
defecto estructural del generador anterior, que ligaba `MAX_TIME` al nº de tareas y por tanto
**confundía el nº de robots con la dificultad**.

**Pilotaje de calibración (2026-08-17, 3 réplicas, ventana E, determinista), `final_reward` de cbba:**

| carga L | R=2 | R=5 | R=10 | lectura |
|---|---|---|---|---|
| 4 (T=50) | 0.750 | 0.850 | 0.975 | el problema se vuelve trivial al crecer el equipo |
| 5 (T=40) | 0.600 | 0.760 | 0.800 | aún crece |
| **6 (T=40)** | **0.667** | **0.667** | **0.683** | **ISO-DIFICULTAD ⇒ punto de operación elegido** |
| 7 (T=40) | 0.500 | 0.571 | 0.629 | vuelve a crecer, y más caro |

Con L=6 el tamaño del equipo deja de estar confundido con la dificultad y puede estudiarse
como factor aislado. Explicación del fenómeno: por debajo de la capacidad, los equipos
grandes se benefician del **multiplexado estadístico** (las fluctuaciones de cuántas ventanas
hay abiertas se promedian mejor con más robots); solo cuando la capacidad temporal es el
cuello de botella el rendimiento se hace invariante de escala.

### Bloques

| Bloque | Qué aísla | Celdas | Esc. |
|---|---|---|---|
| **A1** | nº robots a iso-dificultad | R=2…10 × {det,est} × 5 instancias | 90 |
| **A2** | rejilla robots × holgura | R∈{2,5,10} × L∈{3,4,5,8} × 2 reg × 3 inst | 72 |
| **B** | ventana: ancha (A) / escalonada (C) | R∈{2,5,10} × 2 × 2 reg × 3 inst | 36 |
| **C** | coaliciones: q=2 / mezcla 1-2-3 | R∈{3,6,10} × 2 × 2 reg × 3 inst | 36 |
| **D** | geometría: uniforme / asimétrica | R∈{2,5,10} × 2 × 2 reg × 3 inst | 36 |
| **E** | gradiente de incertidumbre: ρ=0.9 / 0.5 | R∈{2,5,10} × 2 × 3 inst | 18 |

Nomenclatura: `cat_{bloque}_r{R}_n{n}_{geom}_{win}_{reg}_{q}_i{inst}.yaml`. Nº de tareas de
6 a 80. Regímenes: `det` (ρ=1, σ=0), `lev` (0.9, 3), `est` (0.75, 10 — **idéntico al `stype 2`
anterior**, comparabilidad conservada), `fue` (0.5, 10). Semilla por CRC32 del nombre ⇒ el
catálogo se regenera byte a byte.

**Instancias vs réplicas**: la variación entre *instancias* de una misma celda (±0.15) es un
orden de magnitud mayor que el ruido entre *réplicas* (±0.016). Por eso cada celda lleva 3-5
instancias con semillas distintas además de las 10 réplicas.

### Verificación previa (smoke test, 288 esc. × 4 solvers baratos × 2 réplicas, 23 s)

Los 288 ficheros cargan y ejecutan. `final_reward` de cbba por celda:

- **A1 determinista**: 0.583 (R=2) → 0.710 (R=10). **Estocástico**: 0.425 → 0.592. Sin techo
  ni suelo en todo el barrido (el diseño anterior daba 1.000 con R=10).
- **A2**: gradiente limpio de 0.889 (L=3) a 0.479 (L=8). La esquina R=10/L=3 satura a 0.989:
  es el régimen de abundancia y es informativo conservarlo.
- **B**: ventana ancha 0.72–0.83, escalonada 0.53–0.56. Hallazgo colateral: **cbaa se hunde
  con equipos grandes y ventanas anchas** (0.339 con R=10 frente a 0.828 de cbba).
- **E**: degradación monótona con R=10 — det 0.710 → lev 0.678 → est 0.592 → fue 0.436.

### Coste de cómputo (modelo ajustado sobre medidas reales)

`t_run(dec-mcts) ≈ 2.0e-4 · R^1.6 · n · T` segundos (error < 10 % con T=40). Los cuatro
solvers de subasta y los baselines cuestan ~0. Con este modelo, la tanda completa son **5.3 h
en serie / ~1.3 h con 4 procesos**. Memoria pico medida: 12 MB por proceso.
⚠️ En paralelo el `computing_time` **deja de ser comparable** por contención de CPU: la
figura coste-calidad hay que sacarla de una tanda en serie.

Referencias de coste medidas: Solomon (`c101`, 10r/100t, T≈1200) cuesta **425 s/run** con
dec-mcts ⇒ un bloque de validación externa de 6 instancias × 5 réplicas son ~3.5 h.

### 🔴 RESULTADOS de `logs/eval_catalogo` (14 400 runs, 2026-08-17) — LA TANDA DE REFERENCIA

Unidad de análisis: **escenario** (media de sus 10 réplicas); las Δ son **pareadas** y el error
estándar se calcula **entre instancias**.

| Solver | global | det | lev | est | fue |
|---|---|---|---|---|---|
| **cbba** | **0.6031** | **0.666** | **0.611** | **0.553** | **0.404** |
| dec-mcts-v4-g9999 | 0.5676 | 0.661 | 0.582 | 0.489 | 0.341 |
| cbaa | 0.4730 | 0.536 | 0.490 | 0.421 | 0.284 |
| greedy | 0.3894 | 0.433 | 0.364 | 0.357 | 0.246 |
| random | 0.3314 | 0.369 | 0.317 | 0.303 | 0.205 |

**Δ global (dec-mcts − cbba) = −0.0355 ± 0.0034.** Gana en 67 escenarios, empata en 26,
pierde en 195 de 288.

#### A1 — la respuesta a D-07: NO hay ventaja creciente con el nº de robots

| R | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | pendiente |
|---|---|---|---|---|---|---|---|---|---|---|
| **det** Δ | −0.002 | +0.019 | −0.010 | −0.011 | +0.016 | −0.023 | −0.015 | −0.005 | −0.026 | −0.0032/robot (t=−2.4) |
| **est** Δ | −0.032 | −0.037 | −0.047 | −0.065 | −0.097 | −0.109 | −0.106 | −0.110 | −0.131 | **−0.0130/robot (t=−8.4)** |

1. **En determinista y a iso-dificultad, Dec-MCTS empata con cbba** (Δ global −0.006 ± 0.004)
   y **no hay tendencia creciente con R**. ⇒ El hallazgo de `eval_1E` era un **artefacto del
   confundido robots/holgura**. C-1 queda **refutado** tal y como estaba enunciado.
2. **En estocástico la desventaja crece de forma monótona y muy significativa con el tamaño
   del equipo**: −0.032 con 2 robots → −0.131 con 10 (t = −8.4). Es el resultado más nítido
   de toda la tanda y tiene una explicación mecánica: condicionar sobre el plan comunicado de
   un vecino inyecta **sesgo** en cuanto ese plan se desvía, y el sesgo se **acumula** con el
   nº de vecinos sobre los que se condiciona. Enlaza con H-06 / B4 / B5 de `05_dudas.md`.

#### A2 — rejilla robots × holgura (Δ dec-mcts − cbba, DETERMINISTA)

| R \ L | 3 | 4 | 5 | 6 | 8 |
|---|---|---|---|---|---|
| 2 | +0.000 | +0.042 | −0.003 | −0.002 | +0.008 |
| 5 | **+0.067** | +0.030 | −0.001 | −0.011 | +0.006 |
| 10 | −0.002 | −0.003 | −0.001 | −0.026 | −0.021 |

En estocástico **toda** la rejilla es negativa y empeora con R y con la carga (peor celda:
R=10, L=6 ⇒ −0.131 ± 0.013). La única celda favorable es **equipo mediano + holgura**
(R=5, L=3), coherente con "planificar renta cuando hay margen para planificar".

#### Familias (bloques B/C/D/E)

| Familia | cbba | dec-mcts | Δ | mejor |
|---|---|---|---|---|
| ventana **escalonada** (C) | 0.521 | **0.532** | **+0.011 ± 0.009** | **dec-mcts** |
| ventana ancha (A) | 0.730 | 0.723 | −0.007 | **greedy** (0.733) |
| ventana estrecha (E, = A1) | 0.605 | 0.561 | −0.044 | cbba |
| geometría uniforme / asimétrica | 0.584 / 0.588 | 0.557 / 0.561 | −0.027 / −0.027 | cbba |
| coalición q=2 / mixta | 0.474 / 0.540 | 0.378 / 0.465 | **−0.096 / −0.075** | cbba |
| incert. leve / fuerte | 0.611 / 0.404 | 0.582 / 0.341 | −0.029 / −0.064 | cbba |

- **La ventana escalonada es el único terreno favorable**: +0.026 ± 0.009 en determinista
  (gana 7 de 9), aunque se desvanece en estocástico (−0.004) y **decrece** con R
  (+0.038 con 2r → −0.004 con 10r). Mecanismo plausible: los plazos distintos por racimo
  premian ordenar por urgencia, y el factor de urgencia del rollout de v4 lo captura;
  greedy es el que más sufre aquí (−0.018 frente a cbba).
- **Las coaliciones son el peor terreno** (−0.096). Encaja con H-04: `blockingProb` es binaria
  y no modela bien la cobertura parcial de una coalición.
- Solo **4 de 84 celdas** tienen Δ > 0 con |Δ| > 2·EE, y **3 de esas 4 son ventana escalonada**.

#### Coste y métricas secundarias (A1 determinista)

| Solver | reward | makespan | distancia total | s/run |
|---|---|---|---|---|
| cbba | 0.654 | 54.4 | 78.9 | 0.01 |
| dec-mcts-v4-g9999 | 0.647 | 56.7 | **94.1** | **12.50** |
| cbaa | 0.534 | 44.1 | 63.9 | 0.00 |

Dec-MCTS **no compensa por otra vía**: recorre un 19 % más de distancia y termina algo más
tarde para completar lo mismo, pagando ~1000× de cómputo. 0 robots muertos en toda la tanda.

### `logs/eval_b4` (sesión 4) — mejora B4: `blockingProb` probabilística. **RESULTADO NULO**

Variante `dec-mcts-v4-b4` (flag `useProbBlocking` en el constructor de v4): `blockingProb`
deja de ser binaria y estima P(≥ q vecinos lleguen realmente a cubrir la tarea) acumulando el
instante de llegada **y su varianza** a lo largo del bundle del vecino, combinando vecinos con
una Poisson-binomial exacta. En el límite determinista recupera la semántica anterior.

126 escenarios (bloques A1 y C) × 3 réplicas, comparado contra la línea base de
`logs/eval_catalogo` (mismos ficheros, 10 réplicas).

| Conjunto | cbba | v4 original | **v4-B4** | Δ original | Δ B4 | intento orig → B4 |
|---|---|---|---|---|---|---|
| A1 determinista (45) | 0.654 | 0.647 | 0.646 | −0.006 | −0.008 ± 0.004 | 65 % → 65 % |
| A1 estocástico (45) | 0.555 | 0.474 | 0.477 | −0.081 | −0.078 ± 0.008 | 63 % → 63 % |
| C coaliciones (36) | 0.507 | 0.421 | 0.428 | −0.085 | −0.079 ± 0.011 | 49 % → 49 % |

Pendiente de la ventaja frente a R en A1 estocástico: **−0.0130 → −0.0120 por robot**
(t = −8.4 → −4.6). **No se aplana.** La tasa de intento —el mecanismo que B4 pretendía
corregir— **no se mueve ni un punto**. Sin regresión en determinista, pero sin ganancia.

**Diagnóstico de por qué (ver D-08)**: los bundles comunicados tienen **longitud media 1.25**
(74 % son de longitud 0 o 1). B4 opera sobre la posición de la tarea dentro del bundle del
vecino, y en la posición 0 coincide exactamente con la versión binaria ⇒ casi nunca actúa.
El problema no es la calibración de `blockingProb`, sino que **la distribución sobre planes
que Dec-MCTS comunica es degenerada**.

Control de la tanda: cbba reproduce su valor con 3 réplicas (0.5709) frente a 10 (0.5767),
diferencia 0.006 ⇒ 3 réplicas son suficientes con 3-5 instancias por celda.

### `logs/eval_d8` (sesión 4) — corrección D-08 del bundle comunicado. **RESULTADO NULO**

Variantes `dec-mcts-v4-d8` (cadena del bundle restringida a `EXECUTE_TASK`) y
`dec-mcts-v4-d8b4` (D-08 + B4). Mismos 126 escenarios (A1 + C) × 3 réplicas.

La corrección **sí arregla el defecto estructural**: longitud media del bundle 1.58 → 2.50,
cortes por `FINISH` 68.7 % → 1.8 %, cortes en hoja real 28.6 % → 96.5 %. Pero **no mueve el
rendimiento**: comparación pareada directa contra v4 original sobre los 126 escenarios,
**+0.0022 ± 0.0032** (d8) y **+0.0010 ± 0.0029** (d8+b4). Tasa de intento: 63 % → 63 %.

Pendiente frente a R en A1 estocástico: −0.0130 (v4) → −0.0127 (d8) → −0.0081 (d8b4), pero el
EE sube de 0.0015 a 0.0028 y la Δ media de d8b4 **empeora** (−0.085 frente a −0.081) ⇒ el
aplanamiento aparente es redistribución de ruido, no una mejora real.

⇒ **Cuatro explicaciones de implementación descartadas** (cómputo, rondas de comunicación,
calibración de `blockingProb`, profundidad del plan). Ver D-08 en `05_dudas.md`.

## ⚠️ CATÁLOGO v2 — `scenarios/catalogo_v2/` · `logs/eval_catalogo_v2` — **OBSOLETO desde la sesión 8**

> 🔴 **Superado por el catálogo v3** (ver la sección siguiente). Mismo diseño y mismas semillas;
> solo cambia la parametrización de los regímenes. **Ninguna cifra de esta sección debe
> presentarse como resultado del trabajo.** Se conserva porque la comparación pareada v2↔v3
> documenta el efecto del cambio y porque la comparación v1↔v2 es la lección metodológica
> del cap. 6.

### (histórico) CATÁLOGO v2 (sesión 6, 2026-08-31)

Generador `scripts/generate_catalog_v2.py` · analizador `scripts/analyze_catalog_v2.py` ·
CSV `analisis/catalogo_v2.csv`. **360 escenarios × 5 solvers × 3 réplicas = 5 400 runs**
(~25 min con 6 procesos). Sustituye a `scenarios/catalogo/` como tanda de referencia.

**Cambios de diseño respecto al v1** (decisiones del usuario, sesión 6):
1. **Bloques del mismo tamaño** (72 c/u) y mitad det / mitad est, salvo el bloque E.
2. **Batería 40 en TODOS los escenarios** (v1: 100) ⇒ la recarga deja de ser decorativa.
   `nav_rate` se queda en 1.0 a propósito (nav caro + batería escasa hunde a Dec-MCTS).
3. **3 réplicas** (v1: 10).
4. Nueva ventana **`P` (solo plazo, sin espera)**, el hallazgo de `logs/eval_probe`.
5. El **bloque A es el control**: los demás varían un factor sobre R ∈ {2,5,10} y se
   comparan contra él. Semilla = f(R, n, instancia) ⇒ pareado exacto tarea a tarea.

### ⚠️ Lectura obligatoria antes de citar cualquier cifra

**El agregado global cambia de signo respecto al v1, pero es un EMPATE, no una victoria:**
Δ = **+0.0047 ± 0.0044 (t = 1.06)**, y por recuento de escenarios Dec-MCTS **pierde**
(gana 154, empata 33, pierde 173 de 360). El v1 daba −0.0355. **La diferencia es del
catálogo, no del algoritmo** — es la lección metodológica del proyecto repitiéndose.
Reportar SIEMPRE por bloque y por régimen; el agregado no significa nada por sí solo.

### Panorama

| Corte | # | random | greedy | cbaa | cbba | dec-mcts | Δ dec−cbba |
|---|---|---|---|---|---|---|---|
| catálogo completo | 360 | 0.272 | 0.348 | 0.425 | 0.482 | **0.487** | +0.005 ±0.004 (n.s.) |
| det (ρ=1.00) | 162 | 0.313 | 0.404 | 0.512 | 0.564 | **0.586** | **+0.022 ±0.006** |
| lev (ρ=0.90) | 18 | 0.272 | 0.343 | 0.464 | 0.490 | **0.508** | +0.018 ±0.018 |
| est (ρ=0.75) | 162 | 0.243 | 0.309 | 0.352 | **0.417** | 0.409 | −0.008 ±0.007 |
| fue (ρ=0.50) | 18 | 0.169 | 0.199 | 0.253 | **0.318** | 0.268 | **−0.050 ±0.020** |

| Bloque | cbba | dec-mcts | Δ det | Δ est | mejor |
|---|---|---|---|---|---|
| A · equipo (control) | **0.495** | 0.487 | +0.001 ±0.005 | −0.018 ±0.011 | cbba |
| B · ventana | 0.509 | **0.525** | **+0.024 ±0.010** | +0.008 ±0.014 | dec-mcts |
| C · carga | 0.557 | **0.598** | **+0.080 ±0.015** | +0.003 ±0.021 | dec-mcts |
| D · coaliciones | 0.402 | 0.390 | −0.011 ±0.013 | −0.013 ±0.012 | **cbaa** (0.424) |
| E · incertidumbre | **0.447** | 0.434 | +0.010 ±0.009 | −0.031 / −0.050 | cbba |

⚠️ **Auditoría de equilibrio, hecha a posteriori**: B y C salieron **más favorables** de lo
previsto porque cada uno tiene 2 de sus 3 niveles en terreno bueno para Dec-MCTS
(B: `P` y `C` favorables, `A` desfavorable · C: L=3 y L=4 favorables, L=8 desfavorable).
A, D y E le son adversos. Al redactar hay que declararlo: el catálogo **no está escorado a
propósito**, pero su composición explica el cambio de signo del agregado.

### 🔑 El hallazgo principal: la batería escasa cierra 2/3 de la brecha estocástica

Comparación **apples-to-apples**: el bloque A1 del v1 y el bloque A del v2 son la **misma
configuración** (L=6, ventana E, q1, R=2…10, bnd). Solo cambia la batería.

| régimen | v1 (bat. 100) cbba / dec / Δ | v2 (bat. 40) cbba / dec / Δ | mejora de Δ |
|---|---|---|---|
| det | 0.654 / 0.647 / −0.006 ±0.004 | 0.576 / 0.577 / **+0.001 ±0.005** | +0.008 |
| est | 0.555 / 0.474 / −0.081 ±0.006 | 0.414 / 0.396 / **−0.018 ±0.011** | **+0.064** |

Confirma a escala de catálogo lo que la sonda vio en 12 escenarios (+0.103 est). **Mecanismo**:
la tasa de éxito de Dec-MCTS supera a la de CBBA (+0.029 en est, +0.045 en la ventana E de
control). CBBA comprueba la batería con la demanda **esperada** (`solver_CBBA.hpp:106`), así
que con duraciones N(10,10) empieza tareas que no puede terminar y el robot se queda a cero a
mitad ⇒ tarea fallida (`simulator.cpp:470-475`). Dec-MCTS muestrea duraciones y deja margen.
⚠️ Esto **matiza la idea 2 de `06_conclusiones.md`**: ya no es cierto que toda la brecha viva
en la tasa de intento y que la de éxito sea idéntica.

### Bloque A — la curva frente a R (el resultado más nítido del proyecto)

| R | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | pendiente/robot |
|---|---|---|---|---|---|---|---|---|---|---|
| **det** Δ | +0.021 | +0.009 | +0.007 | +0.008 | −0.002 | +0.010 | −0.019 | −0.002 | −0.021 | −0.0042 (t=−2.2) |
| **est** Δ | +0.069 | +0.037 | −0.017 | −0.006 | −0.021 | −0.038 | −0.078 | −0.043 | −0.064 | **−0.0155 (t=−4.7)** |

Pendientes propias en **est**: cbba **+0.0150 (t=5.7)** · dec-mcts **−0.0004 (t=−0.2)**.
⇒ Bajo incertidumbre Dec-MCTS **no convierte robots adicionales en rendimiento, en absoluto**;
CBBA sí. En determinista ambos escalan (cbba +0.0165, dec +0.0123). Es la formulación más
limpia obtenida hasta ahora de la tesis central.

### Bloque B — ventana (efecto pareado contra el control `E`)

| ventana | det Δ | efecto vs control | est Δ | efecto vs control |
|---|---|---|---|---|
| `E` estrecha (control) | +0.003 ±0.009 | — | +0.000 ±0.021 | — |
| **`C` escalonada** | **+0.076 ±0.010** (12/12) | **+0.074 ±0.013** | +0.034 ±0.017 | **+0.034 ±0.014** |
| **`P` solo plazo** | **+0.033 ±0.015** | **+0.030 ±0.010** | +0.025 ±0.026 | +0.025 ±0.023 |
| `A` ancha | −0.038 ±0.011 | **−0.041 ±0.010** | −0.035 ±0.026 | −0.035 ±0.034 |

La **ventana escalonada es ahora el mejor terreno de Dec-MCTS con diferencia** (gana 12 de 12
en determinista), muy por encima del +0.026 que daba en el v1: la batería escasa la amplifica.
`P` confirma el hallazgo de la sonda con signo y significación, aunque con la mitad de efecto.

### Bloque C — carga por robot (Δ dec−cbba). Gradiente limpio en las dos direcciones

| det · R \ L | 3 | 4 | 6 | 8 | | est · R \ L | 3 | 4 | 6 | 8 |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | **+0.208** | **+0.177** | +0.021 | +0.089 | | 2 | **+0.139** | +0.073 | +0.069 | −0.042 |
| 5 | **+0.111** | +0.079 | +0.008 | −0.008 | | 5 | +0.044 | +0.050 | −0.006 | −0.069 |
| 10 | +0.078 | +0.015 | −0.021 | −0.030 | | 10 | −0.044 | −0.052 | −0.064 | −0.076 |

**Dec-MCTS gana con carga ligera y equipos pequeños; pierde con carga alta y equipos grandes.**
Monótono en ambos ejes y en ambos regímenes. Es el mapa más informativo del catálogo.

### Bloque D — coaliciones. **Novedad: gana CBAA**

| | cbaa | cbba | dec-mcts | Δ dec−cbba |
|---|---|---|---|---|
| q1 control · det | 0.456 | **0.575** | 0.571 | −0.005 ±0.010 |
| **q2 · det** | **0.474** | 0.440 | 0.393 | −0.047 ±0.020 |
| **qm · det** | 0.520 | 0.503 | **0.527** | **+0.024 ±0.011** |
| q2 · est | **0.328** | 0.315 | 0.282 | −0.034 ±0.020 |
| qm · est | **0.372** | 0.350 | 0.358 | +0.008 ±0.014 |

Dos hallazgos nuevos: (a) **en escenarios de coalición la subasta de tarea única (CBAA) bate a
la de paquete (CBBA)** — el bloque D es el único donde cbaa es el mejor de los cinco; (b) la
**coalición mixta (1-2-3) se comporta al revés que la uniforme q=2**: Dec-MCTS gana en mixta
determinista (+0.024) y pierde en q=2 (−0.047, y −0.096 con R=10).

### Bloque E — gradiente de incertidumbre. Monótono, y con interacción fuerte con R

| ρ | Δ global | R=2 | R=5 | R=10 |
|---|---|---|---|---|
| 1.00 | +0.010 ±0.009 | +0.032 | +0.009 | −0.012 |
| 0.90 | +0.018 ±0.018 | **+0.097** | −0.019 | −0.024 |
| 0.75 | −0.031 ±0.023 | +0.037 | −0.052 | −0.078 |
| 0.50 | −0.050 ±0.020 | +0.032 | −0.091 | −0.092 |

**Con 2 robots Dec-MCTS gana en los cuatro niveles de incertidumbre**; con 5 y 10 pierde en
cuanto ρ < 1. La incertidumbre no le hace daño *per se*: le hace daño **multiplicada por el
número de vecinos sobre los que condiciona**. Encaja exactamente con el mecanismo de la idea 2.

### Diagnóstico de la batería (⚠️ reportar al comparar con los baselines)

| solver | robots muertos/run | % runs con bajas | tasa de éxito | distancia | s/run |
|---|---|---|---|---|---|
| random | 0.932 | 44.7 % | 0.722 | 99.6 | 0.00 |
| greedy | 0.758 | 42.2 % | 0.767 | 62.1 | 0.00 |
| cbaa | **0.000** | 0 % | 0.826 | 77.5 | 0.00 |
| cbba | **0.000** | 0 % | 0.822 | 92.7 | 0.01 |
| dec-mcts | **0.000** | 0 % | **0.836** | 113.5 | 14.24 |

Parte de la desventaja de `random` y `greedy` es **mortalidad**, no mala asignación: hay que
decirlo explícitamente. Dec-MCTS sigue recorriendo un **+22 %** más de distancia que CBBA.

---

## 🟢 CATÁLOGO v3 — `scenarios/catalogo_v3/` · `logs/eval_catalogo_v3` (sesión 8, 2026-09-10)

**EL VIGENTE.** Generador `scripts/generate_catalog_v3.py`, analizador
`scripts/analyze_catalog_v3.py`, comparador `scripts/compare_catalogos.py`,
CSV `analisis/catalogo_v3.csv`. 360 esc. × 5 solvers × 3 réplicas = **5 400 ejecuciones**,
~35 min con 6 procesos. 10 896 tareas, 12 264 plazas, 6–80 tareas/esc. (mediana 30),
2–10 robots, 162 det / 198 est.

### Qué cambia respecto al v2 (y solo eso)

Misma estructura de bloques, misma geometría, **mismas semillas** ⇒ pareado exacto celda a celda.
Dos cambios de parametrización, pedidos por el usuario:

1. **Coste de un intento fallido: 10 fijo** en todos los regímenes (antes 0 / 3 / 10 / 10).
   ⚠️ Con **éxito** el consumo es proporcional a la duración real (tasa 1 ⇒ esperanza 10, pero
   variable); solo el fracaso cuesta 10 exactos. Lo que coincide es la **esperanza**.
2. **σ de la duración: 0 / 1 / 3 / 5** (antes 0 / 3 / 10 / 10) ⇒ CV = 0 / 0.1 / 0.3 / 0.5.

Verificado por diferencia contra el v2: 162 det cambian solo `demand`, 180 solo `success_time`,
18 (lev) ambos.

### Panorama

| celda | # | rand | greedy | cbaa | cbba | decmcts | Δ dec−cbba | G/E/P | mejor |
|---|---|---|---|---|---|---|---|---|---|
| catálogo completo | 360 | 0.271 | 0.354 | 0.437 | 0.494 | **0.502** | +0.009 ±0.004 | 158/42/160 | decmcts |
| det (ρ=1.00 σ=0) | 162 | 0.308 | 0.403 | 0.512 | 0.565 | **0.585** | +0.020 ±0.006 | 76/30/56 | decmcts |
| lev (ρ=0.90 σ=1) | 18 | 0.261 | 0.340 | 0.471 | **0.520** | 0.510 | −0.010 ±0.015 | 4/3/11 | cbba |
| est (ρ=0.75 σ=3) | 162 | 0.247 | 0.324 | 0.377 | 0.438 | **0.444** | +0.006 ±0.006 | 74/9/79 | decmcts |
| fue (ρ=0.50 σ=5) | 18 | 0.162 | 0.207 | 0.263 | **0.328** | 0.278 | −0.050 ±0.017 | 4/0/14 | cbba |

### Por bloque

| bloque | # | cbaa | cbba | decmcts | Δ dec−cbba | mejor |
|---|---|---|---|---|---|---|
| A · equipo (control) | 72 | 0.406 | **0.511** | 0.497 | −0.013 ±0.006 | cbba |
| B · ventana | 72 | 0.480 | 0.525 | **0.546** | +0.021 ±0.007 | decmcts |
| C · carga | 72 | 0.460 | 0.560 | **0.621** | +0.061 ±0.013 | decmcts |
| D · coaliciones | 72 | **0.431** | 0.407 | 0.401 | −0.006 ±0.007 | **cbaa** |
| E · incertidumbre | 72 | 0.407 | **0.466** | 0.446 | −0.019 ±0.007 | cbba |

### Bloque A — pendiente frente al nº de robots (⚠️ la cifra que cambió de lectura)

| régimen | cbba | dec-mcts | pendiente de la Δ |
|---|---|---|---|
| det | +0.0164 (t=6.6) | +0.0121 (t=7.7) | −0.0043 (t=−2.0) |
| est | **+0.0195 (t=6.6)** | **+0.0040 (t=1.5)** | **−0.0155 (t=−4.7)** |

Con el v2, la pendiente propia de dec-mcts en est era −0.0004 (t=−0.2) y se decía «no convierte
robots en rendimiento **en absoluto**». Ya no vale: ahora es **cinco veces menor que la de cbba y
no llega a distinguirse de cero**. Lo que sí sigue intacto —y es más fuerte— es la pendiente de
la **Δ**: −0.0155 por robot (t=−4.7).

### Otras cifras por bloque

- **Ventana** (det): escalonada +0.078 (gana **12/12**), solo plazo +0.038, ancha −0.034.
  Efecto pareado contra el control: +0.071, +0.031, −0.041, los tres significativos.
- **Coaliciones** (det): `q2` −0.052 (peor cuanto mayor el equipo: −0.006 → −0.059 → −0.093),
  `qm` **+0.023**. La mixta va al revés que la uniforme.
- **Mecanismo** (est): tasa de éxito dec 0.754 vs cbba 0.719; tareas intentadas dec 17.7 vs
  cbba 19.3; distancia recorrida dec 121.7 vs cbba 94.9 (+28 %).

### ⚠️ Mortalidad de robots — peor que en el v2 y también en determinista

| | v2 det | v3 det | v2 est | v3 est |
|---|---|---|---|---|
| random | 0.00 (0 %) | **1.85 (65 %)** | 1.78 | 2.12 |
| greedy | 0.00 (0 %) | **1.27 (50 %)** | 1.50 | 1.64 |
| cbaa · cbba · dec-mcts | 0 | 0 | 0 | 0 |

**Mecanismo** (verificado en código y logs): aunque ρ=1, si a un robot no le da la batería para
arrancar una tarea, `simulator.cpp:469-473` fuerza `success = false` y trunca la ejecución; luego
`endTask` le cobra `averageFailDemand` **entera** porque `averageFailTime == 0`
(`simulator.cpp:494-498`). Con el v2 eran 0 y sobrevivía; con el v3 son 10 y muere. Ejemplo real
en `cv3_A_r10_n060_bnd_E_det_q1_i1`: batería 3.16 → se le cobran 10 → −6.84.
⇒ **El determinista NO es un control invariante entre catálogos para `random` y `greedy`**; sí lo
es para cbaa (−0.0000 ±0.0004), cbba (+0.0002 ±0.0005) y dec-mcts (−0.0016 ±0.0013).

---

## 🔬 SONDA DE VARIANTES DE DISEÑO — `logs/eval_probe` (sesión 6, 2026-08-27)

Generador `scripts/generate_probe_scenarios.py` → `scenarios/probe/` (204 esc.).
Análisis `scripts/analyze_probe.py` → `analisis/probe.csv`. **3 060 runs**, ~15 min con 6 procesos.

**Diseño**: 17 variantes, **una sola variación** sobre una referencia común (racimos, L=6, T=40,
q=1, batería 100, `nav_rate` 1, fracaso instantáneo, robots homogéneos en la estación).
Cada variante: R ∈ {4,8} × {det,est} × 3 instancias = **12 escenarios**, × 3 réplicas.
La semilla depende de (R, régimen, instancia) y **no** de la variante ⇒ dos variantes comparten
geometría y sorteos y el contraste entre ellas es **pareado tarea a tarea**.

⚠️ Verificado antes de lanzar: v4 (`solver_DecMCTS_v4.hpp:336,452-456,527-529`) y CBBA
(`solver_CBBA.hpp:41,106,126-130`) **modelan** `averageFailTime`, `stdFailTime`,
`averageFailDemand` y `batteryRateWhileNavigating`. Los ejes nuevos son justos para ambos.

### Suelo de ruido (medido gratis)

Con ρ=1 las variantes `f_igual`/`f_doble` son **configuraciones idénticas** a la referencia.
greedy, cbaa y cbba reproducen **exactamente** (Δ = 0.0000); Dec-MCTS ±0.008 de media y
**máx 0.028 por escenario**. ⇒ En determinista **no interpretar |Δ| < 0.02**.

### Resultado principal: ¿en cuántas variantes gana Dec-MCTS?

| | mejor de los 5 | por encima de CBBA | y con \|Δ\| > 2·EE |
|---|---|---|---|
| determinista | **7/17** | 7/17 | 1/17 (`w_plazo`) |
| estocástico | **2/17** | 2/17 | 0/17 |
| agregado | 2/17 | 3/17 | 1/17 |

Det: `w_plazo, w_escalon, w_libre, f_igual, f_doble, b_escasa, h_capacidad`.
Est: `w_plazo, b_escasa`.

### Efecto de cada eje sobre la ventaja (Δ_variante − Δ_referencia, pareado)

| variante | qué cambia | det | est | ambos |
|---|---|---|---|---|
| **`w_plazo`** | `[0, t0+10]` — plazo aleatorio **sin espera** | **+0.060 ±0.020** | **+0.079 ±0.022** | **+0.069 ±0.014** |
| **`b_escasa`** | capacidad 40 (obliga a recargar) | +0.012 ±0.012 | **+0.103 ±0.046** | **+0.057 ±0.027** |
| `w_escalon` | plazo escalonado por racimo | +0.022 ±0.016 | +0.052 ±0.026 | +0.037 ±0.015 |
| `f_doble` | fracaso = 2× éxito (20) | +0.006 ±0.008 | +0.046 ±0.044 | +0.026 ±0.022 |
| `f_igual` | fracaso = éxito (10) | +0.008 ±0.005 | +0.009 ±0.034 | +0.009 ±0.016 |
| `b_navcara` | `nav_rate` 4 | +0.002 ±0.017 | +0.008 ±0.041 | +0.005 ±0.021 |
| `w_libre` | apertura y cierre aleatorios independientes | +0.021 ±0.013 | −0.014 ±0.026 | +0.003 ±0.015 |
| `w_espera` | `[t0, T]` — espera **sin plazo apretado** | +0.000 ±0.014 | −0.021 ±0.027 | −0.010 ±0.015 |
| `h_capacidad` | 3 clases de habilidad, cada robot cubre 2 | +0.012 ±0.008 | −0.034 ±0.038 | −0.011 ±0.020 |
| `h_duracion` | duraciones 5/10/15 (media 10) | −0.013 ±0.013 | −0.013 ±0.034 | −0.013 ±0.017 |
| `b_navbarata` | `nav_rate` 0.25 | **−0.016 ±0.007** | −0.023 ±0.029 | −0.020 ±0.015 |
| `w_dura` | ventana de anchura 5 | +0.001 ±0.009 | −0.041 ±0.039 | −0.020 ±0.020 |
| `w_ancha` | `[0, T+10]` | −0.019 ±0.011 | −0.029 ±0.030 | −0.024 ±0.015 |
| **`h_disperso`** | robots repartidos por los racimos | **−0.027 ±0.007** | −0.027 ±0.022 | **−0.027 ±0.011** |
| **`b_navcara_escasa`** | `nav_rate` 3 + capacidad 40 | **−0.068 ±0.013** | −0.042 ±0.032 | **−0.055 ±0.017** |
| `c_q2` | coalición q=2 (**control**) | −0.060 ±0.037 | −0.075 ±0.051 | **−0.068 ±0.030** |

**Validación**: el control `c_q2` reproduce el catálogo (aquí −0.065 det / −0.141 est; catálogo
−0.084 / −0.109) y `w_escalon` también (+0.017 aquí, +0.026 en el catálogo). La sonda está
calibrada.

### Los tres hallazgos

**1. La ESPERA es lo que hunde a Dec-MCTS, no el plazo.** Factorial 2×2 de la ventana
(Δ dec−cbba, det / est):

| | cierre suelto (`T+10`) | cierre apretado (`t0+10`) |
|---|---|---|
| **apertura en 0** (sin espera) | `w_ancha` −0.023 / −0.095 | **`w_plazo` +0.056 / +0.013** |
| **apertura en t0** (con espera) | `w_espera` −0.005 / −0.087 | `w_base` −0.005 / −0.066 |

Sin espera, apretar el plazo **le da la vuelta** a la comparación (−0.023 → +0.056). Con espera,
apretar el plazo no cambia nada (−0.005 → −0.005). `w_plazo` es la **única** variante donde
Dec-MCTS es el mejor de los cinco en los dos regímenes, y en det con R=8 gana **3/3** con
+0.111/+0.076/+0.104. El mecanismo: la espera obligatoria convierte el tiempo muerto en el
recurso escaso y anula el valor de anticipar *el orden*, que es justo lo que Dec-MCTS optimiza.

**2. Batería escasa: el único eje donde Dec-MCTS mejora bajo incertidumbre.** El efecto en
recompensa es ruidoso (+0.037 ± 0.038 est), pero **el mecanismo es 6/6 consistente**: la tasa
de éxito de Dec-MCTS sube **+0.120** sobre CBBA (0.720 vs 0.600). CBBA comprueba la batería con
la **demanda esperada** (`solver_CBBA.hpp:106`), así que con duraciones N(10,10) empieza tareas
que no puede terminar y el robot se queda sin batería a mitad ⇒ la tarea cuenta como fallida
(`simulator.cpp:470-475`). Dec-MCTS muestrea duraciones en los rollouts y deja margen.
Es la **primera vez en todo el proyecto** que la brecha no vive entera en la tasa de intento.
⚠️ Confirmar con más instancias antes de convertirlo en bloque del catálogo.

**3. El sobrecoste de desplazamiento es el hilo conductor de las derrotas.** Dec-MCTS recorre
entre **+6 % y +81 %** más distancia que CBBA en *todas* las variantes. Es inocuo mientras
navegar sea barato, y **letal** cuando el desplazamiento es el recurso que ata:
`b_navcara_escasa` pierde **12/12 escenarios** (−0.073 det, −0.108 est) y es la peor variante
del barrido, peor incluso que las coaliciones.

### Mecanismo por régimen (recompensa = tasa de intento × tasa de éxito)

En **det** la tasa de éxito es 1 por construcción ⇒ Δrecompensa = Δintento exactamente.
En **est** se confirma la idea 2 de `06_conclusiones.md`: la brecha vive en la **tasa de
intento** (negativa en las 17 variantes) mientras la de **éxito** es igual o mejor.
Las dos excepciones son informativas: `b_escasa` (Δéxito **+0.120**) y `b_navbarata`
(Δéxito +0.064 pero Δintento −0.179).

## 🎯 Criterios para el diseño del catálogo definitivo (objetivo 1.2)

> Evidencia acumulada sobre **qué hace que un escenario discrimine** entre solvers. Consultar
> este apartado antes de generar o descartar escenarios. Ver también D-02 en `05_dudas.md`.

### C-1 · ⚠️ **REFUTADO (sesión 4)** — «el eje que más discrimina es el nº de robots»
> Este criterio, y la conclusión que lo acompaña, **son falsos**: estaban medidos sobre un
> catálogo que confundía el nº de robots con la carga por robot. Ver **D-07** en `05_dudas.md`
> y la sección "CATÁLOGO DEFINITIVO" de este mismo fichero. Texto original conservado abajo
> porque el error en sí es material para el cap. 6.

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

### C-6 · ⚠️ **DEROGADO (sesión 4)** — potencia estadística
> **Decisión del usuario (2026-08-17): usar 3 réplicas, no 10.** Este criterio se escribió
> cuando los conjuntos de prueba tenían **una sola instancia por celda**. El catálogo definitivo
> lleva 3-5 instancias distintas por celda, y ahí la varianza que domina es la de **entre
> instancias** (±0.15), no la de entre réplicas (±0.016). Comprobado: cbba reproduce su valor
> con 3 réplicas (0.5709) frente a 10 (0.5767). **Calcular siempre el error estándar entre
> escenarios, no entre réplicas.**

Texto original: con 5 réplicas el ruido entre dos tandas idénticas es ≈ ±0.016, del orden de las
diferencias que queremos detectar. Con 10 réplicas los solvers no modificados reproducen su
valor con ±0.007.

## Herramientas de análisis

- `analyze_ablation.py <logs_dir>` — tabla media/mediana/sd por solver (tiene una sección
  comentada al final con diagnósticos H1/H4 y la tabla por escenario está a medias).
- `compare_v4.py <logs_dir>` — comparación por escenario entre versiones de Dec-MCTS.
- `mrtau metrics -i logs/ -o results.csv` — herramienta externa del proyecto; parsea `.log` a CSV.
- `mrtau video -i test.log -o simulacion.mp4` — vídeo de una ejecución.

✅ **HECHO (objetivo 1.3, sesión 4)**: `analisis/evaluacion_final.ipynb`, alimentado por
`scripts/extract_metrics.py` (logs → CSV, solo biblioteca estándar). `analyze_ablation.py` y
`compare_v4.py` quedan **obsoletos** y deben borrarse en la limpieza (objetivo 3.1).
⚠️ **Actualizado en la sesión 9**: aquel cuaderno (catálogo v1) se archivó como
`analisis/evaluacion_v1_obsoleto.ipynb` y `evaluacion_final.ipynb` es ahora el análisis del
**catálogo v3** en tres partes, con 14 figuras `figuras/p1_*`, `p2_*`, `p3_*`.
