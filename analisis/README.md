# Análisis de resultados

Evaluación comparativa de los cinco solvers sobre el **catálogo** del trabajo
(`scenarios/catalogo/`, 360 escenarios, tanda `logs/eval_catalogo`, 5 400 ejecuciones).

> El cuaderno vigente es **`evaluacion_final.ipynb`**, estructurado en **tres partes**
> (conjunto · por bloque · magnitudes adicionales) con **14 figuras** en `figuras/p1_*`,
> `p2_*` y `p3_*`. El análisis equivalente en línea de comandos, sin dependencias externas,
> es `scripts/analyze_catalog.py`.

## Ejecución

```bash
# Datos (logs → CSV)
python3 scripts/extract_metrics.py logs/eval_catalogo -o analisis/catalogo.csv

# Cuaderno — necesita el venv
python3 -m venv .venv && .venv/bin/pip install -r analisis/requirements.txt
.venv/bin/jupyter lab analisis/evaluacion_final.ipynb
# o, sin abrirlo:
.venv/bin/python -m jupyter nbconvert --to notebook --execute --inplace \
    analisis/evaluacion_final.ipynb

# Mismas tablas en consola, solo biblioteca estándar
python3 scripts/analyze_catalog.py analisis/catalogo.csv
```

## Contenido

| Fichero | Qué es |
|---|---|
| `evaluacion_final.ipynb` | Análisis del catálogo en tres partes |
| `catalogo.csv` | Una fila por ejecución (5 400): identificación, factores, métricas y derivadas |
| `figuras/p1_*`, `p2_*`, `p3_*` | Las 14 figuras del cuaderno, en PNG y PDF |
| `figuras/*_memoria.png` | Copia sin título de las figuras que se incrustan en la memoria |
| `requirements.txt` | Versiones con las que se ejecutó el cuaderno |

## Estructura de `evaluacion_final.ipynb`

**Unidad de análisis: el escenario** (media de sus 3 réplicas). Comparaciones **pareadas por
escenario** y error estándar calculado **entre escenarios**, nunca entre réplicas. El cuaderno
está redactado en **registro descriptivo**: cada apartado enuncia qué se mide y cómo se obtiene,
las explicaciones mecanísticas van marcadas como *interpretación* y las valoraciones quedan para
el capítulo 7 de la memoria.

| Parte | Secciones | Figuras |
|---|---|---|
| **1 · Análisis conjunto** | 1.1 recompensa media, agregado y desglose por bloque · 1.2 diferencia pareada entre los dos solvers de cabeza · 1.3 dominancia pareada y arrepentimiento · 1.4 sensibilidad del agregado a la composición del catálogo | `p1_panorama`, `p1_agregado`, `p1_dominancia`, `p1_composicion` |
| **2 · Análisis por bloque** | 2.1 A tamaño del equipo a iso-dificultad · 2.2 B forma de la ventana · 2.3 C carga por robot · 2.4 D coaliciones · 2.5 E gradiente de incertidumbre | `p2_A_escalado`, `p2_B_ventana`, `p2_C_carga`, `p2_D_coaliciones`, `p2_E_incertidumbre` |
| **3 · Análisis adicional** | 3.1 robots perdidos por batería · 3.2 descomposición en tasa de intento y de éxito · 3.3 distancia recorrida · 3.4 tiempo de cómputo · 3.5 ocupación del horizonte | `p3_mortalidad`, `p3_mecanismo`, `p3_distancia`, `p3_coste`, `p3_tiempos` |

**Figuras para la memoria**: `guardar(fig, nombre, memoria='<fichero>')` guarda, además de la
figura normal, una variante **sin título** en `memoria/figures/06_experimentación_pruebas/`, que es
la que se incrusta en el capítulo 6 (allí el pie de figura cumple esa función). La copia
`figuras/<nombre>_memoria.png` permite revisarla sin abrir el PDF. Al reejecutar el cuaderno, las
figuras de la memoria se actualizan solas.

**Convenio de color, constante en todas las figuras**: `Random` gris · `Greedy` morado ·
`CBAA` verde · **`CBBA` rojo** · **`Dec-MCTS` azul**. En las figuras de diferencias, rojo = Δ<0
(ventaja de CBBA) y azul = Δ>0 (ventaja de Dec-MCTS). Los cortes que **no** son un solver
—régimen, tamaño de equipo, términos de la descomposición— usan negro, naranja o una rampa de
grises, para no confundirse con esa codificación.

## Avisos que deben acompañar a los resultados

- Los regímenes `det` y `est` están equilibrados entre bloques (162 escenarios cada uno), pero
  `lev` y `fue` proceden **solo del bloque E** (18 cada uno). Las figuras con eje de cuatro
  regímenes se restringen a ese bloque.
- La **magnitud** de la ventaja de Dec-MCTS depende de la composición de bloques; el cuaderno la
  mide con un *leave-one-block-out* en §1.4.
- **Los cinco solvers pierden robots** por agotamiento de batería. Medido sobre esta tanda, en
  robots perdidos por escenario: `Random` 2.79 · `Greedy` 2.54 · `CBBA` 2.51 · `CBAA` 1.83 ·
  `Dec-MCTS` 1.48. Parte de las diferencias de recompensa es mortalidad, no calidad de asignación.
- Los `computing_time` proceden de una tanda con **6 procesos en paralelo**: son comparables entre
  solvers, no en valor absoluto.
- **No hay semilla configurable** (`std::random_device`): de ahí las 3 réplicas por
  (escenario, solver) y el promediado. Reejecutar la tanda no reproduce los valores exactos.
- Las cifras de referencia del trabajo son las del **capítulo 6 de la memoria**, generadas desde
  este cuaderno.
