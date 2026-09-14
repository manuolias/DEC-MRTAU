# Briefing — Capítulo 5: Casos de estudio

> **Para la sesión de redacción.** Todo lo necesario para escribir
> `memoria/sections/05_casos_de_estudio.tex` sin volver a mirar el código ni los logs.
> Las cifras están verificadas contra `scenarios/catalogo_v2/` y `logs/eval_catalogo_v2`.
>
> ⚠️ **El capítulo describe el catálogo v2, NO el v1.** `scenarios/catalogo/` (288 esc.) y
> `logs/eval_catalogo` quedan **obsoletos** desde la sesión 6: se conservan en el repositorio
> pero no se describen aquí. La única mención legítima del v1 en toda la memoria es en el
> cap. 6, como contraste metodológico (ver §«Qué NO va en este capítulo»).

---

## 1 · Qué va y qué no va en este capítulo

**Va**: la descripción del conjunto de prueba y la justificación de su diseño. Es un capítulo
**descriptivo y metodológico**, sin un solo resultado comparativo entre solvers.

**No va**:
- **Ningún número de rendimiento.** Ni una recompensa, ni una Δ, ni un ranking. Todo eso es
  cap. 6. La única excepción admisible es el **pilotaje de calibración** (§3), donde el valor
  de CBBA se usa como *instrumento de medida de la dificultad*, no como resultado. Conviene
  decirlo explícitamente en el texto para que no se lea como un adelanto de resultados.
- La arquitectura del simulador, el formato YAML, la función de recompensa, los
  hiperparámetros de Dec-MCTS: todo eso está en el cap. 4 y se referencia con `\ref{}`.
- La sonda exploratoria de variantes (`logs/eval_probe`). Es material de cap. 6 o apéndice.
  Aquí basta una frase: los ejes del catálogo se eligieron a partir de un estudio previo de
  sensibilidad, con referencia adelantada.
- El catálogo v1 y la comparación v1↔v2. Es la **lección metodológica del cap. 6**.

**Referencias cruzadas disponibles** (etiquetas que ya existen):
`\ref{cap:marco_teorico}`, `\ref{def:instancia}`, `\ref{def:solucion}`, `\ref{eq:objetivo}`,
`\ref{eq:recompensa}`, `\ref{subsec:mrtau}`, `\ref{cap:materiales_y_metodos}`,
`\ref{sec:mat-simulador}`, `\ref{subsec:mat-cola}`, `\ref{sec:mat-recompensa}`,
`\ref{tab:decmcts-hiper}`, `\ref{subsec:mat-comunicacion}`.

**Estructura propuesta** (≈8-10 páginas, 5 tablas, 1-2 figuras):

| § | Contenido |
|---|---|
| 5.1 | Objetivo del conjunto de prueba y criterios de diseño |
| 5.2 | El sustrato común: geometría, robots, tareas, batería |
| 5.3 | El principio de iso-dificultad y su calibración |
| 5.4 | Los ejes de variación |
| 5.5 | Los cinco bloques |
| 5.6 | Protocolo experimental |
| 5.7 | Alcance y limitaciones del conjunto de prueba |

---

## 2 · §5.1 Objetivo y criterios de diseño

El conjunto debe permitir **atribuir** las diferencias de rendimiento a propiedades concretas
del problema, no solo constatarlas. De ahí cuatro criterios, que conviene enunciar como tales:

1. **Un factor por bloque.** Cada bloque varía un único eje y todo lo demás queda fijado en la
   configuración de referencia. El bloque A *es* la referencia, así que los demás se comparan
   contra él sin gastar escenarios en replicarla.
2. **Iso-dificultad.** El tamaño del equipo no puede estar confundido con la dificultad del
   problema (§5.3).
3. **Bloques del mismo tamaño y regímenes equilibrados.** 72 escenarios por bloque, mitad
   determinista y mitad estocástico, para que el agregado no lo domine un bloque grande.
4. **Pareado por construcción.** La semilla depende solo de $(R, n, \text{instancia})$ y no del
   régimen, la ventana ni la coalición: dos escenarios del mismo tamaño comparten geometría y
   sorteos de ventana. Las comparaciones son pareadas **tarea a tarea**, no solo escenario a
   escenario.

El criterio 4 merece un párrafo propio: es lo que permite afirmar que la diferencia entre la
ventana con espera y sin ella se debe **exclusivamente** a la espera, porque los plazos son
literalmente los mismos números en ambos ficheros.

---

## 3 · §5.2 El sustrato común (tabla 1)

Todos los escenarios comparten:

| Elemento | Valor |
|---|---|
| Grafo | completo, $n+1$ nodos; nodo 1 = estación de recarga en $(0,0)$; nodos $2..n{+}1$ = una tarea cada uno |
| Geometría | 4 racimos con centros en $(0,4)$, $(4,0)$, $(0,-4)$, $(-4,0)$; tareas equiespaciadas en la circunferencia unidad de cada centro |
| Distancias | euclídeas; tareas a distancia 3–5 de la estación, $\le 2$ entre tareas del mismo racimo |
| Robots | homogéneos; $\mathit{Cap}(i,j)$ verdadero para todo par; todos parten de la estación |
| Cinemática | velocidad 1 ⇒ **distancia = tiempo**; consumo al navegar 1 por unidad de tiempo |
| **Batería** | **capacidad e inicio 40** |
| Duración de tarea | $\mathcal N(10,\sigma)$ truncada en 0; el fracaso es **instantáneo** |
| Reintentos | 1 ⇒ un fracaso es definitivo |
| Horizonte | $T = 40$, cota superior del **inicio** de las ventanas (no un corte de la simulación) |
| Recompensa | `reward00` con $k_1=1$: fracción de tareas completadas (`\ref{sec:mat-recompensa}`) |

**Cifras agregadas del catálogo**: 360 escenarios, 10 896 tareas, 12 264 plazas de trabajador;
de 6 a 80 tareas por escenario (mediana 30); de 2 a 10 robots.

### El párrafo importante: por qué la batería 40

Es el cambio de fondo respecto a cualquier configuración anterior y hay que justificarlo:

- **Aritmética**: un ciclo típico es estación → racimo (4) + tarea (10) + desplazamiento
  interno (~1) + tarea (10) + ~1 + tarea (10) = 36 ≤ 40. Es decir, **unas tres tareas por
  carga**. Con $L=6$ cada robot necesita al menos dos ciclos; con $L=8$, tres.
- **Motivación**: el modelo del cap. 2 (`\ref{def:solucion}`) incluye la restricción de batería
  y las operaciones de recarga como parte de la definición de factibilidad. Con capacidad 100
  esa restricción **no se activa nunca** y el catálogo dejaría sin ejercitar una parte
  sustancial del modelo formal. Verificado en el pilotaje: con capacidad 60 los resultados son
  indistinguibles de los de capacidad 100.
- **Calibración**: se eligió 40 y no menos porque a 40 la restricción muerde sin volver el
  problema infactible, y porque `battery_rate_while_navigating` se mantiene en 1 —
  deliberadamente — para que el coste de navegar y el de ejecutar sean iguales por unidad de
  tiempo. (El *porqué* de esta última decisión viene de la sonda; aquí basta con presentarla
  como decisión de diseño y remitir al cap. 6.)

### La incertidumbre, con una advertencia de notación

Tres fuentes, todas gobernadas por el régimen:

| Régimen | $\rho_j$ | $\sigma$ duración | demanda en fracaso |
|---|---|---|---|
| `det` | 1.00 | 0 | 0 |
| `lev` | 0.90 | 3 | 3 |
| `est` | 0.75 | 10 | 10 |
| `fue` | 0.50 | 10 | 10 |

⚠️ **Trampa de notación que hay que evitar en el texto**: el segundo valor del campo `demand`
del YAML **no es una desviación típica**, es la batería que consume un intento **fallido**
(`src/scenario.cpp:63-64`). La tercera fuente de incertidumbre —el consumo— es estocástica
porque es **proporcional a la duración**, que sí lo es, más un coste fijo por fracaso. No
escribir «consumo $\mathcal N(10,10)$».

---

## 4 · §5.3 Iso-dificultad y su calibración (tabla 2)

**El principio**: la dificultad se controla con la **carga por robot**
$$L = \frac{\sum_j q_j}{R}$$
(plazas de trabajador por robot), manteniendo $T=40$ fijo. Así el número de robots deja de
estar confundido con la dificultad y puede estudiarse como factor aislado.

**Por qué hace falta decirlo**: si el horizonte crece con el número de tareas, «más robots»
acaba significando «menos carga por robot», y cualquier tendencia frente a $R$ mide holgura en
lugar de coordinación. Es un error real, cometido y detectado en este trabajo; **aquí basta con
enunciar el criterio**, y el relato del error va en el cap. 6.

**La calibración** (720 ejecuciones, solvers de subasta, 3 réplicas). Se busca el $L$ que
minimiza la dispersión del rendimiento entre tamaños de equipo. Dispersión de la recompensa de
CBBA entre $R \in \{2,5,10\}$ con capacidad 40:

| ventana / régimen | $L$=3 | $L$=4 | $L$=5 | **$L$=6** | $L$=8 |
|---|---|---|---|---|---|
| con espera · det | 0.200 | 0.350 | 0.320 | **0.078** | 0.075 |
| con espera · est | 0.178 | 0.125 | 0.227 | **0.144** | 0.125 |
| solo plazo · det | 0.200 | 0.358 | 0.320 | **0.133** | 0.125 |
| solo plazo · est | 0.189 | 0.333 | 0.067 | **0.044** | 0.037 |

⇒ **$L=6$ es el punto de operación.** Añadir que la calibración se rehízo con la nueva
restricción de batería y que el punto no se movió respecto al que ya se usaba: es un dato a
favor de la robustez del criterio.

**Explicación del fenómeno**, que da profundidad al capítulo: por debajo de la capacidad, los
equipos grandes se benefician del **multiplexado estadístico** — las fluctuaciones de cuántas
ventanas hay abiertas en cada instante se promedian mejor cuantos más robots hay. Solo cuando
la capacidad temporal es el cuello de botella el rendimiento se vuelve invariante de escala.

---

## 5 · §5.4 Los ejes de variación (tabla 3)

| Eje | Niveles | Referencia |
|---|---|---|
| $R$ robots | 2 … 10 | — (es el eje del bloque A) |
| $L$ carga | 3, 4, **6**, 8 | 6 |
| ventana | **E**, P, C, A | E |
| $q$ coalición | **q1**, q2, qm | q1 |
| $\rho$ régimen | **det**, lev, **est**, fue | det / est |

**Las cuatro ventanas.** Todas se derivan del mismo sorteo por tarea: $t_0 \sim \mathcal U\{0..40\}$
y $\varepsilon \sim \mathcal N(0,3)$.

| Código | $[t_j^s,\ t_j^l]$ | Apertura media | Anchura media | Qué exige |
|---|---|---|---|---|
| **E** estrecha | $[t_0,\ t_0+10+\varepsilon]$ | 20.0 | 10.0 | estar allí **en un instante concreto** |
| **P** solo plazo | $[0,\ t_0+10+\varepsilon]$ | 0.0 | 29.7 | llegar **antes de un plazo**, sin esperar |
| **C** escalonada | $[0,\ 40\frac{g+1}{4}+\varepsilon]$ | 0.0 | 24.7 | ordenar por **urgencia de racimo** |
| **A** ancha | $[0,\ 50+\varepsilon]$ | 0.0 | 49.9 | nada temporal: solo recorrido |

El par **E / P es el núcleo conceptual del capítulo** y merece explicarse despacio: comparten
*exactamente* los mismos plazos $t_j^l$ (los mismos números, por construcción de la semilla) y
difieren solo en si la tarea está disponible desde el instante 0 o hay que esperar a $t_0$.
Aísla, por tanto, el **tiempo de espera** como factor puro. Es un eje que la literatura de MRTA
con ventanas rara vez separa, y es una aportación metodológica menor pero real de este trabajo.
Merece un párrafo con la intuición: la espera introduce un recurso escaso nuevo —el tiempo
muerto— que no existe cuando la ventana solo impone un plazo.

**Coaliciones.** `q1` = 1 trabajador; `q2` = 2 para todas las tareas; `qm` = patrón cíclico
1-2-3. En `q2` y `qm` el número de tareas se **reajusta** ($n = LR/\bar q$) para mantener
$\sum_j q_j = LR$: mitad de tareas, cada una el doble de pesada. Hay que decirlo, porque cambia
el significado de la métrica —cada tarea de esos bloques vale el doble en la fracción
completada— y porque `qm` con $\bar q = 2$ tiene la misma carga que `q2` pero heterogénea.

---

## 6 · §5.5 Los cinco bloques (tabla 4)

| Bloque | Eje aislado | Niveles | Estructura | det | est | Esc. | $n$ |
|---|---|---|---|---|---|---|---|
| **A** · equipo *(control)* | $R$ | 2 … 10 | 9 $R$ × 2 reg × 4 inst | 36 | 36 | 72 | 12–60 |
| **B** · ventana | forma de $[t_j^s,t_j^l]$ | P, C, A | 3 win × 3 $R$ × 2 reg × 4 inst | 36 | 36 | 72 | 12–60 |
| **C** · carga | $L$ | 3, 4, 8 | 3 $L$ × 3 $R$ × 2 reg × 4 inst | 36 | 36 | 72 | 6–80 |
| **D** · coaliciones | $q_j$ | q2, qm | 2 $q$ × 3 $R$ × 2 reg × 6 inst | 36 | 36 | 72 | 9–30 |
| **E** · incertidumbre | $\rho_j$ | 1.0, 0.9, 0.75, 0.5 | 4 reg × 3 $R$ × 6 inst | 18 | 54 | 72 | 12–60 |
| | | | | **162** | **198** | **360** | |

Los bloques B, C, D y E usan $R \in \{2,5,10\}$ (D usa $\{3,6,10\}$, para que $n=3R$ sea
divisible entre los cuatro racimos) y se comparan contra las celdas correspondientes de A.

**La excepción del bloque E hay que declararla en el texto**, y la justificación es limpia: el
régimen determinista **es** $\rho=1.0$, o sea un extremo del propio gradiente que el bloque
barre; forzar la mitad exigiría replicar tres veces ese único punto. La regla mitad/mitad se
cumple en A, B, C y D, es decir en 288 de los 360 escenarios, y el catálogo completo queda en
162/198. Una frase y una nota al pie bastan; **no esconderlo**.

**Nomenclatura**: `cv2_{bloque}_r{R}_n{n}_bnd_{win}_{reg}_{q}_i{inst}.yaml`. Conviene incluirla
y descodificar un ejemplo, porque los nombres aparecerán en las figuras del cap. 6.

**Reproducibilidad del catálogo**: la semilla de cada escenario se deriva por CRC32 de
$(R, n, \text{instancia})$, así que `scripts/generate_catalog_v2.py` regenera los 360 ficheros
byte a byte.

---

## 7 · §5.6 Protocolo experimental — **resuelve la duda D-10**

Este apartado **no existe en ninguna otra parte de la memoria**: el cap. 4 no lo contiene. Es
obligatorio y va aquí, cerrando el capítulo, porque el cap. 6 debe entrar directo a resultados.

- **Ejecutor**: bucle escenarios × solvers × funciones de recompensa × réplicas, un fichero de
  registro por ejecución. Es **idempotente** (salta los registros ya completos), de modo que una
  tanda puede interrumpirse y reanudarse. Paralelización repartiendo escenarios entre procesos.
- **Réplicas: 3.** ⚠️ **Dato correcto para el v2** (5 400 registros = 360 × 5 × 3). Justificar la
  cifra, que es baja a primera vista: con 4–6 **instancias** distintas por celda, la varianza
  dominante es la de **entre instancias** (±0.15), un orden de magnitud mayor que la de entre
  réplicas (±0.016). Verificado: CBBA reproduce su valor con 3 réplicas (0.5709) frente a 10
  (0.5767). Aumentar réplicas afina la media de un escenario concreto, que no es la cantidad
  de interés.
- **Unidad de análisis**: el **escenario** (media de sus réplicas). Las comparaciones son
  **pareadas por escenario** y el error estándar se calcula **entre escenarios**, nunca entre
  réplicas. Enunciarlo explícitamente: es lo que da validez a las Δ del cap. 6.
- **Reproducibilidad de las ejecuciones**: el muestreo usa `std::random_device` sin semilla
  configurable, así que **una ejecución concreta no es reproducible**; lo que se reproduce es la
  media sobre réplicas. Es una limitación real y hay que declararla (duda **D-05**).
- **Coste**: la tanda completa son ~2.2 h en serie; ~25 min con 6 procesos. ⚠️ En paralelo la
  métrica `computing_time` **deja de ser comparable** por contención de CPU; si el cap. 6
  presenta el frente coste-calidad, tiene que salir de una tanda en serie. Decirlo aquí.
- **Solvers evaluados**: `random`, `greedy`, `cbaa`, `cbba` y `dec-mcts-v4-g9999`. Recordar
  (una frase + `\ref{}`) que v1–v3 se describen en el cap. 4 como historia del desarrollo pero
  no entran en la evaluación.

---

## 8 · §5.7 Alcance y limitaciones — el apartado que da credibilidad

Cuatro puntos, todos comprobados. Ninguno invalida nada, y escribirlos protege frente a
preguntas de tribunal:

1. **Heterogeneidad de robots no ejercitada.** $\mathit{Cap}(i,j)$ está en el modelo formal
   (`\ref{def:instancia}`) pero en el catálogo todos los robots son capaces de toda tarea. Es
   una simplificación deliberada: el trabajo estudia la **coordinación**, no la asignación por
   competencias. Se propone como trabajo futuro (cap. 7).
2. **Geometría única.** Los 360 escenarios usan la disposición en cuatro racimos. Se probaron
   geometría uniforme y asimétrica en estudios previos y **no discriminaban** entre algoritmos;
   se descartaron para no gastar celdas. (Cifras en el cap. 6 si se quiere respaldar.)
3. **Una sola estación de recarga**, en el origen y coincidente con la posición inicial de todos
   los robots. Con batería escasa esto convierte la estación en un punto de congestión, que es
   parte del problema, pero limita la generalidad.
4. **La mortalidad de robots es un modo de fallo distinto de la mala asignación.** Con capacidad
   40, `random` y `greedy` pierden en torno a 0.9 y 0.8 robots por ejecución (en el 42–45 % de
   las ejecuciones), mientras que `cbaa`, `cbba` y `dec-mcts` **no pierden ninguno en las 5 400
   ejecuciones**. ⚠️ Al comparar con los baselines en el cap. 6 hay que decir que parte de su
   desventaja es esto y no calidad de asignación. **Anunciarlo aquí y recordarlo allí.**

---

## 9 · Figuras sugeridas

No existe ninguna todavía; hay que crearlas (o decidir prescindir de ellas).

1. **Geometría del escenario** — croquis de los 4 racimos, la estación y un ejemplo de
   trayectoria con recarga. Es la figura que hace entender el catálogo de un vistazo.
   Probablemente merezca la pena hacerla a mano (TikZ o el mismo flujo con que se hizo
   `figures/04_materiales_y_metodos/arquitectura_simulador.pdf`).
2. **Las cuatro ventanas** — diagrama temporal con las cuatro tipologías para una misma tarea,
   mostrando que E y P comparten el cierre. Vende el eje conceptual del capítulo mejor que
   cualquier párrafo.
3. *(Opcional)* Mapa de calor de la estructura del catálogo (bloques × ejes).

---

## 10 · Decisiones abiertas antes de escribir

1. **¿El pilotaje de calibración entra en el capítulo?** Recomendación: **sí**, con la tabla,
   dejando claro que CBBA se usa como instrumento de medida y no como resultado. Justifica
   $L=6$ y la capacidad 40, que si no quedan como números arbitrarios.
2. **¿Cuánto se adelanta de la sonda de variantes?** Recomendación: una frase y una referencia
   adelantada al cap. 6. Si se cuenta aquí, el capítulo pierde su carácter descriptivo.
3. **Nombre del catálogo en la memoria.** «v2» es jerga interna del repositorio. Recomendación:
   llamarlo simplemente **«el catálogo»** y no mencionar que hubo uno previo hasta el cap. 6.
4. **D-03 sigue abierta** (cómo citar el paper del tutor). No bloquea este capítulo, pero sí el 7.

---

## 11 · Recordatorio de estilo (de `CLAUDE.md`)

LaTeX y español, registro académico formal y riguroso; reutilizar los nombres de variables ya
definidos en el cap. 2 ($\mathcal R$, $\mathcal T$, $q_j$, $\rho_j$, $[t_j^s, t_j^l]$,
$\mathit{Cap}(i,j)$, $L$, $T$); `\ref{}` a las secciones previas; citas adecuadas. El usuario no
compila LaTeX localmente: importa el contenido, no que compile. **Sin ablaciones.**
