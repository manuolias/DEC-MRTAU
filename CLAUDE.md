# CLAUDE.md — TFM: MRTA Distribuido bajo Incertidumbre (DEC-MRTAU)

## Antes de nada

Lee `.claude-notes/README.md` y sigue el protocolo de sesión que describe. En resumen:
al empezar, leer `.claude-notes/01_contexto.md`, las últimas entradas de
`.claude-notes/04_bitacora.md` y `.claude-notes/05_dudas.md`; al terminar, actualizarlos.

📌 **La fase actual es la REDACCIÓN DE LA MEMORIA.** Antes de escribir cualquier capítulo es
obligatorio leer **`.claude-notes/06_conclusiones.md`**: contiene las cinco ideas que sostienen
el trabajo, dictadas por el usuario, con las cifras que las respaldan y los avisos sobre qué no
se puede afirmar. ⚠️ Ese fichero se escribió sobre el catálogo v1; **leer sus DOS addendums
finales**, que recogen lo que los catálogos v2 y v3 confirman y lo que matizan. El vigente es
el del **v3**.

📌 **Capítulo 5 CERRADO** (sesión 8): introducción + §5.1 configuración común + §5.2 los cinco
bloques, uno por subapartado. Sus dos figuras existen. ⚠️ El usuario **eliminó la
Recapitulación** y la reserva para **abrir el capítulo 6**: con ella se fueron las cifras
agregadas del catálogo, las **3 réplicas** y las **5 400 ejecuciones**, que hoy no aparecen en
ninguna parte de la memoria.

📌 **Capítulo 6 ESCRITO** (sesión 10): ~4 400 palabras, 7 tablas y **4 de las 14 figuras** del
cuaderno, copiadas a `memoria/figures/06_experimentación_pruebas/`. Estructura del cuaderno
(conjunto · por bloque · magnitudes secundarias) + §6.1 protocolo experimental, que **resuelve
D-10**. ⚠️ Pendiente: **borrar el bloque comentado del final del cap. 5** (su contenido, incluida
`eq:ejecuciones`, está ya en la introducción del 6 ⇒ etiqueta duplicada si se descomenta).

📌 **SIGUIENTE SESIÓN**: escribir el **capítulo 7** (`07_conclusiones_trabajo_futuro.tex`, etiqueta
`cap:conclusiones_trabajo_futuros`) con las ideas 1, 2, 4 y 5 de `.claude-notes/06_conclusiones.md`
— **addendum 2**, el vigente. Es el capítulo donde van las valoraciones que el 6 deja fuera.
Antes, decidir D-03 (cómo citar el paper del tutor), D-13 y D-14.

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

| Solver | global | det (ρ=1) | lev (ρ=.9) | est (ρ=.75) | fue (ρ=.5) |
|---|---|---|---|---|---|
| **dec-mcts-v4-g9999** | **0.502** | **0.585** | 0.510 | **0.444** | 0.278 |
| cbba | 0.494 | 0.565 | **0.520** | 0.438 | **0.328** |
| cbaa | 0.437 | 0.512 | 0.471 | 0.377 | 0.263 |
| greedy | 0.354 | 0.403 | 0.340 | 0.324 | 0.207 |
| random | 0.271 | 0.308 | 0.261 | 0.247 | 0.162 |

⚠️ **El agregado sigue siendo un EMPATE**: Δ = +0.0086 ± 0.0040 y por recuento queda
158 gana / 42 empata / 160 pierde de 360. **Nunca liderar con el agregado; reportar siempre por
bloque y por régimen.**

Lo sólido: (a) en determinista Dec-MCTS **gana** (+0.020 ± 0.006); (b) bajo incertidumbre su
pendiente frente al nº de robots es **+0.0040 (t = 1.5)**, indistinguible de cero, frente a
**+0.0195 (t = 6.6)** de CBBA — convierte robots en rendimiento unas cinco veces peor;
(c) en incertidumbre fuerte pierde con claridad (−0.050 ± 0.017); (d) **CBAA es el mejor solver
del bloque de coaliciones**. Argumento completo en `.claude-notes/06_conclusiones.md` + addendums.

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
- **Cap. 6 (`06_experimentación_pruebas.tex`): borrador completo (sesión 10)**, pendiente de la
  revisión del usuario. Estructura del cuaderno + §6.1 protocolo experimental (resuelve D-10).
  7 tablas y 4 figuras (`memoria/figures/06_experimentación_pruebas/`). ⚠️ Notación: la memoria
  usa **$n$ = robots** y $m$ = tareas; el cuaderno se corrigió para coincidir.
- **Siguiente capítulo a escribir: el 7 (Conclusiones y trabajo futuro)**, etiqueta
  `cap:conclusiones_trabajo_futuros`. Ideas 1, 2, 4 y 5 del **addendum 2** de
  `.claude-notes/06_conclusiones.md`. Es donde van las valoraciones que el cap. 6 deja fuera.
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
  OBSOLETOS**; el vigente es el **v3**. Se conservan a propósito: la comparación v1↔v2 **es** la
  lección metodológica del cap. 6. Pero **ninguna cifra del v1 ni del v2 debe presentarse como
  resultado del trabajo**.
- ⚠️ **El agregado global depende de la composición de bloques.** Auditoría hecha a
  posteriori: los bloques B y C tienen 2 de sus 3 niveles en terreno favorable a Dec-MCTS
  (B: ventanas `P` y `C` sí, `A` no; C: L=3 y L=4 sí, L=8 no); A, D y E le son adversos. No
  está escorado a propósito, pero **hay que declararlo en la memoria**.
- ⚠️ **Mortalidad de robots, y en el v3 es MAYOR que en el v2**: `random` pierde 1.9–2.3 robots
  por ejecución y `greedy` 0.9–1.8, **también en régimen determinista** (65 % y 50 % de las
  ejecuciones, donde en el v2 eran 0). `cbaa`, `cbba` y `dec-mcts`, **ninguno en 5400**.
  Parte de la desventaja de los baselines es esto, no calidad de asignación. Reportarlo aparte.
- ⚠️ **En determinista TAMBIÉN puede fracasar una tarea**, aunque ρ=1: si al arrancarla el robot
  no tiene batería suficiente, `simulator.cpp:469-473` fuerza `success = false` y trunca la
  ejecución; después `endTask` (`simulator.cpp:494-498`) le cobra `averageFailDemand` **entera**
  porque `averageFailTime == 0`. Con el v3 eso son 10 y el robot muere. Es el mecanismo que
  explica la mortalidad de los baselines en det y **por qué el det NO es un control invariante
  entre catálogos** para `random` y `greedy` (sí lo es para cbaa, cbba y dec-mcts).
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
