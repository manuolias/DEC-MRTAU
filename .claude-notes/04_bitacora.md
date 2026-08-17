# 04 — Bitácora de sesiones

Entrada más reciente arriba. Formato: fecha · objetivo · qué se hizo · qué queda.

---

# 📍 ESTADO AL CIERRE DE LA SESIÓN 3 (2026-08-06) — LEER ESTO PRIMERO

**Objetivo 1.1 (cerrar el solver Dec-MCTS): CERRADO.** No se aplicarán más cambios al solver
por ahora (decisión del usuario). El algoritmo no es perfecto, pero está entendido y sin
defectos conocidos, y sabemos en qué condiciones gana. Las mejoras de diseño pendientes
(B1–B5) quedan documentadas y aparcadas en `05_dudas.md`.

**Siguiente objetivo: 1.2 — definición del catálogo de escenarios.** Punto de partida
obligado: sección **"Criterios para el diseño del catálogo definitivo (C-1 a C-7)"** de
`03_experimentos.md`. Resume, con datos, por qué los catálogos actuales no discriminan.

**Las tres cosas que hay que recordar:**
1. **Dec-MCTS gana condicionalmente**: con ≥4 robots y planes predecibles (determinista,
   ventanas estrechas tipo 1E). Con 2-3 robots o en régimen estocástico, no.
2. **La ventaja viene realmente del canal de comunicación** (ablación C1: sin él, Dec-MCTS
   cae de 0.619 a 0.553 en 1E y queda por debajo de CBBA). Pero ese canal **no aporta nada
   bajo incertidumbre**: un plan comunicado solo vale lo que valga su predictibilidad.
3. **El diseño del conjunto de prueba estaba determinando la conclusión.** El generador de
   escenarios se detiene en 5 robots, justo donde la ventaja empieza a crecer, y más de la
   mitad del catálogo es estocástico.

**Decisiones tomadas:** se conservan las 4 versiones hasta elegir con el catálogo final
(D-04); la lectura de `obs.getKnownRobots()` se mantiene como simplificación aceptada y
auditada (D-01).

**⚠️ Pendiente que bloquea la comparación final de versiones:** D-06 — v1, v2 y v3 replican
en sus rollouts la semántica **antigua** de caducidad; solo v4 la corregida.

**Tandas de logs válidas de referencia**: `eval_versiones_fix2` (21 esc. mixtos),
`eval_1E` (familia 1E completa), `eval_c1_mix` + `eval_c1_1E` (ablación de comunicación).

---

## 2026-08-06 — Sesión 3 (cont.): familia 1E y ablación C1 de comunicación

**Familia 1E completa** (`logs/eval_1E`, 16 escenarios × 10 réplicas): confirma la intuición
del usuario. Dec-MCTS lidera (v4 0.6184, v4-g9999 0.6179) por delante de cbba (0.6089), pero
la Δ pareada frente a cbba es **+0.009 ± 0.010 (empate estadístico)**. La ventaja **escala con
el nº de robots**: −0.007 (2r) → +0.024 (5r). Detalle y criterios de diseño derivados en
`03_experimentos.md`.

**Ablación C1** (`logs/eval_c1_mix`, `logs/eval_c1_1E`; nuevo solver `dec-mcts-v4-nocomm`,
flag `useComm` en el constructor de v4): **el canal de comunicación SÍ aporta y mi sospecha
previa queda refutada.** En 1E, +0.066 ± 0.019; sin comunicación Dec-MCTS cae de 0.619 a
0.553, por debajo de cbba. Pero el aporte es **nulo en régimen estocástico (−0.005)** y
**nulo con 2-3 robots**. El conjunto MIX lo subestimaba porque 2 de sus 3 tamaños tienen 2-3
robots y 4 de sus 7 familias son estocásticas.

**Consecuencia para el rumbo del proyecto**: el trabajo pendiente no es sustituir la
coordinación (funciona), sino **hacerla robusta a la incertidumbre** (nueva hipótesis H-06 en
`05_dudas.md`), y **diseñar el catálogo con equipos de ≥4 robots** (criterio C-7).

---

## 2026-08-06 — Sesión 3: corrección de los defectos y reevaluación

**Objetivo**: aplicar las correcciones A1–A4 y volver a medir.

**Cambios aplicados** (los cuatro solvers, `include/tau/solver_DecMCTS_v*.hpp`)
- **A1 · Decisión filtrada por factibilidad.** Nuevo método `mostVisitedFeasibleChild`
  (`mostVisitedFeasibleActionChild` en v3): la política de explotación final elige el hijo
  del root con más visitas **de entre los aplicables ahora mismo**, con el mismo criterio
  que ya usaba la selección. Sustituye a `mostVisitedChild` en `decideNextAction`.
- **A2 · v3 recupera la fase de rollout.** `inTree = false` tras expandir; los chance nodes
  se siguen anotando en el `path` aunque ya no se esté dentro del árbol (se eliminó la
  condición `inTree &&` de los manejadores de `TASK_END`/`TASK_EXPIRATION`).
- **A3 · Fidelidad del consumo en fracaso.** `processTaskEnd` replica ahora
  `simulator.cpp::endTask`: si `averageFailTime == 0`, el consumo es la **demanda completa**,
  no 0.
- **A4 · Fidelidad de `robot.time` al esperar coalición**: `tInfo.latestStart`, como el
  simulador real (en la práctica es inerte, pero reduce la superficie de divergencia).
- Corregido el comentario obsoleto sobre γ en v4 (decía 0.999 por defecto; es 0.9999).
- Compilación en Release (`-O3`). No altera resultados (el nº de iteraciones es fijo), solo
  el `computing_time`.

**Iteración intermedia (error propio, corregido)**: en la primera versión de A1 sustituí la
poda de v4 por un criterio más estricto basado en `treeActions`. Eso **destruía subárboles**
cuando una tarea era temporalmente infactible por batería y provocó una regresión en
determinista (0.551 → 0.508, `logs/eval_versiones_fix`). Lección: la poda es destructiva y
su criterio debe ser conservador (solo lo que ya no volverá a ser aplicable); la
infactibilidad temporal se filtra en la decisión, sin borrar nada. Rehecho así en
`logs/eval_versiones_fix2`.

**Resultados** (misma tanda: 21 escenarios × 9 solvers × 5 réplicas; Δ pareada por escenario;
el ruido medido sobre los solvers **no modificados** es ≈ ±0.016)

| Solver | global antes | global después | Δ pareada |
|---|---|---|---|
| dec-mcts-v3 | 0.397 | 0.454 | **+0.057 ± 0.010** |
| dec-mcts-v1 | 0.415 | 0.461 | **+0.046 ± 0.007** |
| dec-mcts-v4-g9999 | 0.416 | 0.463 | **+0.046 ± 0.013** |
| dec-mcts-v4 | 0.413 | 0.458 | **+0.045 ± 0.013** |
| dec-mcts-v2 | 0.432 | 0.451 | +0.020 ± 0.012 (marginal) |
| random/greedy/cbaa/cbba | — | — | −0.016 … +0.011 (ruido) |

- El retiro prematuro **desaparece**: v4-g9999 en estocástico pasa de retirarse en t=31.6 con
  65 % de batería a t=42.1 con 57 %.
- Ya no hay regresión en determinista (v4-g9999: 0.553 → 0.551).
- **Las cuatro versiones quedan empatadas** (0.451–0.463 global): los defectos pesaban
  bastante más que las diferencias de diseño entre v1, v2, v3 y v4.
- **cbba sigue por delante** (0.483 global, 0.557 DET, 0.427 EST). Dec-MCTS es ahora segundo,
  por encima de cbaa (0.433) y greedy (0.406).

**Qué queda**
1. Decidir la versión definitiva (recomendación: v4 con γ=0.9999) y retirar el resto.
2. La brecha que queda frente a cbba ya no es un bug: son las cuestiones de diseño B1
   (crédito marginal) y C1 (¿aporta algo el canal de comunicación?) de `05_dudas.md`.
3. Enlazar con el objetivo 1.2: diseñar escenarios donde la comunicación tenga valor real.

---

## 2026-08-06 — Sesión 2: evaluación de las 4 versiones de Dec-MCTS

**Objetivo**: objetivo 1.1 — evaluar el estado real de los cuatro solvers Dec-MCTS y
localizar por qué no superan a CBAA/CBBA.

**Qué se hizo**
- Recompilado con `-O3` (`CMAKE_BUILD_TYPE=Release`). Antes se compilaba **sin optimizar**;
  el número de iteraciones es fijo, así que los *resultados* no cambian, solo el
  `computing_time` (~3 s/run en el escenario mayor). Los `computing_time` de logs antiguos
  no son comparables con los nuevos.
- Nueva tanda `logs/eval_versiones`: 21 escenarios (familias 1A,1B,1E,2A,2B,3C,4D ×
  {12t_2r, 16t_3r, 24t_5r}) × 9 solvers × 5 réplicas = **945 runs**.
- **Resultado 1**: el ranking depende del régimen. En determinista v4≈cbba (0.551 vs 0.559);
  en estocástico **las cuatro versiones pierden incluso contra greedy** (v2 0.370, v1 0.349,
  v4g 0.314, v3 0.307 vs greedy 0.382, cbaa 0.416, cbba 0.406).
- **Resultado 2**: **v4 NO es la mejor versión**. En estocástico es de las peores; su
  supremacía previa era un artefacto de haberse medido solo en `1E` (determinista).
- **Resultado 3 — defecto confirmado (D-BUG-01)**: en escenarios estocásticos, v3/v4/v4g
  **se retiran prematuramente**: dejan ~5 tareas con ventana abierta y ~62 % de batería sin
  usar, a t≈35 en vez de ≈45 (v1/v2 dejan 0.45-0.70 tareas; cbba 1.03). Intentan solo el
  43 % de las tareas frente al 61 % de cbba. Mecanismo identificado y documentado en
  `05_dudas.md` (incoherencia entre selección filtrada por factibilidad y decisión final
  sin filtrar).
- **Resultado 4 — defecto confirmado (D-BUG-02)**: v3 no tiene fase de rollout; tras expandir
  un `EXECUTE_TASK` **no** pone `inTree=false`, así que cada iteración expande una cadena
  entera de nodos y la evaluación de la hoja la produce la heurística "primera tarea no
  expandida por TaskID". Explica por qué v3 < v2.

**Qué queda / siguiente paso propuesto**
1. Arreglar D-BUG-01 (decisión final filtrada por factibilidad + poda de FINISH obsoleto) y
   re-medir sobre la misma tanda. Es el cambio de mayor retorno esperado.
2. Después, D-BUG-02 y las divergencias de fidelidad H-02/A4.
3. Ablación pendiente y nunca hecha: los 7 cambios de v4, uno a uno, **en régimen estocástico**.
4. Ablación diagnóstica: ¿aporta algo el canal de comunicación? (variante sin
   `knownDistributions`).

---

## 2026-08-06 — Sesión 1: puesta en contexto y creación del cuaderno

**Objetivo**: que el asistente entienda a fondo el problema y el código, y dejar montada la
infraestructura de contexto para sesiones futuras.

**Qué se hizo**
- Lectura completa de: caps. 2 y 3 de la memoria, `informacion.txt`, briefing del cap. 4,
  los 4 solvers Dec-MCTS, CBAA/CBBA, greedy/random, `simulator.cpp`, `main.cpp`, cabeceras de
  `include/tau/`, generadores de escenarios y scripts de análisis.
- Análisis cuantitativo de **todas** las tandas de logs (`logs/*`), separando las válidas
  (post-corrección del bug de caducidad) de las inválidas.
- **Hallazgo principal**: en `logs/eval_bundles` (208 escenarios), Dec-MCTS v4-g9999 gana en
  las 5 familias **deterministas** y pierde en las 8 **estocásticas**, sin excepción.
  Descartada la mortalidad de robots como causa (0% en ambos regímenes). Ni más cómputo
  (`eval_hc`) ni más rondas de comunicación (`eval_rounds`) cierran la brecha.
- **Hallazgo secundario**: divergencia de fidelidad entre el simulador real y la réplica
  interna de Dec-MCTS en el consumo de batería de una tarea fallida cuando
  `averageFailTime == 0` (real: demanda completa; réplica: 0). Se activa exactamente en las
  familias estocásticas. → hipótesis H-02.
- **Hallazgo secundario**: CBAA, CBBA y Dec-MCTS leen `obs.getKnownRobots()` (estado privado
  real de los compañeros), lo que contradice la observabilidad parcial del cap. 2. → duda D-01.
- Creados `CLAUDE.md` (raíz) y esta carpeta `.claude-notes/` con 5 documentos de contexto.

**Qué queda / siguiente paso propuesto**
1. Decidir con el usuario el orden de ataque (objetivo 1.1 vs 1.2).
2. Experimento más barato y con mejor retorno: corregir H-02 en `solver_DecMCTS_v4.hpp` y
   re-ejecutar las familias `2A/2B/2C/2D` para ver si se cierra la brecha en estocástico.
3. Resolver dudas D-01 a D-05 antes de escribir el cap. 4.

**Estado del repo al cerrar**: sin cambios en el código. Solo ficheros nuevos de documentación
(`CLAUDE.md`, `.claude-notes/`). `git status` previo: `.gitignore` modificado,
`dec-mcts.pdf` borrado, `informacion.txt` sin versionar.
