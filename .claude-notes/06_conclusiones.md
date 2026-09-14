# 06 — Conclusiones del trabajo (material para los caps. 6 y 7)

> **Origen**: conclusión dictada por el usuario al cerrar la sesión 4 (2026-08-18), tras completar
> los objetivos 1.1, 1.2 y 1.3. Este documento la estructura y la respalda con las cifras medidas,
> para poder convertirla en prosa LaTeX cuando toque. **Es la tesis del trabajo**: si algo de lo
> que sigue se reformula, hay que mantener estas cinco ideas.
>
> 🔴🔴 **LEER PRIMERO EL ADDENDUM 2 DEL FINAL (sesión 8) — es la fuente de verdad.**
> Este cuerpo se midió sobre el **catálogo v1** y el addendum 1 sobre el **v2**; ambos están
> obsoletos. El vigente es el **v3**. Las cinco ideas siguen en pie, pero **la idea 1 hay que
> reformularla** (la pendiente de Dec-MCTS bajo incertidumbre ya no es cero, es cinco veces
> menor que la de CBBA). No citar ninguna cifra de aquí ni del addendum 1 sin comprobarla en el
> addendum 2.

---

## La tesis, en una frase

Dec-MCTS no supera a CBBA, pero **no es un algoritmo fallido**: bate con holgura a los baselines
y a CBAA, y su techo no viene de la implementación sino de una **limitación real del problema** —
bajo incertidumbre, comunicar intenciones o planes individuales deja de ser informativo.

---

## Idea 1 · Dec-MCTS no es un algoritmo fallido

**Lo que hay que decir**: queda segundo de cinco, muy por encima de los baselines y de CBAA. La
distancia con CBBA es pequeña; la que le saca a los métodos sin anticipación, grande.

**Evidencia** (`logs/eval_catalogo`, 288 escenarios × 10 réplicas):

| Solver | global | vs Dec-MCTS |
|---|---|---|
| CBBA | 0.6031 | +0.036 |
| **Dec-MCTS** | **0.5676** | — |
| CBAA | 0.4730 | −0.095 |
| Greedy | 0.3894 | −0.178 |
| Random | 0.3314 | −0.236 |

Y sobre todo: **Dec-MCTS es, junto a CBBA, el único método que convierte robots adicionales en
rendimiento** (bloque A1, a iso-dificultad, régimen determinista):

| Solver | pendiente por robot | t |
|---|---|---|
| CBBA | +0.0163 | 6.4 |
| **Dec-MCTS** | **+0.0131** | **5.9** |
| CBAA | +0.0059 | 1.0 |
| Greedy | +0.0034 | 1.1 |

⇒ **Su coordinación funciona.** CBAA, que es una subasta de tarea única, es plano: añadirle
robots no le sirve de nada (y bajo incertidumbre su pendiente llega a ser negativa). Esto es un
resultado positivo del trabajo y conviene no enterrarlo bajo el resultado negativo.

Además hay **familias donde Dec-MCTS es el mejor de los cinco** y CBBA deja de serlo:

| Familia | Dec-MCTS | CBBA | Δ |
|---|---|---|---|
| ventana escalonada · determinista | 0.573 | 0.546 | **+0.026 ± 0.009** (gana 7/9) |
| ventana ancha · determinista | 0.816 | 0.791 | **+0.025 ± 0.015** |
| holgura con equipo mediano (5 robots, carga 3) | 0.956 | 0.889 | **+0.067** |

---

## Idea 2 · La limitación es del planteamiento, no de la implementación

**Lo que hay que decir**: la desventaja bajo incertidumbre está **mecánicamente identificada** y
**cuatro intervenciones independientes sobre el código no la mueven**.

**El mecanismo** (§5 del notebook): la recompensa se descompone en tasa de intento × tasa de
éxito. La tasa de **éxito es idéntica** a la de CBBA (+0.24 p.p. en los escenarios con
incertidumbre) ⇒ Dec-MCTS **no elige peor**. Toda la brecha está en la **tasa de intento**
(r = 0.810 con la Δ de recompensa, frente a 0.381 de la tasa de éxito).

El déficit de intento **solo aparece bajo incertidumbre y crece con el equipo**: −7.5 p.p. con
2 robots → −18.7 p.p. con 10. En determinista se queda plano en ~−1 p.p.

**Interpretación — exceso de deconflicción**: Dec-MCTS cede las tareas que cree cubiertas por un
vecino (`blockingProb`). Bajo incertidumbre el plan comunicado del vecino deja de cumplirse en
cuanto una tarea falla o se alarga; la creencia es errónea, y cuantos más vecinos hay sobre los
que condicionar, más tareas se ceden por error y acaban sin que las haga nadie. Explica a la vez
el signo, la dependencia del régimen y el crecimiento lineal con el tamaño del equipo.

**Las cuatro refutaciones**:

| Intervención | n | efecto | ¿descarta? |
|---|---|---|---|
| ×40 presupuesto de cómputo (`eval_hc`) | 12 | +0.012 ± 0.031 | solo efectos grandes |
| ×30 rondas de comunicación (`eval_rounds`) | 12 | −0.000 ± 0.017 | solo efectos grandes |
| **B4** · `blockingProb` probabilística (`eval_b4`) | 126 | **+0.002 ± 0.003** | **sí** |
| **D-08** · plan comunicado más profundo (`eval_d8`) | 126 | **+0.002 ± 0.003** | **sí** |
| *contraste positivo*: quitar el canal de comunicación | 16 | **+0.066 ± 0.019** | — detecta efectos |

⚠️ **Ser honesto con la potencia**: las dos primeras heredan tandas antiguas de 12 escenarios y
solo descartan efectos grandes. El peso del argumento lo llevan B4, D-08 y el mecanismo.

**El cierre lógico del argumento** (ver también la idea 5): el paper del tutor demuestra que
**MCTS centralizado resuelve bien este mismo problema con la misma incertidumbre**. Si el método
funciona centralizado, falla distribuido, y las intervenciones sobre el código no lo arreglan,
entonces lo que falla es **la descentralización con comunicación limitada** — no la búsqueda ni
el código. Esta es la forma más sólida de enunciar la idea 2.

**D-08 es el argumento empírico más fuerte** y merece contarse entero: al instrumentar el solver se
descubrió que el **69 % de los planes comunicados se truncaba** en un nodo `FINISH` (longitud
media 1.58), de modo que Dec-MCTS comunicaba de facto una distribución sobre *su próxima acción*
— es decir, **lo mismo que comunica CBAA** — en lugar de sobre *secuencias*. Era un defecto real,
se corrigió (longitud 1.58 → 2.50, cortes por `FINISH` 68.7 % → 1.8 %) y **el rendimiento no
cambió**. Que duplicar la profundidad del plan comunicado no cambie nada demuestra que, bajo
incertidumbre, **un plan más largo no es más informativo: solo se equivoca en más sitios**.

---

## Idea 3 · Esto quizá explica la escasez de literatura sobre MCTS descentralizado

**Lo que hay que decir**: apenas hay artículos sobre variantes descentralizadas de MCTS, y las
pocas implementaciones encontradas son de dominios muy específicos. El resultado de este trabajo
ofrece una explicación plausible: el enfoque tiene un techo estructural en cuanto la
incertidumbre rompe la fiabilidad de los planes comunicados.

✅ **Decisión del usuario (sesión 4): se plasma como TEORÍA, no como resultado.** Es una
conjetura interpretativa y así debe redactarse: «este resultado **sugiere** una posible
explicación de…», «resulta **consistente** con la escasez de…». Apoyarla en el estado del arte
del cap. 3, nunca en los datos propios. Un tribunal puede objetar con razón que la ausencia de
publicaciones tiene otras causas posibles (sesgo de publicación, madurez del área, preferencia
industrial por métodos de mercado); enunciada como hipótesis es defendible, como hecho no.
Redactarla en un párrafo claramente marcado como especulativo, y no construir nada encima.

---

## Idea 4 · CBBA es un hallazgo, no un consuelo

**Lo que hay que decir**: el trabajo ha identificado un algoritmo que resuelve el problema muy
bien. No hay que menospreciar ese resultado por el hecho de que no fuera el esperado.

**Evidencia**: CBBA es el mejor en los cuatro regímenes de incertidumbre, gana en 195 de 288
escenarios, mantiene su escalado con el equipo incluso bajo incertidumbre (+0.0141 por robot,
t = 9.0, donde Dec-MCTS cae a +0.001) y **cuesta ~0.01 s por ejecución frente a los ~12.5 s de
Dec-MCTS** — tres órdenes de magnitud menos.

La razón estructural de su robustez, que conviene explicitar en el cap. 6: **una puja compromete
un precio, no una trayectoria.** Por eso no se degrada cuando el plan del vecino se desvía,
mientras que condicionar rollouts sobre planes muestreados sí.

---

## Idea 5 · El coste de distribuir es mayor de lo esperado, y la culpa es del canal

**Lo que hay que decir**: era esperable perder rendimiento al pasar de un modelo centralizado a
uno distribuido, pero la pérdida observada es **mayor de lo que cabría esperar**, y el
responsable es la **comunicación limitada**. No basta con comunicar intenciones o planes
individuales. Para recuperar rendimiento hay que diseñar una comunicación más sofisticada, capaz
de compartir información sobre **coaliciones**.

**Evidencia directa de que el canal es el cuello de botella** (ablación C1): el canal de
comunicación aporta **+0.066 ± 0.019 en régimen determinista** y **−0.005 (nulo) bajo
incertidumbre**. El valor de un plan comunicado es exactamente el valor de su predictibilidad.

**Evidencia que respalda específicamente lo de las coaliciones** — y es la que mejor sostiene
esta línea: **las coaliciones son el peor terreno de Dec-MCTS**, y ya lo son **en régimen
determinista**, donde el resto de sus problemas desaparecen:

| Familia | Dec-MCTS | CBBA | Δ |
|---|---|---|---|
| coalición q=2 · determinista | 0.436 | 0.520 | **−0.084 ± 0.025** |
| coalición mixta · determinista | 0.544 | 0.600 | −0.056 ± 0.015 |
| coalición q=2 · estocástico | 0.319 | 0.428 | −0.109 ± 0.015 |

⇒ El mecanismo encaja: `blockingProb` decide de forma **binaria** si los vecinos cubren una tarea
(«¿hay al menos `requiredWorkers` vecinos que la tengan en su bundle?»). Con q ≥ 2 esa decisión
se equivoca con el doble de facilidad, y **el canal no transporta ninguna noción de coalición
parcial**: un robot no puede decir «voy a esta tarea *si* alguien más viene», que es justo la
información que haría falta. La intuición del usuario está respaldada por el dato.

### El argumento centralizado → distribuido (D-09, resuelta)

**No hace falta medir el centralizado.** El punto de partida ya está establecido: el paper del
tutor implementa **MCTS centralizado sobre exactamente el mismo problema y las mismas condiciones
de incertidumbre**, y funciona bien. La cadena de razonamiento del cap. 7 es:

1. **MCTS sí resuelve bien este problema** — demostrado, en régimen centralizado y con la misma
   incertidumbre (paper del tutor).
2. Este TFM implementa el **mismo método** en régimen distribuido y **pierde frente a CBBA**.
3. Las **cuatro refutaciones** descartan que la pérdida venga de la implementación.
4. ⇒ La pérdida es atribuible al **paso a distribuido con comunicación limitada**, no al método
   de búsqueda ni al código.

Es un argumento **más limpio que una medición propia**: se apoya en un resultado ya establecido
en vez de en un experimento nuevo, y aísla exactamente la variable de interés. Y encaja con la
ablación C1, que ya localiza el problema en el canal.

⚠️ **Dos límites que respetar al redactar**:
- **Nada de cifras concretas de pérdida.** Los escenarios del paper del tutor no son los de este
  catálogo. La afirmación se queda en cualitativa: «la reducción observada es mayor de lo que
  cabría esperar del mero paso a un esquema distribuido».
- **Depende de D-03** (cómo citar un paper no publicado). Eso convierte la decisión de citación
  en algo **estructural** para el cap. 7, no en un detalle de formato. Resolverla antes.

---

## Reparto sugerido en la memoria

| Capítulo | Qué llevar de aquí |
|---|---|
| **6 · Experimentación** | Ideas 1, 2 y 4 con todas sus tablas. El notebook `analisis/evaluacion_final.ipynb` (rehecho sobre el **v3** en la sesión 9) y sus **14 figuras** `figuras/p1_*`, `p2_*`, `p3_*` son el material directo. Reportar SIEMPRE por régimen: el ranking cambia. |
| **7 · Conclusiones** | Idea 1 (no es fallido), idea 4 (CBBA es un hallazgo) e idea 2 (limitación estructural). |
| **7 · Trabajo futuro** | Idea 5 completa: comunicación consciente de coaliciones. Es la línea más original y está respaldada por el dato de que las coaliciones fallan ya en determinista. Añadir las mejoras aparcadas B1 (crédito marginal), B2 y B5 de `05_dudas.md`. |
| **3 · Estado del arte** | Idea 3, como conjetura, enlazada con la revisión ya escrita. ⚠️ El cap. 3 está terminado: **no tocarlo sin preguntar**. |

## Lección metodológica transversal (va en el cap. 6)

**El diseño del conjunto de prueba determinaba la conclusión, en las dos direcciones.** El
catálogo antiguo ligaba el horizonte temporal al nº de tareas, así que «más robots» significaba
«menos carga por robot»: eso producía una ventaja aparente de Dec-MCTS que crecía con el equipo
y que resultó ser un **artefacto**. Solo al calibrar a **iso-dificultad** (carga por robot
constante, horizonte fijo) se ve que esa tendencia no existe. Es un aviso metodológico de valor
general y conviene contarlo, incluyendo el error propio: da credibilidad, no la quita.

---

# 🔴 ADDENDUM — qué cambia con el catálogo v2 (sesión 6, 2026-08-31)

Todo lo anterior se midió sobre el **catálogo v1** (288 esc. × 10 rép., batería 100), hoy
obsoleto. La tanda de referencia es **`logs/eval_catalogo_v2`** (360 esc. × 3 rép., batería 40).
Las cinco ideas **siguen siendo válidas como estructura del argumento**, pero hay que actualizar
cifras y matizar una. Este addendum es la fuente de verdad; el cuerpo de arriba, el histórico.

## Lo que la lección metodológica gana

Es lo primero que cambia, y a mejor. Ahora hay **dos catálogos, ambos defendibles, que dan
conclusiones opuestas sobre el mismo código**:

| | v1 | v2 |
|---|---|---|
| Δ (dec − cbba) global | **−0.0355 ± 0.0034** | **+0.0047 ± 0.0044** |
| ¿quién gana el agregado? | cbba | dec-mcts (pero **t = 1.06 ⇒ empate**) |
| recuento de escenarios | 67 / 26 / 195 | 154 / 33 / 173 (**pierde**) |

⇒ La lección deja de ser «cometí un error de diseño y lo corregí» y pasa a ser **«el agregado
de un catálogo de MRTA no es un hecho sobre el algoritmo, es una propiedad de la composición
del catálogo»**. Es un resultado metodológico de valor general, y mucho más fuerte.
⚠️ **Obliga a no liderar nunca con el agregado en el cap. 6.** Y obliga a declarar la
**auditoría de composición** del v2: sus bloques B y C tienen 2 de 3 niveles en terreno
favorable a Dec-MCTS, mientras A, D y E le son adversos.

## Idea 1 — «no es un algoritmo fallido»: **REFORZADA**

Ya no queda segundo de cinco en el agregado: **empata con CBBA y le gana en determinista**
(0.586 vs 0.564, Δ +0.022 ± 0.006). Y el argumento del escalado es ahora más nítido que nunca
(bloque A, pendientes propias frente a $R$):

| | determinista | estocástico |
|---|---|---|
| cbba | +0.0165 (t=6.6) | **+0.0150 (t=5.7)** |
| **dec-mcts** | **+0.0123 (t=7.7)** | **−0.0004 (t=−0.2)** |

Y ahora hay **familias enteras donde es el mejor de los cinco**, no solo celdas sueltas:
ventana escalonada (+0.076 det, gana **12/12**), ventana sin espera (+0.033 det / +0.025 est),
carga ligera (+0.208 con R=2, L=3), coalición mixta determinista (+0.024).

## Idea 2 — «la limitación es del planteamiento»: **MATIZADA** ⚠️ hay que reescribir un párrafo

La estructura del argumento aguanta (cuatro refutaciones + mecanismo + cierre lógico con el
paper del tutor), y el mecanismo se confirma: en el v2, bajo incertidumbre la brecha sigue
viviendo en la **tasa de intento** (−0.072 en est, −0.181 en fue).

**Pero ya NO es cierto que «la tasa de éxito sea idéntica».** Con batería escasa, la de
Dec-MCTS es **superior**: +0.029 en est, +0.045 en la ventana de control. Motivo: CBBA
comprueba la batería con la demanda **esperada** (`solver_CBBA.hpp:106`), así que con
duraciones $\mathcal N(10,10)$ empieza tareas que no puede terminar y el robot se queda a cero
a mitad ⇒ tarea fallida (`simulator.cpp:470-475`). Dec-MCTS muestrea duraciones y deja margen.

⇒ **Reformulación correcta**: «la brecha bajo incertidumbre no viene de elegir peor las tareas
—de hecho Dec-MCTS las termina mejor cuando la batería aprieta— sino de **intentar menos**».
Es una versión más fuerte, no más débil, del argumento.

**Cifra nueva y valiosa** (comparación *apples-to-apples*: bloque A1 del v1 y bloque A del v2
son la misma configuración, solo cambia la batería):

| régimen | v1 (bat. 100) | v2 (bat. 40) | mejora |
|---|---|---|---|
| det | −0.006 ± 0.004 | +0.001 ± 0.005 | +0.008 |
| **est** | **−0.081 ± 0.006** | **−0.018 ± 0.011** | **+0.064** |

**La restricción de batería cierra dos tercios de la brecha estocástica.** Es el hallazgo
empírico nuevo más importante de la sesión 6 y merece sitio propio en el cap. 6.

## Idea 3 — «explica la escasez de literatura»: **SIN CAMBIOS**

Sigue siendo conjetura interpretativa y sigue redactándose como tal.

## Idea 4 — «CBBA es un hallazgo»: **MATIZADA, y aparece un segundo hallazgo**

CBBA ya no es el mejor en los cuatro regímenes: **pierde en determinista y en leve**. Sigue
siendo el mejor bajo incertidumbre real (est 0.417, fue 0.318) y sigue costando ~1000× menos.
La razón estructural de su robustez —*una puja compromete un precio, no una trayectoria*— sigue
siendo la frase correcta.

**Hallazgo nuevo que hay que añadir**: en el bloque de **coaliciones, CBAA bate a CBBA**
(0.424 vs 0.402) y es el mejor de los cinco. Es el único bloque donde la subasta de tarea única
gana a la de paquete, y contradice la intuición habitual. Merece un párrafo.

## Idea 5 — «el coste de distribuir y la culpa del canal»: **CONFIRMADA y afinada**

Las coaliciones siguen siendo terreno malo (`q2`: −0.047 det / −0.034 est, hasta −0.096 con
R=10) y **siguen fallando ya en determinista**, que es lo que sostiene la línea de trabajo
futuro sobre comunicación consciente de coaliciones. Dos precisiones nuevas:

1. **La coalición mixta se comporta al revés que la uniforme**: `qm` da **+0.024 en
   determinista**. O sea, el problema no es «coordinar coaliciones» en general, sino la
   **decisión binaria de `blockingProb` cuando todas las tareas exigen lo mismo**. Afina el
   diagnóstico y hace la propuesta de trabajo futuro más concreta.
2. **La incertidumbre no hace daño por sí sola: hace daño multiplicada por los vecinos.**
   Con 2 robots Dec-MCTS **gana en los cuatro niveles de $\rho$** (+0.032, +0.097, +0.037,
   +0.032); con 5 y 10 pierde en cuanto $\rho<1$ (hasta −0.092). Es la confirmación más directa
   del mecanismo de deconflicción excesiva (H-07) que se ha obtenido.

## Idea 6 (nueva) — el sobrecoste de desplazamiento

Transversal a todo y no estaba en las cinco ideas originales: **Dec-MCTS recorre entre un 6 % y
un 81 % más de distancia que CBBA**, en todas las variantes medidas (v2: 113.5 vs 92.7, +22 %).
Es inocuo mientras navegar sea barato y **letal** cuando el desplazamiento es el recurso que
ata: con navegación cara *y* batería escasa pierde 12 de 12 escenarios (−0.055). Da una
caracterización operativa útil para el cap. 7: **Dec-MCTS compra cobertura temporal pagando
desplazamiento**, y eso solo renta cuando el desplazamiento no es el cuello de botella.

---

# 🔴🔴 ADDENDUM 2 — catálogo v3, el VIGENTE (sesión 8, 2026-09-10)

El v2 queda obsoleto. La tanda de referencia es **`logs/eval_catalogo_v3`** (360 esc. × 3 rép.).
Mismo diseño, mismas semillas, solo cambia la parametrización de los regímenes, por decisión del
usuario:

1. **El coste de un intento FALLIDO es 10 fijo** en todos los regímenes (antes 0/3/10/10).
   Razón: no es una fuente de incertidumbre, así que no tenía por qué variar con ella.
   ⚠️ Con **éxito** el consumo no es 10 fijo, sino proporcional a la duración real (tasa 1,
   esperanza 10): lo que coincide entre desenlaces es la **esperanza**, no el valor.
2. **σ de la duración: 0 / 1 / 3 / 5** (antes 0/3/10/10) ⇒ CV = 0 / 0.1 / 0.3 / 0.5. Con μ=10, el
   v2 llegaba a CV = 1.0 en `est` y `fue`, un régimen degenerado.

**Este addendum es la fuente de verdad. Los dos anteriores son histórico.**

## Lo que NO cambia (que es casi todo)

Comparación pareada v2↔v3 (`scripts/compare_catalogos.py`): el determinista no se mueve
(Δ +0.0219 → +0.0201), el régimen fuerte tampoco (−0.0500 → −0.0497), el recuento global sigue
siendo un empate a cara o cruz (154/33/173 → **158/42/160**) y **el mejor solver de cada bloque
es idéntico**: A cbba, B dec-mcts, C dec-mcts, D **cbaa**, E cbba. La estructura completa del
argumento —las cinco ideas— sobrevive sin tocar una coma.

## Lo que SÍ cambia

| | v2 | v3 |
|---|---|---|
| Δ global (dec−cbba) | +0.0047 ± 0.0044 | +0.0086 ± 0.0040 |
| **Δ en `est`** | **−0.0079 ± 0.0071** | **+0.0057 ± 0.0064** ← cambia de signo |
| mejor solver en `est` | cbba | **dec-mcts** |

Todos los solvers suben, y **todo el movimiento vive en `est`** (dec +0.035, cbaa +0.025,
cbba +0.022, greedy +0.015): menos dispersión ⇒ más predecible ⇒ todos rinden mejor, y quien más
gana es el que planifica sobre el horizonte. Único bloque con movimiento significativo: **C·est**
(+0.003 → +0.047). Niveles globales: rand 0.271, greedy 0.354, cbaa 0.437, cbba 0.494,
**dec-mcts 0.502**.

## ⚠️ La idea 1 hay que REFORMULARLA (es lo único que se resiente)

La frase más limpia de todo el trabajo era «bajo incertidumbre Dec-MCTS **no convierte robots en
rendimiento, en absoluto**». Con el v3 **ya no se puede escribir «en absoluto»**:

| bloque A, régimen est | v2 | v3 |
|---|---|---|
| cbba | +0.0150 (t=5.7) | **+0.0195 (t=6.6)** |
| dec-mcts | −0.0004 (t=−0.2) | **+0.0040 (t=1.5)** |

**Formulación correcta para el cap. 6**: «CBBA convierte cada robot adicional en rendimiento
también bajo incertidumbre (+0.0195 por robot, t = 6.6), mientras que la pendiente de Dec-MCTS es
unas cinco veces menor y **no llega a distinguirse de cero** (+0.0040, t = 1.5)». Y el dato que
sigue siendo demoledor y no se ha movido: **la pendiente de la propia Δ es −0.0155 por robot
(t = −4.7)** — la desventaja crece linealmente con el tamaño del equipo, que es exactamente el
mecanismo H-07. En determinista la pendiente de la Δ es −0.0043 (t = −2.0), tres veces menor.

## ⚠️ Mortalidad de robots: MÁS grave que en el v2, y ahora también en determinista

| | v2 det | v3 det | v2 est | v3 est |
|---|---|---|---|---|
| random | 0.00 (0 %) | **1.85 (65 %)** | 1.78 | 2.12 |
| greedy | 0.00 (0 %) | **1.27 (50 %)** | 1.50 | 1.64 |
| cbaa · cbba · dec-mcts | 0 | 0 | 0 | 0 |

**Mecanismo, verificado en código y logs**: aunque ρ=1, si a un robot no le da la batería para
arrancar una tarea, `simulator.cpp:469-473` fuerza `success = false` y trunca la ejecución; luego
`endTask` le cobra `averageFailDemand` **entera** porque `averageFailTime == 0`
(`simulator.cpp:494-498`). Con el v2 eran 0 y el robot sobrevivía; con el v3 son 10 y muere.
Ejemplo real: batería 3.16 → se le cobran 10 → −6.84.

⇒ **Los tres solvers coordinados no pierden un solo robot** porque comprueban la demanda de
ejecución, no solo la del desplazamiento. Es un resultado a favor de ellos, pero **parte de la
desventaja de los baselines es mortalidad y no calidad de asignación**: hay que declararlo
explícitamente en el cap. 6 y reportar la mortalidad en tabla aparte.

## Lo que se mantiene del addendum del v2

- La **tasa de éxito de Dec-MCTS sigue por encima de la de CBBA** en est (0.754 vs 0.719), así
  que la reformulación de la idea 2 sigue siendo válida: la brecha no viene de elegir peor, sino
  de **intentar menos** (17.7 vs 19.3 tareas intentadas en est).
- **CBAA sigue ganando el bloque de coaliciones** (0.431 vs 0.407 de cbba).
- **La coalición mixta va al revés que la uniforme**: `qm` +0.023 en det, `q2` −0.052.
- El **sobrecoste de desplazamiento** persiste: 121.7 vs 94.9 en est (+28 %).
- La **lección metodológica** (el agregado es una propiedad de la composición del catálogo, no un
  hecho sobre el algoritmo) se mantiene intacta y ahora tiene un tercer punto de apoyo.

## 📌 Notas de redacción para el capítulo 6 (sesión 8)

- **Abrir el capítulo con la recapitulación del catálogo.** El usuario la eliminó del cap. 5
  precisamente para eso. Debe recoger: 360 escenarios, 10 896 tareas, 12 264 plazas, equipos de
  2 a 10 robots, escenarios de 6 a 80 tareas, cinco bloques de 72; los cinco algoritmos, las
  **3 réplicas** con su justificación (con varias instancias por celda, la varianza dominante
  es la de *entre instancias*, no la de entre réplicas) y el total
  $360\times5\times3 = 5\,400$ ejecuciones; y la **unidad de análisis = el escenario**
  (media de sus réplicas), con las Δ pareadas y el error estándar calculado *entre escenarios*.
  ⚠️ Nada de esto aparece hoy en ninguna parte de la memoria.
- **La ventana escalonada es ~5 unidades más apretada** que la de solo plazo (plazo medio 24.67
  frente a 29.67; la estrecha 29.97, la ancha 49.87). Es deliberado y correcto, pero conviene
  declararlo al presentar que la escalonada es el mejor terreno de Dec-MCTS (12/12 en det).
- **Mortalidad de robots en tabla aparte**, y recordando que con el v3 aparece también en
  determinista (D-16).
- El **protocolo experimental** completo (idempotencia del ejecutor, paralelización, coste de
  la tanda, `computing_time` no comparable en paralelo, irreproducibilidad por
  `std::random_device`) no está escrito en ningún sitio: decidir si entra aquí (D-10, D-05).
- **Composición de bloques** (D-14) y **las cuatro refutaciones medidas sobre el v1** (D-13).
