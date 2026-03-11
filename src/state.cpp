#include "tau/state.hpp"
#include <limits>
#include <vector>
#include <random>

namespace tau {

State::State(std::shared_ptr<const Scenario> scen) : scenario(scen) {

    // Inicializar el estado físico de los robots
    for (const auto& [id, info] : scenario->getRobots()) {
        Robot robot;
        robot.status = RobotStatus::AVAILABLE;
        robot.batteryLevel = info.initialBatteryLevel;
        robot.time = scenario->initialTime;
        robot.node = info.initialNode;
        robot.onTask = NULL_ID; 
        robot.completedTasks = 0;
        robot.travelDistance = 0.0;
        robot.failed = false;
        
        robots[id] = robot;
    }

    // Inicializar el estado físico de las tareas
    for (const auto& [id, info] : scenario->getTasks()) {
        Task task;
        task.status = TaskStatus::PENDING;
        task.assignedWorkers = 0;
        task.initTime = scenario->initialTime;
        task.finalTime = scenario->initialTime;
        task.attempts = 0;
        
        tasks[id] = task;
    }
}

bool State::isFinal() const {
    // La simulación termina si TODOS los robots han FAILED o FINISHED
    for (const auto& [id, robot] : robots) {
        if (robot.status != RobotStatus::FAILED && 
            robot.status != RobotStatus::FINISHED) {
            return false;
        }
    }
    return true;
}



RobotID State::getNextRobotID() const {
    std::vector<RobotID> candidates;
    
    // Inicializamos el tiempo mínimo con el valor máximo posible
    // (Asumiendo que 'Time' es un tipo numérico como int, float o double)
    Time min_time = std::numeric_limits<Time>::infinity(); 
    // 1. Encontrar el tiempo mínimo y recolectar candidatos
    for (const auto& [id, robot] : robots) {
        // Ignoramos los robots que no pueden tomar decisiones
        if (robot.status != RobotStatus::AVAILABLE) {
            continue; 
        }
        if (robot.time < min_time) {
            // Encontramos un nuevo mínimo absoluto: 
            // actualizamos el tiempo, limpiamos la lista y añadimos este robot
            min_time = robot.time;
            candidates.clear();
            candidates.push_back(id);
            
        } else if (robot.time == min_time) {
            // Hay un empate: añadimos este robot a la lista de candidatos
            candidates.push_back(id);
        }
    }
    // 2. Comprobación de seguridad
    if (candidates.empty()) {
        // Dependiendo de cómo manejes los errores, puedes lanzar una excepción
        // o devolver una constante como 'NULL_ID'
        return NULL_ID;
    }
    // 3. Retorno rápido si solo hay un candidato
    if (candidates.size() == 1) {
        return candidates.front();
    }
    // 4. Desempate aleatorio si hay múltiples candidatos
    // Usamos 'static thread_local' para que el generador se inicialice solo una vez,
    // mejorando el rendimiento y evitando secuencias repetidas si llamas a la función muy rápido.
    static thread_local std::mt19937 generador(std::random_device{}());
    std::uniform_int_distribution<std::size_t> distribucion(0, candidates.size() - 1);
    std::size_t indice_aleatorio = distribucion(generador);
    return candidates[indice_aleatorio];
}

}