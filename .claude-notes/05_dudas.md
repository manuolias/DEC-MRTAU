# 05 — Dudas abiertas e hipótesis

Estado: `ABIERTA` / `RESUELTA` / `DESCARTADA`.

---

## A0. Defectos CONFIRMADOS (sesión 2, tanda `logs/eval_versiones`)

> **Estado (sesión 3): D-BUG-01 y D-BUG-02 CORREGIDOS**, junto con las divergencias de
> fidelidad H-02/A4. Resultado: +0.045…+0.057 de recompensa global en las cuatro versiones,
> desaparece el retiro prematuro y no hay regresión en determinista. Ver
> `03_experimentos.md` → `logs/eval_versiones_fix2`. Se conserva el análisis por su valor
> documental (y para el cap. 6).

### D-BUG-01 · Retiro prematuro en v3/v4 — `CORREGIDO` (sesión 3)
**Síntoma medido** (21 escenarios × 5 réplicas): en régimen estocástico, v3/v4/v4-g9999
retiran a sus robots dejando **5.1 tareas de media con la ventana todavía abierta** y **62 %
de batería sin usar**, a t≈35 en lugar de ≈45. v1/v2 dejan 0.45–0.70 tareas; cbba 1.03.
En régimen determinista el fenómeno no aparece (todos ~0.2 tareas).

**Mecanismo** (lectura de código + coincidencia exacta con el patrón determinista/estocástico):
1. `treeActions()` devuelve `{FINISH}` **en solitario** solo cuando no hay ninguna tarea
   factible.
2. En un rollout que alcanza un estado sin tareas factibles (frecuente bajo incertidumbre:
   fallos + ventanas que se cierran), el hijo `FINISH` es el **único** en `avail`, así que
   recibe **todas** las visitas de ese rollout. Los hijos `EXECUTE_TASK` se reparten las
   visitas de los rollouts restantes entre 12–24 hermanos.
3. `pruneStaleRootChildren()` **nunca marca `FINISH` como obsoleto** (`default: return false`).
4. En `decideNextAction`, `mostVisitedChild()` compara `N_disc` **en bruto, sin filtrar por
   factibilidad** ⇒ gana el hijo `FINISH` aunque haya tareas disponibles.
5. v2 no filtra la selección, así que su hijo `FINISH` también se elige en estados donde
   retirarse es malo, acumula un `Q` bajo y D-UCT deja de visitarlo ⇒ sufre mucho menos.

**Causa raíz**: el cambio 3 de v4 (*selección D-UCT filtrada por factibilidad*) introdujo una
**incoherencia** con la regla de decisión final, que quedó sin filtrar. Los conteos de visitas
dejan de ser comparables entre hermanos cuando el conjunto de acciones disponibles varía —
que es justo el problema que la estadística de disponibilidad (`N_avail_disc`) de v3
pretendía resolver, pero v3 solo la usó en el bono UCB, no en la decisión.

**Corrección propuesta**: ver A1 en la lista de modificaciones.

### D-BUG-02 · v3 no ejecuta fase de rollout — `CORREGIDO` (sesión 3)
En `solver_DecMCTS_v3.hpp`, tras expandir un nodo `EXECUTE_TASK` **no** se pone
`inTree = false` (solo se hace con `FINISH`). Consecuencia: cada iteración expande una
**cadena completa** de nodos hasta el final del episodio, no hay política de rollout, y la
"evaluación de la hoja" la produce de facto la regla de expansión = *primera acción factible
no expandida en orden de TaskID*. Como los TaskID están ordenados por racimo, eso equivale a
"todos los robots van primero al racimo superior": evaluaciones sistemáticamente sesgadas.
Explica que v3 (0.397 global) quede por debajo de v2 (0.432) pese a incorporar dos ideas
correctas (chance nodes + estadística de disponibilidad).

---

## A. Hipótesis sobre el bajo rendimiento de Dec-MCTS

**Hecho a explicar** (`logs/eval_bundles`, ver `03_experimentos.md`): Dec-MCTS v4-g9999 gana
en las **5 familias deterministas** y pierde en las **8 estocásticas**, sin excepción. Ni más
cómputo (`eval_hc`, ×40 iteraciones) ni más rondas de comunicación (`eval_rounds`) lo arreglan.

### H-01 · Varianza del estimador bajo estocasticidad — `ABIERTA`
Con `success_time = [10,10]` (μ=10, σ=10, ¡coeficiente de variación 1!) y `success_prob<1`,
la recompensa de un rollout es enormemente ruidosa. Con 30 iteraciones por latido, la
diferencia entre dos acciones queda enterrada en el ruido, mientras CBAA/CBBA usan la
esperanza determinista y son inmunes.
*En contra*: `eval_hc` (1200 iters) no mejora. Si fuera puro ruido, debería mejorar.
*Cómo comprobarlo*: medir la desviación típica de `Q_disc/N_disc` entre hermanos del root al
decidir, en un escenario 1A vs uno 2A.

### H-02 · Divergencia de fidelidad del modelo generativo — `ABIERTA` ⚠️ prioritaria
En las familias estocásticas `fail_time = [0,0]`. El simulador real cobra
`consumptionFinal = averageDemand` (la demanda **completa**) cuando `averageTime == 0`,
mientras la réplica interna de Dec-MCTS cobra **0**. Es decir, en los rollouts **fracasar es
gratis** en tiempo y en batería. Un fracaso deja de ser penalizado ⇒ el árbol sobrevalora las
tareas de baja probabilidad. Esto se activa **exactamente** en las familias donde Dec-MCTS
colapsa y no en las deterministas.
*Ya comprobado*: no mata robots (batería 100 vs demanda 10), pero sí sesga la valoración.
*Cómo comprobarlo*: alinear `processTaskEnd` de v4 con `endTask` del simulador y re-ejecutar
`2A/2B/2C/2D`. **Es el experimento más barato y con mejor relación coste/información.**

### H-03 · Strategy fusion (el árbol no ramifica por desenlace) — `ABIERTA`
Sin chance nodes, un nodo promedia rollouts en los que la tarea salió bien y mal, planes que
son incompatibles entre sí. Solo importa cuando hay incertidumbre ⇒ encaja con el patrón.
*En contra*: v3 implementó chance nodes y quedó **peor** que v2 (0.5245 vs 0.5641)… pero v3
se midió sobre la familia `1E`, que es **determinista**, donde los chance nodes solo
fragmentan estadísticas sin aportar nada. **La comparación v2/v3 nunca se hizo en escenarios
estocásticos.** Merece la pena re-evaluar v3 (o portar los chance nodes a v4) sobre `2x/3x/4x`.

### H-04 · `blockingProb` mal calibrado bajo incertidumbre — `ABIERTA`
`blockingProb = successProb` si ≥ `requiredWorkers` vecinos cubren la tarea. Cuando
`successProb` es baja (familias 3 y 4, hasta 0.25), la tarea apenas se descuenta ⇒ varios
robots convergen sobre las mismas tareas de baja probabilidad. La ablación existente sugiere
que el valor da igual, pero está hecha sobre logs pre-fix y solo en `1E` (determinista).

### H-05 · La recompensa `completadas/n` es demasiado plana — `ABIERTA`
Con incertidumbre, muchos planes distintos completan el mismo número esperado de tareas.
El tie-break de *earliness* de v4 mitiga esto pero pesa `W_e/n` (muy poco). Habría que
comprobar si la señal discrimina de verdad entre las acciones del root.

---

## A-bis. Mejoras de diseño pendientes y ablaciones

### Ablaciones
- **C1 · ¿Aporta algo el canal de comunicación?** — ✅ **HECHA (sesión 3). Sospecha REFUTADA.**
  Implementada como `dec-mcts-v4-nocomm` (flag `useComm` en el constructor de v4). El canal
  **sí aporta** y es lo que pone a Dec-MCTS por encima de cbba en 1E (+0.066 ± 0.019; sin él
  cae de 0.619 a 0.553, por debajo de cbba). Pero su valor **depende del régimen y del tamaño
  del equipo**: +0.038 determinista vs **−0.005 (nulo) estocástico**; nulo con 2-3 robots,
  máximo con 4-5 (+0.174 con 4r). Detalle en `03_experimentos.md`.
- **C2 · Ablación de los 7 cambios de v4, uno a uno, EN RÉGIMEN ESTOCÁSTICO** — pendiente.
  Nunca se hizo: v4 se validó solo en 1E, que es determinista.
- **C3 · Rehacer la ablación de `blockingProb`** con el simulador corregido — pendiente
  (la existente está sobre logs pre-fix y solo en régimen determinista).

### H-06 · La coordinación no sobrevive a la incertidumbre — `ABIERTA` (nace de C1)
C1 deja el diagnóstico afinado: el problema no es que Dec-MCTS no coordine, sino que **su
coordinación se evapora en régimen estocástico**. Explicación plausible: un bundle comunicado
es un plan *determinista*; en cuanto una tarea del vecino falla, su plan real diverge del
comunicado, así que condicionar sobre él aporta sesgo en vez de información. Es exactamente
donde queda la brecha frente a cbba (0.427 vs 0.396).
Líneas de ataque, por orden de coste:
- **B4 · `blockingProb` sensible a la incertidumbre**: hoy es binaria (0 o `successProb`) e
  ignora la posición de la tarea en el bundle del vecino y si le da tiempo a llegar.
  Descontar por probabilidad de que el vecino *llegue* y *tenga éxito*.
- **B5 · Comunicar la incertidumbre del plan, no solo el plan**: la distribución $\psi^i$ se
  construye siguiendo la cadena más visitada; bajo incertidumbre esa cadena es poco fiable
  más allá de los primeros pasos. Truncar el bundle o ponderar por profundidad.
- **B1 · Crédito marginal** (ver abajo): sigue siendo la mejora estructural de mayor calado.

### Mejoras de diseño no aplicadas (cambian la definición del algoritmo)
- **B1 · Utilidad local / crédito marginal.** Lo retropropagado es el objetivo global
  `completadas/n`, que diluye la contribución propia (si me retiro, los vecinos recogen mis
  tareas en el rollout). Opciones: recompensa = tareas completadas **por mí** (+ peso pequeño
  del global), o $f^r$ con un contrafactual decente ("yo sigo la política de rollout") en vez
  del degenerado `PASSIVE_ME` actual, que es el que hizo que la ablación lo declarase inútil.
- **B2 · Normalización por disponibilidad** (`N_avail_disc`, estilo ISMCTS) usada también en
  la decisión y en `extractDistribution`, no solo en el bono UCB.
- **B3 · Modelado del desenlace en el árbol** sin fragmentar como hacía v3.

## B. Decisiones que necesita tomar el usuario

### D-01 · Observabilidad parcial: qué contamos en la memoria — `RESUELTA` (sesión 3)
**Decisión del usuario**: se mantiene el acceso a `obs.getKnownRobots()` como **simplificación
del mecanismo de comunicación**; simplifica el código y no altera los resultados.
**Auditoría realizada**: no hay fuga de información **futura**. El desenlace y la duración
muestreados en `startTask` viajan solo en el `payload` del evento y nunca llegan al estado;
`Robot.time` de un robot que ejecuta vale `latestStart` de su tarea, no su instante de
finalización; `Task.initTime` es información del pasado (el estadístico $\delta$ que el cap. 2
declara legítimamente observable) y `nextDecisionInfo` es una estimación calculada con
$\mathbb{E}[X\mid X>\delta]$ a partir de distribuciones públicas. Lo que se expone es el
**estado privado actual** de los compañeros (posición, batería, `onTask`), de forma simétrica
para CBAA, CBBA y Dec-MCTS.
**Para la memoria (cap. 4)**: declararlo con transparencia como decisión de implementación,
subrayando que el *leak* temporal —que es lo que motiva el DEC-POGSMDP— sigue cerrado.

### D-02 · Qué conjunto de escenarios va a la memoria — `ABIERTA`
📌 **Antes de decidir, leer la sección "Criterios para el diseño del catálogo definitivo"
de `03_experimentos.md`** (C-1 a C-6): resume qué ejes discriminan de verdad. El resultado
más accionable: la ventaja de Dec-MCTS crece de forma monótona con el nº de robots y el
generador se detiene en 5, justo donde empieza a separarse.

Hoy hay tres familias (bundles 208, random 45, salomon 56) y el usuario dice que ninguna le
convence (objetivo 1.2). Decisiones a tomar: ¿se rediseña el catálogo desde cero?
¿Se conservan los bundles como "escenarios estructurados" y los random como "no
estructurados"? ¿Qué se hace con salomon (no hay generador ni logs)?

### D-03 · Cómo citar el paper del tutor — `ABIERTA`
Hay varios `\red{CITA AL TUTOR}` en los caps. 2 y 3. El paper no está publicado.
¿Se cita como preprint/comunicación personal? ¿Se pide un identificador al tutor?

### D-04 · ¿Se conservan v1, v2 y v3? — `RESUELTA` (sesión 3)
**Decisión del usuario: sí, se conservan las cuatro versiones por ahora**, y se elegirá la
definitiva evaluando cuál rinde mejor **sobre el catálogo de escenarios final**. Tiene
sentido: tras las correcciones las cuatro están estadísticamente empatadas, así que la
elección no puede hacerse con los datos actuales. ⚠️ Antes de esa comparación hay que
resolver D-06.

### D-06 · Asimetría de fidelidad entre versiones — `ABIERTA` ⚠️ bloquea la comparación final
`processTaskExpiration` de **v1, v2 y v3** sigue tratando una tarea `ASSIGNED` como protegida
de la caducidad: es la semántica **anterior** al commit `466190a`. Solo **v4** replica el
simulador corregido. Sus rollouts son por tanto optimistas (creen que comprometerse tarde con
una tarea aún la completa), lo que **confunde cualquier comparación entre versiones**.
Fue una decisión deliberada en su momento (no tocar v1–v3 al corregir el simulador), pero si
la versión definitiva se va a elegir comparándolas sobre el catálogo final, hay que decidir:
(a) alinear v1–v3 con la semántica corregida (cambio de 1 línea por fichero, igual que el ya
hecho en v4), o (b) mantenerlas como están y **declarar la comparación como confundida** en
el cap. 6. Recomendación: (a).

### D-05 · Semilla y reproducibilidad — `ABIERTA`
Todo usa `std::random_device`. ¿Se añade una semilla configurable por experimento para que
el tribunal pueda reproducir los resultados exactos? Es un cambio pequeño y de alto valor
para la defensa, pero invalida la comparabilidad con los logs ya generados.

---

## C. Cosas menores a arreglar

- `solver_DecMCTS_v4.hpp`: el comentario de cabecera dice γ=0.999 por defecto, el constructor
  usa 0.9999. Corregir el comentario.
- `simulator.cpp::initLogging()`: `logInfo("scenario_name", ...)` está **duplicado** (líneas
  73-74).
- `analyze_ablation.py`: la tabla "por escenario" imprime una cabecera de 8 columnas pero solo
  rellena 3; y el bloque de diagnósticos H1/H4 está dentro de un string (código muerto).
- `definitions.hpp`: horizonte de makespan *hardcodeado* (`1080.0`) y distancia máxima
  `10000·nº_robots`. Inactivos con $k_1=1$, pero conviene documentarlos o parametrizarlos.
- `build/` versionado en git (binarios); `copy/` ya está en `.gitignore`.
