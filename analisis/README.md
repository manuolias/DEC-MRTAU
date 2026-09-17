# Análisis de resultados

Evaluación comparativa de los cinco solvers sobre el **catálogo v3**
(`scenarios/catalogo_v3/`, 360 escenarios, tanda `logs/eval_catalogo_v3`, 5 400 ejecuciones).

> ✅ **Estado (sesión 9)**: el cuaderno vigente es **`evaluacion_final.ipynb`**, rehecho sobre el
> catálogo v3 y estructurado en **tres partes** (conjunto · por bloque · métricas adicionales),
> con **14 figuras nuevas** (`figuras/p1_*`, `p2_*`, `p3_*`). El análisis en línea de comandos
> equivalente, sin dependencias, es `scripts/analyze_catalog_v3.py`.
> El cuaderno anterior se conserva como **`evaluacion_v1_obsoleto.ipynb`** (catálogo v1): es la
> única versión ejecutable de las cuatro refutaciones y de la ablación del canal.

## Ejecución

```bash
# Datos (logs → CSV)
python3 scripts/extract_metrics.py logs/eval_catalogo_v3 -o analisis/catalogo_v3.csv

# Cuaderno vigente — necesita el venv
python3 -m venv .venv && .venv/bin/pip install -r analisis/requirements.txt
.venv/bin/jupyter lab analisis/evaluacion_final.ipynb
# o, sin abrirlo:  .venv/bin/python -m jupyter nbconvert --to notebook --execute --inplace \
#                      analisis/evaluacion_final.ipynb

# Mismas tablas en consola, solo biblioteca estándar
python3 scripts/analyze_catalog_v3.py analisis/catalogo_v3.csv
python3 scripts/compare_catalogos.py analisis/catalogo_v2.csv analisis/catalogo_v3.csv

# Sonda de variantes de diseño
python3 scripts/extract_metrics.py logs/eval_probe -o analisis/probe.csv
python3 scripts/analyze_probe.py analisis/probe.csv
```

## Contenido

| Fichero | Qué es | Estado |
|---|---|---|
| `evaluacion_final.ipynb` | **Análisis del catálogo v3 en tres partes** | ⭐ **vigente** |
| `catalogo_v3.csv` | Una fila por ejecución (5 400) del catálogo v3 | ⭐ **vigente** |
| `figuras/p1_*`, `p2_*`, `p3_*` | Las 14 figuras del cuaderno vigente, en PNG y PDF | ⭐ **vigente** |
| `evaluacion_v1_obsoleto.ipynb` | Cuaderno del catálogo v1 (8 secciones) | histórico — refutaciones y ablación C1 |
| `figuras/01_*` … `08_*` | Las ocho figuras del v1 | obsoletas |
| `catalogo_v2.csv` | Una fila por ejecución (5 400) del catálogo v2 | obsoleto (contraste) |
| `probe.csv` | Sonda de 17 variantes de diseño (3 060 ejecuciones) | vigente |
| `resultados.csv` | Catálogo v1 y tandas auxiliares (17 860 ejecuciones) | obsoleto / contraste |
| `requirements.txt` | Versiones con las que se ejecutó el cuaderno | — |

## Estructura de `evaluacion_final.ipynb`

**Unidad de análisis: el escenario** (media de sus 3 réplicas). Comparaciones **pareadas por
escenario** y error estándar calculado **entre escenarios**. El cuaderno está redactado en
**registro descriptivo**: los apartados enuncian qué se mide y cómo se obtiene, las explicaciones
mecanísticas van marcadas como *interpretación* y las valoraciones quedan para el capítulo 7.

**Figuras para la memoria**: `guardar(fig, nombre, memoria='<fichero>')` guarda, además de la
figura normal, una variante **sin título** en `memoria/figures/06_experimentación_pruebas/`, que es
la que se incrusta en el capítulo 6 (allí el pie de figura cumple esa función). La copia
`figuras/<nombre>_memoria.png` permite revisarla sin abrir el PDF.

**Convenio de color, constante en todas las figuras**: `Random` gris · `Greedy` morado ·
`CBAA` verde · **`CBBA` rojo** · **`Dec-MCTS` azul**. En las figuras de diferencias, rojo = Δ<0
(ventaja de CBBA) y azul = Δ>0 (ventaja de Dec-MCTS). Los cortes que **no** son un solver
—régimen, tamaño de equipo, términos de la descomposición— usan negro, naranja o una rampa de
grises, para no confundirse con esa codificación.

| Parte | Secciones | Figuras |
|---|---|---|
| **1 · Análisis conjunto** | 1.1 panorama global y por bloque · 1.2 el agregado es un empate (Δ pareada, distribución, por régimen) · 1.3 dominancia pareada y arrepentimiento · 1.4 **cuánto del resultado es del catálogo** (Δ por bloque + *leave-one-block-out*) | `p1_panorama`, `p1_agregado`, `p1_dominancia`, `p1_composicion` |
| **2 · Análisis por bloque** | 2.1 A equipo (pendientes frente al tamaño del equipo $n$) · 2.2 B ventana (pareado contra el control) · 2.3 C carga (rejilla robots × carga) · 2.4 D coaliciones · 2.5 E gradiente de incertidumbre | `p2_A_escalado`, `p2_B_ventana`, `p2_C_carga`, `p2_D_coaliciones`, `p2_E_incertidumbre` |
| **3 · Análisis adicional** | 3.1 mortalidad de robots · 3.2 descomposición intento × éxito · 3.3 distancia recorrida · 3.4 coste computacional · 3.5 ocupación del horizonte | `p3_mortalidad`, `p3_mecanismo`, `p3_distancia`, `p3_coste`, `p3_tiempos` |

Cierra con una **síntesis** de siete puntos y la lista de lo que hay que declarar al presentar
los resultados (composición del catálogo, mortalidad de los baselines, tiempos medidos en
paralelo, anchura de la ventana escalonada, ausencia de semilla).

## Avisos que el cuaderno hace explícitos

- ⚠️ **Nunca liderar con el agregado**: es un empate (Δ +0.0086 ± 0.0040, recuento 158/42/160) y
  **quitar el bloque C le cambia el signo**. Se reporta por bloque y por régimen.
- ⚠️ Los regímenes `det` y `est` están equilibrados entre bloques (162 escenarios cada uno);
  `lev` y `fue` existen **solo en el bloque E** (18 cada uno). Las figuras con eje de cuatro
  regímenes se restringen a ese bloque.
- ⚠️ `random` y `greedy` **pierden robots** (1.98 y 1.44 por escenario, también en determinista);
  los tres solvers coordinados, ninguno en 5 400 ejecuciones. Parte de su desventaja es
  mortalidad, no calidad de asignación.
- ⚠️ Los `computing_time` proceden de una tanda con **6 procesos en paralelo**: comparables entre
  solvers, no en valor absoluto.
