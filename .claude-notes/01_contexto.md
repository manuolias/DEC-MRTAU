# 01 — Contexto del proyecto

## Qué es

TFM: **Multi-Robot Task Allocation Distribuido bajo Incertidumbre (DEC-MRTAU)**.
Autor: Manuel Olías. Tutor: Ignacio Pérez-Hurtado (autor del paper base, aún no publicado:
*"Online Monte-Carlo Tree Search for Multi-Robot Task Allocation in Uncertain Domains
requiring Recharging Operations"*).

**Punto de partida**: el modelo del tutor es **centralizado** (un MCTS único con estado global).
**Aportación del TFM**: pasar a un paradigma **puramente distribuido** — cada robot es un
agente autónomo con su propio solver, sin coordinador ni estado compartido; la coordinación
emerge de la comunicación.

**Hipótesis a demostrar**: Dec-MCTS (el algoritmo más elaborado, el que más esfuerzo ha
costado) debe rendir **por encima** de los otros cuatro solvers (random, greedy, CBAA, CBBA).

**Estado de la hipótesis (tras la sesión 8): NI se confirma NI se refuta — depende del
conjunto de prueba, y esa es la conclusión.**

| | catálogo v1 (obsoleto) | catálogo v2 (obsoleto) | **catálogo v3 (oficial)** |
|---|---|---|---|
| escenarios × réplicas | 288 × 10, batería 100 | 360 × 3, batería 40 | **360 × 3, batería 40** |
| regímenes (σ / coste de fallo) | — | 0/3/10/10 · 0/3/10/10 | **0/1/3/5 · 10 fijo** |
| cbba | **0.6031** | 0.482 | 0.494 |
| dec-mcts-v4-g9999 | 0.5676 | **0.487** | **0.502** |
| Δ pareada | **−0.0355 ± 0.0034** | +0.0047 ± 0.0044 | **+0.0086 ± 0.0040** |
| recuento | 67 / 26 / 195 | 154 / 33 / 173 | **158 / 42 / 160** |

El v2 dio la vuelta al signo del agregado **sin tocar una línea de código del solver**, y el v3
lo mantiene; pero en ambos es un **empate** y por recuento de escenarios sigue siendo una moneda
al aire. La lectura correcta —y la aportación metodológica más fuerte del trabajo— es que **el
diseño del conjunto de prueba determina la conclusión**, ahora demostrado en las dos direcciones.

**Lo que es sólido en el v3, por bloque y por régimen:**
- **Determinista: Dec-MCTS gana** (+0.020 ± 0.006). Incertidumbre media: empate (+0.006 ± 0.006);
  incertidumbre fuerte: **pierde claramente** (−0.050 ± 0.017).
- **Escalado** (la formulación más limpia de la tesis): bajo incertidumbre la pendiente propia de
  Dec-MCTS frente a $R$ es **+0.0040 (t = 1.5)**, indistinguible de cero, frente a
  **+0.0195 (t = 6.6)** de cbba — unas **cinco veces peor**. Y la pendiente de la propia Δ es
  **−0.0155 por robot (t = −4.7)**: la desventaja crece linealmente con el equipo (mecanismo
  H-07). En determinista sí escala (+0.0121, t = 7.7) y la Δ apenas cae (−0.0043, t = −2.0).
  ⚠️ Con el v2 la pendiente propia era −0.0004 (t = −0.2) y se decía «no convierte robots en
  rendimiento **en absoluto**». **Esa frase ya no vale**; ver el addendum 2 de `06_conclusiones.md`.
- **Ventana escalonada**: +0.078, gana **12 de 12** en determinista. Su mejor terreno.
- **Espera vs plazo**: quitar la espera manteniendo los plazos le da +0.031 ± 0.011.
- **Carga**: es su mejor bloque (+0.061 ± 0.013); gradiente monótono en L y en R.
- **Coaliciones**: es el único bloque donde **cbaa** es el mejor de los cinco (0.431 vs 0.407).
  Y la coalición mixta va al revés que la uniforme: `qm` +0.023 det, `q2` −0.052 det.
- **Tasa de éxito**: Dec-MCTS por encima de cbba en est (0.754 vs 0.719); la brecha vive en la
  **tasa de intento** (17.7 vs 19.3 tareas intentadas).
- ⚠️ **Mortalidad**: `random` y `greedy` pierden 0.9–2.3 robots por ejecución **también en
  determinista**; cbaa, cbba y dec-mcts, ninguno en 5 400. Reportar aparte.

⚠️ Cifras del v1 y del v2 que **ya no deben citarse como resultado**: los rankings globales, la
Δ −0.0355 (v1) y +0.0047 (v2), la pendiente −0.0130/robot (v1) y −0.0004 (v2) y las tablas por
familias. Se conservan solo como contraste metodológico en el cap. 6.

## El problema (resumen formal, cap. 2 de la memoria)

Variante de MRTA con: robots ST (una tarea a la vez), tareas MR (coaliciones de $q_j$
trabajadores), asignación TA (secuencias), ventanas de ejecución $[t_j^s, t_j^l]$,
heterogeneidad (`Cap(i,j)`), batería con estaciones de recarga. Sobre esto, tres fuentes de
incertidumbre: éxito de tarea ($\rho_j$), duraciones gaussianas (distintas en éxito y fracaso)
y consumo de batería estocástico derivado.

Progresión de modelos formalizada en la memoria:

| Variante | Tiempo | Incertidumbre | Observabilidad | Modelo |
|---|---|---|---|---|
| MRTA | discreto | no | global | MDP |
| MRTAU | continuo | sí | global | GSMDP |
| **DEC-MRTAU** | continuo | sí | local + comunicación | **DEC-POGSMDP** |

**DEC-POGSMDP** es la aportación teórica: DEC-POSMDP + lógica de eventos del GSMP (relojes
ocultos). Resuelve el *leak temporal* (que la política vea instantes de finalización aún no
producidos). En código se materializa como *sample-at-fire*: el resultado y la duración de
una tarea se muestrean al **dispararse** el evento y viajan ocultos en el `payload`.

Los cinco algoritmos se leen de forma unificada como **políticas de comunicación** con grados
crecientes de coordinación:
`Random/Greedy` (sin comunicación) → `CBAA` (puja por tarea) → `CBBA` (puja por paquete) →
`Dec-MCTS` (distribución de probabilidad sobre planes).

## Estado de la memoria (`memoria/`)

| Capítulo | Fichero | Estado |
|---|---|---|
| 1. Introducción | `01_introducción.tex` | **vacío** (se deja para el final) |
| 2. Marco teórico | `02_marco_teórico.tex` | **completo** — NO modificar sin preguntar |
| 3. Estado del arte | `03_estado_del_arte.tex` | **completo** — NO modificar sin preguntar |
| 4. Materiales y métodos | `04_materiales_y_metodos.tex` | **completo y CERRADO (sesión 5)** — NO modificar salvo necesidad estricta |
| 5. Casos de estudio | `05_casos_de_estudio.tex` | vacío |
| 6. Experimentación | `06_experimentación_pruebas.tex` | vacío |
| 7. Conclusiones | `07_conclusiones_trabajo_futuro.tex` | vacío |

Pendientes marcados dentro del cap. 2/3: varios `\red{CITA AL TUTOR}` (el paper no está
publicado; hay que decidir cómo citarlo) y un `% _________ SEGUIR AQUI ____` en el cap. 2
(línea ~680) que parece un resto de edición ya superado.

**Estilo de redacción exigido**: LaTeX, español, registro académico formal y riguroso,
reutilizando los nombres de variables ya definidos, con `\ref{}` a las secciones previas y
citas adecuadas. El usuario no compila LaTeX localmente: importa el contenido, no que compile.

## Objetivos pendientes (de `informacion.txt`)

### Bloque 1 — Finalización del proyecto
- **1.1 ✅ CERRADO (sesión 3).** Se corrigieron los defectos que hundían a Dec-MCTS
  (`05_dudas.md`, D-BUG-01/02 y divergencias de fidelidad) y se acotó dónde aporta el
  algoritmo. **Decisión del usuario: no se aplican más cambios al solver por ahora**; las
  mejoras de diseño pendientes (B1–B5) quedan aparcadas y documentadas. **Se conservan las
  cuatro versiones** hasta ver cuál rinde mejor sobre el catálogo definitivo (⚠️ ojo a la
  duda D-06: la comparación entre versiones está sesgada mientras v1–v3 no repliquen la
  semántica corregida de caducidad).
- **1.2-ter ✅ CERRADO (sesión 8). CATÁLOGO v3, el OFICIAL.** `scenarios/catalogo_v3/`, misma
  estructura y mismas semillas que el v2, con dos cambios de parametrización pedidos por el
  usuario: **coste de un intento fallido 10 fijo** (antes 0/3/10/10; con éxito el consumo es
  proporcional a la duración, esperanza 10) y **σ de la
  duración 0/1/3/5** (antes 0/3/10/10, que daba CV=1 en `est` y `fue`). Generador
  `scripts/generate_catalog_v3.py`, analizador `scripts/analyze_catalog_v3.py`, comparador
  `scripts/compare_catalogos.py`, tanda `logs/eval_catalogo_v3` (5 400 runs, ~35 min con 6
  procesos), CSV `analisis/catalogo_v3.csv`. La estructura de las conclusiones **aguanta**: solo
  se mueve el régimen `est` (la Δ cambia de signo) y hay que reformular la idea 1.
  📌 Detalle en el **addendum 2** de `06_conclusiones.md`.
- **1.2-bis ✅ CERRADO (sesión 6), superado por el v3. Catálogo v2.** `scenarios/catalogo_v2/`, **360
  escenarios en 5 bloques de 72**, generador `scripts/generate_catalog_v2.py`, analizador
  `scripts/analyze_catalog_v2.py`, tanda `logs/eval_catalogo_v2` (5 400 runs, ~25 min con 6
  procesos), CSV `analisis/catalogo_v2.csv`. Cambios de fondo: **batería 40 en todos los
  escenarios** (la recarga deja de ser decorativa), bloques del mismo tamaño y mitad det/est
  (salvo E, documentado), **3 réplicas**, nueva ventana `P` (solo plazo, sin espera) y el
  bloque A como **control**. Precedido por la sonda `logs/eval_probe` (17 variantes de diseño,
  3 060 runs) y por un pilotaje de calibración de 720 runs que revalidó L=6.
  📌 Detalle en la sección «CATÁLOGO v2» de `03_experimentos.md`.
- **1.2 ✅ CERRADO (sesión 4) — ⚠️ OBSOLETO desde la sesión 6.** Catálogo v1: `scenarios/catalogo/`, 288
  escenarios en 6 bloques, generador `scripts/generate_catalog_scenarios.py`, lanzador
  `scripts/run_catalog.sh`. Principio de diseño: la dificultad se controla con la **carga por
  robot** (L = Σq_j/R) y el horizonte T=40 se mantiene fijo, de modo que el nº de robots deja
  de estar confundido con la dificultad. Se evalúa **solo con `dec-mcts-v4-g9999`** (decisión
  del usuario) sobre `logs/eval_catalogo`. 📌 Detalle en la sección "CATÁLOGO DEFINITIVO" de
  `03_experimentos.md`. El bloque F (validación externa sobre Solomon) se **descartó**
  explícitamente: 3.5 h de cómputo para confirmar un rendimiento bajo ya conocido.
- **1.3 ✅ CERRADO (sesión 4).** `analisis/evaluacion_final.ipynb` (8 secciones, 8 figuras en
  PNG/PDF en `analisis/figuras/`), alimentado por `scripts/extract_metrics.py` (logs → CSV, solo
  biblioteca estándar) y `analisis/resultados.csv` (17 860 ejecuciones de 8 tandas). Entorno en
  `.venv/` con `analisis/requirements.txt`. Estructura: panorama → escalado con el equipo →
  perfil por familia → equipo × incertidumbre → mecanismo → refutaciones → coste → síntesis.

### Bloque 2 — Escritura de la memoria ⏭️ **FASE ACTUAL**
📌 Antes de redactar nada, leer **`06_conclusiones.md`**: las cinco ideas del trabajo con sus
cifras y con los avisos sobre qué **no** puede afirmarse (conjetura sobre la literatura,
comparación centralizado/distribuido no medida → D-09).
- **2.1 ✅ CERRADO (sesión 5).** Cap. 4 redactado, revisado y reescrito por el usuario a partir
  del borrador del asistente. ~3 700 palabras (≈9-10 págs.), 3 tablas y 1 figura
  (`memoria/figures/04_materiales_y_metodos/arquitectura_simulador.pdf`, creada por el usuario).
  ⚠️ El briefing `memoria/notas_cap4_materiales_y_metodos.md` quedó **obsoleto**: el capítulo
  final descarta la mayor parte de su contenido (ver bitácora, sesión 5).
- **2.2 ✅ CERRADO (sesión 8).** Cap. 5 Casos de estudio, sobre el **catálogo v3**. Borrador del
  asistente (sesión 7) reescrito por el usuario y dado por finalizado. Estructura: introducción
  + §5.1 configuración común + §5.2 los cinco bloques (uno por subapartado) . Dos figuras, las
  dos ya creadas: `geometria_escenario.pdf` (tres escenarios en estado inicial, generada con
  `scripts/figura_geometria_cap5.jl`) y `ventanas.pdf` (las cuatro ventanas, TikZ, hecha por el
  usuario). ⚠️ El briefing `memoria/notas_cap5_casos_de_estudio.md` quedó **desbordado y
  obsoleto**. ⚠️ **El usuario eliminó la Recapitulación** y la reserva para abrir el cap. 6:
  con ella se van las cifras agregadas, las 3 réplicas y las 5 400 ejecuciones.
  ⚠️ **D-10 queda sin resolver de hecho**: el protocolo experimental NO entró en el cap. 5.
- **2.3 ⏭️ EN CURSO.** Dos pasos:
  (a) ✅ **CERRADO (sesión 9)**: `analisis/evaluacion_final.ipynb` rehecho sobre el catálogo v3,
  en **tres partes** (conjunto · por bloque · métricas adicionales), 74 celdas y **14 figuras**
  `figuras/p1_*`, `p2_*`, `p3_*`. El cuaderno del v1 queda archivado como
  `analisis/evaluacion_v1_obsoleto.ipynb` (conserva las cuatro refutaciones y la ablación C1,
  D-13). Índice en `analisis/README.md`; hallazgos nuevos en la bitácora, sesión 9; y
  (b) ⏭️ **escribir `memoria/sections/06_experimentación_pruebas.tex`** con el **addendum 2** de
  `06_conclusiones.md`. Abrirlo con la recapitulación del catálogo que el usuario quitó del
  cap. 5. Ver la sección «PARA LA PRÓXIMA SESIÓN» de la bitácora (sesión 8).
- **2.4** Cap. 7 Conclusiones y trabajo futuro → ideas 1, 2, 4 y 5 de `06_conclusiones.md`.
- **2.5** Cap. 1 Introducción (al final).
- **2.6** Resumen/abstract, título, pulido final.

### Bloque 3 — Limpieza del repositorio (se sube a GitHub, lo evalúa el tribunal)
- **3.1** Eliminar logs de prueba, vídeos `.mp4`, temporales, `copy/`, scripts auxiliares,
  esta misma carpeta `.claude-notes/`, y las versiones anteriores del catálogo (v1 y v2, ver
  D-15). 📌 **Incluye el renombrado de los escenarios (D-17)**: la memoria los llama
  `esc_{bloque}_r{robots}_n{tareas}_{ventana}_{régimen}_{coalición}_i{instancia}` y en el
  repositorio son `cv3_..._bnd_...`. Afecta a 360 escenarios, 5 400 logs, el CSV, el generador
  y la expresión regular de `extract_metrics.py`. **Hacerlo después del cap. 6.**
- **3.2** README completo + fichero de requisitos.
- **3.3** Limpieza de código comentado y comentarios explicativos.

## Reparto de contenidos entre capítulos (para no invadir)

- **Cap. 4** (ya escrito): materiales, arquitectura software, comunicación, registro, función
  de recompensa, simulador de eventos y sus hiperparámetros, Dec-MCTS (refinamientos e
  hiperparámetros). ⚠️ **NO contiene el protocolo experimental** (réplicas, ejecutor,
  reproducibilidad): se eliminó por decisión del usuario ⇒ hay que colocarlo en el cap. 5 o 6
  (duda **D-10**).
- **Cap. 5**: catálogo de escenarios concretos + protocolo experimental.
- **Cap. 6**: resultados numéricos. ⚠️ El usuario ha decidido **no incluir ablaciones**
  en la memoria (sesión 5).
