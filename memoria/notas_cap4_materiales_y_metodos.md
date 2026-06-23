# Briefing para redactar el Capítulo 4 — «Materiales y métodos»

> **Propósito de este documento.** Reúne toda la información del repositorio necesaria
> para redactar el capítulo de Materiales y métodos sin acceso al código fuente. Está
> organizado como (1) lista de detalles a incluir y (2) la información concreta —clases,
> campos, valores, mecanismos y decisiones de diseño— necesaria para redactar cada uno.
> El modelo que redacte debe escribir en español, registro académico de TFM, en LaTeX.

---

## 0. Contexto y reglas de redacción (leer primero)

- El **Capítulo 2 (Marco Teórico)** ya formaliza el problema como **DEC-POGSMDP** y describe
  los algoritmos de forma genérica (definiciones, ecuaciones, pseudocódigo de Random,
  Greedy, CBAA, CBBA y Dec-MCTS). El **Capítulo 4 NO debe repetir** esas definiciones ni
  los algoritmos: debe describir **cómo se materializa en código** cada pieza formal y
  las **decisiones de implementación específicas**.
- Hilo conductor recomendado: **«de la fórmula al código»**. En cada apartado, abrir con
  la pieza formal del cap. 2 que se realiza y cerrar con la decisión concreta de implementación.
- **Reparto entre capítulos** (no invadir):
  - Cap. 4 (este): materiales, herramientas, arquitectura del software, clases, simulador,
    hiperparámetros, marco experimental (el *framework*).
  - Cap. 5 (Casos de estudio): el **catálogo de escenarios** concretos (Solomon, generados).
  - Cap. 6 (Experimentación): los **resultados** numéricos y ablaciones.
- **Etiquetas de referencia del cap. 2** que se pueden citar con `\ref{}` para no repetir:
  `sec:definicion`, `subsec:mrta`, `subsec:mrtau`, `subsec:online-offline`,
  `subsec:centralizado-descentralizado`, `sec:modelos`, `subsec:gsmdp`, `subsec:decposmdp`,
  `sec:formalizacion`, `subsec:mrta-mdp`, `subsec:mrtau-gsmdp`, `subsec:decmrtau-decpogsmdp`,
  `sec:algoritmos`, `subsec:baselines`, `subsec:pujas`, `subsec:dec-mcts`.
- Ecuaciones del cap. 2 reutilizables: `eq:recompensa` (recompensa del episodio),
  `eq:puja` (score CBAA/CBBA), `eq:duracion-esperada`, `eq:cond-exp` (esperanza condicionada),
  `eq:duct` (cota D-UCT), `eq:dist-decmcts` (construcción de la distribución),
  `eq:rollout-score` (política de rollout marginal-aware).

---

## 1. MATERIALES (software, bibliotecas, herramientas, hardware)

### 1.1 Lenguaje y entorno de desarrollo
- **C++17** (estándar fijado en CMake: `CMAKE_CXX_STANDARD 17`, `REQUIRED ON`).
- Sistema de construcción: **CMake (≥ 3.10) + Make**. Ejecutable resultante: `simulador`.
- Compilación típica: `cd build && cmake .. && make && ./simulador`. Recompilación limpia:
  `rm -rf build/* && cd build && cmake .. && make`.
- Sistema operativo de desarrollo/ejecución: **Linux sobre WSL2** (kernel 6.6,
  distribución Ubuntu). *(Completar con la versión exacta del compilador gcc/clang.)*
- **Justificación de C++**: rendimiento. El simulador y, sobre todo, los millones de
  pasos de simulación de los *rollouts* de Dec-MCTS son intensivos en cómputo; C++ permite
  control fino de memoria (punteros inteligentes, `unique_ptr` para los nodos del árbol) y
  ejecución nativa.

### 1.2 Bibliotecas y dependencias
- **yaml-cpp** (única dependencia externa, instalada a nivel de sistema;
  `find_package(yaml-cpp REQUIRED)`): carga de los ficheros de escenario y de configuración.
- **STL (biblioteca estándar)**, usada intensivamente:
  - Contenedores: `std::map`, `std::set`, `std::vector`, `std::unordered_map`.
  - Punteros inteligentes: `std::shared_ptr` (escenario, logger, solvers compartidos),
    `std::unique_ptr` (nodos del árbol de MCTS, propiedad exclusiva).
  - `std::priority_queue` (cola de eventos del simulador y de los rollouts).
  - `<random>`: `std::mt19937` (Mersenne Twister), `std::normal_distribution`,
    `std::uniform_real_distribution`, `std::uniform_int_distribution`.
  - `std::optional` (resultado de la planificación de fondo de los solvers anytime).
  - `<chrono>` (medición del tiempo de cómputo real, métrica `computing_time`).
  - `std::filesystem` (recorrido de directorios de escenarios y gestión de logs).
  - `std::erfc` (función de error complementaria, para la función de supervivencia normal).
- **Diseño header-only de los solvers**: cada algoritmo se implementa íntegramente en un
  `.hpp` bajo `include/tau/`. Solo el motor (simulador, escenario, logger, estado) tiene
  `.cpp` en `src/`.

### 1.3 Herramientas auxiliares y flujo de trabajo
- **Python 3** para la generación de escenarios (dos scripts; detalle en cap. 5).
- **`mrtau`** (herramienta de línea de comandos del proyecto) para post-procesado:
  - `mrtau metrics -i logs/ -o results.csv` → parsea los `.log` y calcula métricas a CSV.
  - `mrtau video -i test.log -o simulacion.mp4` → genera un vídeo de una ejecución.
- **`analyze_ablation.py`** para analizar resultados de estudios de ablación.
- **Git/GitHub** como control de versiones; el repositorio debe publicarse con **licencia
  de código abierto** y documentación online (requisito del TFM).

### 1.4 Hardware y entorno de ejecución *(completar con datos reales)*
- CPU (modelo, núcleos/hilos), RAM, almacenamiento. **Relevante** porque:
  - `computing_time` (tiempo real de pared) es una métrica registrada.
  - Dec-MCTS es **anytime** y consume todo el cómputo disponible; los resultados de
    calidad dependen de cuánto se llega a explorar por unidad de tiempo virtual.
- **Nota metodológica importante (tiempo virtual vs. tiempo real)**: el simulador avanza
  en *tiempo virtual*. Las rondas de planificación de fondo (`PLANNING_UPDATE`) se ejecutan
  de forma síncrona dentro del bucle de eventos, de modo que **el cómputo real por “latido”
  de tiempo virtual no está acotado por el reloj**: cada `PLANNING_UPDATE` ejecuta un número
  fijo de iteraciones de MCTS independientemente del tiempo de pared que tarden. Es decir,
  la naturaleza anytime se modela asumiendo que el robot “piensa gratis” mientras el tiempo
  virtual avanza. Conviene explicitar esta hipótesis.

---

## 2. ARQUITECTURA GENERAL DEL SOFTWARE

**Detalle a incluir:** visión de conjunto de los módulos y el flujo de datos, y el patrón
de diseño central (un solver independiente por robot + pizarra pública).

**Información para redactarlo:**
- Espacio de nombres único: `namespace tau`. Cabeceras en `include/tau/`.
- Módulos principales:
  - `Scenario` (`scenario.hpp/.cpp`): instancia **estática** del problema.
  - `State` (`state.hpp/.cpp`): estado **dinámico** global (verdad de fondo).
  - `Observation` (`observation.hpp`): vista **local de solo lectura** que recibe un solver.
  - `Action` (`action.hpp`): salida de un solver.
  - `ISolver` (`solver_interface.hpp`): interfaz polimórfica de todos los algoritmos.
  - `DistributedSimulator` (`simulator.hpp/.cpp`): motor de **eventos discretos**.
  - `RewardFunction` / `RewardFunction00` (`reward_interface.hpp`, `reward_00.hpp`).
  - `Logger` (`logger.hpp/.cpp`): registro de eventos a fichero `.log`.
  - Utilidades: `utils/estimation.hpp` (estimaciones probabilísticas), `utils/svector.hpp`
    (vector pequeño con media/desviación/min/max).
- **Patrón clave (decentralización en código):** el simulador mantiene un mapa
  `map<RobotID, shared_ptr<ISolver>>`; **cada robot tiene su propia instancia de solver**,
  registrada con `registerSolver(robotID, solver)`. Los solvers **no comparten estado
  directamente**: se comunican exclusivamente a través de la **pizarra pública**
  (ver §3.4 y §4).
- Flujo de datos (diagrama recomendado):
  `Escenario (YAML) → DistributedSimulator → cola de eventos → generateObservation() →
   ISolver::decideNextAction() → applyAction()/simulateTask() → Logger → .log`.

---

## 3. REPRESENTACIÓN DEL PROBLEMA EN CÓDIGO

### 3.1 Alias de tipos (semántica)
`NodeID = TaskID = RobotID = StationID = int`; `Time = Distance = BatteryLevel =
BatteryRate = Velocity = double`. Identificador nulo `NULL_ID = -1`.
`struct Position { double x, y; }`.

### 3.2 `Scenario` — la instancia estática  *(↔ Definición de instancia MRTA, `subsec:mrta`)*
**Detalle a incluir:** cómo se codifica la entrada del problema (Definición 4.1 del cap. 2).

**Campos de `Scenario`:** `name`, `height`, `width`, `initialTime`, y cuatro contenedores:
`map<NodeID,Node> nodes`, `map<TaskID,TaskInfo> tasks`, `map<RobotID,RobotInfo> robots`,
`map<StationID,StationInfo> stations`. Método estático `loadFromYAML(filepath)`.
- `distanceBetween(a,b)`: usa la distancia de la arista registrada si existe; en caso
  contrario, distancia **euclídea** entre coordenadas. (El mapa es un grafo de nodos con
  vecinos ponderados por distancia.)
- **`Node`**: `id`, `description`, `coords (Position)`, `nearestStation` (estación de
  recarga más cercana, precomputada), `neighbors: map<NodeID,Distance>`.
- **`TaskInfo`** (parámetros estáticos de cada tarea):
  `id`, `node`, `description`, `earliestStart`, `latestStart` (la ventana $[t_j^s,t_j^l]$),
  `successProb` ($\rho_j$), `averageSuccessTime`/`stdSuccessTime` ($\mu_j^s,\sigma_j^s$),
  `averageFailTime`/`stdFailTime` ($\mu_j^f,\sigma_j^f$),
  `averageSuccessDemand`/`averageFailDemand` (consumo medio de batería **por trabajador**
  según éxito/fracaso), `requiredWorkers` ($q_j$), `maxAttempts` (por defecto 1).
- **`RobotInfo`**: `id`, `initialNode`, `description`, `initialBatteryLevel`,
  `batteryCapacity` ($B_i$), `navigationVelocity` ($v_i$), `batteryRateWhileNavigating`
  (tasa de consumo al desplazarse), `capabilities: set<TaskID>` (la función de
  compatibilidad $\textit{Cap}(\cdot,\cdot)$ codificada como conjunto de tareas que el
  robot puede ejecutar).
- **`StationInfo`**: `id`, `node`, `description`. **La recarga se modela como instantánea
  y completa** (equivalente a un cambio de batería): no hay tasa de recarga; recargar
  deja la batería a `batteryCapacity`.

### 3.3 `State`, `Robot`, `Task` — el estado dinámico  *(↔ $s^{R_i}$, $s^{T_j}$, `subsec:mrta-mdp`)*
**Detalle a incluir:** distinción entre **estado estático (Scenario)** y **estado dinámico
(State)**; correspondencia con el estado global $s=(t, s^{R_1..n}, s^{T_1..m})$.

- `State` contiene `shared_ptr<const Scenario>`, `map<RobotID,Robot> robots`,
  `map<TaskID,Task> tasks`. Accesores de solo lectura y referencias mutables
  (`getRobot`, `getTask`) que **solo el simulador** usa para actualizar la física.
  Método `isFinal()` (condición de finalización global).
- **`struct Robot`** (estado dinámico de un robot):
  - `status` (`RobotStatus`), `batteryLevel` ($b_i$), `time` (**instante en que el robot
    quedará libre**), `node` (ubicación actual), `onTask` (tarea a la que está vinculado,
    `NULL_ID` si ninguna), `completedTasks`, `travelDistance` (acumulada), `failed`.
  - **`enum RobotStatus { AVAILABLE, FINISHED, WAITING, EXECUTING, FAILED }`**.
    `WAITING` = ha llegado a la tarea y espera a que se complete la coalición;
    `EXECUTING` = la tarea está en marcha; `FAILED` = se quedó sin batería.
- **`struct Task`** (estado dinámico de una tarea):
  - `status` (`TaskStatus`), `assignedWorkers` (cuántos robots han llegado/están
    comprometidos), `initTime` (instante de inicio efectivo de la ejecución),
    `finalTime`, `attempts`.
  - **`enum TaskStatus { PENDING, ASSIGNED, EXECUTING, COMPLETED, FAILED }`**.
    `PENDING` = aún faltan trabajadores; `ASSIGNED` = coalición completa, ejecución
    programada pero no iniciada; `EXECUTING` = en curso; terminal `COMPLETED`/`FAILED`.

### 3.4 `Action` — el espacio de acciones  *(↔ $\mathcal{A}_i$, `subsec:decmrtau-decpogsmdp`)*
**Detalle a incluir:** las acciones locales que un solver puede devolver.

- **`enum Action::Type { FINISH, RECHARGE, EXECUTE_TASK, START, IDLE }`**, con un campo
  `target` (`TaskID`) usado por `EXECUTE_TASK`.
- Correspondencia con el modelo: `EXECUTE_TASK(T_j)`, `RECHARGE`, `FINISH` son las tres
  macro-acciones del DEC-POGSMDP. `START` e `IDLE` son **internos**: `START` no debe llegar
  nunca al simulador (si lo recibe, lanza una excepción — solo existe “en la mente del
  solver”); `IDLE` se reserva para cuando un robot no puede hacer nada.

### 3.5 `Observation` — la vista local  *(↔ $\Omega_i$, función de observación $\mathcal{O}$)*
**Detalle a incluir:** qué información recibe un solver, cómo se realiza la observabilidad
parcial, y **la nota de transparencia** (importante).

- Tipos asociados (definidos aquí): **`Bundle = vector<TaskID>`** (secuencia de tareas
  planificada) y **`Distribution = map<Bundle,double>`** (distribución de probabilidad
  sobre planes). `NextDecisionInfo = pair<Time, NodeID>`.
- **Campos de `Observation`** (todos de solo lectura):
  - `currentTime` (reloj global $t$), `myId`, `myState` (el propio `Robot`, $s^{R_i}$),
  - `knownTasks: map<TaskID,Task>` (estado de **todas** las tareas $\mathbf{s}^T$ — totalmente
    observable),
  - `knownRobots: map<RobotID,Robot>` (estado de los demás robots),
  - `knownDistributions: map<RobotID,Distribution>` (**la pizarra pública**: las
    distribuciones $\psi^k$ comunicadas por todos los robots — el canal de comunicación),
  - `nextDecisionInfo: map<RobotID,NextDecisionInfo>` (instante estimado en que cada robot
    quedará libre y su nodo en ese momento).
- **NOTA DE TRANSPARENCIA (incluir explícitamente y con honestidad).** El modelo formal
  (cap. 2) establece que un robot **no observa** el estado interno ni la ubicación de los
  demás ($s^{R_k}, k\neq i$). Sin embargo, la estructura `Observation` **sí contiene**
  `knownRobots` y `nextDecisionInfo`. La forma de reconciliarlo: `Observation` es un
  **contenedor de conveniencia** del simulador; la observabilidad parcial se respeta **por
  contrato en cada algoritmo**:
  - Los *baselines* (Random/Greedy) ignoran por completo a los demás robots.
  - CBAA/CBBA y Dec-MCTS usan **solo la información comunicada** (pizarra de distribuciones
    / pujas) y estimaciones derivadas, no el estado interno privado de los compañeros.
  - Conviene declarar que esto es una decisión de implementación y que un sistema realmente
    desplegado restringiría la `Observation` por robot; el simulador centraliza la estructura
    por simplicidad, pero los solvers se ciñen al modelo distribuido.

---

## 4. LA PIZARRA PÚBLICA (realización de la comunicación)  *(↔ mensajes $\mathcal{M}_{in}$)*

**Detalle a incluir:** cómo se materializa el intercambio de información del DEC-POGSMDP.

**Información para redactarlo:**
- El simulador mantiene `map<RobotID, Distribution> globalDistributions`, la **pizarra
  pública**. Cada vez que se genera una `Observation`, esta pizarra se inyecta entera.
- Es el mecanismo concreto que sustituye la observabilidad global ausente: en lugar de
  ver el estado de los compañeros, cada robot lee las **distribuciones $\psi^k$ sobre
  planes** que estos han publicado (Dec-MCTS). Los métodos de pujas usan análogamente la
  pizarra para difundir/leer pujas.
- Es una abstracción de comunicación **idealizada** (memoria compartida, sin pérdidas ni
  latencia). Conviene señalarlo como simplificación: el modelo formal admite comunicación
  acotada/intermitente; la implementación usa una pizarra fiable, y Dec-MCTS es tolerante
  a no recibir mensajes (planifica con la última distribución conocida o una por defecto).

---

## 5. EL SIMULADOR DISTRIBUIDO DE EVENTOS DISCRETOS

> Esta es la sección de más enjundia. Realiza en código la **cola de eventos del GSMDP**
> (`subsec:mrtau-gsmdp`). Conviene explicarla con detalle.

### 5.1 La cola de eventos y los tipos de evento
**Detalle a incluir:** estructura del evento, tipos y orden de prioridad.

- `DistributedSimulator` mantiene `std::priority_queue<Event, vector<Event>, greater<Event>>`,
  un **min-heap por tiempo**.
- **`struct Event`**: `time`, `type` (`EventType`), `robotID`, `taskID`,
  `randomTieBreaker` (entero aleatorio grande), `payload` (entero para transportar datos
  ocultos entre eventos; p. ej. éxito=1/fracaso=0).
- **`enum EventType`** con prioridad explícita (valores menores se procesan **antes** a
  igualdad de tiempo): `TASK_END=0`, `TASK_START=1`, `TASK_EXPIRATION=2`,
  `ROBOT_DECISION=3`, `PLANNING_UPDATE=4`.
- **Orden de extracción (`operator>`)**: primero menor `time`; a igual tiempo, menor `type`
  (la prioridad anterior); a igual tiempo y tipo, menor `randomTieBreaker`. **El desempate
  aleatorio es deliberado**: si varios robots deben decidir en el mismo instante, su orden
  se aleatoriza para no introducir sesgos por `RobotID`.
- Justificación del orden: las finalizaciones (`TASK_END`) deben procesarse antes que los
  inicios y que las decisiones del mismo instante para que el estado quede coherente; el
  “pensamiento de fondo” (`PLANNING_UPDATE`) tiene la prioridad más baja.

### 5.2 El bucle principal `run()` y el ciclo de vida de la ejecución
**Detalle a incluir:** las fases de una ejecución completa.

1. **Cronómetro**: se arranca un `std::chrono::high_resolution_clock` para medir
   `computing_time` (tiempo real total).
2. **`initLogging()`**: vuelca la cabecera del log (nombre, nº de tareas/robots/estaciones,
   tamaño del escenario) y los eventos de aparición (`spawn`) de estaciones, tareas y robots.
3. **FASE 0 — Warm-up del planificador (solo solvers anytime).** Antes de encolar ninguna
   decisión física, se ejecutan `WARMUP_ROUNDS` rondas en las que, para cada robot cuyo
   solver `requiresContinuousPlanning()` es verdadero, se genera su `Observation`, se llama a
   `performBackgroundPlanning()` y se **publica** la distribución resultante en la pizarra.
   Objetivo: que Dec-MCTS llegue a la primera decisión real con un árbol ya poblado y con
   las distribuciones de los vecinos ya intercambiadas (evita decidir “en frío”). Ver §6.
4. **Inicialización de la cola**:
   - Para cada robot: `scheduleRobotDecision(initialTime)`.
   - Si el solver es anytime: además `schedulePlanningUpdate(initialTime + PLANNING_INTERVAL)`.
   - Para cada tarea: se encola un `TASK_EXPIRATION` en su `latestStart`.
5. **Bucle principal** `while (!cola.vacía && !state.isFinal())`: extrae el evento de menor
   tiempo, **avanza el reloj global** (nunca retrocede) y lo despacha según su tipo (§5.3).
6. **Cierre**: se detiene el cronómetro, se evalúa `reward->getValue(state)` y se vuelca
   `finalLogging()` con todas las métricas (§9).

### 5.3 Tratamiento de cada tipo de evento
- **`ROBOT_DECISION`**: si el robot no está `AVAILABLE`, se ignora (evento obsoleto). Si lo
  está, se llama a `generateObservation()` → `solver->decideNextAction()` → `applyAction()`.
- **`PLANNING_UPDATE`** (motor anytime): si el robot está `FAILED`/`FINISHED`, se detiene su
  pensamiento. En otro caso: `performBackgroundPlanning()`, y si devuelve distribución se
  **publica en la pizarra**; después **se re-encola otro `PLANNING_UPDATE`** en
  `globalTime + PLANNING_INTERVAL` (latido continuo).
- **`TASK_START`** → `startTask()`. **`TASK_END`** → `endTask(payload==1)`.
  **`TASK_EXPIRATION`** → `expireTask()`.

### 5.4 Aplicación de acciones y modelo físico
- `applyAction()`: `EXECUTE_TASK`→`simulateTask`, `RECHARGE`→`simulateRecharge`,
  `FINISH`→`simulateFinish`, `START`→excepción (no debe ocurrir).
- **`simulateTask(robot, task)`**:
  - **Barrera de defensa física (resolución de conflictos):** si al ir a ejecutar la tarea
    esta ya **no está `PENDING`** o ya tiene todos los `requiredWorkers`, es físicamente
    imposible unirse. El robot se queda quieto **+1 s virtual** y se re-encola una decisión,
    para que su solver lea una `Observation` actualizada. Este es el mecanismo concreto que
    resuelve los conflictos inherentes a la descentralización (dos robots eligen la misma
    tarea casi simultáneamente).
  - **Navegación**: `travelTime = distancia / navigationVelocity`; consumo de batería
    `= rate_navegación · travelTime`. Si la batería resultante < 0 → el robot **muere**
    (`FAILED`), se calcula el instante de agotamiento y se registra.
  - Si llega con éxito: acumula `travelDistance`, fija `node`, pasa a `WAITING`, fija
    `onTask`, incrementa `assignedWorkers`, y calcula
    `task.initTime = max(initTime_previo, instante_de_llegada, earliestStart)`.
  - Cuando `assignedWorkers == requiredWorkers`: la tarea pasa a `ASSIGNED` y se encola un
    `TASK_START` en `initTime`.
- **`startTask(task)`** — **aquí se materializa la incertidumbre (sample-at-fire):**
  - Pasa a `EXECUTING`, incrementa `attempts`.
  - **Muestrea el desenlace**: `success = U(0,1) < successProb`.
  - **Muestrea la duración** de una normal `N(μ,σ)` con los parámetros de éxito o de fracaso
    según el desenlace (truncada a ≥ 0).
  - Calcula el consumo de batería de ejecución como `rate = demanda/tiempo_medio` × duración;
    si algún trabajador no tiene batería suficiente, el desenlace se fuerza a fracaso y se
    trunca la duración al instante de agotamiento.
  - **Programa el `TASK_END`** en `initTime + execTime`, **llevando el desenlace oculto en
    `payload`** (1=éxito, 0=fracaso).
  - **PUNTO CLAVE para enlazar con la teoría:** este `payload` es la realización en código
    del **“reloj oculto” del GSMDP**. El resultado y la duración se muestrean en el momento
    de **disparo** (`sample-at-fire`) y permanecen ocultos en el evento; ningún robot puede
    anticiparlos → **ausencia de *leak* temporal** (Figura del cap. 2).
- **`endTask(task, success)`**: recupera `execTime = globalTime − initTime`, descuenta la
  batería de ejecución a cada trabajador; si la batería ≤ 0 → `FAILED`, en otro caso el robot
  vuelve a `AVAILABLE` y se le encola una nueva decisión. La tarea pasa a `COMPLETED`/`FAILED`.
- **`simulateRecharge`**: navega a la estación más cercana (`nearestStation` precomputado),
  consume batería de viaje (puede morir por el camino); al llegar, **batería = capacidad
  máxima** (recarga instantánea), vuelve a `AVAILABLE` y se le encola una decisión.
- **`simulateFinish`**: navega a la estación más cercana y pasa a `FINISHED` (no vuelve a
  decidir). Puede morir por el camino si no le llega la batería.
- **`expireTask`**: si la tarea ya está `COMPLETED`/`FAILED`/`EXECUTING`, se ignora. (Detalle
  fino corregido: una tarea `ASSIGNED` cuyo `initTime > latestStart` **también caduca** en
  `latestStart`, porque empezaría fuera de ventana.) Si caduca, pasa a `FAILED` y **libera a
  los robots `WAITING`** vinculados a ella, devolviéndolos a `AVAILABLE` con una nueva decisión.

### 5.5 La interfaz `ISolver` y la distinción on-request / anytime
**Detalle a incluir:** la abstracción que permite tratar a todos los algoritmos por igual.

- Métodos:
  - `Action decideNextAction(obs, scenario)` — **obligatorio**; se invoca en cada instante
    de decisión y devuelve una única acción.
  - `bool requiresContinuousPlanning()` — por defecto `false`; los anytime devuelven `true`.
  - `optional<Distribution> performBackgroundPlanning(obs, scenario)` — por defecto
    `nullopt`; solo lo implementan los anytime (Dec-MCTS), que devuelven su nueva $\psi^i$.
- Esta interfaz materializa la distinción del cap. 2: **on-request** (Random, Greedy, CBAA,
  CBBA: sin estado entre decisiones, calculan al ser invocados) vs **anytime** (Dec-MCTS:
  mantiene un árbol persistente y piensa de fondo entre decisiones).

---

## 6. HIPERPARÁMETROS DEL SIMULADOR (para solvers anytime)

**Detalle a incluir:** los dos parámetros globales del motor que gobiernan la planificación
de fondo, con su valor y justificación.

- **`PLANNING_INTERVAL = 0.1`** (segundos de **tiempo virtual**). Periodo del “latido” entre
  eventos `PLANNING_UPDATE`: cada robot anytime re-planifica de fondo cada 0,1 de tiempo
  virtual. Más pequeño ⇒ más rondas de pensamiento (más cómputo y más intercambios de
  distribución) por unidad de tiempo virtual.
- **`WARMUP_VIRTUAL_TIME = 30.0`** y **`WARMUP_ROUNDS = ⌈WARMUP_VIRTUAL_TIME / PLANNING_INTERVAL⌉
  = 300`**. Número de rondas de planificación de fondo que se ejecutan **antes** de encolar
  la primera decisión física (FASE 0). Da a Dec-MCTS un árbol ya desarrollado y unas
  distribuciones de vecinos ya intercambiadas en `t = initialTime`. Sin warm-up, la primera
  decisión se tomaría con un árbol vacío (de ahí también el mecanismo de iteraciones de
  emergencia del propio solver, §7).
- Reiterar la **hipótesis tiempo virtual ↔ cómputo real**: el número de iteraciones por
  latido es fijo (no depende del reloj de pared); modela el supuesto de que el robot dispone
  del tiempo entre decisiones para pensar.

---

## 7. DEC-MCTS v4: DECISIONES E HIPERPARÁMETROS DE IMPLEMENTACIÓN

> El cap. 2 ya describe el algoritmo Dec-MCTS (árbol local por robot, D-UCT, comunicación de
> distribuciones, rollout *marginal-aware*, construcción de $\psi^i$ por visitas, re-enraizado).
> El cap. 4 debe centrarse en **detalles de implementación y los hiperparámetros concretos**.
> **La versión definitiva es la v4** (no mencionar v1/v2/v3 salvo de pasada). El solver
> efectivamente usado en los experimentos es la variante **`dec-mcts-v4-g9999`** (γ = 0.9999).

### 7.1 Estructura del nodo y descuento perezoso
- **`struct MCTSNode`**: `action`, `parent`, `children` (vector de `unique_ptr`),
  `N_disc` (visitas descontadas), `Q_disc` (suma de recompensas descontada), `lastTick`.
  La media empírica descontada es `Q_disc / N_disc`.
- **Descuento perezoso (lazy)**: en lugar de multiplicar por γ todos los nodos en cada
  iteración, cada nodo guarda `lastTick` y, al accederse, aplica `γ^(tickActual − lastTick)`
  de una sola vez. Se usa una **tabla de potencias de γ precomputada y cacheada** (se
  recalcula solo si cambia γ; tope ~40000 entradas o hasta que el factor < 1e-15) para
  evitar llamadas repetidas a `pow`/`exp`. Si el factor es despreciable, los estadísticos
  se ponen a 0.
- Operaciones del nodo: `findChild(action)`, `mostVisitedChild` (aplica el descuento a los
  hijos y devuelve el de mayor `N_disc`), `consolidateTimestamps` (propaga el descuento por
  el subárbol tras re-enraizar).

### 7.2 Las 7 decisiones de diseño de v4 (respecto a versiones previas)
Incluir como “refinamientos de implementación” (no como teoría nueva):
1. **Política de rollout consciente de ventanas temporales.** El score pasa de
   `valor/(viaje+ejecución)` a
   `score = valor/(viaje + ESPERA + ejecución) · (1 + W_u · τ/(τ+slack))`, con
   `espera = max(0, earliestStart − llegada)`, `slack = latestStart − llegada`,
   `τ = viaje + ejecución`. Prioriza tareas cuya ventana se cierra pronto (estilo
   heurísticas de inserción para *orienteering* con ventanas temporales, TOPTW).
   (Refina la `eq:rollout-score` del cap. 2 añadiendo el factor de urgencia.)
2. **Widening progresivo + expansión ordenada por heurística.** El nº máximo de hijos de un
   nodo se limita a `1 + PW_C · N_disc^PW_ALPHA`; el siguiente hijo a expandir es el de mayor
   score heurístico (*progressive bias*). Evita árboles planos y sesgados con 12–24 acciones
   y solo 30 iteraciones por latido.
3. **Selección D-UCT filtrada por factibilidad.** En la selección solo se consideran hijos
   cuya acción **está disponible** en el estado simulado (tarea aún `PENDING`, dentro de
   ventana, etc.), para no contaminar las estadísticas con acciones imposibles.
4. **Poda de hijos obsoletos de la raíz.** Al inicio de cada `performBackgroundPlanning`/
   `decideNextAction` se eliminan los hijos de la raíz cuya acción ya es imposible según la
   `Observation` (tarea no `PENDING`, ventana inalcanzable, `RECHARGE` con batería llena).
   Evita que el robot real elija acciones muertas y pague la penalización defensiva de 1 s.
5. **Factibilidad de batería completa.** Para comprometerse con una tarea se exige batería
   para **navegar Y para la demanda esperada de ejecución** (no solo para llegar), evitando
   la catástrofe doble “llego sin batería → la tarea falla → el robot muere”.
6. **Tie-break de *earliness* en la recompensa.** `reward = completadas/n + (W_e/n)·earliness_media`,
   con `W_e < 1` para que **completar una tarea más siempre domine** al bono; el bono rompe
   las mesetas (muchos planes completan el mismo número) prefiriendo arrancar las tareas
   antes dentro de su ventana.
7. **`extractDistribution` acumula** (`+=`) cuando varios hijos mapean al mismo bundle
   (p. ej. `RECHARGE` y `FINISH` → bundle vacío).

### 7.3 El modelo generativo interno (réplica del simulador)
**Detalle importante a incluir:** los *rollouts* de Dec-MCTS se ejecutan sobre un
**simulador interno propio**, réplica de `DistributedSimulator` sin logging:
- Reconstruye el estado a partir de la `Observation` (`makeState`) y una **cola de eventos
  propia** (`makeEventQueue`) inicializada igual que el simulador real (decisiones de robots
  `AVAILABLE`; `TASK_EXPIRATION` para `PENDING`; `TASK_START` —y posible expiración— para
  `ASSIGNED`; `TASK_END` con desenlace muestreado para `EXECUTING`). Mismas reglas de física.
- En cada iteración: se **muestrea un bundle por vecino** de sus distribuciones $\psi^k$
  recibidas; se precomputa la **probabilidad de bloqueo** `blockingProb` por tarea (si el nº
  de vecinos que la incluyen en su bundle **alcanza `requiredWorkers`**, la tarea se considera
  bloqueada con probabilidad igual a su propia **`successProb`** —es decir, se asume que la
  coalición vecina la completará con la probabilidad de éxito de la propia tarea—; en caso
  contrario, `blockingProb = 0`, porque sin la participación de este robot no podrá completarse);
  el robot planificador sigue D-UCT mientras está dentro del árbol y la política de rollout
  fuera; cada vecino sigue su bundle muestreado (o la política de rollout si se agota o deja
  de ser válido).
- En la política de rollout, ese `blockingProb` entra en el numerador del score como
  `valor = successProb · (1 − blockingProb) + ε · successProb` (la $\varepsilon$ mantiene un
  orden razonable entre tareas “bloqueadas”). Codifica el razonamiento sobre coaliciones que,
  en las pujas, hacía el factor de cooperación.
- **Decisión de fidelidad crítica:** esta réplica **debe coincidir exactamente** con la física
  del simulador real (mismas reglas de caducidad, coaliciones, consumo). Una divergencia entre
  ambos invalida las estimaciones (se puede mencionar como lección aprendida: la duplicación
  del modelo generativo es una fuente de error sutil).
- `useDiffReward` (doble rollout para recompensa diferencial $f^r$): **desactivado por defecto**
  (las ablaciones lo confirmaron como peor/innecesario).

### 7.4 Tabla de hiperparámetros de Dec-MCTS v4 (valores definitivos)

| Hiperparámetro | Valor | Qué controla |
|---|---|---|
| `gamma` (γ, D-UCT) | **0.9999** | Factor de descuento de las estadísticas D-UCT. Muy próximo a 1: conserva casi toda la información (≈ UCT) pero atenúa gradualmente las simulaciones obsoletas tras un *breakpoint*. **Valor definitivo** (variante `-g9999`). |
| `C_EXPLORE` ($C_p$) | **0.7** ($\approx 1/\sqrt2$) | Constante de exploración. El bono efectivo de la cota es **2·C_EXPLORE = 1.4** (ver `eq:duct`). Satisface la condición teórica $C_p > 1/\sqrt8$ de D-UCT. |
| `ITERATIONS_PER_CALL` | **30** | Iteraciones de MCTS por cada latido de planificación de fondo (`PLANNING_UPDATE`). |
| `EMERGENCY_ITERS` | **300** | Iteraciones que se ejecutan en el acto si, al pedirse una decisión, el árbol está vacío. |
| `MAX_ROLLOUT_EVENTS` | **200000** | Tope de eventos por rollout (salvaguarda contra bucles). |
| `PW_C` | **1.5** | Coeficiente del widening progresivo (máx. hijos = 1 + PW_C·N_disc^PW_ALPHA). |
| `PW_ALPHA` | **0.6** | Exponente del widening progresivo. |
| `URGENCY_WEIGHT` ($W_u$) | **1.0** | Peso del factor de urgencia (ventana temporal) en la política de rollout. |
| `EARLINESS_WEIGHT` ($W_e$) | **0.5** | Peso del bono de *earliness* en la recompensa (< 1 para que completar una tarea domine). |
| `RECHARGE_SCORE` | **1e-6** | Score heurístico mínimo de `RECHARGE` (el widening lo expande el último salvo que no haya tareas factibles). |
| `useDiffReward` | **false** | Doble rollout para recompensa diferencial (desactivado). |
| `blockingProb` (prob. de bloqueo) | **= `successProb` de la tarea** si la cubren ≥ `requiredWorkers` vecinos; **0** en caso contrario | No es una constante: codifica el razonamiento sobre coaliciones. Si los vecinos cubren la coalición, se descuenta el valor de la tarea por la probabilidad de que la completen ellos (`1 − successProb` de aprovechamiento residual); si no, la tarea sigue valiendo pleno porque sin este robot no se completará. |

- **Constructor**: `DecMCTSSolverV4(RobotID id, double gamma = 0.9999, bool useDiffReward = false,
  double cExplore = 0.7)`. `requiresContinuousPlanning()` devuelve `true`.
- **Política de decisión y re-enraizado**: ante una decisión real devuelve el hijo de la raíz
  con **mayor `N_disc`** (explotación robusta, no la mayor media); luego **re-enraíza** el árbol
  en ese hijo, conserva el subárbol y consolida los `timestamps` (aplica el descuento pendiente),
  reutilizando el conocimiento en la siguiente ronda.
- **Aviso de coherencia (resolver antes de redactar):** un comentario del código menciona
  γ = 0.999 “por compatibilidad de logs”, pero el **valor por defecto real del constructor y la
  variante usada en los experimentos es γ = 0.9999**, confirmada como la mejor en la ablación
  (16 escenarios × 4 réplicas: 0.6171 vs 0.6069 con 0.999; C = 0.7 mejor que 0.35). **El capítulo
  debe fijar γ = 0.9999** y el cap. 2 no debe contradecirlo.

---

## 8. LA FUNCIÓN DE RECOMPENSA EN CÓDIGO  *(↔ `eq:recompensa`)*

**Detalle a incluir:** cómo se evalúa la calidad de un episodio (no la del rollout, que es
interna a MCTS, sino la métrica final del simulador).

- **`RewardFunction00`**: combinación lineal de **8 componentes normalizados** $a_1..a_8\in[0,1]$
  con pesos $k_1..k_8$ que **deben sumar 1** (si no, lanza excepción):
  - $a_1$ = completadas/total, $a_2 = 1 -$ pendientes/total, $a_3 = 1 -$ fallidas/total.
  - $a_4$ = robots disponibles/n, $a_5$ = robots finalizados/n, $a_6 = 1 -$ robots fallidos/n.
  - $a_7 = 1 -$ ratio de makespan máximo, $a_8 = 1 -$ ratio de distancia total recorrida.
  - `getValue = Σ k_i·a_i`.
- **Configuración usada en los experimentos**: `reward00` con **$k_1 = 1$ y el resto 0**, es
  decir, la **fracción de tareas completadas** (recompensa $\in [0,1]$). Es el objetivo que se
  reporta.
- **Limitaciones de calibración a mencionar**: el ratio de makespan usa un **horizonte fijo
  hardcodeado (1080.0)** y el de distancia un máximo `10000 · nº_robots`. Son constantes de
  normalización que solo afectan a $a_7$/$a_8$ (inactivos con $k_1=1$), pero conviene señalarlo
  como decisión de implementación / posible mejora.

---

## 9. EL MARCO EXPERIMENTAL (el *framework*, no los resultados)

> Los escenarios concretos → cap. 5; los resultados → cap. 6. Aquí solo el mecanismo.

### 9.1 Configuración del experimento
- **`experiment_config.yaml`** define: `solvers` (lista de nombres), `reward_functions`
  (lista) y `replicas` (entero). Configuración de referencia usada:
  `solvers: [random, greedy, cbaa, cbba, dec-mcts-v4-g9999]`, `reward_functions: [reward00]`,
  `replicas: 3`.
- **Nombres de solver reconocidos** (mapeados a clases en `main.cpp`): `greedy`, `random`,
  `cbaa`, `cbba`, `dec-mcts-v4-g9999` (la variante definitiva). (Existen otras variantes de
  ablación —`dec-mcts-v4`, `-c035`, etc.— pero no son la configuración final.)

### 9.2 El ejecutor de experimentos (`main.cpp`)
- **Bucles anidados**: escenarios × solvers × funciones de recompensa × réplicas.
- **Una instancia de solver por robot** se crea en cada réplica (fábrica por nombre), y el
  escenario se **recarga desde el YAML en cada réplica** (restaura baterías, tiempos, etc.).
- **Resumible / idempotente**: antes de ejecutar, comprueba si el `.log` ya existe y termina
  con la línea `metric: computing_time;`; si es así, **salta** esa combinación. Permite
  reanudar tandas interrumpidas sin recomputar.
- **Rutas por argumentos de CLI** (para no recompilar entre tandas):
  `./simulador <scenariosPath> <logsDir> [configPath]`.
- Nombre de cada log: `<escenario>_<solver>_<reward>_<NNN>.log` (réplica con 3 dígitos).


### 9.3 Registro (`Logger`) y *pipeline* de datos
- El `Logger` escribe un **log textual basado en eventos**: líneas `info: clave; value: ...`,
  eventos de simulación (aparición de estaciones/tareas/robots, navegación, consumo de batería,
  ejecución y resolución de tareas, robot finalizado/fallido/recargado) y, al final, líneas
  `metric: clave; value: ...`.
- **Métricas finales registradas** (vía utilidad `SVector` con media/sd/min/max): agentes
  disponibles/fallidos/finalizados; tareas completadas/pendientes/fallidas; makespan por robot
  (media, sd, máx, mín); distancia recorrida (media, sd, máx, mín, suma); tareas completadas por
  robot (media, sd, máx, mín); `final_reward`; `computing_time`.
- **Flujo de post-procesado**: `.log` → `mrtau metrics -i logs/ -o results.csv` → CSV →
  análisis (`analyze_ablation.py`). El log es la **interfaz** entre la simulación (C++) y el
  análisis (Python). `mrtau video` puede reconstruir un vídeo de una ejecución a partir de su log.

---

## 10. LISTA DE DECISIONES/NOTAS TRANSVERSALES A DESTACAR (resumen)

Marcar estas como “decisiones de diseño” a lo largo del capítulo:
1. **Un solver independiente por robot** → descentralización real en la arquitectura.
2. **Pizarra pública** como realización idealizada de la comunicación (memoria compartida,
   sin pérdidas); Dec-MCTS tolerante a no recibir mensajes.
3. **Transparencia sobre observabilidad**: la `Observation` expone más de lo que el modelo
   permite; la observabilidad parcial se respeta **por contrato** en cada solver (§3.5).
4. **Cola de eventos = realización del GSMDP**; **desenlace oculto en `payload`** = *sample-at-fire*
   = ausencia de *leak* temporal (§5.4).
5. **Barrera de defensa física (+1 s)** = resolución de conflictos de la descentralización.
6. **Modelo generativo interno de Dec-MCTS = réplica del simulador**; la fidelidad entre ambos
   es crítica (fuente de error sutil).
7. **Tiempo virtual vs. cómputo real**: la naturaleza anytime asume cómputo “gratis” entre
   decisiones; nº de iteraciones por latido fijo, no acotado por el reloj de pared.
8. **Reproducibilidad limitada** (siembra con `random_device`; se promedia sobre réplicas).
9. **γ definitivo = 0.9999** (variante `dec-mcts-v4-g9999`); coherencia con el cap. 2.
10. **Recarga instantánea** (cambio de batería) y **horizonte de makespan hardcodeado (1080)**
    como simplificaciones/limitaciones del modelo de coste.

---

## 11. ESQUELETO DE SECCIONES SUGERIDO (orientativo, el modelo puede recohesionarlo)

1. Introducción del capítulo (objetivo; hilo «del modelo al código»; reparto con cap. 5/6).
2. Materiales: lenguaje y entorno; bibliotecas; herramientas auxiliares; hardware; repositorio
   y licencia. → §1.
3. Arquitectura general del software (módulos + flujo de datos + patrón un-solver-por-robot). → §2.
4. Representación del problema en código: `Scenario`; `State`/`Robot`/`Task`; `Action`;
   `Observation` (+ nota de observabilidad); la pizarra pública. → §3, §4.
5. El simulador distribuido de eventos discretos: cola y tipos de evento; bucle `run()`;
   tratamiento de eventos; modelo físico; incertidumbre (sample-at-fire / no-leak); barrera de
   conflictos; interfaz `ISolver` (on-request vs anytime). → §5.
6. Hiperparámetros del simulador: `PLANNING_INTERVAL`, `WARMUP_VIRTUAL_TIME`/`WARMUP_ROUNDS`;
   tiempo virtual vs real. → §6.
7. Dec-MCTS v4: decisiones de implementación (nodo, descuento perezoso, las 7 mejoras, modelo
   generativo interno) + tabla de hiperparámetros. → §7.
8. La función de recompensa en código (`RewardFunction00`). → §8.
9. Marco experimental: configuración; ejecutor; reproducibilidad; logger y pipeline; utilidades
   de estimación. → §9.
10. Resumen y enlace con los casos de estudio (cap. 5).
