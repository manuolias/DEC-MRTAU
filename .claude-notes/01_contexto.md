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

**Estado de la hipótesis (tras la sesión 3)**: se cumple **condicionalmente**, y ya sabemos
bajo qué condiciones. Dec-MCTS gana cuando hay **presión de coordinación** (≥4 robots) y los
planes son **predecibles** (régimen determinista o poco estocástico), sobre todo con ventanas
temporales estrechas y escalonadas (familia 1E: 0.618 frente a 0.609 de CBBA). Pierde con
equipos pequeños (2-3 robots, donde su comunicación no aporta nada) y en régimen estocástico,
donde CBBA mantiene ventaja (0.427 vs 0.396). La ablación C1 demuestra que **la ventaja
procede efectivamente del canal de comunicación**, que es la aportación conceptual del método.
⇒ El reto ya no es arreglar el solver, sino **construir un catálogo de escenarios que
represente honestamente el problema y permita que esa ventaja se manifieste** (objetivo 1.2).

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
| 4. Materiales y métodos | `04_materiales_y_metodos.tex` | vacío — hay briefing en `memoria/notas_cap4_materiales_y_metodos.md` |
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
- **1.2 ⏭️ SIGUIENTE OBJETIVO.** Definir un conjunto de escenarios **variado y discriminante**
  (nº robots, nº tareas, dificultad, distribución). El usuario no está satisfecho con los
  actuales: en la mayoría no hay diferencias grandes entre solvers. Lo ideal es que Dec-MCTS
  destaque. 📌 **Punto de partida: la sección "Criterios para el diseño del catálogo
  definitivo" de `03_experimentos.md` (C-1 a C-7)**, que explica con datos por qué los
  catálogos actuales no discriminan.
- **1.3** Análisis de resultados en un **notebook de Jupyter**: rendimientos medios y óptimos
  por criterio (robots, tareas, dificultad, distribución).

### Bloque 2 — Escritura de la memoria
- **2.1** Cap. 4 Materiales y métodos (hay briefing detallado ya escrito).
- **2.2** Cap. 5 Casos de estudio (catálogo de escenarios).
- **2.3** Cap. 6 Experimentación y pruebas (resultados).
- **2.4** Cap. 7 Conclusiones y trabajo futuro.
- **2.5** Cap. 1 Introducción (al final).
- **2.6** Resumen/abstract, título, pulido final.

### Bloque 3 — Limpieza del repositorio (se sube a GitHub, lo evalúa el tribunal)
- **3.1** Eliminar logs de prueba, vídeos `.mp4`, temporales, `copy/`, scripts auxiliares,
  esta misma carpeta `.claude-notes/`.
- **3.2** README completo + fichero de requisitos.
- **3.3** Limpieza de código comentado y comentarios explicativos.

## Reparto de contenidos entre capítulos (para no invadir)

- **Cap. 4**: materiales, herramientas, arquitectura software, clases, simulador,
  hiperparámetros, marco experimental (el *framework*).
- **Cap. 5**: catálogo de escenarios concretos.
- **Cap. 6**: resultados numéricos y ablaciones.
