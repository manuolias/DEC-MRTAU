# 04 — Bitácora de sesiones

Entrada más reciente arriba. Formato: fecha · objetivo · qué se hizo · qué queda.

---

## 2026-09-14 — Sesión 10: redacción del capítulo 6 (Experimentación y pruebas)

**Petición del usuario**: escribir `memoria/sections/06_experimentación_pruebas.tex` con **la misma
estructura del cuaderno** (conjunto · por bloque · adicional), enfocado en las evaluaciones y su
comentario; **no incluir todas las figuras del cuaderno**, saber distinguir lo esencial para que no
quede excesivamente largo; tomar los capítulos anteriores como referencia de estilo, notación y
extensión; y **abrir el capítulo con la sección que él dejó comentada al final del cap. 5**.

**Qué se hizo**
- Capítulo 6 completo: **~4 400 palabras, 7 tablas y 4 figuras**, en registro objetivo (la norma
  fijada en la sesión 9). Estructura: introducción con la recapitulación del catálogo ·
  §6.1 protocolo experimental · §6.2 resultados agregados · §6.3 los cinco bloques ·
  §6.4 magnitudes secundarias · §6.5 síntesis.
- **§6.1 resuelve por fin D-10**: el protocolo experimental (unidad de análisis, pareado, EE entre
  escenarios, justificación de la Δ dec−cbba, ejecutor idempotente, 6 procesos en paralelo, 35 min,
  `computing_time` no comparable en absoluto, irreproducibilidad por `std::random_device`) no
  estaba en ninguna parte de la memoria y ahora está aquí.
- **Figuras seleccionadas (4 de las 14)**, copiadas a
  `memoria/figures/06_experimentación_pruebas/`: `panorama.pdf` (p1_panorama),
  `diferencia_pareada.pdf` (p1_agregado), `escalado_equipo.pdf` (p2_A_escalado) e
  `intento_exito.pdf` (p3_mecanismo). El resto del material va en tablas.
- **7 tablas**: niveles por régimen y bloque · Δ por régimen y por bloque **con la columna de
  sensibilidad** (leave-one-block-out) · ventanas · rejilla robots × carga · coaliciones ·
  gradiente de incertidumbre × tamaño de equipo · magnitudes secundarias.
- ⚠️ **Corrección de notación en el cuaderno**: la memoria usa **$n$ = nº de robots** y $m$ = nº de
  tareas; el cuaderno usaba $R$. Se renombró en los rótulos de figura (`p2_A` panel c, `p2_C`
  eje x, `p3_coste` panel b) y en el markdown, y se regeneraron las 14 figuras.
- No se tocó `\multirow` (no está en el preámbulo de `TFE.tex`) ni ningún capítulo anterior.
  Verificado: entornos balanceados, columnas de cada `tabular` correctas y **todas las
  `\ref`/`\eqref` externas resuelven** (`sec:algoritmos`, `tab:materiales`, `tab:decmcts-hiper`,
  `tab:regimenes`, `eq:alcanzable`, `eq:recompensa`, `subsec:casos-bloqueB`, …).
  ⚠️ El capítulo 7 se etiqueta `cap:conclusiones_trabajo_futuros`, no `cap:conclusiones`.

**Pendiente de decisión del usuario**
1. **Borrar el bloque comentado al final del cap. 5** (`% \section{Recapitulación}`): su contenido
   está ya en la introducción del cap. 6, incluida la ecuación `eq:ejecuciones`. Si se descomenta
   habría **etiqueta duplicada**. No se ha tocado porque el usuario prohibió modificar capítulos
   anteriores sin permiso.
2. **Extensión**: 4 393 palabras frente a las 3 024 del cap. 4 y las 2 520 del cap. 5. Se ofreció
   recortar más (candidatos: la síntesis y el párrafo de dominancia de §6.2.2).
3. **Rótulos de las figuras**: las cuatro llevan título propio además del pie de figura. Se ofreció
   regenerar versiones sin `suptitle` para la memoria si prefiere que el pie haga todo el trabajo.

**Qué queda**: capítulo 7 (conclusiones y trabajo futuro) con las ideas 1, 2, 4 y 5 de
`06_conclusiones.md`; capítulo 1; y la limpieza del repositorio (D-17, renombrado de escenarios).

---

## 2026-09-11 — Sesión 9: notebook de evaluación rehecho sobre el catálogo v3

**Petición del usuario**: rehacer `analisis/evaluacion_final.ipynb`, desactualizado (catálogo v1
y métricas viejas), con un índice de **tres partes** dictado por él:
(1) análisis conjunto de todos los ficheros, (2) análisis por bloque —uno por cada uno de los 5—,
(3) análisis adicional con métricas distintas de la objetivo (coste, mortalidad, distancia…).

**Decisiones del usuario al empezar** (las tres opciones recomendadas):
- El cuaderno viejo se **archiva** como `analisis/evaluacion_v1_obsoleto.ipynb` con un banner de
  obsoleto, en vez de sobrescribirse: es la única versión ejecutable de las **cuatro
  refutaciones** (`eval_b4`, `eval_d8`, `eval_hc`, `eval_rounds`) y de la **ablación del canal**
  (`eval_c1_*`), que no se han remedido sobre el v3 (**D-13** sigue abierta).
- Esas refutaciones **NO entran** en el cuaderno nuevo: el índice pedido es sobre el conjunto
  final, y mezclar cifras del v1 rompería la coherencia.
- Las figuras nuevas usan **nombres nuevos** (`p1_*`, `p2_*`, `p3_*`); las ocho del v1
  (`01_*`…`08_*`) sobreviven hasta la limpieza del repositorio.

**Qué se hizo**
- `analisis/evaluacion_final.ipynb` **nuevo, 74 celdas, ejecutado sin errores** (nbconvert con el
  venv; pandas 3.0.5 / matplotlib 3.11.1, sin scipy — los estadísticos t se calculan a mano).
  Mantiene la paleta y el estilo del cuaderno anterior.
- **14 figuras** en `analisis/figuras/`, PNG + PDF:
  `p1_panorama`, `p1_agregado`, `p1_dominancia`, `p1_composicion`;
  `p2_A_escalado`, `p2_B_ventana`, `p2_C_carga`, `p2_D_coaliciones`, `p2_E_incertidumbre`;
  `p3_mortalidad`, `p3_mecanismo`, `p3_distancia`, `p3_coste`, `p3_tiempos`.
- `analisis/README.md` reescrito (índice del cuaderno, estado de cada fichero, avisos).
- `CLAUDE.md`: el aviso de «notebook desactualizado» pasa a descripción del cuaderno vigente;
  mapa del repositorio y bloque «SIGUIENTE SESIÓN» actualizados.

**Análisis nuevos que no existían en el cuaderno del v1** (y que son material directo del cap. 6):
- 🔴 ***Leave-one-block-out* del agregado** (§1.4). Cuantifica la lección metodológica mejor que
  nada anterior: **quitar el bloque C invierte el signo** del agregado (+0.0086 → **−0.0043**) y
  con él el orden de los dos primeros solvers; quitar A o E lo casi duplica (+0.0141 / +0.0156);
  sin B queda en +0.0055 y sin D en +0.0123. Los cinco bloques aportan 72 escenarios cada uno, así
  que **no es un desequilibrio de tamaño: es qué factores decidió barrer el catálogo**.
- **Matriz de dominancia pareada 5×5** (§1.3): los tres primeros escalones son estrictos
  (dec gana a random 358/360, a greedy 333, cbba gana a cbaa 209); el último es simétrico
  (158 vs 160).
- **Arrepentimiento medio** (distancia al mejor solver de cada escenario): **dec-mcts 0.034 es el
  MENOR de los cinco**, por debajo de cbba (0.043). Gana menos escenarios y pierde menos
  recompensa: es el solver más **regular**, no el más fuerte. Ganador absoluto por escenario:
  cbba 131, **dec 108, cbaa 107**, greedy 14, random 0 — y **cbaa gana 45 de los 72 del bloque D**.
- **Mortalidad como fracción de flota** (§3.1): en el bloque A, random pierde el **27.7 %** de sus
  robots con independencia del tamaño del equipo (r de la fracción con R = 0.01, frente a 0.72 del
  recuento absoluto) y greedy el **13.3 %** con leve tendencia creciente (r = 0.23). **Es una tasa
  de la política, no un efecto de escala.**
- **Distancia por tarea completada** (§3.3): dec 8.86 · cbba 7.44 · **cbaa 7.13 (el más
  eficiente)** · greedy 7.46 · random 16.44. Sobrecoste de dec frente a cbba: +25.3 % por robot,
  +19.1 % por tarea completada.
- **Coste frente al tamaño del equipo** (§3.4, bloque A): dec pasa de **0.55 s con 2 robots a
  44.4 s con 10** mientras su Δ recorre el camino contrario ⇒ el coste se dispara exactamente
  donde la ventaja se vuelve negativa. Global: 15.32 s de media (mediana 6.79, máx 88.3) frente a
  los 8.2 ms de cbba, **≈1 864×**.
- **Instante de retirada** (§3.5): **cbaa abandona pronto** bajo incertidumbre (bloque E:
  46.9 → 31.9) sin perder un solo robot ⇒ sus robots se quedan **sin tareas que pujar**, lo que
  explica su baja tasa de intento y su pendiente plana frente a R. Dec-MCTS es el que más estira
  el horizonte (55.3 → 49.7). El makespan apenas discrimina (53–61).

**Correcciones de rigor aplicadas durante la revisión** (lo que se escribió primero y no era
exacto):
- La rejilla robots × carga **no es monótona sin excepciones**: hay una celda que rompe la
  tendencia (2 robots, L=8, determinista: +0.078 frente a +0.021 de L=6). Declarado.
- La ventana ancha: dec **no gana ninguno** de los 12 escenarios (6 empates, 6 derrotas); decir
  «pierde 0 de 12» era confuso.
- La ventaja de los coordinados sobre los baselines **no es «0.17–0.23»**: va de 0.08 (cbaa vs
  greedy) a 0.23 (dec vs random).
- Los `computing_time` por robots del bloque A (0.55 → 44.4) no coinciden con los del catálogo
  completo (0.65 → 38.9): son cortes distintos.

**Aviso metodológico que el cuaderno declara y conviene recordar en el cap. 6**: `det` y `est`
están **equilibrados entre bloques** (162 escenarios cada uno, 36 por bloque salvo E que aporta
18), pero `lev` y `fue` **solo existen en el bloque E** (18 cada uno). Por eso toda figura con eje
de cuatro regímenes se restringe al bloque E, y el panorama global se lee en det/est.

**🔴 Revisión del usuario, mismo día — REGISTRO OBJETIVO**. El usuario señaló que el borrador
daba por supuesto en todo momento que «queremos que gane Dec-MCTS», y que eso puede sugerir que
las métricas y las gráficas se eligieron para favorecerlo, **cosa que es falsa**. Pidió:
(a) reescribir **todas las celdas markdown, títulos de figura y leyendas** en registro objetivo y
formal, describiendo **cómo se obtiene** cada gráfica y no qué se espera ver, y dejar los
comentarios y los deseos personales para el capítulo de conclusiones; (b) fijar **un color por
solver** constante en todo el cuaderno, con **CBBA rojo y Dec-MCTS azul**.

Qué se cambió (el código de cálculo y la geometría de las figuras **no** se tocaron):
- **Registro**: portada reescrita como evaluación comparativa de cinco políticas, con un apartado
  **«Criterios de comparación»** que fija de antemano métrica, unidad de análisis, pareado, error
  estándar, **por qué se detalla precisamente la pareja Dec-MCTS / CBBA** (son las dos de mayor
  media y la única ordenación no resuelta) y el convenio de color. Los títulos de figura pasan de
  enunciar conclusiones («El mejor terreno de Dec-MCTS es…») a describir contenido («Bloque B:
  recompensa media y diferencia pareada según la forma de la ventana»). Las lecturas se titulan
  **«Resultados»** y las explicaciones mecanísticas se marcan explícitamente como
  ***interpretación (hipótesis, no contrastada en este cuaderno)***. La síntesis final pasa a
  «Resumen de resultados» + «Condiciones que deben acompañar a la presentación».
- **Color por solver**: Random gris · Greedy morado · CBAA verde · **CBBA rojo (`#e34948`)** ·
  **Dec-MCTS azul (`#2a78d6`)**. Encaja con la escala divergente que ya se usaba para las Δ
  (rojo = Δ<0 = ventaja de CBBA, azul = Δ>0 = ventaja de Dec-MCTS), y así se declara.
- **Colores auxiliares**, para que ningún corte que no sea un solver reutilice sus colores:
  régimen → negro / naranja (`C_REG`); tamaño de equipo → rampa de grises (`C_TAM`); términos de
  la descomposición intento/éxito → negro / naranja. La **matriz de dominancia** pasa de la escala
  divergente a una **secuencial de grises** (`SEQ`), porque cuenta victorias, no diferencias.
- Añadida una tabla que define las columnas (`Δ`, `EE`, `t`, `n`, `G/E/P`, `máx.`) usadas en todo
  el cuaderno; la columna «mejor» se renombra a «máx.» (solver con la media más alta).
- `analisis/README.md` recoge el convenio de color y el registro.
80 celdas, 0 errores, las mismas 14 figuras regeneradas.

**Qué queda**: escribir el capítulo 6 (ver «PARA LA PRÓXIMA SESIÓN», sigue vigente salvo el punto
1, ya hecho). Las figuras del cuaderno son material directo para él. El usuario anticipa que
pedirá **retoques pequeños de presentación en las gráficas** más adelante.
📌 **Al redactar el cap. 6, mantener este mismo registro**: describir lo medido, no lo esperado.
Las valoraciones y la defensa de la hipótesis van en el cap. 7.

---

## 2026-09-10 — Sesión 8: catálogo v3 (reparametrización de los regímenes)

**Petición del usuario**: dos cambios en la definición de los escenarios, y reejecutar todo.
1. El **coste energético de un intento fallido deja de depender del régimen**: 10 fijo (antes
   0 / 3 / 10 / 10). Razón del usuario: no es una fuente de incertidumbre, así que no tiene
   por qué variar con ella.
2. **σ de la duración baja a 0 / 1 / 3 / 5** (antes 0 / 3 / 10 / 10). Con μ=10 el v2 llegaba a
   CV = 1.0 en `est` y `fue`; ahora el gradiente es CV = 0 / 0.1 / 0.3 / 0.5.

**Qué se hizo**
- `scripts/generate_catalog_v3.py` (copia del v2 con solo esos dos cambios; misma estructura,
  mismas semillas ⇒ pareado exacto celda a celda), `scenarios/catalogo_v3/` (360 esc.,
  prefijo `cv3_`), `scripts/analyze_catalog_v3.py`, `scripts/compare_catalogos.py`.
  `extract_metrics.py` acepta ya el prefijo `cv3_`.
- Verificación por diferencia contra el v2: 162 det cambian solo `demand`, 180 solo
  `success_time`, 18 (lev) ambos. Exactamente lo esperado.
- Tanda `logs/eval_catalogo_v3` (5 400 runs, 6 procesos, ~35 min), CSV en
  `analisis/catalogo_v3.csv`. **v2 intacto** (escenarios, logs y CSV) por si hay marcha atrás.

**🔴 HALLAZGO — el cambio 1 NO es inocuo en régimen determinista.** Mi hipótesis de partida
(«con ρ=1 no hay fracasos, así que el det es un control invariante») era **falsa**. Mecanismo,
verificado en el código y en los logs:
`simulator.cpp:469-473` — si al arrancar una tarea a un robot no le da la batería, el simulador
fuerza `success = false` y trunca la ejecución. Entonces `endTask` (`simulator.cpp:494-498`) usa
`averageFailDemand` y, como `averageFailTime == 0`, cobra la **demanda completa**. Con el v2 en
det eso eran 0 (el robot sobrevivía); con el v3 son 10 y **el robot muere**.
Ejemplo real (`cv3_A_r10_n060_bnd_E_det_q1_i1`, random): batería 3.16 → se le cobran 10 → −6.84.

⇒ **Mortalidad de robots, v2 → v3**: `random` det 0.00 → **1.85** (0 % → 65 % de las
ejecuciones), est 1.78 → 2.12; `greedy` det 0.00 → **1.27** (0 % → 50 %), est 1.50 → 1.64.
`cbaa`, `cbba` y `dec-mcts` siguen en **0 muertos en las 5 400 ejecuciones** de ambos catálogos
(comprueban la demanda de ejecución, no solo el desplazamiento). El aviso de CLAUDE.md sobre
reportar la mortalidad aparte se vuelve **más importante**, no menos.

**Control corregido**: los que sí son invariantes en det son **cbaa (−0.0000 ±0.0004), cbba
(+0.0002 ±0.0005) y dec-mcts (−0.0016 ±0.0013)**. Eso valida el pareado para la comparación que
importa. `random` y `greedy` no lo son, por el mecanismo de arriba.

**Resultado: la estructura de las conclusiones AGUANTA; solo se mueve `est`.**

| | v2 | v3 |
|---|---|---|
| Δ global (dec−cbba) | +0.0047 ±0.0044 (t=1.1) | +0.0086 ±0.0040 (t=2.2) |
| recuento global | 154/33/173 | 158/42/160 |
| Δ det | +0.0219 ±0.0058 | +0.0201 ±0.0055 |
| **Δ est** | **−0.0079 ±0.0071** | **+0.0057 ±0.0064** ← cambia de signo |
| Δ fue | −0.0500 ±0.0204 | −0.0497 ±0.0167 |
| mejor en est | cbba | **dec-mcts** ← único cambio de ranking |

- Niveles globales: rand 0.272→0.271, greedy 0.348→0.354, cbaa 0.425→0.437,
  cbba 0.482→0.494, **dec-mcts 0.487→0.502**. Todo el movimiento vive en `est`
  (dec +0.035, cbaa +0.025, cbba +0.022, greedy +0.015).
- **Mejor solver por bloque: idéntico en los dos catálogos** (A cbba, B dec, C dec, D cbaa,
  E cbba). CBAA sigue ganando el bloque de coaliciones.
- ⚠️ **Bloque A, pendiente en est** (la formulación más limpia de la tesis): dec-mcts
  **−0.0004 (t=−0.2) → +0.0040 (t=1.5)**, cbba +0.0150 (t=5.7) → +0.0195 (t=6.6). El argumento
  sobrevive pero **hay que reformularlo**: ya no es «no convierte robots en rendimiento en
  absoluto», sino «los convierte ~5 veces peor que CBBA y su pendiente sigue sin distinguirse
  de cero». Es el punto que más se resiente del cambio.
- Único bloque con movimiento significativo: **C·est** (+0.003 → +0.047, cambio +0.045 ±0.022).
- Tasa de éxito: dec-mcts sigue por encima de cbba en est (0.710→0.754 vs 0.681→0.719), o sea
  el matiz del addendum de `06_conclusiones.md` se mantiene.

**✅ DECISIÓN DEL USUARIO (misma sesión): el v3 pasa a ser el catálogo DEFINITIVO.**
Actualizados en consecuencia:
- `CLAUDE.md`: tabla de resultados del v3, comandos, mapa del repositorio y avisos (v1 **y** v2
  obsoletos; mortalidad mayor y también en det; aviso nuevo sobre el fracaso por batería en
  determinista; `demand[1]` = 10 en todos los regímenes).
- `01_contexto.md`: estado de la hipótesis con las tres columnas v1/v2/v3, lo sólido del v3 por
  bloque, objetivo **1.2-ter** cerrado, 2.2 hecho y 2.3 como siguiente.
- `03_experimentos.md`: sección **🟢 CATÁLOGO v3** completa (panorama, bloques, pendientes,
  mortalidad y mecanismo) y banner de **OBSOLETO** sobre la del v2.
- `06_conclusiones.md`: **ADDENDUM 2** (el vigente) + aviso de cabecera reescrito.
- `05_dudas.md`: **D-02 revisada** (oficial = v3), D-13 agravada, D-14 y D-15 ampliadas y
  **D-16 nueva** (el fracaso por falta de batería cobra la demanda completa; ¿se acepta la
  semántica o se cambia? — recomendación: aceptarla y declararla).
- `analisis/README.md`: v3 vigente, v2 como contraste.
- **`memoria/sections/05_casos_de_estudio.tex`**: Tabla 5.1 (filas «Consumo al ejecutar» y
  «Coste de un intento fallido»), párrafo «Tareas» (partido en dos: éxito y fracaso), párrafo
  «Las fuentes de incertidumbre» (el régimen fija solo ρ y σ), **Tabla 5.2** (σ = 0/1/3/5 y
  columna de coeficiente de variación en lugar de «Batería por fracaso»), el párrafo que
  hablaba del CV igual a la unidad (reescrito sobre el régimen fuerte) y el prefijo
  `cv2_` → `cv3_`.

**🔴 CORRECCIÓN DEL USUARIO (misma sesión) — error factual mío que llegué a escribir en la
memoria y en cuatro ficheros de contexto**: dije que «un intento cuesta 10 salga bien o mal».
**Falso.** En `endTask` (`simulator.cpp:494-498`):
- **Éxito**: `rate = averageSuccessDemand / averageSuccessTime = 10/10 = 1` ⇒
  `consumptionFinal = 1 · execTime`, o sea **proporcional a la duración real**, tan disperso
  como σ. Su esperanza es ~10 (algo por encima por el truncamiento en 0: 10.04 con σ=5).
- **Fracaso**: `averageFailTime == 0` ⇒ `consumptionFinal = averageFailDemand = 10` **exactos**.
⇒ Lo que coincide entre desenlaces es la **esperanza**, no el valor. Corregido en el cap. 5, en
`CLAUDE.md`, `01_contexto.md`, `03_experimentos.md`, `06_conclusiones.md` y en la cabecera de
`generate_catalog_v3.py`. **Lección: no describir el consumo como una constante.**

**Reestructuración de §5.2 (petición del usuario)**: los cinco bloques pasan de `\paragraph` a
**`\subsection`** (`subsec:casos-bloqueA`…`E`). El párrafo de nomenclatura se subió al material
común, antes de los subapartados, para que no quedara colgando dentro del bloque E, y las
aperturas de los cinco se reescribieron para que no dependan gramaticalmente del título.

**CIERRE DEL CAPÍTULO 5 (el usuario lo reescribió con el material anterior como referencia)**
- ✅ **Dado por finalizado.** El usuario pasó el capítulo por su propia redacción; el contenido
  y la estructura son los acordados. **Eliminó por completo la sección de Recapitulación**
  (queda comentada al final del `.tex`): la reserva para **abrir el capítulo 6**. ⚠️ Con ella se
  van las cifras agregadas (10 896 tareas, 12 264 plazas), las **3 réplicas** y el total de
  **5 400 ejecuciones**: si no se recuperan al principio del cap. 6, no aparecen en ninguna
  parte de la memoria.
- Erratas detectadas y **ya corregidas por el usuario**: «el bloque recorre se centra»,
  «responde a como responde», «tipos… construidas» (concordancia), «Esto se trata de», y la
  incoherencia «paquetes» vs «racimos» en la Tabla 5.1. También cambió $L \approx 6$ por
  $L = 6$ (correcto: comprobado que el reajuste $m=Ln/\bar q$ da entero en las 360 celdas).
- ⚠️ **Dos correcciones señaladas que NO se aplicaron** (decidir en la próxima sesión, son de
  una línea): (a) §5.1, «formato yaml» → «formato YAML», como en el cap. 4; (b) §5.1, «$L$…
  representa el número de **tareas** que corresponden a cada robot» — con coaliciones es falso:
  $L$ cuenta **plazas de trabajador**, y con $q_j=2$ le tocan $L/2$ tareas por robot. Es
  justamente el motivo del reajuste $m=Ln/\bar q$ que el propio bloque D explica.

**DUDA RESUELTA — la ventana escalonada NO lleva `+10`.** El usuario preguntó si
$[0,\;\tfrac{g}{4}T+\varepsilon]$ era un error frente a $[0,\;\tfrac{g}{4}T+10+\varepsilon]$.
No lo es: la fórmula reproduce exactamente el generador. Medido sobre los 816 plazos reales de
cada tipo de ventana:

| ventana | apertura media | plazo medio | anchura media |
|---|---|---|---|
| estrecha (referencia) | 20.01 | 29.97 | 9.96 |
| solo plazo | 0 | 29.67 | 29.67 |
| **escalonada** | 0 | **24.67** | 24.67 |
| ancha | 0 | 49.87 | 49.87 |

⇒ Tal cual está, la escalonada es **5.00 unidades más apretada** que la de solo plazo; con
`+10` sería **5.00 más holgada**. El `+10` no corrige la asimetría, **le cambia el signo**
(para igualar habría que sumar exactamente 5, un número sin significado de diseño). Razones
para dejarlo: el `+10` de las otras tres es la anchura nominal de la ventana de referencia, que
`P` hereda al copiar el plazo y `A` al tomar su envolvente ($t_0^{máx}=T$, más 10), mientras
que la escalonada no deriva su plazo de $t_0$; con `+10` el último racimo cerraría en 50, más
allá del horizonte, y la frase «vencen a un cuarto, la mitad, tres cuartos y la totalidad del
horizonte» dejaría de ser cierta; y el bloque B nunca buscó iso-dificultad entre sus niveles
(la ancha tiene plazo medio 49.87). Además los cinco solvers ven ventanas idénticas y las
comparaciones son pareadas ⇒ más apretada es *distinta*, no *injusta*.
📌 **Para el cap. 6**: al destacar la escalonada (mejor terreno de Dec-MCTS, 12/12 en det),
mencionar que su plazo medio es ~5 unidades más corto que el de la ventana sin espera.

---

## 🔜 PARA LA PRÓXIMA SESIÓN

**Objetivo: rehacer el notebook de evaluación y escribir el capítulo 6.**

Orden sugerido:
1. ✅ **HECHO en la sesión 9**: `analisis/evaluacion_final.ipynb` rehecho sobre el v3 en tres
   partes, con 14 figuras `p1_*`/`p2_*`/`p3_*`. Ver la entrada de esa sesión.
2. **Escribir `memoria/sections/06_experimentación_pruebas.tex`.** Material: **addendum 2** de
   `06_conclusiones.md` (el vigente; los dos anteriores son histórico).

Recordatorios para el cap. 6, todos ya anotados en sus ficheros:
- **Abrirlo con la recapitulación del catálogo** que el usuario quitó del cap. 5 (cifras
  agregadas, 3 réplicas, 5 400 ejecuciones, unidad de análisis = escenario).
- **Nunca liderar con el agregado**; reportar por bloque y por régimen.
- Reformular la idea 1: la pendiente de Dec-MCTS bajo incertidumbre ya no es cero
  (+0.0040, t=1.5), es ~5× menor que la de CBBA. Lo que no se ha movido es la pendiente de la
  **Δ**: −0.0155 por robot (t=−4.7).
- **Mortalidad de robots** aparte, y ahora también en determinista (D-16).
- El **protocolo experimental** completo no está en ninguna parte de la memoria (D-10).
- La **composición de bloques** (D-14) y las **cuatro refutaciones medidas sobre el v1** (D-13).
- El plazo medio de la ventana escalonada (arriba).

Decisiones que siguen abiertas y bloquean: **D-03** (cómo citar el paper del tutor, estructural
para el cap. 7), **D-13**, **D-14**, **D-16**, **D-17**.

---

## 2026-08-31 — Sesión 7: redacción del capítulo 5

**Objetivo**: escribir `memoria/sections/05_casos_de_estudio.tex`.

**Lo que hay que saber para la próxima sesión**: el usuario **rechazó la estructura de 7
secciones** del briefing `notas_cap5_casos_de_estudio.md` por excesiva (~20 páginas) y dictó
un guion mucho más escueto, que es el que se ha escrito:

- **Introducción**: objetivos del conjunto de prueba + los 4 criterios de diseño, resumidos;
  anuncio de los 360 escenarios en 5 bloques.
- **5.1 · Configuración común**: geometría, robots y cinemática, tareas, batería 40,
  iso-dificultad ($L=6$, $T=40$), ventana y coalición de referencia, regímenes, métrica.
  Tablas 5.1 (sustrato) y 5.2 (regímenes).
- **5.2 · Los cinco bloques**: A control, reparto 50/50 det/est con la excepción de E,
  instancias, pareado por semilla, un párrafo por bloque (con las tres ventanas P/C/A dentro
  del bloque B) y nomenclatura. Tabla 5.3.
- **5.3 · Recapitulación**: cifras agregadas, 3 réplicas y $360\times5\times3=5\,400$
  ejecuciones.

**Decisiones del usuario (cierran las 4 dudas abiertas del briefing)**
1. **No** entra el pilotaje de calibración (ni su tabla): $L=6$ y la capacidad 40 se presentan
   justificados por su razonamiento, sin cifras de CBBA.
2. Las **figuras no se crean**: quedan dos marcas `\red{PENDIENTE: ...}` en el `.tex`
   (croquis de la geometría en §5.1 y diagrama de las cuatro ventanas en §5.2).
3. **No se menciona** ninguna versión anterior del catálogo. Se llama «el catálogo».
4. La advertencia sobre la composición de bloques (D-14) **no va en el cap. 5**.
5. Notación del cap. 2: $n$ robots, $m$ tareas (no $R$/$n$ como en el briefing).

**Además, y a diferencia de lo que decía D-10**: el protocolo experimental **no** va como
sección propia. Solo sobreviven, en la recapitulación, las 3 réplicas, el total de 5 400
ejecuciones y la frase de que la unidad de análisis es el escenario. El resto (idempotencia
del ejecutor, coste de la tanda, aviso sobre `computing_time` en paralelo, irreproducibilidad
por `std::random_device`) **no está escrito en ninguna parte de la memoria**: si se quiere,
hay que colocarlo en el cap. 6. Tampoco hay sección de limitaciones (mortalidad de robots,
geometría única, estación única, heterogeneidad no ejercitada); la heterogeneidad no
ejercitada sí queda declarada de pasada en §5.1.

**Verificado contra el catálogo real** antes de escribir: 360 escenarios, 10 896 tareas,
12 264 plazas, 6–80 tareas por escenario (mediana 30), 2–10 robots, 162 det / 198 est,
5 400 registros en `logs/eval_catalogo_v2`. ⚠️ La cabecera de `generate_catalog_v2.py`
(línea 51) tiene **intercambiado** el reparto: dice «198 det / 162 est» y es al revés.

**Figura 5.1 generada** (misma sesión): `scripts/figura_geometria_cap5.jl` → PDF vectorial en
`memoria/figures/05_casos_de_estudio/geometria_escenario.pdf` (+ PNG gemelo para revisarla).
⚠️ **El primer intento no gustó**: era un único escenario con robots, leyenda de estados, flechas
de recorrido y anotaciones de distancia. Lo que el usuario quería es mucho más simple: **tres
escenarios en su estado inicial, uno al lado de otro**, con 3, 4 y 5 tareas por racimo (12, 16 y
20 tareas), **sin robots, sin leyenda, sin anotaciones y sin coordenadas**, con un único rótulo
«N tareas» bajo cada panel. Lección: para las figuras de la memoria, preguntar por la composición
antes de invertir en detalle.

Reproduce el lenguaje visual de `mrtau video`, cuyo renderizador real es
`~/tfm/mrtau/scripts/mrtau_visualization.jl` (Julia + CairoMakie, instalado en el entorno
`@v1.10`): tareas `lightgreen` #90EE90 y estación `yellow` #FFFF00, con los diámetros
equivalentes a los 20 y 24 px del vídeo sobre un lienzo de 800 px que abarca 25 unidades.
Como aquí el eje se recorta a ±5,5, los marcadores se expresan en unidades del escenario
(`markerspace = :data`), de modo que los racimos se ven igual de apretados que en el vídeo.
Se conserva la caja del eje (el vídeo también la tiene) para separar los tres paneles, pero sin
ticks ni rótulos. Las posiciones se leen de los YAML reales; la geometría solo depende del número
de tareas, así que se toma el primer fichero del catálogo con 12, 16 y 20. La figura se compone
a su **tamaño final de impresión** (440 × 168 pt ≈ 15,5 × 5,8 cm), así que los cuerpos de letra
del PDF son los que se verán en papel. Regenerar con
`julia --project=@v1.10 scripts/figura_geometria_cap5.jl`.
⚠️ Trampa de Makie: un `Label` con `tellwidth = true` (el valor por omisión) impone su anchura a
la columna y encoge los ejes con `DataAspect()` hasta dejarlos diminutos.

**Figura 5.2 (las cuatro ventanas) — ✅ TERMINADA E INSERTADA** (`fig:ventanas`, en §5.2.2).
La generó el usuario en TikZ con el chat de Claude, que sí compila LaTeX, igual que hizo con
`figures/04_materiales_y_metodos/arquitectura_simulador.pdf`. Hubo **dos rondas**: la primera
versión llevaba anotaciones a la derecha de cada carril («estar allí en un instante concreto»,
etc.) que el usuario mandó quitar por saturar, y su fila «escalonada» no explicaba la
particularidad de esa ventana. La segunda añade **cuatro marcas discontinuas** en t = 11,2 /
21,2 / 31,2 / 41,2 (los cierres de cada racimo), rotuladas «racimo 1…4», con la del racimo 3
destacada por ser la de la tarea del ejemplo.

⚠️ **Cómo revisar un PDF sin rasterizador**: en esta máquina no hay poppler, ni ghostscript, ni
PyMuPDF. El método que funciona: descomprimir los flujos (`zlib`) y reconstruir el contenido
vectorial con un intérprete mínimo de operadores PDF (`q/Q/cm/rg/RG/w/m/l/c/f/S` y `BT…ET`),
acumulando la CTM. Verificado en la versión final: 470,39 × 207,23 pt; escala 6,094 pt/unidad
con origen en x = 83,51; las cuatro barras en [18, 29,2], [0, 29,2], [0, 31,2] y [0, 51,2] —
exactas; línea roja en t = 29,20 abarcando solo las filas 1-2; marcas de racimo exactas y
confinadas a la fila 3 (y ∈ [71,3 · 97,7]); racimo 3 a 0,90 pt / gris 10 % frente a 0,50 pt /
gris 53 %; el rótulo «racimo 4» desplazado a x = 337,3 para no chocar con la línea de T = 40
(x = 327,3); anotaciones de la derecha ausentes.

⚠️ **Trampa al leer texto de un PDF**: los códigos de carácter son índices del subconjunto de
fuente, **no ASCII**. El numerador de la fracción aparecía como glifo `6` y parecía un error;
se resolvió leyendo el `/CharSet` del descriptor de fuente: `/u1D454/u1D457/u1D459/u1D460`
(= *g, j, l, s* en cursiva matemática), así que el `6` era la **g**. Para decidir si un carácter
es letra o dígito hay que mirar **qué fuente** lo compone, no su byte.

**Qué queda**
1. **Capítulo 6**. Antes: leer el addendum de `06_conclusiones.md`, decidir D-13 (¿re-medir B4
   y D-08 sobre el v2?) y D-14 (declarar la composición de bloques), y rehacer el notebook y
   las figuras de `analisis/` sobre el v2.
2. Recordar en el cap. 6 lo que el cap. 5 ya no dice: mortalidad de robots de `random`/`greedy`
   y protocolo experimental completo.
3. D-03 (cómo citar el paper del tutor) sigue abierta y es estructural para el cap. 7.

---

## 2026-08-27 — Sesión 6: sonda de variantes de diseño de escenario

**Objetivo (del usuario)**: antes de rediseñar el catálogo en bloques equilibrados, medir con
una tanda pequeña qué ejes de diseño —muchos nunca probados— favorecen a cada algoritmo.
Pregunta principal: **¿en cuántas variantes gana Dec-MCTS?**

**Qué se hizo**
- Verificado que v4 y CBBA **sí** modelan `averageFailTime`, `stdFailTime`, `averageFailDemand`
  y `batteryRateWhileNavigating` ⇒ los ejes nuevos son justos para ambos.
- Nuevo `scripts/generate_probe_scenarios.py` → `scenarios/probe/` (204 esc.) y
  `scripts/analyze_probe.py`. Tanda `logs/eval_probe` (3 060 runs, ~15 min con 6 procesos),
  CSV en `analisis/probe.csv`.
- 17 variantes × (R∈{4,8} × {det,est} × 3 inst) × 3 réplicas, **una sola variación** sobre una
  referencia común y **semilla independiente de la variante** ⇒ pareado exacto entre variantes.

**Resultados** (detalle en `03_experimentos.md`, sección «SONDA DE VARIANTES»)
- Dec-MCTS es el mejor de los cinco en **7/17 variantes en determinista** y **2/17 en
  estocástico**; por encima de CBBA en **3/17** en el agregado.
- **Hallazgo 1 — la espera es el problema, no el plazo.** El factorial 2×2 de la ventana
  {apertura 0/t0} × {cierre suelto/apretado} muestra que con plazos heterogéneos **sin espera**
  (`w_plazo`) Dec-MCTS gana (+0.056 det, +0.013 est) y es el único caso donde es el mejor en
  ambos regímenes; añadir la espera anula la ventaja (efecto pareado **+0.069 ± 0.014** a favor
  de quitar la espera). Confirma y cuantifica la intuición del usuario.
- **Hallazgo 2 — batería escasa (`b_escasa`).** Único eje donde Dec-MCTS mejora bajo
  incertidumbre. Mecanismo **6/6 consistente**: la tasa de éxito sube **+0.120** porque CBBA
  empieza tareas que no puede terminar (su comprobación de batería usa la demanda *esperada*)
  y Dec-MCTS, que muestrea duraciones, deja margen. El efecto en recompensa es ruidoso
  (+0.037 ± 0.038): **hay que confirmarlo con más instancias**.
- **Hallazgo 3 — el sobrecoste de desplazamiento es el hilo conductor.** Dec-MCTS recorre entre
  +6 % y +81 % más distancia que CBBA en *todas* las variantes. Es inocuo cuando navegar es
  barato y **letal** cuando el desplazamiento es el recurso que ata: `b_navcara_escasa` es la
  peor variante de todas (−0.073 det, −0.108 est, **12/12 escenarios perdidos**).
- **Suelo de ruido medido gratis**: en determinista `f_igual`/`f_doble` son configuraciones
  idénticas a la referencia. greedy/cbaa/cbba reproducen **exactamente** (Δ = 0.0000);
  Dec-MCTS ±0.008 de media, máx 0.028 por escenario ⇒ en det no interpretar |Δ| < 0.02.
- **Validación de la sonda**: reproduce los resultados conocidos del catálogo — coaliciones
  q=2 (−0.065 det / −0.141 est, catálogo: −0.084 / −0.109) y ventana escalonada
  (+0.017 det, catálogo: +0.026).
- Sin efecto interpretable: duración del fracaso (`f_igual`/`f_doble`), navegar caro a secas.
  Perjudican: ventana ancha, robots dispersos, duraciones heterogéneas, capacidades
  heterogéneas (esta última solo bajo incertidumbre, −0.100).

**Qué queda**
1. ✅ Hecho en la misma sesión: catálogo v2 diseñado, generado y ejecutado (abajo).
2. ✅ `b_escasa` confirmado a escala de catálogo (+0.064 en est, bloque A v2 vs A1 v1).
3. El cap. 5 sigue sin escribirse: ahora ya hay catálogo definitivo que describir.

### 2026-08-31 (misma sesión) — CATÁLOGO v2 construido y ejecutado

**Decisiones del usuario**: batería escasa en todos los escenarios; conservar R, carga,
coaliciones y ρ como ejes; bloques de igual tamaño; 3 réplicas; aceptar y documentar que el
bloque E no tenga mitad determinista; **no** añadir el bloque de capacidades heterogéneas.

**Hecho**: pilotaje de calibración (720 runs) → L=6 sigue siendo iso-dificultad con batería 40,
y 40 es el valor correcto (60 no ata). `scripts/generate_catalog_v2.py` → `scenarios/catalogo_v2/`
(360 esc.), `scripts/analyze_catalog_v2.py`, tanda `logs/eval_catalogo_v2` (5 400 runs, ~25 min).
Una línea de `extract_metrics.py` extendida para el prefijo `cv2_`.

**Resultado que hay que interiorizar antes de redactar nada** (detalle en `03_experimentos.md`):
- El agregado global **cambia de signo** (v1 −0.0355 → v2 **+0.0047 ± 0.0044**) pero es un
  **EMPATE** (t=1.06) y por recuento Dec-MCTS **pierde** (154/33/173). **El cambio es del
  catálogo, no del algoritmo.** Nunca liderar con el agregado.
- **La batería escasa cierra 2/3 de la brecha estocástica** en configuración idéntica
  (bloque A1 v1 vs A v2: est −0.081 → −0.018). Mecanismo: la tasa de éxito de Dec-MCTS pasa a
  ser **superior** a la de CBBA. ⚠️ Esto **matiza la idea 2 de `06_conclusiones.md`**.
- Bloque A: en estocástico la pendiente propia de Dec-MCTS es **−0.0004 (t=−0.2)** frente a
  **+0.0150 (t=5.7)** de CBBA. La formulación más limpia de la tesis central.
- Ventana **escalonada**: +0.076, gana **12/12** en determinista. Mejor terreno con diferencia.
- Carga: gradiente monótono en L y en R, +0.208 (R=2,L=3) → −0.076 (R=10,L=8).
- **CBAA es el mejor solver del bloque de coaliciones** (0.424 vs 0.402 de cbba). Novedad.
- Con **2 robots Dec-MCTS gana en los cuatro niveles de ρ**; con 5-10 pierde en cuanto ρ<1.
- ⚠️ Auditoría a posteriori: los bloques B y C salieron más favorables de lo previsto (2 de
  3 niveles en terreno bueno). Declararlo en la memoria.
- ⚠️ Con batería 40, `random` y `greedy` pierden ~0.9 y ~0.8 robots por ejecución (40 % de los
  runs); cbaa, cbba y dec-mcts, ninguno. Reportar la mortalidad aparte.

### 2026-08-31 (cierre de sesión) — v2 promovido a estudio principal y cap. 5 preparado

**Decisión del usuario**: el catálogo v2 pasa a ser el estudio principal; el v1 queda obsoleto
pero se mantiene en el repositorio por ahora; `analisis/` se actualiza **después**; lo
prioritario es dejar preparada la escritura del cap. 5.

**Ficheros de registro actualizados**
- `CLAUDE.md`: tabla de resultados del v2 con el aviso de que el agregado es un **empate**;
  comandos de generación/ejecución/análisis del v2; mapa del repositorio; cuatro avisos nuevos
  (v1 obsoleto · composición de bloques · mortalidad de robots · `demand[1]` no es una σ);
  réplicas 10 → 3; «no subir `nav_rate` por encima de 1».
- `01_contexto.md`: estado de la hipótesis reescrito con la tabla v1 vs v2 y la lista de lo que
  es sólido por bloque; objetivo 1.2-bis (catálogo v2) añadido; 1.2 marcado obsoleto; 2.2 y 2.3
  apuntan al briefing del cap. 5 y avisan de que el notebook está desactualizado.
- `03_experimentos.md`: sección «CATÁLOGO v2» (ya escrita antes) + banner de **OBSOLETO** sobre
  la del v1, explicando por qué se conserva y qué cifras no pueden citarse.
- `05_dudas.md`: **D-02 revisada** (conjunto oficial = v2), **D-10 RESUELTA** (protocolo al
  final del cap. 5, **3 réplicas**), y tres dudas nuevas: **D-13** (las cuatro refutaciones se
  midieron con batería 100 — conviene re-medir B4 y D-08 sobre el v2 antes del cap. 6),
  **D-14** (composición del v2 levemente favorable a Dec-MCTS: declararlo), **D-15** (¿se
  jubilan del repositorio los logs del v1?).
- `06_conclusiones.md`: **ADDENDUM** al final. Las cinco ideas siguen en pie; la lección
  metodológica se **refuerza** (dos catálogos defendibles, conclusiones opuestas, mismo código);
  la idea 1 se refuerza; la **idea 2 se matiza** (ya no es cierto que la tasa de éxito sea
  idéntica: con batería escasa favorece a Dec-MCTS); la idea 4 se matiza (CBBA ya no gana en
  los cuatro regímenes, y **CBAA gana el bloque de coaliciones**); la idea 5 se afina
  (la coalición mixta va al revés que la uniforme); **idea 6 nueva** (sobrecoste de
  desplazamiento). Aviso al principio del fichero para que no se citen cifras sin leer el addendum.
- `analisis/README.md`: reescrito. `catalogo_v2.csv` vigente, notebook y figuras marcados como
  desactualizados, y una tabla con qué debe mostrar cada figura al rehacerla sobre el v2.

**Preparado para la próxima sesión**
- 📌 **`memoria/notas_cap5_casos_de_estudio.md`** — briefing completo del cap. 5: qué va y qué
  no va, estructura en 7 secciones, las 5 tablas con sus cifras verificadas, las etiquetas
  `\ref{}` disponibles, el protocolo experimental que resuelve D-10, el apartado de
  limitaciones, 3 figuras sugeridas (ninguna existe aún) y 4 decisiones abiertas.

**Qué queda**
1. ⏭️ Escribir `memoria/sections/05_casos_de_estudio.tex`.
2. Rehacer `analisis/evaluacion_final.ipynb` y las 8 figuras sobre el v2 (antes del cap. 6).
3. D-13: re-medir B4 y D-08 sobre el v2 (~25 min) para apuntalar la idea 2.
4. D-03 sigue abierta y es **estructural** para el cap. 7.

---

# 📍 ESTADO AL CIERRE DE LA SESIÓN 5 (2026-08-21) — LEER ESTO PRIMERO

## 🚩 CAPÍTULO 4 TERMINADO Y CERRADO. SIGUIENTE PASO: CAPÍTULO 5 (CASOS DE ESTUDIO).

`memoria/sections/04_materiales_y_metodos.tex` está **cerrado por decisión del usuario**: no se
modifica salvo necesidad estricta, y nunca sin preguntar. Lo mismo se aplicaba ya a los caps. 2 y 3.

**Lo que hay que saber al retomar:**

1. **El siguiente capítulo es el 5 (Casos de estudio)**: el catálogo de 288 escenarios. Material
   en `03_experimentos.md` → sección «CATÁLOGO DEFINITIVO» y en
   `scripts/generate_catalog_scenarios.py`. Principio de diseño a explicar: **iso-dificultad**
   (carga por robot L=6 constante, horizonte T=40 fijo), y el error metodológico del catálogo
   antiguo que motivó rehacerlo (ligar el horizonte al nº de tareas confundía «más robots» con
   «menos carga»).
2. **El protocolo experimental se quedó sin sitio** (duda **D-10**): el usuario eliminó ese
   apartado del cap. 4, así que las **10 réplicas**, el ejecutor y la limitación de
   reproducibilidad (`std::random_device`, sin semilla) **no están declaradas en ningún sitio de
   la memoria todavía**. Hay que colocarlas en el cap. 5 o al inicio del cap. 6.
3. **Tres pegas menores del cap. 4 quedaron sin aplicar** (dudas **D-11** y **D-12**): la fila
   `p^blq_MAX = ρ_j` de la Tabla 4.4 se quedó sin explicación y en tensión aparente con el cap. 2,
   y hay tres erratas de redacción localizadas. El usuario decide si se tocan.
4. **Nada de ablaciones en la memoria** (decisión del usuario, sesión 5). El cap. 6 reporta la
   comparación final, no los estudios de ablación. Los hiperparámetros se presentan como
   «fijados de forma empírica durante el desarrollo».
5. **Sigue pendiente D-03** (cómo citar el paper del tutor), que es estructural para el cap. 7.

---

## 2026-08-21 — Sesión 5: redacción del capítulo 4 (Materiales y métodos)

**Objetivo**: escribir el cap. 4 de la memoria.

**Qué se hizo**
- Lectura completa de los caps. 2 y 3 y del briefing `notas_cap4_materiales_y_metodos.md`, y
  auditoría del código para verificar cada dato antes de escribirlo (`simulator.cpp`,
  `solver_DecMCTS_v4.hpp`, `reward_00.hpp`, `observation.hpp`, `solver_interface.hpp`,
  `CMakeLists.txt`, `experiment_config.yaml`, un log de `eval_catalogo`).
- **Hallazgo sobre el reparto**: el cap. 2 tiene **cuatro bloques comentados** (líneas ~721-729)
  con γ_D, C_p, la recompensa del rollout y el descuento perezoso. El usuario los comentó
  *a propósito* para que aterrizasen en el cap. 4. Eso definió el capítulo: es «lo que el cap. 2
  dejó abierto + lo que solo existe en el código».
- Se propuso un índice, se escribió un primer borrador (~7 100 palabras) y el usuario lo
  **rechazó por excesivo y redundante**: pedía visión global, no inventario de decisiones de
  código. Se reescribió con un índice nuevo dictado por él y se recortó a ~4 700; después él
  lo reescribió en su propia voz hasta **~3 700 palabras (≈9-10 págs.)**.
- Datos de entorno medidos y fijados en el capítulo: g++ 13.3.0, CMake 3.28, Ubuntu 24.04 sobre
  WSL2 (núcleo 6.6), i5-1135G7 (4c/8h @2,4 GHz), 3,7 GiB, Python 3.12, yaml-cpp.
- Se generó el *prompt* con el que el usuario creó la figura de arquitectura
  (`memoria/figures/04_materiales_y_metodos/arquitectura_simulador.pdf`), en TikZ *standalone*.
- Verificación final: **las 20 referencias cruzadas del capítulo resuelven** contra etiquetas
  existentes y ninguna etiqueta nueva colisiona con las del cap. 2.

**Estructura final del cap. 4** (5 secciones): 4.1 Materiales y entorno · 4.2 Organización del
software (+ 4.2.1 La simplificación de la comunicación, 4.2.2 El registro de la ejecución) ·
4.3 La función de recompensa · 4.4 El simulador distribuido de eventos discretos (+ 4.4.1 La
cola de eventos, 4.4.2 Hiperparámetros del motor) · 4.5 Dec-MCTS (+ 4.5.1 Refinamientos sobre
el esquema base, 4.5.2 La selección de hiperparámetros).

**Qué se descartó explícitamente del capítulo** (decisiones del usuario, no olvidar y no
reintroducir): la interfaz `ISolver`; el modelo físico y sus simplificaciones (recarga
instantánea, navegación determinista, un solo intento por tarea); la barrera de defensa de
+1 s y la resolución de conflictos; la nota de transparencia sobre observabilidad (D-01); la
hipótesis de tiempo virtual como apartado propio; el protocolo experimental completo; la
exigencia de fidelidad del modelo generativo interno; y toda mención a ablaciones.
⚠️ El briefing `memoria/notas_cap4_materiales_y_metodos.md` queda **obsoleto**: describe un
capítulo mucho más extenso y detallado que el que finalmente se ha escrito.

**Hallazgo en el código (no corregido)**: `Robot.completedTasks` se inicializa a 0 en
`state.cpp` y **nunca se incrementa** ⇒ las cuatro métricas `*_completed_tasks_by_robot` de
**todos** los logs valen 0. No afecta a `final_reward` (que cuenta estados de tarea), pero el
reparto de tareas por robot **no está en los logs**: si el cap. 6 lo necesita, hay que derivarlo
de la traza con `scripts/extract_metrics.py`. Anotado en `02_codigo.md`.

**Estado del repo al cerrar**: sin cambios en el código. Modificados
`memoria/sections/04_materiales_y_metodos.tex` (escrito), `.claude-notes/*` y `CLAUDE.md`.
Añadida `memoria/figures/04_materiales_y_metodos/arquitectura_simulador.pdf`.

**Qué queda / siguiente paso propuesto**
1. **Cap. 5 (Casos de estudio)**: el catálogo de 288 escenarios a iso-dificultad.
2. Resolver **D-10** (dónde va el protocolo experimental y las 10 réplicas) al empezar el cap. 5.
3. Decidir sobre **D-11** y **D-12** (pegas menores del cap. 4).
4. **D-03** antes de llegar al cap. 7.

---

# 📍 ESTADO EXPERIMENTAL AL CIERRE DE LA SESIÓN 4 (2026-08-18) — SIGUE VIGENTE

## 🚩 EL BLOQUE 1 ESTÁ CERRADO. LA FASE ACTUAL ES ESCRIBIR LA MEMORIA.

**Objetivos 1.1, 1.2 y 1.3: ✅ TERMINADOS.** No se toca más el solver ni se lanzan más tandas.
La validación externa sobre Solomon (bloque F) se **descartó** por decisión del usuario: 3.5 h
de cómputo para confirmar un rendimiento bajo que ya se conoce.

📌 **Antes de redactar cualquier capítulo, leer `06_conclusiones.md`.** Contiene las cinco ideas
que sostienen el trabajo —dictadas por el usuario— con las cifras que las respaldan y con los
avisos sobre qué **no** puede afirmarse. Es el documento más importante del cuaderno ahora mismo.

**La tesis en una frase**: Dec-MCTS no supera a CBBA, pero **no es un algoritmo fallido**; bate
con holgura a los baselines y a CBAA, y su techo no viene de la implementación sino de una
limitación real del problema — bajo incertidumbre, comunicar intenciones o planes individuales
deja de ser informativo. La línea de trabajo futuro que se desprende: **comunicación consciente
de coaliciones**.

**Dudas D-09 y la formulación de la idea 3: RESUELTAS por el usuario al cerrar la sesión.**
- **D-09**: no se mide el centralizado y no hace falta — el paper del tutor ya lo implementa
  sobre el **mismo problema y la misma incertidumbre**. El argumento del cap. 7 se apoya en esa
  referencia: MCTS funciona centralizado → falla distribuido → las refutaciones descartan la
  implementación → la pérdida es de la **descentralización con comunicación limitada**. Sin
  cifras concretas (los escenarios no son los mismos).
- **Idea 3** (la escasez de literatura): se plasma **como teoría**, en párrafo marcado como
  especulativo, sin construir nada encima.

**Lo primero que hay que resolver al retomar**: **D-03** — cómo citar el paper del tutor, que no
está publicado. Ha pasado de ser un detalle de formato a ser **estructural**, porque el argumento
central del cap. 7 se apoya en él.

---

# Estado experimental al cierre (referencia)

⚠️ Este bloque se reescribió el 2026-08-18. Las versiones anteriores afirmaban que «Dec-MCTS
gana condicionalmente con ≥4 robots» y que «la ventaja crece con el nº de robots»: **ambas
quedaron refutadas** por `logs/eval_catalogo` (duda D-07). Si aparecen en notas viejas, ignorar.

**Lo que se sostiene tras la experimentación completa:**
1. **Ranking final** (288 escenarios controlados a iso-dificultad): cbba 0.603 · dec-mcts 0.568 ·
   cbaa 0.473 · greedy 0.389 · random 0.331.
2. **Dec-MCTS empata con CBBA en determinista** (Δ −0.006 ± 0.004) y pierde bajo incertidumbre,
   con la brecha creciendo **−0.013 por robot** (t = −8.4).
3. **Es, junto a CBBA, el único que escala con el equipo** (+0.013/robot, t = 5.9). CBAA y
   greedy son planos. Su coordinación **sí funciona**.
4. **El mecanismo de la brecha**: exceso de deconflicción — no elige peor (tasa de éxito
   idéntica), **intenta menos tareas**. Ver `06_conclusiones.md`, idea 2.
5. **Cuatro intervenciones sobre la implementación no la mueven** (cómputo ×40, rondas ×30, B4,
   D-08); el contraste positivo (quitar la comunicación) sí produce efecto (+0.066 ± 0.019).
6. **La ablación C1 sigue vigente**: el canal de comunicación aporta en determinista y **nada**
   bajo incertidumbre. Un plan comunicado solo vale lo que valga su predictibilidad.

**Decisiones acumuladas**: solo **v4-g9999** en la evaluación (D-04/D-06); catálogo nuevo como
conjunto oficial y bundles/random como material exploratorio (D-02); **Solomon descartado**;
régimen estocástico como gradiente conservando el actual; el simulador **no se toca**;
**3 réplicas** por experimento (deroga C-6); D-09 resuelta apoyándose en el paper del tutor;
idea 3 se redacta **como teoría**.

**Tandas de logs válidas de referencia**: `eval_catalogo` (la definitiva, 14 400 runs),
`eval_b4` y `eval_d8` (los dos intentos de corrección), `eval_c1_mix` + `eval_c1_1E` (ablación
de comunicación), `eval_versiones_fix2`, `eval_1E`.

---

## 2026-08-17 — Sesión 4: diseño y construcción del catálogo definitivo (objetivo 1.2)

**Objetivo**: definir el conjunto de escenarios del TFM, buscando que Dec-MCTS pueda destacar
(hipótesis de partida: más robots ⇒ más ventaja frente a CBBA) sin dejar de ser honesto.

**Pilotaje previo (lo más importante de la sesión)**
- **Efecto techo confirmado**: añadir robots manteniendo el nº de tareas vuelve el problema
  trivial (24t/10r ⇒ cbba y dec-mcts empatan a **1.000**). El eje "nº de robots" no se puede
  barrer sin recalibrar la dificultad.
- **Punto iso-dificultad localizado**: con carga **L=6** plazas/robot y horizonte **T=40**,
  cbba obtiene 0.667 / 0.667 / 0.683 con R = 2 / 5 / 10. Es el punto de operación del catálogo.
- ⚠️ **El hallazgo de `eval_1E` está confundido** (nueva duda **D-07**): allí el nº de tareas
  era fijo por familia, así que más robots significaba menos carga por robot. A iso-dificultad
  la ventaja **no se reprodujo** (Δ = 0.000 / 0.000 / −0.022 con R = 2/5/10; 3 réplicas ×
  1 instancia, ruido ±0.05 ⇒ indicativo, no concluyente).
- **Las instancias importan más que las réplicas**: la variación entre instancias de una misma
  celda es ±0.15, un orden de magnitud sobre el ruido entre réplicas (±0.016).

**Qué se hizo**
- `scripts/generate_catalog_scenarios.py` (nuevo). Admite cualquier nº de tareas — el
  generador anterior exigía múltiplo de 4 y con `n=10` escribía 8 en silencio. Semilla por
  CRC32 del nombre ⇒ catálogo reproducible byte a byte.
- `scenarios/catalogo/`: **288 escenarios** en 6 bloques (A1 coordinación a iso-dificultad,
  A2 rejilla robots × holgura, B ventanas, C coaliciones, D geometría, E gradiente de
  incertidumbre). Detalle en `03_experimentos.md`.
- `scripts/run_catalog.sh` (nuevo): reparte los escenarios en shards de enlaces simbólicos y
  lanza N procesos del ejecutable sobre el mismo directorio de logs.
- Smoke test completo (288 esc. × 4 solvers baratos × 2 réplicas, 23 s): los 288 ficheros
  cargan y el perfil de dificultad cae en la banda discriminante en todos los bloques.
- Lanzada la tanda `logs/eval_catalogo` (288 × 5 solvers × 10 réplicas = 14 400 runs).

**Decisiones del usuario**
- Evaluar **solo `dec-mcts-v4-g9999`** (no las cuatro versiones): reduce el coste ×5 y elimina
  el sesgo de D-06. → D-04 y D-06 cerradas.
- Régimen estocástico como **gradiente** (`lev`/`est`/`fue`) conservando el actual ⇒ se
  mantiene la comparabilidad con las tandas anteriores.
- **No tocar el simulador**: no se instrumentan los conflictos de asignación.
- Catálogo nuevo + Solomon como validación externa; `bundles/` y `random/` pasan a
  material exploratorio. → D-02 cerrada. *(Solomon se **descartó** después, el 18-ago.)*

**Lección de medida**: el modelo de coste `t ≈ 2.0e-4·R^1.6·n·T` es exacto en ejecución
**serie**, pero con 4 procesos en paralelo el coste por run se multiplica por ~3.5 (turbo
all-core + contención de caché en este portátil), así que la paralelización rinde mucho menos
de lo esperado. Medir el factor antes de prometer tiempos.

**RESULTADOS (tanda completa, 14 400 runs, 2 h 55 min de reloj)** — detalle en `03_experimentos.md`

| Solver | global | det | est |
|---|---|---|---|
| **cbba** | **0.6031** | **0.666** | **0.553** |
| dec-mcts-v4-g9999 | 0.5676 | 0.661 | 0.489 |
| cbaa | 0.4730 | 0.536 | 0.421 |
| greedy | 0.3894 | 0.433 | 0.357 |
| random | 0.3314 | 0.369 | 0.303 |

1. ⚠️ **La hipótesis del trabajo NO se sostiene sobre un catálogo controlado.** Δ global
   (dec-mcts − cbba) = **−0.0355 ± 0.0034**; gana en 67 escenarios, empata en 26, pierde en 195.
2. **D-07 resuelta y C-1 refutado**: a iso-dificultad no hay ventaja creciente con el nº de
   robots. En determinista la Δ es plana (−0.0032/robot, t=−2.4, empate); en estocástico
   **la desventaja crece** (−0.0130/robot, **t = −8.4**), de −0.032 con 2r a −0.131 con 10r.
   El hallazgo de `eval_1E` era un artefacto del confundido robots/holgura.
3. **Nace H-07** (nueva hipótesis, `05_dudas.md`): el sesgo de condicionar sobre los planes
   comunicados **se acumula con el nº de vecinos**. Tiene predicción falsable: B4/B5 deberían
   aplanar la pendiente.
4. **Único terreno favorable: ventana escalonada** (+0.026 ± 0.009 en determinista, gana 7/9).
   3 de las 4 únicas celdas con ventaja significativa del catálogo son de esta familia.
5. **Peor terreno: coaliciones** (−0.096 con q=2). Encaja con H-04 (`blockingProb` binaria).
6. **No compensa por otra vía**: +19 % de distancia recorrida y makespan algo peor para el
   mismo completado, pagando ~1000× de cómputo (12.5 s/run frente a 0.01 de cbba).

**ÚLTIMO INTENTO SOBRE EL SOLVER (mismo día) — dos mejoras, ambas con resultado nulo**

- **B4** (`dec-mcts-v4-b4`): `blockingProb` probabilística en vez de binaria, con
  Poisson-binomial exacta sobre P(≥q vecinos lleguen). → `logs/eval_b4`. **Nula.**
- **D-08** (`dec-mcts-v4-d8`): al instrumentar se descubrió que **el 68.7 % de los bundles
  comunicados se truncaban en un nodo `FINISH`** — la misma incoherencia de D-BUG-01, que se
  corrigió en `decideNextAction` pero **nunca en `extractDistribution`**. Corregido con
  `mostVisitedTaskChild`: longitud media del bundle 1.58 → 2.50, cortes por FINISH
  68.7 % → 1.8 %. → `logs/eval_d8`. **El defecto era real; la mejora de rendimiento, nula**
  (+0.0022 ± 0.0032 pareada sobre 126 escenarios).

⇒ **Cuatro explicaciones de implementación descartadas**: presupuesto de cómputo (`eval_hc`),
rondas de comunicación (`eval_rounds`), calibración de `blockingProb` (B4) y profundidad del
plan comunicado (D-08). La desventaja bajo incertidumbre **no es un artefacto de
implementación**. Material de primera para los caps. 6 y 7.

**OBJETIVO 1.3 COMPLETADO (mismo día) — notebook de evaluación final**

**Decisión del usuario**: no se adopta `dec-mcts-v4-d8` como definitiva (las correcciones no
mejoran nada sustancial); se mantiene `eval_catalogo` con `v4-g9999` como tanda de referencia y
**se asume el resultado negativo**, compensándolo con un análisis a fondo.

- `scripts/extract_metrics.py` (nuevo, solo biblioteca estándar): logs → CSV ordenado, una fila
  por ejecución, con métricas derivadas de eventos (tareas intentadas, tasa de éxito, instante de
  retirada) que la herramienta externa `mrtau metrics` no da. 17 860 ejecuciones de 8 tandas.
- `analisis/evaluacion_final.ipynb` (37 celdas, 8 figuras, ejecutado sin errores) +
  `analisis/README.md`, `analisis/requirements.txt`, `analisis/figuras/` (PNG y PDF).
- Entorno: `.venv/` con pandas/matplotlib/jupyterlab (el sistema es PEP 668 y no traía pip).
  Añadido a `.gitignore`.

**Dos correcciones de rigor detectadas al construirlo** (importantes, iban a debilitar el cap. 6):
1. La correlación Δrecompensa ~ Δtasa de intento de **r = 0.897** está inflada: en determinista
   la tasa de éxito vale 1 por construcción, así que la relación es **tautológica** (r = 1.000).
   La cifra honesta es la del subconjunto **con incertidumbre**: **r = 0.810** frente a 0.381 de
   la tasa de éxito (n = 153). El notebook reporta las tres por separado.
2. Las cuatro refutaciones **no tienen la misma potencia**: B4 y D-08 son n = 126 (IC ±0.006, sí
   descartan); cómputo y rondas heredan tandas de n = 12 (IC ±0.06, solo descartan efectos
   grandes). El notebook lo dice explícitamente y muestra la n en la figura.

**CIERRE DE SESIÓN (2026-08-18)**

Decisiones finales del usuario:
- **Objetivos 1.1, 1.2 y 1.3 dados por terminados.** No se toca más el solver.
- **Solomon descartado**: no aporta ver otra vez un rendimiento bajo.
- **Se asume el resultado negativo** y se compensa con el análisis, que ya está hecho.
- Conclusión del trabajo dictada y recogida en **`06_conclusiones.md`** (cinco ideas).

Creado `06_conclusiones.md` e indexado en `README.md`. Actualizado `CLAUDE.md` de la raíz —
llevaba el estado de la hipótesis desactualizado (decía que se cumplía condicionalmente) y ahora
refleja el resultado final, el catálogo definitivo, el nuevo flujo de análisis y el criterio de
3 réplicas. Añadida la duda **D-09**.

**Qué queda**
1. **Escribir la memoria** (bloque 2). Orden sugerido: cap. 4 (hay briefing) → cap. 5 (catálogo)
   → cap. 6 (resultados, material en el notebook) → cap. 7 (conclusiones, material en
   `06_conclusiones.md`) → cap. 1 → resumen.
2. ✅ D-09 resuelta. Queda **D-03** (cómo citar el paper del tutor, no publicado) — ahora es
   **estructural**, porque el argumento central del cap. 7 se apoya en esa referencia.
3. Limpieza (bloque 3): `.venv/`, `build/`, `logs/` inválidos, `copy/`, `.mp4`, `.claude-notes/`,
   y los `analyze_ablation.py` / `compare_v4.py` que el notebook deja obsoletos.
2. Bloque F: validación externa sobre Solomon (~3.5 h, 425 s/run con dec-mcts).
3. Objetivo 1.3: notebook de análisis por criterios (el script de trabajo está en el
   scratchpad de la sesión; hay que convertirlo en notebook).

---

## 2026-08-06 — Sesión 3 (cont.): familia 1E y ablación C1 de comunicación

**Familia 1E completa** (`logs/eval_1E`, 16 escenarios × 10 réplicas): confirma la intuición
del usuario. Dec-MCTS lidera (v4 0.6184, v4-g9999 0.6179) por delante de cbba (0.6089), pero
la Δ pareada frente a cbba es **+0.009 ± 0.010 (empate estadístico)**. La ventaja **escala con
el nº de robots**: −0.007 (2r) → +0.024 (5r). Detalle y criterios de diseño derivados en
`03_experimentos.md`.

**Ablación C1** (`logs/eval_c1_mix`, `logs/eval_c1_1E`; nuevo solver `dec-mcts-v4-nocomm`,
flag `useComm` en el constructor de v4): **el canal de comunicación SÍ aporta y mi sospecha
previa queda refutada.** En 1E, +0.066 ± 0.019; sin comunicación Dec-MCTS cae de 0.619 a
0.553, por debajo de cbba. Pero el aporte es **nulo en régimen estocástico (−0.005)** y
**nulo con 2-3 robots**. El conjunto MIX lo subestimaba porque 2 de sus 3 tamaños tienen 2-3
robots y 4 de sus 7 familias son estocásticas.

**Consecuencia para el rumbo del proyecto**: el trabajo pendiente no es sustituir la
coordinación (funciona), sino **hacerla robusta a la incertidumbre** (nueva hipótesis H-06 en
`05_dudas.md`), y **diseñar el catálogo con equipos de ≥4 robots** (criterio C-7).

---

## 2026-08-06 — Sesión 3: corrección de los defectos y reevaluación

**Objetivo**: aplicar las correcciones A1–A4 y volver a medir.

**Cambios aplicados** (los cuatro solvers, `include/tau/solver_DecMCTS_v*.hpp`)
- **A1 · Decisión filtrada por factibilidad.** Nuevo método `mostVisitedFeasibleChild`
  (`mostVisitedFeasibleActionChild` en v3): la política de explotación final elige el hijo
  del root con más visitas **de entre los aplicables ahora mismo**, con el mismo criterio
  que ya usaba la selección. Sustituye a `mostVisitedChild` en `decideNextAction`.
- **A2 · v3 recupera la fase de rollout.** `inTree = false` tras expandir; los chance nodes
  se siguen anotando en el `path` aunque ya no se esté dentro del árbol (se eliminó la
  condición `inTree &&` de los manejadores de `TASK_END`/`TASK_EXPIRATION`).
- **A3 · Fidelidad del consumo en fracaso.** `processTaskEnd` replica ahora
  `simulator.cpp::endTask`: si `averageFailTime == 0`, el consumo es la **demanda completa**,
  no 0.
- **A4 · Fidelidad de `robot.time` al esperar coalición**: `tInfo.latestStart`, como el
  simulador real (en la práctica es inerte, pero reduce la superficie de divergencia).
- Corregido el comentario obsoleto sobre γ en v4 (decía 0.999 por defecto; es 0.9999).
- Compilación en Release (`-O3`). No altera resultados (el nº de iteraciones es fijo), solo
  el `computing_time`.

**Iteración intermedia (error propio, corregido)**: en la primera versión de A1 sustituí la
poda de v4 por un criterio más estricto basado en `treeActions`. Eso **destruía subárboles**
cuando una tarea era temporalmente infactible por batería y provocó una regresión en
determinista (0.551 → 0.508, `logs/eval_versiones_fix`). Lección: la poda es destructiva y
su criterio debe ser conservador (solo lo que ya no volverá a ser aplicable); la
infactibilidad temporal se filtra en la decisión, sin borrar nada. Rehecho así en
`logs/eval_versiones_fix2`.

**Resultados** (misma tanda: 21 escenarios × 9 solvers × 5 réplicas; Δ pareada por escenario;
el ruido medido sobre los solvers **no modificados** es ≈ ±0.016)

| Solver | global antes | global después | Δ pareada |
|---|---|---|---|
| dec-mcts-v3 | 0.397 | 0.454 | **+0.057 ± 0.010** |
| dec-mcts-v1 | 0.415 | 0.461 | **+0.046 ± 0.007** |
| dec-mcts-v4-g9999 | 0.416 | 0.463 | **+0.046 ± 0.013** |
| dec-mcts-v4 | 0.413 | 0.458 | **+0.045 ± 0.013** |
| dec-mcts-v2 | 0.432 | 0.451 | +0.020 ± 0.012 (marginal) |
| random/greedy/cbaa/cbba | — | — | −0.016 … +0.011 (ruido) |

- El retiro prematuro **desaparece**: v4-g9999 en estocástico pasa de retirarse en t=31.6 con
  65 % de batería a t=42.1 con 57 %.
- Ya no hay regresión en determinista (v4-g9999: 0.553 → 0.551).
- **Las cuatro versiones quedan empatadas** (0.451–0.463 global): los defectos pesaban
  bastante más que las diferencias de diseño entre v1, v2, v3 y v4.
- **cbba sigue por delante** (0.483 global, 0.557 DET, 0.427 EST). Dec-MCTS es ahora segundo,
  por encima de cbaa (0.433) y greedy (0.406).

**Qué queda**
1. Decidir la versión definitiva (recomendación: v4 con γ=0.9999) y retirar el resto.
2. La brecha que queda frente a cbba ya no es un bug: son las cuestiones de diseño B1
   (crédito marginal) y C1 (¿aporta algo el canal de comunicación?) de `05_dudas.md`.
3. Enlazar con el objetivo 1.2: diseñar escenarios donde la comunicación tenga valor real.

---

## 2026-08-06 — Sesión 2: evaluación de las 4 versiones de Dec-MCTS

**Objetivo**: objetivo 1.1 — evaluar el estado real de los cuatro solvers Dec-MCTS y
localizar por qué no superan a CBAA/CBBA.

**Qué se hizo**
- Recompilado con `-O3` (`CMAKE_BUILD_TYPE=Release`). Antes se compilaba **sin optimizar**;
  el número de iteraciones es fijo, así que los *resultados* no cambian, solo el
  `computing_time` (~3 s/run en el escenario mayor). Los `computing_time` de logs antiguos
  no son comparables con los nuevos.
- Nueva tanda `logs/eval_versiones`: 21 escenarios (familias 1A,1B,1E,2A,2B,3C,4D ×
  {12t_2r, 16t_3r, 24t_5r}) × 9 solvers × 5 réplicas = **945 runs**.
- **Resultado 1**: el ranking depende del régimen. En determinista v4≈cbba (0.551 vs 0.559);
  en estocástico **las cuatro versiones pierden incluso contra greedy** (v2 0.370, v1 0.349,
  v4g 0.314, v3 0.307 vs greedy 0.382, cbaa 0.416, cbba 0.406).
- **Resultado 2**: **v4 NO es la mejor versión**. En estocástico es de las peores; su
  supremacía previa era un artefacto de haberse medido solo en `1E` (determinista).
- **Resultado 3 — defecto confirmado (D-BUG-01)**: en escenarios estocásticos, v3/v4/v4g
  **se retiran prematuramente**: dejan ~5 tareas con ventana abierta y ~62 % de batería sin
  usar, a t≈35 en vez de ≈45 (v1/v2 dejan 0.45-0.70 tareas; cbba 1.03). Intentan solo el
  43 % de las tareas frente al 61 % de cbba. Mecanismo identificado y documentado en
  `05_dudas.md` (incoherencia entre selección filtrada por factibilidad y decisión final
  sin filtrar).
- **Resultado 4 — defecto confirmado (D-BUG-02)**: v3 no tiene fase de rollout; tras expandir
  un `EXECUTE_TASK` **no** pone `inTree=false`, así que cada iteración expande una cadena
  entera de nodos y la evaluación de la hoja la produce la heurística "primera tarea no
  expandida por TaskID". Explica por qué v3 < v2.

**Qué queda / siguiente paso propuesto**
1. Arreglar D-BUG-01 (decisión final filtrada por factibilidad + poda de FINISH obsoleto) y
   re-medir sobre la misma tanda. Es el cambio de mayor retorno esperado.
2. Después, D-BUG-02 y las divergencias de fidelidad H-02/A4.
3. Ablación pendiente y nunca hecha: los 7 cambios de v4, uno a uno, **en régimen estocástico**.
4. Ablación diagnóstica: ¿aporta algo el canal de comunicación? (variante sin
   `knownDistributions`).

---

## 2026-08-06 — Sesión 1: puesta en contexto y creación del cuaderno

**Objetivo**: que el asistente entienda a fondo el problema y el código, y dejar montada la
infraestructura de contexto para sesiones futuras.

**Qué se hizo**
- Lectura completa de: caps. 2 y 3 de la memoria, `informacion.txt`, briefing del cap. 4,
  los 4 solvers Dec-MCTS, CBAA/CBBA, greedy/random, `simulator.cpp`, `main.cpp`, cabeceras de
  `include/tau/`, generadores de escenarios y scripts de análisis.
- Análisis cuantitativo de **todas** las tandas de logs (`logs/*`), separando las válidas
  (post-corrección del bug de caducidad) de las inválidas.
- **Hallazgo principal**: en `logs/eval_bundles` (208 escenarios), Dec-MCTS v4-g9999 gana en
  las 5 familias **deterministas** y pierde en las 8 **estocásticas**, sin excepción.
  Descartada la mortalidad de robots como causa (0% en ambos regímenes). Ni más cómputo
  (`eval_hc`) ni más rondas de comunicación (`eval_rounds`) cierran la brecha.
- **Hallazgo secundario**: divergencia de fidelidad entre el simulador real y la réplica
  interna de Dec-MCTS en el consumo de batería de una tarea fallida cuando
  `averageFailTime == 0` (real: demanda completa; réplica: 0). Se activa exactamente en las
  familias estocásticas. → hipótesis H-02.
- **Hallazgo secundario**: CBAA, CBBA y Dec-MCTS leen `obs.getKnownRobots()` (estado privado
  real de los compañeros), lo que contradice la observabilidad parcial del cap. 2. → duda D-01.
- Creados `CLAUDE.md` (raíz) y esta carpeta `.claude-notes/` con 5 documentos de contexto.

**Qué queda / siguiente paso propuesto**
1. Decidir con el usuario el orden de ataque (objetivo 1.1 vs 1.2).
2. Experimento más barato y con mejor retorno: corregir H-02 en `solver_DecMCTS_v4.hpp` y
   re-ejecutar las familias `2A/2B/2C/2D` para ver si se cierra la brecha en estocástico.
3. Resolver dudas D-01 a D-05 antes de escribir el cap. 4.

**Estado del repo al cerrar**: sin cambios en el código. Solo ficheros nuevos de documentación
(`CLAUDE.md`, `.claude-notes/`). `git status` previo: `.gitignore` modificado,
`dec-mcts.pdf` borrado, `informacion.txt` sin versionar.
