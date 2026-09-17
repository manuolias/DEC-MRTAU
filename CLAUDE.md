# CLAUDE.md — TFM: MRTA Distribuido bajo Incertidumbre (DEC-MRTAU)

## Antes de nada

Lee `.claude-notes/README.md` y sigue el protocolo de sesión que describe. En resumen:
al empezar, leer `.claude-notes/01_contexto.md`, las últimas entradas de
`.claude-notes/04_bitacora.md` y `.claude-notes/05_dudas.md`; al terminar, actualizarlos.

📌 **La fase actual es la REDACCIÓN DE LA MEMORIA.** Antes de escribir cualquier capítulo es
obligatorio leer **`.claude-notes/06_conclusiones.md`**, que contiene las ideas que sostienen el
trabajo. 🔴 **PERO ESTÁ OBSOLETO desde el 17-09-2026** y lleva un banner que lo dice: sus cinco
ideas y sus dos addendums defienden el **empate** frente a CBBA, que la reejecución del catálogo
deshizo. **Rehacerlo es la primera tarea pendiente** y requisito previo al capítulo 7. Mientras
tanto, la fuente de verdad de las cifras son este fichero, «REEJECUCIÓN 17-09-2026» en
`.claude-notes/03_experimentos.md` y el capítulo 6 ya cerrado.

📌 **Capítulo 5 CERRADO** (sesión 8): introducción + §5.1 configuración común + §5.2 los cinco
bloques, uno por subapartado. Sus dos figuras existen. ⚠️ El usuario **eliminó la
Recapitulación** y la reserva para **abrir el capítulo 6**: con ella se fueron las cifras
agregadas del catálogo, las **3 réplicas** y las **5 400 ejecuciones**, que hoy no aparecen en
ninguna parte de la memoria.

📌 **Capítulo 6 CERRADO por el usuario** (sesión 12), sobre la **tanda reejecutada**: 7 figuras y
5 tablas en `memoria/figures/06_experimentación_pruebas/`. Estructura del cuaderno (conjunto · por
bloque · magnitudes secundarias) + §6.1 protocolo experimental, que **resuelve D-10**.
⚠️ Sigue pendiente: **borrar el bloque comentado del final del cap. 5** (su contenido, incluida
`eq:ejecuciones`, está ya en la introducción del 6 ⇒ etiqueta duplicada si se descomenta).

📌 **COLA DE TRABAJO — LO PRIMERO DE LA PRÓXIMA SESIÓN**:
1. ✅ **Sesión 12 cerrada**: simulador corregido (dos defectos de batería), catálogo reejecutado,
   cuaderno reejecutado y **capítulo 6 CERRADO por el usuario** sobre los datos nuevos.
   Detalle completo en la sesión 12 de `.claude-notes/04_bitacora.md`.
2. 🔴 **AHORA: rehacer `.claude-notes/06_conclusiones.md`.** Sus cinco ideas y sus dos addendums
   sostienen la tesis del **empate** y la de «el conjunto de prueba determina la conclusión», y
   **las dos se han caído**. El fichero lleva un banner de obsoleto. Como el protocolo obliga a
   leerlo antes de redactar cualquier capítulo, **rehacerlo es requisito previo al capítulo 7**.
   Material: sesión 12 de la bitácora, «REEJECUCIÓN 17-09-2026» en `03_experimentos.md` y el
   propio capítulo 6. Conservar de él la **estructura** y los **límites que el usuario fijó**
   (la conjetura sobre la literatura va como teoría; la comparación centralizado/distribuido no se
   mide aquí y se apoya en el paper del tutor sin cifras de pérdida ⇒ D-03).
3. Después, el **capítulo 7** (`07_conclusiones_trabajo_futuro.tex`, etiqueta
   `cap:conclusiones_trabajo_futuros`). Antes hay que decidir **D-03** (cómo citar el paper del
   tutor; es estructural para ese capítulo), D-13 y D-14.
4. Luego el **capítulo 1** y la **limpieza** (objetivo 3.1), que ahora incluye borrar
   `logs/eval_catalogo_v3_bug` y `analisis/catalogo_v3_bug.csv` (decisión del usuario).

`.claude-notes/` es el cuaderno de trabajo del asistente y **no forma parte de la entrega**.

## Qué es este proyecto

TFM de Manuel Olías (tutor: Ignacio Pérez-Hurtado). Resuelve la **asignación de tareas
multi-robot bajo incertidumbre** con tiempos continuos, coaliciones, ventanas de ejecución y
restricciones de batería, pasando del modelo **centralizado** del tutor a un paradigma
**puramente distribuido** (DEC-POGSMDP: cada robot decide con información local + comunicación).

Cinco solvers comparados: `random`, `greedy`, `cbaa`, `cbba` y **`dec-mcts`** (v1–v4).

### Resultado de la experimentación — CATÁLOGO v3 (sesión 8, tanda de referencia)

Conjunto oficial: **`scenarios/catalogo_v3/`, 360 escenarios × 5 solvers × 3 réplicas**,
tanda `logs/eval_catalogo_v3`. Batería escasa (capacidad 40) en todos los escenarios.
Regímenes: coste de un intento **fallido** 10 fijo en todos ellos (con éxito el consumo es
proporcional a la duración real, esperanza 10) y σ de la duración 0/1/3/5.

🔴 **Cifras de la REEJECUCIÓN de la sesión 12** (17-09-2026), tras corregir los dos defectos de
batería del simulador. Las anteriores ya no valen.

| Solver | global | det (ρ=1) | lev (ρ=.9) | est (ρ=.75) | fue (ρ=.5) |
|---|---|---|---|---|---|
| **dec-mcts-v4-g9999** | **0.502** | **0.586** | **0.495** | **0.443** | **0.282** |
| cbba | 0.459 | 0.532 | 0.467 | 0.405 | 0.273 |
| cbaa | 0.401 | 0.483 | 0.413 | 0.339 | 0.207 |
| greedy | 0.346 | 0.396 | 0.338 | 0.313 | 0.198 |
| random | 0.268 | 0.305 | 0.266 | 0.242 | 0.168 |

✅ **La hipótesis del TFM se cumple**: Δ = **+0.0431 ± 0.0039** (t = 10.9), recuento
**244 gana / 33 empata / 83 pierde** de 360. Gana en los **cuatro regímenes** y en **cuatro de los
cinco bloques** (en D gana cbaa, 0.418). Máxima recompensa en 190 de 360; arrepentimiento 0.018.

Lo sólido: (a) la corrección **no beneficia a Dec-MCTS** —su recompensa no se mueve
(−0.001 ± 0.002)—, son cbaa y cbba las que pierden 0.035 cada una; (b) la ventaja **se erosiona**
con la incertidumbre (+0.054 det → +0.008 fue) y con el equipo: bajo incertidumbre la pendiente de
Dec-MCTS frente al nº de robots es **+0.0016 (t = 0.7)**, nula, frente a **+0.0120 (t = 4.5)** de
CBBA, y la Δ decrece −0.0104/robot (t = −3.5) cruzando el cero hacia los 4 robots; (c) su mejor
terreno sigue siendo la **ventana escalonada** (+0.084, gana 12/12 en det); (d) **CBAA es el mejor
solver del bloque de coaliciones**; (e) Dec-MCTS es el que **menos robots pierde** (1.48/escenario
frente a 2.51 de cbba) y el que más distancia recorre (+53.8 %).

⚠️ **El leave-one-block-out ya NO invierte el signo** (queda entre +0.031 y +0.051 quitando
cualquier bloque) ⇒ **la lección metodológica de «el conjunto de prueba determina la conclusión» se
cayó**, y con ella los dos addendums de `.claude-notes/06_conclusiones.md`, que están **obsoletos**.
📌 **El usuario revisará el cap. 6 y decidirá el nuevo enfoque: no adelantarse.**

## Cómo se ejecuta

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make      # ejecutable: simulador
./simulador <dirEscenarios> <dirLogs> [configExperimento]    # todos los args son opcionales

python3 scripts/generate_catalog_v3.py                       # regenera los 360 escenarios
scripts/run_catalog.sh 6 scenarios/catalogo_v3 logs/eval_catalogo_v3 \
        scenarios/catalogo_v3/experiment_config.yaml         # ~35 min con 6 procesos
```

`experiment_config.yaml` define `solvers`, `reward_functions` y `replicas`. El ejecutor es
**idempotente**: salta los `.log` ya completos, así que se puede reanudar una tanda.
Compilar en Release importa (×3 de velocidad); el número de iteraciones de MCTS es fijo, así
que optimizar **no altera los resultados**, solo el `computing_time`.

Métrica reportada: `final_reward` = **fracción de tareas completadas** (`reward00` con k1=1).

### Análisis

```bash
python3 scripts/extract_metrics.py logs/eval_catalogo_v3 -o analisis/catalogo_v3.csv
python3 scripts/analyze_catalog_v3.py analisis/catalogo_v3.csv   # todas las tablas del v3
python3 scripts/compare_catalogos.py analisis/catalogo_v2.csv analisis/catalogo_v3.csv
```

`scripts/extract_metrics.py` (solo biblioteca estándar) sustituye a la herramienta externa
`mrtau metrics` y añade métricas derivadas de eventos (tareas intentadas, tasa de éxito,
instante de retirada). `scripts/analyze_catalog_v3.py` produce el análisis completo del v3 y
`scripts/compare_catalogos.py` compara dos catálogos de forma pareada. Todo sin dependencias
externas.

⭐ **`analisis/evaluacion_final.ipynb`** (sesión 9) es el análisis vigente del v3, en **tres
partes** —(1) conjunto, (2) por bloque, (3) métricas adicionales— con **14 figuras** en
`analisis/figuras/p1_*`, `p2_*`, `p3_*` (PNG+PDF). Se ejecuta con el venv:

```bash
.venv/bin/python -m jupyter nbconvert --to notebook --execute --inplace analisis/evaluacion_final.ipynb
```

Cubre lo mismo que `analyze_catalog_v3.py` y añade: *leave-one-block-out* del agregado, matriz de
dominancia pareada, arrepentimiento, mortalidad, distancia por tarea completada, coste frente al
tamaño del equipo e instante de retirada. Índice completo en `analisis/README.md`.
⚠️ El cuaderno del catálogo v1 se conserva como **`analisis/evaluacion_v1_obsoleto.ipynb`** (con
banner de obsoleto): es la única versión ejecutable de las **cuatro refutaciones** y de la
**ablación del canal**, que no se han remedido sobre el v3 (D-13). Sus figuras son `01_*`–`08_*`.

## Mapa del repositorio

| Ruta | Contenido |
|---|---|
| `src/` | `main.cpp` (ejecutor), `simulator.cpp` (motor de eventos), `scenario/state/logger` |
| `include/tau/` | Cabeceras y **todos los solvers** (header-only) |
| `scenarios/catalogo_v3/` | ⭐ **Catálogo oficial: 360 escenarios** (`scripts/generate_catalog_v3.py`) |
| `scenarios/catalogo_v2/` | Catálogo v2, 360 esc. — **OBSOLETO**, misma estructura con otros regímenes |
| `scenarios/probe/` | Sonda de variantes de diseño, 204 esc. (`scripts/generate_probe_scenarios.py`) |
| `scenarios/catalogo/` | Catálogo v1, 288 esc. — **OBSOLETO**, se conserva como contraste |
| `scenarios/` | Catálogos exploratorios previos: `bundles/` (208), `random/` (45), `salomon/` (56, sin usar) |
| `scripts/` | Generadores, `run_catalog.sh`, `extract_metrics.py`, `analyze_catalog_v3.py`, `compare_catalogos.py`, `analyze_probe.py`, `figura_geometria_cap5.jl` |
| `logs/` | Tandas de experimentos (⚠️ no todas son válidas, ver notas) |
| `analisis/` | ⭐ `evaluacion_final.ipynb` + `figuras/p1_*,p2_*,p3_*` y `catalogo_v3.csv` (**vigentes**); `evaluacion_v1_obsoleto.ipynb` + `figuras/01_*…08_*`, `catalogo_v2.csv`, `probe.csv`, `resultados.csv` (histórico) |
| `memoria/` | Proyecto LaTeX. Caps. 2, 3, 4 y 5 completos; 1, 6 y 7 pendientes |

## Reglas de trabajo (impuestas por el usuario)

1. **Identificar el propósito** de un fichero, clase o variable **antes** de tocarlo. Ningún
   cambio puede perder información relevante.
2. **Preguntar, no dar nada por sentado** sobre intención, arquitectura o requisitos.
3. **Señalar explícitamente las dudas** antes de seguir adelante. Admitir lo que no se sabe.
4. Se agradecen las sugerencias de mejora, especialmente las de impacto duradero.
5. **Ceñirse a los objetivos y peticiones**: nada de trabajo de más sin preguntar antes.

### Sobre la memoria

- **Caps. 2 (`02_marco_teórico.tex`), 3 (`03_estado_del_arte.tex`) y 4
  (`04_materiales_y_metodos.tex`) están terminados y CERRADOS: NO modificarlos sin preguntar
  antes, y solo si es estrictamente necesario.** El briefing
  `memoria/notas_cap4_materiales_y_metodos.md` quedó obsoleto tras escribir el cap. 4.
- **Cap. 5 (`05_casos_de_estudio.tex`) CERRADO** (sesión 8): borrador del asistente reescrito
  por el usuario. El briefing `memoria/notas_cap5_casos_de_estudio.md` quedó **obsoleto**
  (proponía ~20 páginas y describe los regímenes del v2). ⚠️ **D-10 sigue de hecho sin
  resolver**: el protocolo experimental **no** entró en ninguna parte de la memoria, ni
  tampoco las 3 réplicas ni las 5 400 ejecuciones, al eliminarse la recapitulación.
  ⚠️ Dos correcciones señaladas y no aplicadas, de una línea cada una: «formato yaml» → «YAML»,
  y «$L$ representa el número de **tareas** por robot» → **plazas de trabajador** (con
  coaliciones no coinciden; es el motivo del reajuste $m=Ln/\bar q$).
- **Cap. 6 (`06_experimentación_pruebas.tex`): terminado por el usuario (sesión 11) y
  ACTUALIZADO a la reejecución (sesión 12)**. 5 tablas y 7 figuras
  (`memoria/figures/06_experimentación_pruebas/`). Resuelve D-10. ⚠️ Notación: la memoria usa
  **$n$ = robots** y $m$ = tareas. ✅ **Cerrado por el usuario el 17-09-2026**, sin marcas `\red{}`.
  ✅ **Enfoque decidido: conserva la Δ dec−cbba como hilo**, porque «establece la mejora introducida
  por mi algoritmo», aunque ya no sea la comparación más reñida. **Sin pendientes.**
  ⚠️ Las figuras de la memoria las exporta el propio cuaderno con
  `guardar(fig, nombre, memoria='<fichero>')`, **sin `suptitle`**: al reejecutarlo se actualizan solas.
- **Siguiente capítulo a escribir: el 7 (Conclusiones y trabajo futuro)**, etiqueta
  `cap:conclusiones_trabajo_futuros`. Es donde van las valoraciones que el cap. 6 deja fuera.
  🔴 **Antes hay que rehacer `.claude-notes/06_conclusiones.md`**: sus cinco ideas y su addendum 2
  ya no describen el resultado medido.
- ⚠️ **Registro OBJETIVO en el análisis de resultados** (exigido por el usuario, 2026-09-11):
  describir **qué se mide y cómo se obtiene**, no lo que se espera ver; nada de dar por supuesto
  que «queremos que gane Dec-MCTS»; las explicaciones mecanísticas se marcan como
  **interpretación**. Las valoraciones y la defensa de la hipótesis van **solo en el cap. 7**.
  Aplica al cap. 6 y ya está aplicado en `analisis/evaluacion_final.ipynb`.
- **Sin ablaciones en la memoria** (decisión del usuario, 2026-08-21): los hiperparámetros se
  presentan como fijados empíricamente durante el desarrollo.
- Redacción en **LaTeX y español**, registro académico formal y riguroso, reutilizando los
  nombres de variables ya definidos, con `\ref{}` a las secciones previas y citas adecuadas.
- El usuario no compila LaTeX localmente: importa el contenido, no que compile.
- Reparto: cap. 4 = materiales/arquitectura/framework · cap. 5 = escenarios · cap. 6 =
  resultados · cap. 7 = conclusiones y trabajo futuro. Hay un briefing detallado para el cap. 4
  en `memoria/notas_cap4_materiales_y_metodos.md`, y el material de los caps. 6 y 7 en
  `.claude-notes/06_conclusiones.md`.

## Avisos importantes

- ⚠️ **Los catálogos v1 (`scenarios/catalogo/`) y v2 (`scenarios/catalogo_v2/`) están
  OBSOLETOS**; el vigente es el **v3 reejecutado**. **Ninguna cifra del v1 ni del v2 debe
  presentarse como resultado del trabajo.** ⚠️ La comparación v1↔v2 **ya no entra en el cap. 6**.
- ⚠️ **Toda tanda anterior al 17-09-2026 se ejecutó con los dos defectos de batería** y sus cifras
  de recompensa **no son comparables** con las actuales. `logs/eval_catalogo_v3_bug` y
  `analisis/catalogo_v3_bug.csv` guardan la tanda anterior; **se borran en la limpieza**.
- ⚠️ **La magnitud de la ventaja depende de la composición de bloques** (el signo ya no): va de
  +0.012 en el bloque D a +0.090 en el C, y el *leave-one-block-out* la deja entre +0.031 y +0.051.
  Declarado en §6.2.3.
- ✅ **LOS DOS DEFECTOS DE BATERÍA ESTÁN CORREGIDOS (sesión 12).** Se conserva la descripción
  porque explica la diferencia entre la tanda vieja y la nueva:
  1. **Navegar no mataba**: `calculateBatteryConsumption` (`simulator.cpp:40-46`) saturaba en 0.
     Corregido quitando el `std::max`; las tres ramas `finalBattery < 0` (293, 328, 390) ya se
     activan.
  2. **El intento truncado por batería se re-tarifaba**: `startTask` ya truncaba la duración a
     `tFail`, pero `endTask` recalculaba el consumo desde el flag `success` y aplicaba la tarifa
     del fracaso (10 planos), así que el robot **sobrevivía con batería de regalo**. Corregido con
     dos bits en el `payload` del `TASK_END` (bit 0 = desenlace muestreado, bit 1 = interrumpido
     por batería) y `endTask(TaskID, int)`; ver `BATTERY_EPS` en `definitions.hpp`.
  Ambos replicados en **`solver_DecMCTS_v4.hpp`** (v1–v3 **no**: nueva divergencia a declarar).
  ⇒ `failed_agents` **ya es válido**: random 2.79 · greedy 2.54 · cbba 2.51 · cbaa 1.83 ·
  **dec-mcts 1.48**. En el bloque A, fracción de flota: **cbba 55 %** (el peor: sus paquetes no
  presupuestan la vuelta a la estación), random 45 %, cbaa 37 %, greedy 35 %, **dec-mcts 30 %**.
  Las tareas completadas por robots ya sin batería cayeron del 8.7–8.9 % (cbaa/cbba) al **0.01 %**.
- ⚠️ **En determinista TAMBIÉN puede fracasar una tarea**, aunque ρ=1: si al arrancarla el robot
  no tiene batería suficiente, `simulator.cpp:470-474` fuerza `success = false` y trunca la
  ejecución en el instante en que se agota. Desde la sesión 12 el cargo es **proporcional al tiempo
  ejecutado** (no los 10 planos), así que el robot acaba exactamente en 0 y **muere**. Explica la
  mortalidad de los baselines en det. ⚠️ Con σ=0 y reserva de 10, cbaa/cbba/dec-mcts **nunca**
  entran en esta rama en determinista: sus tasas de éxito en det valen exactamente 1.000.
- ⚠️ **El segundo valor del campo `demand` del YAML NO es una desviación típica**: el simulador
  lo lee como `averageFailDemand`, la batería que cuesta un intento fallido
  (`src/scenario.cpp:63-64`). En el v3 vale 10 en todos los regímenes. ⚠️ Con **éxito** el
  consumo NO es 10 fijo: es proporcional a la duración real (tasa 1, esperanza 10, tan disperso
  como σ). Solo el **fracaso** cuesta 10 exactos. No describirlo como σ ni decir que un intento
  cuesta siempre 10.
- ⚠️ **Logs inválidos**: `logs/prueba`, `logs/ablacion_blockprob` y `logs/ablacion_bpcap` son
  anteriores a la corrección del bug de caducidad (commit `466190a`) y están inflados. Los
  `*.csv` de `logs/` proceden todos de `logs/prueba` ⇒ **no usarlos**.
- ⚠️ **Dec-MCTS lleva dentro una réplica del simulador** (`makeState`/`makeEventQueue`/
  `physics*`). Si se toca la física de `simulator.cpp`, hay que replicar el cambio en los
  cuatro solvers o las estimaciones quedan sesgadas. **Divergencia viva**: v1, v2 y v3 aún
  modelan la semántica *antigua* de caducidad; solo v4 replica la corregida. Como la evaluación
  final usa **solo v4**, no afecta a los resultados, pero hay que declararlo si el cap. 4
  describe v1–v3. Ver `.claude-notes/02_codigo.md` y la duda D-06.
- ⚠️ **Réplicas: 3** (el v2 y el v3 se ejecutaron así; el v1 usó 10). Con 4-6 instancias distintas por
  celda, la varianza que domina es la de **entre instancias** (±0.15), no la de entre réplicas
  (±0.016). Al reportar diferencias, calcular el error estándar **entre escenarios**.
  Esto deroga el antiguo criterio C-6 de `03_experimentos.md`.
- ⚠️ **La ventana escalonada es ~5 unidades más apretada** que la de solo plazo (plazo medio
  24.67 frente a 29.67; la estrecha 29.97 y la ancha 49.87). Es correcto y deliberado —el `+10`
  de las otras tres es la anchura nominal de la ventana de referencia, que la escalonada no
  hereda porque su plazo no depende de $t_0$—, pero **declararlo en el cap. 6** al destacar que
  la escalonada es el mejor terreno de Dec-MCTS (gana 12/12 en determinista).
- ⚠️ **Comparaciones siempre pareadas por escenario** y **reportadas por régimen**: el ranking
  cambia entre determinista y estocástico, y agregarlo todo esconde el resultado.
- ⚠️ **Cuidado con el diseño del conjunto de prueba**: ligar el horizonte al nº de tareas
  confunde «más robots» con «menos carga por robot» y produjo una conclusión falsa que costó
  una sesión detectar. El catálogo actual está calibrado a **iso-dificultad** (carga por robot
  constante L=6, horizonte fijo T=40); la calibración se rehízo con batería 40 y L=6 aguantó.
- ⚠️ **No subir `battery_rate_while_navigating` por encima de 1.** Medido: batería escasa sola
  favorece a Dec-MCTS (+0.057), pero **batería escasa con navegación cara es su peor terreno**
  (−0.055, pierde 12/12 escenarios). Dec-MCTS recorre un ~22 % más de distancia que CBBA.
- No hay semilla configurable (`std::random_device` en todas partes): promediar réplicas.
