La memoria completa del trabajo está en [`memoria.pdf`](memoria.pdf) (101 páginas).

---

Para ejecutar el código de este proyecto, se recomienda seguir los siguientes pasos:
1. Modfificar direcciones de entrada y salida en src/main.cpp
2. Modificar fichero data/experiment_config.yaml para configurar los experimentos a ejecutar
3. Compilar el proyecto con make. Para esto nos colocamos en el directorio build y ejecutamos:

(---- OPCIONAL: Eliminar contenido del directorio build antes de compilar para evitar problemas con archivos obsoletos ----)
```bash
rm -rf *
```

```bash
cmake ..
```
```bash
make
```
```bash
./simulador
```


---
Para obtener las métricas correspondientes, hacer:
```
mrtau metrics -i logs/ -o results.csv
```

Para generar un video, hacer:
```
mrtau video -i test.log -o simulacion.mp4
```

---

## Licencia

Este proyecto se distribuye bajo la **licencia MIT**: ver el fichero [`LICENSE`](LICENSE).

Es el código del Trabajo de Fin de Máster *«Búsqueda en árbol de Monte Carlo descentralizada para
la asignación de tareas multi-robot bajo incertidumbre»*, de Manuel Olías López (tutor: Ignacio
Pérez-Hurtado). Si lo utilizas o lo citas, referencia la memoria del trabajo.

La licencia MIT cubre **el software** de este repositorio: el código, los generadores de
escenarios y las herramientas de análisis. **El texto de la memoria** (`memoria.pdf`) no forma
parte de ese permiso: © 2026 Manuel Olías López, todos los derechos reservados.
