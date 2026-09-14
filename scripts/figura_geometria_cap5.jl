#!/usr/bin/env julia
#
# Figura de la geometría de los escenarios (capítulo 5 de la memoria).
#
# Tres escenarios en su estado inicial, uno al lado de otro, con 3, 4 y 5 tareas
# por racimo (12, 16 y 20 tareas). Sin robots, sin ejes y sin anotaciones: solo la
# disposición de las tareas y la estación de recarga.
#
# Reproduce el lenguaje visual de `mrtau video`, cuyo renderizador es
# `~/tfm/mrtau/scripts/mrtau_visualization.jl` (CairoMakie):
#
#   tarea con la ventana abierta   lightgreen #90EE90   markersize 20
#   estación de recarga            yellow     #FFFF00   markersize 24
#
# El vídeo dibuja los marcadores en píxeles sobre un lienzo de 800 px que abarca
# 25 unidades del escenario. Aquí el eje se recorta a lo que ocupan los racimos,
# así que los tamaños se expresan en UNIDADES DEL ESCENARIO (markerspace = :data)
# con el diámetro equivalente: los racimos se ven igual de apretados que en el vídeo.
#
# Las posiciones se leen de los ficheros de escenario reales, no se inventan.
#
# Uso:  julia --project=@v1.10 scripts/figura_geometria_cap5.jl [salida.pdf]

using CairoMakie

const AQUI = @__DIR__
const CATALOGO = joinpath(AQUI, "..", "scenarios", "catalogo_v2")
const SALIDA = length(ARGS) >= 1 ? ARGS[1] :
    joinpath(AQUI, "..", "memoria", "figures", "05_casos_de_estudio", "geometria_escenario.pdf")

const TAMANOS = (12, 16, 20)      # tareas por escenario: 3, 4 y 5 por racimo

# ── lenguaje visual de `mrtau video` ─────────────────────────────────────────
const C_TAREA    = :lightgreen
const C_ESTACION = :yellow
const D_TAREA    = 0.71           # equivalente a los 20 px del vídeo
const D_ESTACION = 0.86           # equivalente a los 24 px
const BORDE      = 0.4            # en papel, el amarillo se pierde sin un trazo fino
const LIMITE     = 5.5
const MARCO      = :gray70        # caja del panel, como el recuadro del vídeo

"""Devuelve (estación, tareas) leyendo las coordenadas de los nodos del YAML.

El nodo 1 es la estación de recarga; los nodos 2..m+1 alojan una tarea cada uno.
Se parsea con una expresión regular para no depender de YAML.jl.
"""
function leer_nodos(ruta::AbstractString)
    coords = Point2f[]
    for linea in eachline(ruta)
        m = match(r"^\s*coords:\s*\[\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\]", linea)
        m === nothing || push!(coords, Point2f(parse(Float32, m[1]), parse(Float32, m[2])))
    end
    isempty(coords) && error("no se han encontrado coordenadas en $ruta")
    return coords[1], coords[2:end]
end

"""Primer escenario del catálogo con `m` tareas (la geometría solo depende de `m`)."""
function escenario_con(m::Int)
    patron = "_n$(lpad(m, 3, '0'))_"
    ficheros = sort(filter(f -> occursin(patron, f) && endswith(f, ".yaml"),
                           readdir(CATALOGO)))
    isempty(ficheros) && error("no hay ningún escenario de $m tareas en $CATALOGO")
    joinpath(CATALOGO, first(ficheros))
end

# La figura se compone a su tamaño final de impresión (≈15,5 × 5,8 cm), de modo que
# los cuerpos de letra y los grosores de línea del PDF son los que se verán en papel.
fig = Figure(size = (440, 168), figure_padding = 3)

for (col, m) in enumerate(TAMANOS)
    estacion, tareas = leer_nodos(escenario_con(m))
    length(tareas) == m || error("el escenario tiene $(length(tareas)) tareas, no $m")

    ax = Axis(fig[1, col], limits = (-LIMITE, LIMITE, -LIMITE, LIMITE),
              aspect = DataAspect(), spinewidth = 0.5,
              leftspinecolor = MARCO, rightspinecolor = MARCO,
              topspinecolor = MARCO, bottomspinecolor = MARCO)
    hidedecorations!(ax)   # sin ticks, sin rótulos de eje y sin rejilla: solo la caja

    scatter!(ax, tareas; markersize = D_TAREA, markerspace = :data,
             color = C_TAREA, strokewidth = BORDE, strokecolor = (:black, 0.55))
    scatter!(ax, [estacion]; markersize = D_ESTACION, markerspace = :data,
             color = C_ESTACION, strokewidth = BORDE, strokecolor = (:black, 0.7))

    # tellwidth = false: si no, la anchura del rótulo manda sobre la de la columna
    Label(fig[2, col], "$m tareas"; fontsize = 9, tellwidth = false,
          padding = (0, 0, 0, 2))
end

rowgap!(fig.layout, 2)
colgap!(fig.layout, 12)

mkpath(dirname(SALIDA))
save(SALIDA, fig)
save(replace(SALIDA, ".pdf" => ".png"), fig; px_per_unit = 4)
println("escrita ", SALIDA, "  (", join(TAMANOS, ", "), " tareas)")
