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

🔴 **Estado de la hipótesis (tras la sesión 12): SE CONFIRMA.** Al corregir los dos defectos de
batería del simulador (navegar no mataba; el intento truncado por batería se re-tarifaba con la
tarifa del fracaso) la Δ frente a cbba pasó de **+0.0086 (empate)** a **+0.0431 ± 0.0039**, con
244/33/83, y Dec-MCTS gana **en los cuatro regímenes y en cuatro de los cinco bloques** (en D gana
cbaa). La corrección **no le beneficia a él**: su recompensa no se mueve (−0.001 ± 0.002); son
cbaa y cbba las que pierden 0.035 cada una, porque completaban el 8.7–8.9 % de sus tareas con
robots que ya habían agotado la batería.
⚠️ **Todo lo que sigue en este fichero sobre el «empate» y sobre que «el conjunto de prueba
determina la conclusión» está OBSOLETO**, igual que los dos addendums de `06_conclusiones.md`.
✅ **Enfoque resuelto por el usuario (17-09-2026)**: el cap. 6 conserva la Δ dec−cbba como hilo,
porque «establece la mejora introducida por mi algoritmo». Detalle: sesión 12 de `04_bitacora.md`
y «REEJECUCIÓN 17-09-2026» en `03_experimentos.md`.

**Lo sólido, sobre la tanda reejecutada (sesión 12):**
- **Δ positiva en los cuatro regímenes**, decreciente con la incertidumbre: det +0.054 ± 0.005 ·
  lev +0.028 ± 0.019 · est +0.038 ± 0.006 · **fue +0.008 ± 0.016 (ya no separable de cero)**.
- **Δ positiva en los cinco bloques**: A +0.033 · B +0.055 · **C +0.090** · D +0.012 · E +0.025.
  El *leave-one-block-out* deja el agregado entre +0.031 y +0.051: **ninguna exclusión cambia el
  signo** ⇒ lo que depende de la composición del catálogo es la **magnitud**, no la ordenación.
- **Escalado (bloque A)**: bajo incertidumbre la pendiente de Dec-MCTS frente al nº de robots es
  **+0.0016 (t = 0.7)**, nula, frente a **+0.0120 (t = 4.5)** de cbba; la pendiente de la propia Δ
  es **−0.0104/robot (t = −3.5)** y cruza el cero hacia los 4 robots. En determinista las dos
  pendientes casi coinciden (+0.0121 vs +0.0135) y la Δ no decae (−0.0014, t = −0.7).
- **Ventana escalonada**: +0.084, gana **12 de 12** en determinista. Su mejor terreno. En el bloque B
  solo dec-mcts y cbba decrecen de forma monótona al endurecer la ventana, y cbba pierde más
  (0.199 frente a 0.137).
- **Carga (bloque C)**: su mejor bloque (+0.090). La Δ decrece de forma monótona con el nº de robots
  en las ocho columnas; en el eje de la carga no es monótona.
- **Coaliciones (bloque D)**: único bloque donde **cbaa** es el mejor de los cinco (0.418 vs 0.403 de
  dec y 0.391 de cbba) y único donde dec-mcts no lidera. La uniforme `q2` anula la ventaja
  (−0.007 det) y la mixta `qm` no (+0.028 det).
- **Intento × éxito**: en determinista toda la ventaja es **cobertura** (0.586 vs 0.532, éxito 1.000
  en ambos); bajo incertidumbre la cobertura se iguala y la sostiene el **acierto** (0.759 vs
  0.719). Entre escenarios manda la cobertura: r = 0.75 frente a 0.47.
- **Mortalidad**: `failed_agents` ya es válido — random 2.79 · greedy 2.54 · cbba 2.51 · cbaa 1.83 ·
  **dec-mcts 1.48**, el más bajo, y el único cuyas bajas **decrecen** con la incertidumbre.
- **Coste**: dec-mcts recorre un +53.8 % de distancia por robot que cbba y tarda 13.96 s por
  ejecución (≈1 900× cbba).

⚠️ **Cifras OBSOLETAS que no deben citarse**: todo lo medido antes del 17-09-2026, incluidos los
rankings de los catálogos v1 y v2, la Δ +0.0086 del v3 original, las pendientes −0.0155/+0.0195, la
inversión del signo al quitar el bloque C y la afirmación de que cbaa/cbba/dec-mcts «no perdían
ningún robot». El histórico está en `03_experimentos.md`.

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
| 5. Casos de estudio | `05_casos_de_estudio.tex` | **completo y CERRADO (sesión 8)** |
| 6. Experimentación | `06_experimentación_pruebas.tex` | ✅ **CERRADO por el usuario (sesión 12)**, sobre la tanda reejecutada — 7 figuras, 5 tablas, ninguna marca `\red{}`, sin pendientes |
| 7. Conclusiones | `07_conclusiones_trabajo_futuro.tex` | ✅ **CERRADO por el usuario (sesión 14, 29-09-2026)** — 1 782 palabras, tres apartados (Logros · Limitaciones · Trabajo futuro), dos `\red{CITA AL TUTOR}` |

🔴 **SIGUIENTE (sesión 15): el capítulo 1 (Introducción) y el Resumen/Abstract**, acordado con el usuario.
El cap. 1 se escribe ya con la hipótesis confirmada, así que puede anunciar el resultado sin condicionales.

⚠️ **Material del guion que el usuario descartó al cerrar el cap. 7 y que hoy NO está en ninguna parte de la
memoria**: los **planes comunicados de longitud 1.58** (L1), la **semilla no configurable** (D-05) y las
**dificultades del desarrollo** (los dos defectos de batería). No reponerlos en el cap. 1 sin preguntar.

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
📌 Antes de redactar nada, leer **`06_conclusiones.md`**, **rehecho el 24-09-2026**: la tesis
(«la hipótesis se cumple, Dec-MCTS gana»), **seis ideas con su peso**, la estructura del cap. 7, el
material de limitaciones y la lista de lo que se cayó. Remite al cap. 6 para las cifras.
⚠️ **La conjetura sobre la escasez de literatura está ELIMINADA** (decisión del usuario,
24-09-2026). La comparación centralizado/distribuido sigue sin medirse: cualitativa y sin cifras
de pérdida (→ D-09, D-03).
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
- **2.3 ✅ CERRADO (sesión 11).** Dos pasos, y el capítulo ya está escrito y cerrado por el
  usuario. 🔴 **Pero la próxima sesión empieza corrigiendo el defecto de navegación del simulador
  y reejecutando**: el cap. 6 tiene 3 marcas `\red{PENDIENTE}` que dependen de él. Plan completo
  en el «PLAN DE CORRECCIÓN Y REEJECUCIÓN» de D-16, en `05_dudas.md`.
  (a) ✅ **CERRADO (sesión 9)**: `analisis/evaluacion_final.ipynb` rehecho sobre el catálogo v3,
  en **tres partes** (conjunto · por bloque · métricas adicionales), 74 celdas y **14 figuras**
  `figuras/p1_*`, `p2_*`, `p3_*`. El cuaderno del v1 queda archivado como
  `analisis/evaluacion_v1_obsoleto.ipynb` (conserva las cuatro refutaciones y la ablación C1,
  D-13). Índice en `analisis/README.md`; hallazgos nuevos en la bitácora, sesión 9; y
  (b) ✅ **CERRADO (sesión 11)**: `memoria/sections/06_experimentación_pruebas.tex`, borrador del
  asistente (sesión 10) **reescrito y terminado por el usuario** (4 187 palabras, 6 tablas y
  6 figuras en `memoria/figures/06_experimentación_pruebas/`, exportadas por el propio cuaderno).
  Resuelve **D-10** (el protocolo experimental entra en §6.1). ⚠️ Queda por borrar el bloque
  comentado del final del cap. 5, cuyo contenido está ya en la introducción del 6. Se escribió con
  el **addendum 2** de
  `06_conclusiones.md`. Abrirlo con la recapitulación del catálogo que el usuario quitó del
  cap. 5. Ver la sección «PARA LA PRÓXIMA SESIÓN» de la bitácora (sesión 8).
- **2.4** ⏭️ **SIGUIENTE TAREA.** Cap. 7 Conclusiones y trabajo futuro
  (`07_conclusiones_trabajo_futuro.tex`, etiqueta `cap:conclusiones_trabajo_futuros`), con
  `06_conclusiones.md` como guion. **Estructura fijada por el usuario (24-09-2026)**: varias
  partes, dos de ellas obligatorias — **«Logros y aportaciones principales»** (ideas 1 y 2) y
  **«Limitaciones y dificultades encontradas»** (L1–L5, encabezadas por los planes comunicados de
  longitud media 1.58) —, más trabajo futuro (idea 6). **Proponerle el índice y la extensión por
  apartado antes de escribir.** D-03 no bloquea: se deja la marca `\red{CITA AL TUTOR}`.
- **2.5** Cap. 1 Introducción (al final).
- **2.6** Resumen/abstract, título, pulido final.

### Bloque 3 — Limpieza del repositorio (se sube a GitHub, lo evalúa el tribunal)
- **3.1** Eliminar logs de prueba, vídeos `.mp4`, temporales, `copy/`, scripts auxiliares,
  esta misma carpeta `.claude-notes/`, y las versiones anteriores del catálogo (v1 y v2, ver
  D-15). 📌 **Incluye D-18** (sesión 13): `data/` y `logs/` **sí** se suben, depurados; `CLAUDE.md`
  y `.claude-notes/` **no**; y hay que sacar del índice de git los 8 085 logs y los 40 ficheros de
  `build/` que quedaron versionados antes del `.gitignore`. 📌 **Incluye el renombrado de los escenarios (D-17)**: la memoria los llama
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
