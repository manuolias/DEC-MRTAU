# Versión mínima de CMake requerida
cmake_minimum_required(VERSION 3.10)

# Nombre de tu proyecto
project(MultiRobotTaskAllocation)

# Pedirle al compilador que use el estándar C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Buscar la librería externa yaml-cpp instalada en el sistema
find_package(yaml-cpp REQUIRED)

# 2. Decirle al compilador dónde buscar los archivos .hpp (nuestra carpeta 'include')
include_directories(include)

# 3. Crear el ejecutable incluyendo TODOS los archivos fuente nuevos
add_executable(simulador 
    src/main.cpp 
    src/scenario.cpp
    src/logger.cpp
    src/simulator.cpp
    src/state.cpp
)

# 4. Enlazar (link) la librería yaml-cpp a nuestro ejecutable
target_link_libraries(simulador PRIVATE yaml-cpp)