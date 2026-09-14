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

⚠️ **Desenlace real (sesión 5)**: el capítulo 4 final **NO declara esta relajación**. El usuario
retiró la nota de transparencia al recortar el capítulo. Lo que sí queda declarado (§4.2.1) es
que la comunicación es perfecta y que en CBAA/CBBA las pujas ajenas se recalculan localmente en
lugar de recibirse. **Consecuencia**: si en la defensa se pregunta «¿cómo sabe un robot dónde
está otro?», la respuesta no está escrita en la memoria. La contestación preparada es: se relaja
la ocultación del **estado físico** (posición, batería), que en un sistema real sería un mensaje
de tamaño constante, y se mantiene oculta la componente que el trabajo estudia, las
**intenciones**, que solo viajan por el canal explícito.

### D-02 · Qué conjunto de escenarios va a la memoria — `RESUELTA` (sesión 8, revisada)
**Decisión del usuario (sesión 8)**: el conjunto oficial es el **catálogo v3**
(`scenarios/catalogo_v3/`, 360 escenarios, tanda `logs/eval_catalogo_v3`). Misma estructura y
mismas semillas que el v2; cambian solo los regímenes (coste de un intento 10 fijo y σ = 0/1/3/5).
Los catálogos **v1 y v2 quedan obsoletos**; se conservan en el repositorio y su único uso
legítimo es como **contraste metodológico** en el cap. 6. `bundles/`, `random/` y la sonda
`probe/` son material exploratorio. Solomon sigue descartado.

*Decisiones anteriores, superadas*: el catálogo v1 (sesión 4) y el v2 (sesión 6).

### D-07 · ¿La ventaja de Dec-MCTS es del tamaño del equipo o de la holgura? — `RESUELTA` (sesión 4)
**Ninguna de las dos: no hay tal ventaja cuando se controla la dificultad.** Resuelta con
`logs/eval_catalogo` (288 escenarios × 10 réplicas).

El hallazgo de `eval_1E` ("la Δ frente a cbba crece con el nº de robots") **era un artefacto**:
en aquella familia el nº de tareas era fijo y `MAX_TIME` crecía con él, de modo que *más
robots* implicaba *menos carga por robot*. Las celdas con más robots eran las más holgadas.

A iso-dificultad (L=6, T=40, 5 instancias × 10 réplicas por celda):
- **determinista**: Δ = −0.006 ± 0.004, pendiente −0.0032/robot (t = −2.4). **Empate plano.**
- **estocástico**: Δ = −0.081 ± 0.006, pendiente **−0.0130/robot (t = −8.4)**. La desventaja
  **crece** con el equipo, de −0.032 (2r) a −0.131 (10r).

⇒ **C-1 queda refutado tal y como estaba enunciado** y hay que corregirlo al redactar el
cap. 6. Lo que sí sobrevive es su corolario metodológico, más fuerte: *el diseño del conjunto
de prueba determinaba la conclusión*.

**Lo que nace de aquí — H-07**, ver abajo.

### H-07 · El sesgo de la comunicación se acumula con el nº de vecinos — `ABIERTA`
Explica el resultado más nítido de `eval_catalogo`: bajo incertidumbre la desventaja frente a
cbba crece **linealmente con el nº de robots** (−0.0130 por robot, t = −8.4), mientras que en
determinista la pendiente es diez veces menor.

Mecanismo propuesto: Dec-MCTS condiciona sus rollouts sobre el bundle muestreado de **cada**
vecino. Un bundle comunicado es un plan *determinista*; en cuanto una tarea del vecino falla,
su plan real diverge del comunicado. Con R vecinos, el rollout acumula R desviaciones
independientes ⇒ el sesgo del estimador crece con R mientras que su valor informativo no.
En determinista los planes no se desvían y el efecto desaparece: es exactamente el patrón
observado. Coherente con la ablación C1 (el canal aporta +0.038 en determinista y −0.005 en
estocástico) y con H-06.

*Cómo atacarlo*: se intentó **B4** (`blockingProb` probabilística). **Resultado NULO**, y el
diagnóstico que lo explica cambia el foco del problema — ver **D-08**.

### D-08 · Los bundles comunicados tienen longitud ~1: la "distribución sobre planes" es degenerada — `ABIERTA` ⚠️ el hallazgo estructural
Medido instrumentando `sampleNeighbourBundles` en `cat_A1_r10_n060_bnd_E_est_q1_i1`
(2 216 430 bundles muestreados):

| longitud del bundle | 0 | 1 | 2 | 3 | 4 | ≥5 |
|---|---|---|---|---|---|---|
| frecuencia | 17 % | **57 %** | 8 % | 17 % | 0.02 % | **0 %** |

**Media 1.25 tareas.** El 74 % de los planes que un robot comunica a sus vecinos son de
longitud 0 o 1.

**Por qué importa**: la aportación conceptual de Dec-MCTS frente a las subastas es comunicar
una **distribución sobre secuencias de tareas**. Con bundles de longitud ~1 esa distribución
degenera en "distribución sobre mi próxima acción", que es esencialmente lo que comunica
**CBAA**. El método pierde su ventaja estructural sobre las subastas antes de empezar.

**Por qué explica el fracaso de B4**: B4 descuenta según la posición de la tarea en el bundle
del vecino, pero en la posición 0 la varianza acumulada es nula, Φ degenera en un escalón y
B4 **coincide exactamente** con la versión binaria. Con el 74 % de los bundles de longitud
≤1, B4 casi nunca llega a actuar. No es que la idea sea mala: es que no hay profundidad sobre
la que operar.

**CAUSA IDENTIFICADA Y CORREGIDA — y no era el cómputo.** Instrumentando
`extractDistribution` (300 000 cadenas medidas): la cadena del bundle **se corta en un nodo
`FINISH` el 68.7 % de las veces**, y solo el 28.6 % llega a una hoja real del árbol.

Es **la misma incoherencia que D-BUG-01**: el hijo `FINISH` acapara visitas porque en los
rollouts que alcanzan estados sin tareas factibles es el *único* hijo disponible. En la
sesión 3 se corrigió en la regla de decisión (`mostVisitedFeasibleChild`) pero **no en
`extractDistribution`**, que seguía usando `mostVisitedChild` en crudo.

**Corrección** (`mostVisitedTaskChild`, flag `deepBundle` ⇒ variante `dec-mcts-v4-d8`): la
cadena se sigue por el hijo más visitado **restringido a `EXECUTE_TASK`**, con cota de 32.

| | v4 original | v4-d8 |
|---|---|---|
| longitud media del bundle | 1.58 | **2.50** (+58 %) |
| corte por `FINISH` | 68.7 % | **1.8 %** |
| corte en hoja real | 28.6 % | **96.5 %** |

⇒ Ahora el límite sí es la profundidad del árbol (régimen legítimo), no un artefacto.

### 🔴 …pero NO mejora el rendimiento — `RESUELTA` (sesión 4)
126 escenarios × 3 réplicas, comparación **pareada directa** contra v4 original:

| conjunto | B4 − v4 | D8 − v4 | D8+B4 − v4 |
|---|---|---|---|
| A1 determinista (45) | −0.0018 ± 0.0020 | −0.0022 ± 0.0022 | +0.0007 ± 0.0019 |
| A1 estocástico (45) | +0.0030 ± 0.0062 | +0.0067 ± 0.0072 | −0.0036 ± 0.0063 |
| C coaliciones (36) | +0.0065 ± 0.0060 | +0.0021 ± 0.0062 | +0.0072 ± 0.0058 |
| **TODOS (126)** | **+0.0023 ± 0.0029** | **+0.0022 ± 0.0032** | **+0.0010 ± 0.0029** |

Las tres variantes son **indistinguibles de la línea base**. La tasa de intento sigue en 63 %.

**Conclusión de fondo (para los caps. 6 y 7)**: se han descartado ya **cuatro** explicaciones
a nivel de implementación de la desventaja bajo incertidumbre —presupuesto de cómputo
(`eval_hc`, ×40), rondas de comunicación (`eval_rounds`), calibración de `blockingProb` (B4) y
profundidad del plan comunicado (D-08)—. Ninguna la explica. La desventaja **no es un
artefacto de implementación**: es intrínseca al enfoque tal y como está formulado.
Condicionar los rollouts sobre planes muestreados de los vecinos es estructuralmente menos
robusto que el consenso por pujas de CBBA cuando esos planes dejan de ser fiables.
**Se conserva la corrección D-08** de todos modos: sin ella la implementación no hacía lo que
el método dice hacer (comunicar distribuciones sobre *secuencias*), y eso sí había que
arreglarlo antes de defender el trabajo.

### D-03 · Cómo citar el paper del tutor — `ABIERTA`
Hay varios `\red{CITA AL TUTOR}` en los caps. 2 y 3. El paper no está publicado.
¿Se cita como preprint/comunicación personal? ¿Se pide un identificador al tutor?

### D-04 · ¿Se conservan v1, v2 y v3? — `RESUELTA` (sesión 4, revisada)
**Sesión 3**: se conservaban las cuatro para elegir la definitiva sobre el catálogo final.
**Sesión 4 — decisión del usuario**: el catálogo se evalúa **solo con `dec-mcts-v4-g9999`**.
Motivo: evaluar las cuatro versiones multiplicaba por cinco el coste (26.6 h frente a 5.3 h)
y además arrastraba el sesgo de D-06. Si los resultados de v4 son favorables, se cierra la
elección sin más trabajo. v1–v3 permanecen en el repositorio como historia del desarrollo y
se describen en el cap. 4, pero no entran en la evaluación del cap. 6.

### D-06 · Asimetría de fidelidad entre versiones — `RESUELTA por vía indirecta` (sesión 4)
Deja de bloquear nada: al evaluar el catálogo **solo con v4** (D-04), la comparación entre
versiones ya no se hace, así que el sesgo no llega a los resultados. La divergencia **sigue
viva en el código** de v1–v3 y hay que declararla si el cap. 4 las describe. Texto original
conservado abajo por su valor documental.


`processTaskExpiration` de **v1, v2 y v3** sigue tratando una tarea `ASSIGNED` como protegida
de la caducidad: es la semántica **anterior** al commit `466190a`. Solo **v4** replica el
simulador corregido. Sus rollouts son por tanto optimistas (creen que comprometerse tarde con
una tarea aún la completa), lo que **confunde cualquier comparación entre versiones**.
Fue una decisión deliberada en su momento (no tocar v1–v3 al corregir el simulador), pero si
la versión definitiva se va a elegir comparándolas sobre el catálogo final, hay que decidir:
(a) alinear v1–v3 con la semántica corregida (cambio de 1 línea por fichero, igual que el ya
hecho en v4), o (b) mantenerlas como están y **declarar la comparación como confundida** en
el cap. 6. Recomendación: (a).

### D-09 · La comparación centralizado vs distribuido — `RESUELTA` (sesión 4)
**Decisión del usuario**: la comparación **no se mide** en este TFM, pero **no hace falta
medirla**, porque el punto de partida ya está establecido en la literatura: el paper del tutor
implementa **MCTS centralizado sobre exactamente el mismo problema y las mismas condiciones de
incertidumbre**, y funciona muy bien. Se apoya el argumento en esa referencia.

**Por qué esto refuerza el trabajo en vez de debilitarlo** (razonamiento a plasmar en el cap. 7):

1. MCTS **sí** resuelve bien este problema — está demostrado, en régimen centralizado y con la
   misma incertidumbre.
2. Este TFM implementa el **mismo método** en régimen distribuido y **pierde frente a CBBA**.
3. Las cuatro refutaciones descartan que la pérdida venga de la implementación.
4. ⇒ La pérdida es atribuible al **paso a distribuido con comunicación limitada**, no al método
   de búsqueda ni al código.

Es un argumento **más limpio** que una medición propia: se apoya en un resultado ya establecido
en lugar de en un experimento nuevo, y aísla la variable que interesa (la descentralización).

⚠️ **Lo que sigue sin poder afirmarse**: una **cifra concreta** de pérdida. Los escenarios del
paper del tutor no son los de este catálogo, así que no se puede decir «se pierden X puntos».
La afirmación debe quedarse en **cualitativa** («la reducción observada es mayor de lo que cabría
esperar del mero paso a un esquema distribuido»).

⚠️ **Dependencia con D-03**: este argumento se apoya en un paper **aún no publicado**. Eso hace
que la decisión sobre cómo citarlo pase de ser un detalle de formato a ser **estructural** para
el cap. 7. Resolver D-03 antes de redactarlo.

### D-10 · Dónde va el protocolo experimental y cuántas réplicas — `RESUELTA` (sesión 6, revisada en la 7)
⚠️ **Revisión (sesión 7, al escribir el cap. 5)**: el usuario recortó el guion del capítulo y el
protocolo experimental **no** entra como sección. De él solo quedan, en la recapitulación
(§5.3), las **3 réplicas**, el total de **5 400 ejecuciones** y la unidad de análisis. Lo demás
—idempotencia del ejecutor, paralelización, coste de la tanda, aviso de que `computing_time`
no es comparable en paralelo e irreproducibilidad por `std::random_device` (D-05)— **no está
escrito en ninguna parte de la memoria**. Si se quiere conservar, va al cap. 6.

*Decisión anterior (sesión 6), superada*: el cap. 4 no contiene el protocolo experimental (el
usuario lo eliminó al recortar), así que iba al final del cap. 5 con el contenido de
`memoria/notas_cap5_casos_de_estudio.md` §7.

⚠️ **El dato correcto es ahora 3 réplicas**, no 10: la tanda de referencia es
`logs/eval_catalogo_v2` (5 400 logs = 360 × 5 × 3). La cifra de 10 pertenecía al catálogo v1,
obsoleto. Justificación de las 3: con 4-6 instancias por celda domina la varianza **entre
instancias** (±0.15) sobre la de **entre réplicas** (±0.016); comprobado, cbba reproduce su
valor con 3 réplicas (0.5709) frente a 10 (0.5767).

### D-11 · La fila `p^blq_MAX = ρ_j` de la Tabla 4.4 quedó sin explicación — `ABIERTA` (sesión 5)
El cap. 2 (§`subsec:dec-mcts`) define $p^{\mathrm{blq}}_{\mathrm{MAX}}$ como una **constante**
$\in(0,1]$. El cap. 4 la lista en la tabla de hiperparámetros con valor $\rho_j$, que no es una
constante sino un valor por tarea, y el párrafo que lo explicaba se eliminó al recortar. Un
lector atento del cap. 2 verá una aparente contradicción. **Dos salidas**: (a) añadir dos frases
tras la tabla («en la implementación no es un valor global, sino que se instancia con la propia
probabilidad de éxito de la tarea: si se cree que la coalición vecina se ocupará de ella, se
completará con esa probabilidad y solo el complemento $1-\rho_j$ conserva valor»), o (b) retirar
la fila de la tabla. Recomendación: (a), son dos líneas.

### D-12 · Erratas menores localizadas en el cap. 4 — `ABIERTA` (sesión 5)
El capítulo está cerrado; estas tres son las únicas correcciones que el asistente propuso y que
no se han aplicado. Decide el usuario si merecen abrir el fichero:
1. «compromete al robot con **una primer acción**» → «una primera acción» (concordancia).
2. «fijado en $30$ **segundos virtuales**» → «unidades de tiempo virtual», que es lo que usa el
   resto del capítulo (y los escenarios no declaran unidades físicas).
3. §4.3: «Esto se debe **fundamentalmente** a que éste es el objetivo **fundamental**»
   (repetición) y «éste» con tilde, que ya no es normativo.
4. Opcional: §4.4 abre dos párrafos seguidos con «El simulador es…», y alterna «reloj virtual»
   (§4.4) con «reloj global» (§4.4.1); el cap. 2 usa «reloj global $t$».

### D-13 · Las cuatro refutaciones se midieron sobre el catálogo v1 — `ABIERTA` ⚠️ (sesión 6, agravada en la 8)
> ⚠️ **Actualización (sesión 8)**: ahora la distancia es de **dos** catálogos, no de uno. Las
> cuatro intervenciones se midieron con batería 100 **y** con los regímenes antiguos
> (σ = 10, coste de fracaso variable). Si se quieren re-medir, hágase ya sobre el **v3**:
> `scripts/run_catalog.sh` sobre los bloques A y D de `scenarios/catalogo_v3/`.

La **idea 2** de `06_conclusiones.md` («la limitación es del planteamiento, no de la
implementación») se apoya en cuatro intervenciones que no movieron el resultado: cómputo ×40
(`eval_hc`), rondas de comunicación ×30 (`eval_rounds`), `blockingProb` probabilística
(`eval_b4`) y profundidad del plan comunicado (`eval_d8`). **Las cuatro se ejecutaron sobre
escenarios con batería 100**, y el v2 ha demostrado que la restricción de batería cambia el
mecanismo (la tasa de éxito pasa a favorecer a Dec-MCTS).

**La pregunta**: ¿siguen siendo nulas esas intervenciones con batería escasa? No hay motivo
fuerte para pensar que no —ninguna de las cuatro toca la batería—, pero **el argumento del
cap. 6 se apoya en ellas y sería más limpio re-medirlas sobre el v2**. Coste estimado: B4 y
D-08 sobre los bloques A y D del v2 (144 esc. × 3 rép.) ≈ 25 min. Las otras dos heredan tandas
de 12 escenarios y ya se declaran como de baja potencia.

**Recomendación**: re-ejecutar **B4 y D-08** sobre el v2 antes de redactar el cap. 6; para
`eval_hc` y `eval_rounds` basta con mantener la declaración de baja potencia que ya se hace.
No bloquea el cap. 5.

### D-14 · Composición del catálogo y honestidad del agregado — `ABIERTA` (sesión 6, vigente en el v3)
> ⚠️ **Sigue vigente en el v3**, que hereda la composición del v2 sin cambios: el mejor solver de
> cada bloque es idéntico en ambos (A cbba, B dec, C dec, D cbaa, E cbba).

La auditoría a posteriori muestra que los bloques B y C tienen **2 de sus 3 niveles** en
terreno favorable a Dec-MCTS (B: ventanas `P` y `C` sí, `A` no; C: L=3 y L=4 sí, L=8 no),
mientras A, D y E le son adversos. No está escorado a propósito, pero explica el cambio de
signo del agregado. **Tres salidas**: (a) declararlo en el cap. 6 y no liderar nunca con el
agregado — recomendada, no cuesta nada y es lo honesto; (b) añadir un nivel desfavorable a B y
a C para equilibrar (≈+48 esc., ~10 min de cómputo); (c) reportar un agregado ponderado que
compense la composición — desaconsejada, es discutible y nadie lo hace.

### D-16 · El fracaso por falta de batería cobra la demanda completa — `ABIERTA` (sesión 8)
Descubierto al analizar el paso v2 → v3. `simulator.cpp:469-473`: si a un robot no le da la
batería para arrancar una tarea, el simulador fuerza `success = false` y **trunca** la ejecución
al tiempo que su batería le permite. Después, `endTask` (`simulator.cpp:494-498`) le cobra
`averageFailDemand` **entera** (10 en el v3), porque con `averageFailTime == 0` la rama de
consumo proporcional no se aplica. Es decir, **se le cobran 10 a un robot que solo tenía 3**, y
muere.

**Consecuencias medidas**: `random` y `greedy` pierden 0.9–2.3 robots por ejecución en todos los
regímenes, **incluido el determinista**, donde con el v2 no perdían ninguno. `cbaa`, `cbba` y
`dec-mcts` no entran nunca en esta rama porque comprueban la demanda de ejecución y no solo la
del desplazamiento.

**Preguntas para el usuario**:
1. ¿Es la semántica deseada? Alternativa razonable: cobrar solo la parte proporcional al tiempo
   realmente ejecutado, de modo que el robot quede a 0 pero **no** en negativo. Cambiaría la
   mortalidad de los baselines y **obligaría a reejecutar el catálogo otra vez**.
2. Si se mantiene, hay que **declararlo en el cap. 6** al presentar la mortalidad: parte de la
   desventaja de los baselines es este mecanismo y no calidad de asignación.

**Recomendación**: mantenerlo (es defendible: quien arranca una tarea sin poder terminarla paga
el intento) y declararlo. No merece otra reejecución.

### D-15 · ¿Se jubilan los catálogos v1 y v2 del repositorio? — `ABIERTA` (sesión 6, ampliada en la 8)
**Decisión del usuario (sesiones 6 y 8)**: v1 y v2 obsoletos, pero **se mantienen por ahora**. Queda decidir
en la fase de limpieza (objetivo 3.1) si `scenarios/catalogo/` y `logs/eval_catalogo` se suben
a GitHub. Argumento a favor de conservarlos: el cap. 6 cita la comparación v1↔v2 como lección
metodológica y un tribunal podría querer verificarla. Argumento en contra: son 14 400 logs.
Posible término medio: conservar el generador y el CSV (`analisis/resultados.csv`), borrar los
logs en crudo.

### D-17 · Renombrar los escenarios en la limpieza del repositorio — `PENDIENTE` (sesión 8)
**Decisión del usuario (sesión 8)**: el capítulo 5 nombra los ficheros de escenario como

    esc_{bloque}_r{robots}_n{tareas}_{ventana}_{régimen}_{coalición}_i{instancia}

y pone como ejemplo `esc_A_r05_n030_E_est_q1_i3`. **En el repositorio se llaman hoy**
`cv3_A_r05_n030_bnd_E_est_q1_i1`: distinto prefijo (`cv3_` frente a `esc_`) y con un campo
extra de geometría (`bnd`) que la memoria no menciona.

**Es deliberado**: el usuario no quiere que la jerga de versiones del repositorio (`cv2`, `cv3`)
se cuele en la memoria, y el campo `bnd` sobra porque la geometría es única en todo el catálogo.

**Qué hay que hacer, y CUÁNDO**: renombrar en la **fase de limpieza del repositorio**
(objetivo 3.1), junto con el borrado de versiones anteriores y de las tandas innecesarias.
Alcance del renombrado:
- 360 ficheros en `scenarios/catalogo_v3/` (y probablemente el propio directorio).
- **5 400 ficheros de registro** en `logs/eval_catalogo_v3/`, cuyo nombre empieza por el del
  escenario (`{escenario}_{solver}_{reward}_{replica}.log`).
- La columna `escenario` de `analisis/catalogo_v3.csv` (o regenerarlo con
  `scripts/extract_metrics.py` tras el renombrado).
- El prefijo en `scripts/generate_catalog_v3.py` (línea del `name = f"cv3_..."`) y el patrón
  `RE_NEW` de `scripts/extract_metrics.py`, que además espera el campo de geometría de 3
  letras: al quitar `bnd` **hay que ajustar la expresión regular** o el CSV saldrá vacío.
- La búsqueda por patrón `_n{m}_` de `scripts/figura_geometria_cap5.jl` (esa sigue valiendo).

⚠️ **No renombrar antes de terminar el cap. 6**: las figuras y tablas del análisis se generan
desde el CSV y cualquier renombrado a medias deja los logs huérfanos de sus escenarios.
Es mecánico y scriptable, pero toca ~5 760 ficheros: hacerlo de una vez y regenerar el CSV.

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
