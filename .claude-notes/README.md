# Cuaderno de trabajo de Claude — TFM DEC-MRTAU

Carpeta de uso exclusivo del asistente. **No forma parte de la entrega final**: debe
eliminarse (o quedar en `.gitignore`) en la fase de limpieza del repositorio (objetivo 3.1).

## Protocolo de sesión

**Al empezar una sesión** — leer en este orden:

1. `CLAUDE.md` (raíz) — se carga automáticamente; resumen operativo.
2. `01_contexto.md` — qué es el proyecto, qué se persigue, estado de la memoria.
3. `04_bitacora.md` — **las 2-3 últimas entradas**: qué se hizo y qué quedó a medias.
4. `05_dudas.md` — preguntas abiertas y decisiones pendientes del usuario.
5. **`06_conclusiones.md` — la tesis del trabajo.** Obligatorio antes de redactar cualquier
   capítulo de la memoria.
6. Según la tarea: `02_codigo.md` (tocar código) o `03_experimentos.md` (análisis/escenarios).

**Al terminar una sesión** — actualizar siempre:

- `04_bitacora.md`: nueva entrada con fecha, qué se cambió (ficheros), qué se ejecutó,
  qué resultados salieron y qué queda pendiente.
- `05_dudas.md`: añadir dudas nuevas, marcar como resueltas las contestadas.
- `03_experimentos.md`: si se han lanzado ejecuciones, registrar directorio de logs,
  configuración y números obtenidos.
- `02_codigo.md`: si se ha modificado la arquitectura o añadido hiperparámetros.
- `01_contexto.md`: solo si cambia el rumbo del proyecto o el estado de la memoria.

## Índice

| Fichero | Contenido |
|---|---|
| `01_contexto.md` | Problema, modelo formal, objetivos pendientes, estado de la memoria |
| `02_codigo.md` | Arquitectura del código, clases, hiperparámetros, trampas conocidas |
| `03_experimentos.md` | Escenarios, validez de cada tanda de logs, resultados y diagnóstico |
| `04_bitacora.md` | Registro cronológico de cambios y avances |
| `05_dudas.md` | Dudas abiertas, hipótesis sobre el bajo rendimiento de Dec-MCTS, decisiones del usuario |
| `06_conclusiones.md` | **Las cinco ideas que sostienen la memoria**, con las cifras que las respaldan |

## Reglas de trabajo (del usuario, `informacion.txt`)

1. Identificar el propósito de un fichero/clase **antes** de tocarlo. Ningún cambio puede
   perder información relevante.
2. **Preguntar, no dar nada por sentado** (intención, arquitectura, requisitos).
3. Señalar explícitamente las dudas antes de seguir adelante.
4. Se aceptan sugerencias de mejora, sobre todo las de impacto duradero.
5. Ceñirse a los objetivos y peticiones: **no hacer trabajo de más sin preguntar**.
