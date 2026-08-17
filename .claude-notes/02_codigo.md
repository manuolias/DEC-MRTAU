# 02 — Arquitectura del código

C++17, header-only para los solvers. `namespace tau`. Dependencia externa única: **yaml-cpp**.
Compilación: `cd build && cmake .. && make` → ejecutable `simulador`.
Ejecución: `./simulador <scenariosPath> <logsDir> [configPath]`
(por defecto `../data/`, `../logs/fixed_sim`, `../data/experiment_config.yaml`).

## Mapa de ficheros

```
src/
  main.cpp        Ejecutor de experimentos: escenarios × solvers × rewards × réplicas.
                  Fábrica de solvers por nombre. Idempotente: salta logs ya completos
                  (detecta la línea final "metric: computing_time;").
  simulator.cpp   Motor de eventos discretos (DistributedSimulator).
  scenario.cpp    Carga YAML → Scenario.
  state.cpp       Estado dinámico global.
  logger.cpp      Log textual por eventos.

include/tau/
  scenario.hpp        Instancia ESTÁTICA: Node, TaskInfo, RobotInfo, StationInfo. Alias de tipos.
  definitions.hpp     Estado DINÁMICO: struct Robot, struct Task, enums de estado + helpers de métricas.
  state.hpp           State: mapas robots/tasks + isFinal().
  observation.hpp     Observation (vista del solver) + Bundle = vector<TaskID>,
                      Distribution = map<Bundle,double>.
  action.hpp          Action::Type { FINISH, RECHARGE, EXECUTE_TASK, START, IDLE }.
  solver_interface.hpp ISolver: decideNextAction / requiresContinuousPlanning /
                      performBackgroundPlanning.
  simulator.hpp       EventType, struct Event, DistributedSimulator.
  reward_interface.hpp, reward_00.hpp   Función de recompensa (8 componentes ponderados).
  solver_random.hpp, solver_greedy.hpp  Baselines.
  solver_CBAA.hpp, solver_CBBA.hpp      Métodos de mercado.
  solver_DecMCTS_v1..v4.hpp             Cuatro versiones de Dec-MCTS.
  utils/estimation.hpp  phi, survival_Phi, truncated_normal_mean, estimateNextDecisionTime.
  utils/svector.hpp     Vector con mean/sd/min/max/sum para métricas.
```

## Patrón central: un solver por robot

`DistributedSimulator` mantiene `map<RobotID, shared_ptr<ISolver>>`. Cada robot tiene su
**instancia propia** de solver (`registerSolver`). Los solvers no comparten estado: solo la
**pizarra pública** `map<RobotID, Distribution> globalDistributions`, que se inyecta entera
en cada `Observation`. Es la realización (idealizada: sin pérdidas ni latencia) de los
mensajes $\mathcal{M}_{in}$ del modelo.

## El simulador de eventos discretos

Cola: `priority_queue<Event, vector<Event>, greater<Event>>` (min-heap por tiempo).
Orden de desempate: menor `time` → menor `type` → menor `randomTieBreaker` (aleatorio, para
que varios robots que deciden en el mismo instante no se ordenen por `RobotID`).

Prioridad de `EventType` (menor = antes): `TASK_END=0`, `TASK_START=1`, `TASK_EXPIRATION=2`,
`ROBOT_DECISION=3`, `PLANNING_UPDATE=4`.

Ciclo de `run()`:
1. Cronómetro (`computing_time`) e `initLogging()`.
2. **FASE 0 — warm-up**: `WARMUP_ROUNDS` rondas de `performBackgroundPlanning` + publicación
   en la pizarra, para solvers anytime, **antes** de encolar ninguna decisión física.
3. Encolar: una `ROBOT_DECISION` por robot en `initialTime`; un `PLANNING_UPDATE` por robot
   anytime; un `TASK_EXPIRATION` por tarea en su `latestStart`.
4. Bucle `while (!cola.vacía && !state.isFinal())`.
5. Cierre: `reward->getValue(state)` + `finalLogging`.

Hiperparámetros del motor (configurables por variable de entorno desde el commit `466190a`):

| Constante | Valor | Env var | Qué controla |
|---|---|---|---|
| `PLANNING_INTERVAL` | 0.1 | `TAU_PLANNING_INTERVAL` | Periodo virtual del latido de planificación |
| `WARMUP_VIRTUAL_TIME` | 30.0 | `TAU_WARMUP_TIME` | Tiempo virtual de warm-up (→ 300 rondas) |

**Hipótesis metodológica**: el cómputo por latido es un número **fijo** de iteraciones, no
está acotado por reloj de pared. Se modela que el robot "piensa gratis" mientras avanza el
tiempo virtual.

### Física (métodos de `DistributedSimulator`)

- `simulateTask`: **barrera de defensa física** — si la tarea ya no está `PENDING` o ya tiene
  todos los `requiredWorkers`, el robot pierde **1 s virtual** y vuelve a decidir. Es el
  mecanismo que resuelve los conflictos inherentes a la descentralización. Después: navegación
  (consumo = rate·travelTime; si batería < 0 el robot muere), `WAITING`, `assignedWorkers++`,
  `task.initTime = max(initTime, arrival, earliestStart)`; al completarse la coalición la
  tarea pasa a `ASSIGNED` y se encola `TASK_START` en `initTime`.
- `startTask`: **sample-at-fire**. Muestrea `success ~ U(0,1) < successProb`, la duración de
  `N(μ,σ)` (éxito o fracaso) truncada a ≥0, calcula el consumo y encola `TASK_END` con el
  desenlace **oculto en `payload`**. Aquí se materializa la ausencia de *leak temporal*.
- `endTask`: descuenta batería a los trabajadores; muertos → `FAILED`, vivos → `AVAILABLE` +
  nueva decisión. La tarea pasa a `COMPLETED`/`FAILED`.
- `simulateRecharge`: viaja a `nearestStation` (precomputado por nodo) y **recarga instantánea
  y completa** (equivale a cambio de batería). Vuelve a `AVAILABLE`.
- `simulateFinish`: viaja a la estación más cercana y pasa a `FINISHED` (no vuelve a decidir).
- `expireTask`: ignora `COMPLETED`/`FAILED`/`EXECUTING`. **Una tarea `ASSIGNED` SÍ caduca**
  (corregido en `466190a`, ver más abajo). Al caducar libera a los robots `WAITING`.

## Recompensa (`RewardFunction00`)

Combinación lineal de 8 componentes normalizados $a_1..a_8 \in [0,1]$ con pesos $k_1..k_8$
que deben sumar 1 (si no, excepción). Configuración usada en TODOS los experimentos:
**`k1 = 1`, resto 0** → la recompensa reportada es la **fracción de tareas completadas**.

Limitaciones a mencionar en la memoria: $a_7$ usa un horizonte de makespan *hardcodeado*
(`1080.0` en `definitions.hpp`) y $a_8$ un máximo de distancia `10000·nº_robots`. Están
inactivos con $k_1=1$, pero son constantes de normalización arbitrarias.

## Dec-MCTS: las cuatro versiones

Todas comparten la estructura: **simulador interno propio** (réplica de `simulator.cpp` sin
logs) sobre el que se hacen los rollouts, muestreo de bundles vecinos por iteración,
`blockingProb` por tarea y construcción de la distribución $\psi^i$ por visitas descontadas.

| | v1 | v2 | v3 | v4 |
|---|---|---|---|---|
| Selección | UCT clásico (C=1.414) | **D-UCT** + descuento perezoso | D-UCT + `N_avail_disc` | D-UCT filtrado por factibilidad |
| Rollout | greedy por distancia | *marginal-aware* (valor/coste) | igual que v2 | + espera + factor de urgencia (ventanas) |
| Árbol | plano | plano | **chance nodes** (éxito/fallo) | **widening progresivo** + expansión por heurística |
| Recompensa | completadas/n | completadas/n | completadas/n | + tie-break de *earliness* |
| Extra | — | doble rollout $f^r$ opcional | avance bietápico del root | poda de hijos obsoletos, batería completa |
| Caducidad en el rollout | ⚠️ semántica **antigua** | ⚠️ antigua | ⚠️ antigua | ✅ corregida |
| 1E (16 esc. × 10 rép.) | 0.6124 | 0.6074 | 0.5817 | **0.6184** (0.6179 con γ=0.9999) |
| 21 esc. mixtos (5 rép.) | 0.461 | 0.451 | 0.454 | 0.458 (0.463 con γ=0.9999) |

**Tras corregir los defectos (sesión 3) las cuatro versiones quedan estadísticamente
empatadas.** Los fallos pesaban más que todas las diferencias de diseño entre ellas. v4 es la
mejor por muy poco y en los dos conjuntos, y es la única cuyo rollout replica la semántica
corregida de caducidad.

### Métodos añadidos en la sesión 3 (los cuatro solvers)
- `mostVisitedFeasibleChild(obs, sc)` (`mostVisitedFeasibleActionChild` en v3): política de
  explotación final **filtrada por factibilidad**. Sustituye a `mostVisitedChild` en
  `decideNextAction`. Es **no destructiva** (no borra subárboles).
- v4 conserva además `pruneStaleRootChildren` con su criterio original y **conservador**: la
  poda sí destruye subárboles, así que solo elimina lo que ya no volverá a ser aplicable.
- v4: flag `useComm_` + `sampleNeighbourBundles(obs)`, punto único de lectura del canal de
  comunicación (ablación C1).

### Los 7 cambios de v4 respecto a v2

1. Rollout consciente de ventanas: `score = valor/(viaje+espera+ejecución) · (1 + W_u·τ/(τ+slack))`.
2. Widening progresivo (`máx hijos = 1 + PW_C·N_disc^PW_ALPHA`) + expansión por mayor score.
3. Selección D-UCT filtrada por factibilidad en el estado simulado.
4. Poda de hijos obsoletos del root según la `Observation`.
5. Factibilidad de batería completa (navegar **Y** ejecutar).
6. Tie-break de *earliness* en la recompensa (`W_e < 1` ⇒ completar una tarea más siempre domina).
7. `extractDistribution` acumula (`+=`) cuando varios hijos mapean al mismo bundle.

### Hiperparámetros de v4

| Nombre | Valor | Notas |
|---|---|---|
| `gamma` | **0.9999** (por defecto en el constructor) | La variante usada es `dec-mcts-v4-g9999` |
| `cExplore` ($C_p$) | 0.7 ≈ 1/√2 | El bono efectivo es `2·C_p`; cumple $C_p > 1/\sqrt8$ |
| `ITERATIONS_PER_CALL` | 30 | Iteraciones por latido |
| `EMERGENCY_ITERS` | 300 | Si se pide decisión con árbol vacío |
| `MAX_ROLLOUT_EVENTS` | 200000 | Guardia anti-bucle |
| `PW_C` / `PW_ALPHA` | 1.5 / 0.6 | Widening progresivo |
| `URGENCY_WEIGHT` ($W_u$) | 1.0 | Urgencia por ventana temporal |
| `EARLINESS_WEIGHT` ($W_e$) | 0.5 | Bono de earliness |
| `RECHARGE_SCORE` | 1e-6 | Prior mínimo de RECHARGE |
| `useDiffReward` | false | Doble rollout $f^r$ desactivado (ablación: peor) |
| `blockingProb` | `successProb` si ≥ `requiredWorkers` vecinos la cubren; 0 si no | No es constante |

⚠️ **Incoherencia de comentarios**: la cabecera de `solver_DecMCTS_v4.hpp` dice "se mantiene
0.999 por defecto", pero el constructor real usa `gamma = 0.9999`. Hay que arreglar el
comentario (o el valor) antes de redactar el cap. 4.

## Nombres de solver reconocidos en `main.cpp`

`greedy`, `random`, `cbaa`, `cbba`, `dec-mcts-v1`, `dec-mcts-v2`, `dec-mcts-nodiff`,
`dec-mcts-gamma99`, `dec-mcts-v3`, `dec-mcts-v3.2`, `dec-mcts-v4`, `dec-mcts-v4-c035`,
`dec-mcts-v4-g9999`, `dec-mcts-v4-g9999-hc` (alto cómputo; iteraciones por env
`DECMCTS_ITERS`/`DECMCTS_EMERG`, por defecto 1200/12000),
**`dec-mcts-v4-nocomm`** (= `v4-g9999` pero ignorando las distribuciones de los vecinos;
ablación C1, ver `03_experimentos.md`).

Nota: las variantes `dec-mcts-bp-*` que aparecen en `logs/ablacion_*` **ya no existen** en
`main.cpp` (eran una ablación del cap de `blockingProb` y se eliminaron).

## ⚠️ Trampas y divergencias conocidas (verificar antes de fiarse)

1. ⚠️ **DIVERGENCIA VIVA · Semántica de caducidad en v1, v2 y v3.** Sus `processTaskExpiration`
   siguen tratando una tarea `ASSIGNED` como protegida de la caducidad, que es la semántica
   **anterior** al commit `466190a`. Solo **v4** replica el simulador corregido. Es una
   decisión deliberada del usuario (no se tocó al corregir el simulador), pero **sesga la
   comparación entre versiones**: los rollouts de v1–v3 son optimistas, creen que
   comprometerse tarde con una tarea aún la completa. Hay que resolverlo antes de la
   comparación definitiva de versiones sobre el catálogo final → duda **D-06**.
2. ~~Divergencia de consumo en fracaso~~ — **CORREGIDA (sesión 3)** en los cuatro solvers:
   `processTaskEnd` replica `simulator.cpp::endTask` (si `averageFailTime == 0`, el fracaso
   cuesta la demanda completa, no 0).
3. ~~Divergencia de `robot.time` al esperar coalición~~ — **CORREGIDA (sesión 3)**: las
   réplicas usan `tInfo.latestStart`, como el simulador real.
4. **Observabilidad: simplificación aceptada y auditada** (duda D-01, resuelta).
   `CBAA`/`CBBA` recorren `obs.getKnownRobots()` para recalcular las pujas ajenas y
   `Dec-MCTS::makeState()` lo copia como estado inicial del rollout: los tres usan el estado
   privado **actual** de los compañeros. Decisión del usuario: se mantiene como simplificación
   del mecanismo de comunicación. **Auditado: no hay fuga de información futura** — el
   desenlace y la duración muestreados viajan solo en el `payload` del evento, `Robot.time`
   de un robot que ejecuta vale `latestStart` (no su instante de fin) y `nextDecisionInfo` es
   una estimación con $\mathbb{E}[X\mid X>\delta]$. El *leak* temporal sigue cerrado.
5. **Landmine en `decayTo`**: la tabla de potencias de γ es un `static` de función,
   compartido por todas las instancias. Con un único γ por ejecución no hay problema, pero si
   alguna vez se corren robots con γ distintos en el mismo proceso, la tabla se reconstruye
   (40 000 entradas) en cada llamada.
4. **`START` nunca debe llegar al simulador** (lanza excepción). `IDLE` no está contemplado en
   `applyAction` (caería en el `switch` sin `default` ⇒ no hace nada). `decideNextAction` de
   v2/v3/v4 devuelve `IDLE` como valor por defecto teóricamente inalcanzable.
5. **Reproducibilidad limitada**: siembra con `std::random_device` en todas partes (solvers y
   simulador). No hay semilla configurable ⇒ solo se puede promediar sobre réplicas.
6. **`build/` está versionado en git** (binarios `.o` y el ejecutable). Debe salir en la
   limpieza (objetivo 3.1).
