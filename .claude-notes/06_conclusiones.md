# 06 — La tesis del trabajo (material para el capítulo 7)

> **Reescrito el 24-09-2026 (sesión 13)** sobre la tanda reejecutada del 17-09-2026, que sustituye
> por completo a la versión anterior (cinco ideas + dos addendums, escrita cuando el simulador
> tenía los dos defectos de batería y Dec-MCTS empataba con CBBA).
>
> 📌 **Este fichero NO repite las cifras del capítulo 6.** Su función es fijar **qué se defiende**
> y **con qué peso**; la evidencia vive en dos sitios ya verificados, a los que se remite:
> el **capítulo 6, cerrado por el usuario**, y «REEJECUCIÓN 17-09-2026» en `03_experimentos.md`.
> Solo se citan aquí las cifras de titular. Cualquier otra se toma del capítulo 6, nunca de aquí.
>
> ✅ **Decisión del usuario (24-09-2026)**: la experimentación está **cerrada**. Todas las
> conclusiones se obtienen de lo ya medido; no se lanza ninguna tanda nueva (salida **(c)** de
> D-13). Lo que no pueda sostenerse con la tanda vigente, o se declara como interpretación, o no
> se escribe.

---

## ✅ ESTADO (sesión 14, 29-09-2026): el capítulo 7 YA ESTÁ ESCRITO Y CERRADO

Este fichero cumplió su función como guion. Lo que el usuario **conservó** en el capítulo:
las ideas 1 (la hipótesis se cumple), 2 (robustez, sin las cifras del *leave-one-block-out*),
3 (erosión, íntegra), 4 (el precio: distancia y cómputo), la formalización DEC-POGSMDP y la
lectura unificada de los cinco algoritmos, y las tres líneas de trabajo futuro.

🔴 Lo que **descartó** y por tanto **no existe en la memoria**:
- **L1 completo** — los planes comunicados de longitud media **1.58** y el 68.7 % de cadenas
  cortadas en `FINISH`. Con él se cayó «explotar la profundidad de los planes» del trabajo futuro.
- **Las dificultades del desarrollo** (los dos defectos de batería, la trampa iso-dificultad, la
  réplica del simulador dentro de Dec-MCTS): decisión explícita al fijar el índice.
- **La semilla no configurable** (D-05) como límite declarado.
- El matiz uniforme/mixta del bloque D, sustituido por su propia explicación: CBAA forma parejas
  estables de dos robots y las mantiene, y Dec-MCTS no aprovecha esa estrategia.

📌 **Corrección de fondo del usuario sobre la autoría**, que rige de aquí en adelante: los
*baselines*, CBAA y CBBA **no son implementaciones propias** (solo la función de puja y detalles);
**Dec-MCTS sí lo es**, y **su construcción —no el resultado experimental— es el logro central**
del trabajo.

---

## La tesis, en una frase

**La hipótesis del TFM se cumple**: llevado a un esquema puramente distribuido, Dec-MCTS es el
mejor de los cinco algoritmos comparados sobre el catálogo completo, y lo es de forma consistente
—en los cuatro regímenes de incertidumbre y en cuatro de los cinco bloques—. Su ventaja no es
uniforme: **se erosiona con la incertidumbre y con el tamaño del equipo**, y esa erosión, junto
con el coste que paga por su cobertura, es lo que delimita dónde el método rinde y dónde no.

⚠️ **El giro respecto a la versión anterior de este fichero**: aquella defendía que Dec-MCTS
*no* superaba a CBBA y dedicaba casi todo su cuerpo a explicar esa derrota. La derrota era un
artefacto de los dos defectos de batería del simulador: al corregirlos, la recompensa de Dec-MCTS
**no se movió** (−0.001 ± 0.002) y fueron CBAA y CBBA las que perdieron 0.035 cada una, porque
completaban el 8.7–8.9 % de sus tareas con robots que ya habían agotado la batería. El análisis de
limitaciones sigue siendo válido y valioso, pero **cambia de función**: ya no explica por qué
pierde, sino dónde deja de ganar.

---

## Idea 1 · La hipótesis se cumple — PESO MÁXIMO

**Lo que hay que decir**: el algoritmo que costaba más esfuerzo es también el que mejor resuelve
el problema, y la ordenación reproduce el escalonamiento de coordinación que el capítulo 2
formaliza: `Random < Greedy < CBAA < CBBA < Dec-MCTS`, es decir, **a más coordinación, más
rendimiento**. Esa lectura —los cinco algoritmos como políticas de comunicación con grados
crecientes de coordinación— es la aportación conceptual del trabajo, y el resultado la respalda.

**Cifras de titular**: Δ frente a CBBA **+0.0431 ± 0.0039** (t = 10.9), recuento **244/33/83** de
360 escenarios. Niveles: dec 0.502 · cbba 0.459 · cbaa 0.401 · greedy 0.346 · random 0.268.

**Evidencia**: §\ref{subsec:exp-niveles} y §\ref{subsec:exp-delta}, punto 1 de la síntesis.

---

## Idea 2 · La ventaja no depende de la composición del catálogo — PESO ALTO

**Lo que hay que decir**: la ordenación es robusta al diseño del conjunto de prueba. Es positiva
en **los cinco bloques** y en **los cuatro regímenes**, y el *leave-one-block-out* la deja entre
+0.031 y +0.051: ninguna exclusión cambia el signo. Lo que sí depende de la composición es la
**magnitud** (de +0.012 en coaliciones a +0.090 en carga), y así debe declararse.

⚠️ **Esto es exactamente lo contrario de lo que sostenía la versión anterior**, que hacía de «el
conjunto de prueba determina la conclusión» su lección metodológica vertebradora. Con la tanda
corregida esa lección **se cayó** y no debe reaparecer en el capítulo 7 en ninguna forma.

**Lo que sí se conserva de aquella lección**, en una frase y sin dramatizar: el catálogo está
calibrado a **iso-dificultad** (carga por robot constante, horizonte fijo) precisamente porque
ligar el horizonte al número de tareas confundía «más robots» con «menos carga por robot». Es una
decisión de diseño que ya está justificada en el capítulo 5; en el 7 basta con reconocerla como
condición de validez de las pendientes, no como hallazgo.

**Evidencia**: §\ref{subsec:exp-composicion}, punto 2 de la síntesis.

---

## Idea 3 · Dónde se erosiona la ventaja — PESO MEDIO (era la idea central, ya no lo es)

**Lo que hay que decir**: la ventaja no es uniforme y el trabajo delimita con precisión sus
fronteras. Dos factores la reducen, y **interactúan**: la incertidumbre (+0.054 en determinista →
+0.008 con incertidumbre fuerte, ya no separable de cero) y el tamaño del equipo (bajo
incertidumbre la pendiente de Dec-MCTS frente al número de robots, **+0.0016, t = 0.7**, no se
distingue de cero, frente a **+0.0120, t = 4.5** de CBBA; la pendiente de la propia Δ es
**−0.0104/robot, t = −3.5**, y cruza el cero hacia los cuatro robots). Con dos robots la ventaja
se sostiene en todo el gradiente de incertidumbre; con diez se invierte.

**Mejor terreno y peor terreno, delimitados**: plazos escalonados (+0.084, positiva en 12 de 12
escenarios deterministas), carga holgada (hasta +0.208) y equipos pequeños, frente a coaliciones
uniformes (−0.007 en determinista), equipos de diez robots bajo incertidumbre y ventanas anchas
con equipos grandes.

⚠️ **Registro**: esto es **caracterización de lo medido**, no explicación. La interpretación
mecanística —el descuento excesivo de tareas que se creen cubiertas, cuyo sesgo se acumula con el
número de vecinos (H-07)— se enuncia **marcada como hipótesis compatible con los datos**, que es
como el propio capítulo 6 la deja en su cierre. No se puede presentar como demostrada.

🔴 **Lo que NO puede escribirse**: las cuatro refutaciones (cómputo ×40, rondas de comunicación
×30, `blockingProb` probabilística y profundidad del plan comunicado) se midieron con **batería
100, el catálogo v1 y el simulador anterior**. No son citables como resultado de este trabajo
(D-13, salida (c) elegida por el usuario). El capítulo 7 se queda con la erosión **medida** y su
interpretación **declarada como tal**, sin apelar a que «se descartó que fuera la implementación».

**Evidencia**: §\ref{subsec:exp-bloqueA}, §\ref{subsec:exp-bloqueE}, puntos 3 y 5 de la síntesis.

---

## Idea 4 · Qué compra Dec-MCTS y qué paga por ello — PESO MEDIO

**Lo que hay que decir**: la ventaja tiene un mecanismo medido y un precio medido. Es el apartado
que convierte un número agregado en una caracterización operativa del algoritmo.

- **Qué compra, y cambia según el régimen**: en determinista toda la ventaja es **cobertura**
  (intenta más tareas: 0.586 frente a 0.532, con tasa de éxito unitaria en ambos); bajo
  incertidumbre la cobertura se iguala y la sostiene el **acierto** (0.759 frente a 0.719).
- **Qué protege**: es el que **menos robots pierde** (1.48 bajas por escenario frente a 2.51 de
  CBBA y 1.83 de CBAA) y el único cuyas bajas *decrecen* al aumentar la incertidumbre. En el
  bloque de control CBBA llega a perder el 55 % de la flota, frente al 30 % de Dec-MCTS.
- **Qué paga**: un **53.8 %** más de distancia por robot (31.9 % por tarea completada) y unas
  **1 900 veces** más tiempo de cómputo, aunque en términos absolutos el coste es asumible
  (≈40 s en un escenario de diez robots, con los diez robots calculando en paralelo).

**Caracterización para el capítulo 7**: Dec-MCTS **compra cobertura pagando desplazamiento**, y
eso renta mientras el desplazamiento no sea el recurso que ata. ⚠️ Medido aparte y **fuera del
catálogo** (por tanto, citable solo como límite conocido, no como resultado): con navegación cara
*y* batería escasa pierde 12 de 12 escenarios. No subir `battery_rate_while_navigating` por
encima de 1.

**Evidencia**: §\ref{subsec:exp-mecanismo}, §\ref{subsec:exp-mortalidad}, §\ref{subsec:exp-coste},
puntos 4, 6 y 7 de la síntesis.

---

## Idea 5 · Lo que se aprende de los rivales — PESO MEDIO-BAJO

**Lo que hay que decir**: la comparación no solo ordena, también deja dos resultados propios sobre
los métodos de pujas que merecen figurar como hallazgo.

- **CBBA es un segundo muy sólido**: mantiene su escalado con el equipo bajo incertidumbre (donde
  Dec-MCTS se aplana), cuesta tres órdenes de magnitud menos y es la referencia exigente frente a
  la que se establece la mejora. Razón estructural, como **interpretación**: *una puja compromete
  un precio, no una trayectoria*, y por eso no se degrada cuando el plan del vecino se desvía.
- **CBAA gana el bloque de coaliciones** (0.418 frente a 0.403 de Dec-MCTS y 0.391 de CBBA): es la
  **única inversión de la ordenación en todo el catálogo** y contradice la intuición de que la
  puja por paquete domina siempre a la de tarea única. Merece un párrafo propio.
- **El matiz que afina el diagnóstico**: la coalición **uniforme** anula la ventaja (−0.007 en
  determinista) pero la **mixta** no (+0.028). El problema no es cooperar, sino decidir si un
  grupo de vecinos cubrirá una tarea cuando todas exigen lo mismo.

**Evidencia**: §\ref{subsec:exp-bloqueD}, punto 1 de la síntesis.

---

## 📐 Estructura del capítulo 7 (fijada por el usuario, 24-09-2026)

El capítulo consta de **varias partes**. Dos son **requisito explícito del usuario** y no se
negocian:

1. **Logros y aportaciones principales** → ideas 1 y 2, más las aportaciones conceptuales que el
   trabajo ya defiende en los capítulos 2 y 3 (la formalización DEC-POGSMDP y la lectura unificada
   de los cinco algoritmos como políticas de comunicación). Es donde se responde a la hipótesis de
   partida: **se cumple**.
2. **Limitaciones y dificultades encontradas** → el material recogido más abajo, encabezado por
   **L1, los planes comunicados cortos** (decisión del usuario: ese hallazgo va aquí, **no** en
   trabajo futuro).

A ellas se añaden, de forma natural y **a confirmar con el usuario al empezar la redacción**: una
apertura que recupere el objetivo y la hipótesis, el apartado de **trabajo futuro** (idea 6) y un
cierre. ⚠️ **Proponerle el índice y la extensión por apartado antes de escribir**, como en los
capítulos anteriores: su patrón es reescribir el borrador podando lo explicativo.

---

## 🔻 Material para «Limitaciones y dificultades encontradas»

**L1 · Los planes comunicados son mucho más cortos de lo que el método supone.** ⭐ El que
encabeza el apartado. Al instrumentar `sampleNeighbourBundles` se midió una longitud media de
**1.58 tareas**, con el **68.7 %** de las cadenas cortadas en un nodo `FINISH`: la «distribución
sobre secuencias» que distingue a Dec-MCTS de las subastas degenera de hecho en una distribución
sobre la *próxima acción*, que es justo lo que comunica CBAA.
⚠️ **Verificado el 24-09-2026**: el solver evaluado, `dec-mcts-v4-g9999`, se construye con
`deepBundle = false` (`src/main.cpp:184-186`), de modo que **toda la ventaja medida se obtiene con
los planes cortos**, sin ejercer la principal ventaja estructural que el método reclama en teoría.
Es una limitación real de la implementación evaluada y, a la vez, la razón de que el margen no
explotado sea creíble.
⚠️ **La medición de la longitud es del simulador anterior**, pero es una propiedad de cómo se
extrae el plan del árbol, no de la batería; declararlo así. Y **no citar** la variante que los
profundiza como resultado (D-13).

**L2 · La ventaja no es uniforme: se erosiona con la incertidumbre y con el tamaño del equipo.**
Ver idea 3, con sus pendientes y su cruce por cero hacia los cuatro robots. Es la limitación
cuantitativa principal y ya está medida y declarada en el capítulo 6.

**L3 · Hay un bloque donde no lidera: las coaliciones.** Única inversión de la ordenación en todo
el catálogo (gana CBAA). Ver idea 5.

**L4 · El precio: recorrido y cómputo.** +53.8 % de distancia por robot y ~1 900× el tiempo de
CBBA, aunque en absoluto sea asumible. Ver idea 4.

**L5 · Límites de lo que este trabajo puede afirmar** (honestidad metodológica, va en este mismo
apartado o en un cierre propio):
- La comparación **centralizado ↔ distribuido no se ha medido** aquí: la afirmación es cualitativa
  y se apoya en el trabajo del tutor, **sin cifras de pérdida**.
- Las explicaciones mecanísticas son **hipótesis compatibles con los datos**, no contrastadas; el
  capítulo 6 ya lo dice en su cierre y el 7 debe mantener la distinción.
- Las ablaciones y refutaciones **no se han re-medido** sobre la tanda vigente (D-13, salida (c)).
- **No hay semilla configurable** (`std::random_device`): las ejecuciones no son reproducibles bit
  a bit y por eso se promedian réplicas (D-05).
- El resultado está acotado a **un catálogo, una familia de problemas y una implementación**.

**D · Dificultades encontradas durante el desarrollo** (material disponible; ⚠️ **decidir con el
usuario si entra y con qué extensión**, porque expone el proceso):
- **Dos defectos de batería en el simulador**, detectados tarde y corregidos: navegar no podía
  matar, y el intento truncado por falta de batería se re-tarifaba con la tarifa del fracaso, de
  modo que el robot sobrevivía con batería de regalo. Obligaron a **reejecutar el catálogo entero**
  y **cambiaron la conclusión del trabajo**. Contarlo juega a favor: el resultado final es el
  correcto y el proceso fue riguroso.
- **La trampa del diseño del conjunto de prueba**: ligar el horizonte al número de tareas confundía
  «más robots» con «menos carga por robot» y producía una tendencia falsa. Se resolvió calibrando
  a **iso-dificultad**, que es la condición que da sentido a las pendientes del bloque A.
- **Dec-MCTS lleva dentro una réplica del simulador** (`makeState`/`makeEventQueue`/`physics*`):
  todo cambio en la física hay que replicarlo o las estimaciones quedan sesgadas. Es una dificultad
  estructural de implementar un método basado en modelo generativo.

---

## Idea 6 · Trabajo futuro — la línea más original sale del bloque D

**Lo que hay que decir**, por orden de fuerza:

1. **Comunicación consciente de coaliciones.** Es la línea mejor respaldada por el dato: el único
   bloque donde Dec-MCTS no lidera es el de coaliciones, y el único donde se invierte la
   ordenación. `blockingProb` decide de forma **binaria** si los vecinos cubren una tarea; el
   canal no transporta ninguna noción de coalición parcial, de modo que un robot no puede
   comunicar «voy a esta tarea *si* alguien más viene», que es justo la información que haría
   falta. Que la variante uniforme duela y la mixta no encaja con ese diagnóstico.
2. **Explotar la profundidad de los planes comunicados.** Prolongación natural de la
   limitación **L1** (los planes comunicados tienen longitud media 1.58, ver abajo): si la
   distribución llegase a describir secuencias y no la próxima acción, el método ejercería su
   diferencia conceptual frente a las subastas. ⚠️ **Sin prometer mejora**: formulación prudente,
   «queda por determinar si explotar esa profundidad se traduce en rendimiento».
3. **Mejoras de diseño aparcadas y documentadas** en `05_dudas.md`: crédito marginal frente al
   objetivo global diluido (B1), normalización por disponibilidad (B2) y comunicar la
   *incertidumbre* del plan además del plan (B5).
4. **Validación externa** sobre un banco estándar (Solomon), que se dejó fuera por coste.
5. **Reproducibilidad**: semilla configurable (D-05), hoy `std::random_device` en todas partes.

---

## La transición centralizado → distribuido (antes idea 5; REFORMULADA)

La versión anterior usaba el paper del tutor para cerrar un argumento de derrota: «MCTS funciona
centralizado, aquí falla distribuido, luego la culpa es de distribuir». **Ese argumento ya no
aplica**, porque no hay derrota que explicar.

**La forma correcta ahora**: el trabajo parte de un modelo centralizado y muestra que el método
puede llevarse a un esquema **puramente distribuido** —cada robot decide con información local y
comunicación— **sin perder la posición de cabeza** frente a los algoritmos distribuidos de
referencia. Es un resultado de viabilidad, y es lo que sostiene la aportación del TFM.

⚠️ **Dos límites que el usuario fijó y que siguen vigentes**:
- **Nada de cifras de pérdida** centralizado ↔ distribuido: no se ha medido aquí, y los escenarios
  del paper del tutor no son los de este catálogo. La afirmación se queda **cualitativa**.
- **Depende de D-03** (cómo citar un trabajo no publicado). Sigue siendo **estructural** para este
  capítulo, no un detalle de formato: hay seis marcas `\red{CITA AL TUTOR}` en los capítulos 2 y 3.

---

## Lo que se cayó y NO debe resucitar

Lista explícita, porque casi todo ello estuvo escrito y defendido durante ocho sesiones:

- ❌ «Dec-MCTS no supera a CBBA» y todo el marco de *algoritmo que no fracasa pero no gana*.
- ❌ «El diseño del conjunto de prueba determina la conclusión» (el *leave-one-block-out* ya no
  invierte ningún signo).
- ❌ Las **cuatro refutaciones** como prueba de que la desventaja no es de implementación, y la
  **ablación del canal** (+0.066 determinista / nulo estocástico): medidas con batería 100 y el
  simulador anterior.
- ❌ «Bajo incertidumbre Dec-MCTS no convierte robots en rendimiento **en absoluto**»: la pendiente
  es nula en el sentido estadístico (t = 0.7), que no es lo mismo, y así debe decirse.
- ❌ **La conjetura sobre la escasez de literatura en MCTS descentralizado: ELIMINADA POR
  COMPLETO** (decisión del usuario, 24-09-2026). No entra en el capítulo 7 ni en ninguna forma
  atenuada, ni tampoco invertida. Se apoyaba en un techo estructural que esta tanda no observa.
  **No reintroducirla.**
- ❌ «Los tres solvers coordinados no pierden un solo robot» y cualquier cifra de mortalidad,
  tasa de éxito o recompensa anterior al 17-09-2026.
- ❌ Todas las cifras de los catálogos v1 y v2, y las del v3 con defecto.

---

## Reparto sugerido en la memoria

| Capítulo | Qué llevar de aquí |
|---|---|
| **6 · Experimentación** | ✅ Cerrado. Ya contiene las medidas de las ideas 1–5, y su cierre distingue explícitamente lo medido de lo interpretado, dejando la valoración al capítulo 7. **No tocar.** |
| **7 · Logros y aportaciones** | Ideas 1 y 2 (la hipótesis se cumple y es robusta) + las aportaciones conceptuales de los caps. 2 y 3. La transición centralizado → distribuido como marco, en cualitativo. |
| **7 · Limitaciones y dificultades** | **L1–L5 y las dificultades D** de la sección propia de este fichero, encabezadas por los planes comunicados cortos (L1). Recogen las ideas 3, 4 y 5. |
| **7 · Trabajo futuro** | Idea 6 completa, encabezada por la comunicación consciente de coaliciones. |
| **1 · Introducción** | Se escribe al final, con la hipótesis ya confirmada: puede anunciar el resultado sin condicionales. |

## Recordatorios de registro (exigidos por el usuario)

- El capítulo 6 es **descriptivo**; el 7 es donde van las **valoraciones** y la defensa de la
  hipótesis. Aquí sí cabe interpretar, siempre que la interpretación se marque como tal.
- **Comparaciones pareadas por escenario** y **reportadas por régimen**: el capítulo 6 ya lo hace
  y el 7 no debe agregar de una forma que lo esconda.
- **Sin ablaciones en la memoria** (decisión del usuario, 2026-08-21): los hiperparámetros se
  presentan como fijados empíricamente durante el desarrollo.
