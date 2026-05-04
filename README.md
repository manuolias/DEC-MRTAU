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
mrtau metrics -i logs/ -o prueba.csv
```