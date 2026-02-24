#include "tau/simulator.hpp"
#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>

namespace tau {

DistributedSimulator::DistributedSimulator(std::shared_ptr<Scenario> scen, std::shared_ptr<Logger> log) 
    : scenario(scen), logger(log), globalTime(0.0) {}

void DistributedSimulator::registerSolver(RobotID robotID, std::shared_ptr<ISolver> solver) {
    solvers[robotID] = solver;
}

Time DistributedSimulator::calculateTravelTime(RobotID robotID, NodeID src, NodeID dst) {
    if (src == dst) return 0.0;
    Distance dist = scenario->nodes.at(src).neighbors.at(dst);
    return dist / scenario->robots.at(robotID).navigationVelocity;
}

void DistributedSimulator::consumeBattery(Time t, RobotID robotID, Time duration, double rate) {
    if (duration <= 0.0) return;
    BatteryLevel cost = rate * duration;
    BatteryLevel initLvl = robotBatteries[robotID];
    robotBatteries[robotID] = std::max(0.0, initLvl - cost);
    logger->logBatteryConsumption(t, robotID, duration, initLvl, robotBatteries[robotID]);
}

void DistributedSimulator::initLoggingAndEvents() {
    /*
    info: filename; value: mi_simulacion_distribuida.log
    info: scenario_name; value: scenario_12_06_01
    info: number_of_tasks; value: 12
    info: number_of_robots; value: 6
    info: number_of_recharging_stations; value: 1
    info: scenario_size; value: (200,200)
    */

    logger->logInfo("filename", logger->getFilename());
    logger->logInfo("scenario_name", scenario->name);
    logger->logInfo("scenario_name", scenario->name);
    logger->logInfo("number_of_tasks", std::to_string(scenario->getTasks().size()));
    logger->logInfo("number_of_robots", std::to_string(scenario->getRobots().size()));
    logger->logInfo("number_of_recharging_stations", std::to_string(scenario->getStations().size()));
    logger->logInfo("scenario_size", "(" + std::to_string(scenario->height) + "," + std::to_string(scenario->width) + ")");
    

    for (const auto& [id, station] : scenario->stations) {
        logger->logStationSpawn(0.0, id, scenario->nodes.at(station.node).coords);
    }
    for (const auto& [id, task] : scenario->tasks) {
        logger->logTaskSpawn(0.0, task, scenario->nodes.at(task.node).coords);
    }
    for (const auto& [id, info] : scenario->robots) {
        robotPositions[id] = info.initialNode;
        robotBatteries[id] = info.initialBatteryLevel;
        logger->logRobotSpawn(0.0, id, scenario->nodes.at(info.initialNode).coords, 
                              info.batteryCapacity, info.navigationVelocity, info.batteryRateWhileNavigating);
        // Despertar a todos los robots en t=0
        eventQueue.push({0.0, EventType::ROBOT_NEEDS_DECISION, id, -1});
    }
}

void DistributedSimulator::run() {
    initLoggingAndEvents();

    // Preparar el generador de números aleatorios
    std::random_device rd;
    std::mt19937 gen(rd());

    while (!eventQueue.empty()) {
        // Miramos qué hora es en el siguiente evento
        Time currentTime = eventQueue.top().time;
        
        // Contenedores para agrupar los eventos que ocurren a la vez
        std::vector<Event> worldUpdateEvents;
        std::vector<Event> decisionEvents;

        // 1. EXTRAER TODOS LOS EVENTOS DEL MISMO INSTANTE (TIMESTAMP)
        while (!eventQueue.empty() && eventQueue.top().time == currentTime) {
            Event ev = eventQueue.top();
            eventQueue.pop();

            // Separar los eventos físicos de los eventos de decisión
            if (ev.type == EventType::ROBOT_NEEDS_DECISION) {
                decisionEvents.push_back(ev);
            } else {
                worldUpdateEvents.push_back(ev);
            }
        }

        globalTime = currentTime; // Actualizamos el reloj global

        // 2. PROCESAR PRIMERO LA FÍSICA (Llegadas y Resoluciones)
        // Esto garantiza que si una tarea termina en t=10, el robot que decide en t=10 sepa que ya terminó
        for (const auto& ev : worldUpdateEvents) {
            if (ev.type == EventType::ROBOT_ARRIVES_AT_TASK) {
                handleRobotArrivesAtTask(globalTime, ev.robotID, ev.taskID);
            } else if (ev.type == EventType::TASK_RESOLUTION) {
                handleTaskResolution(globalTime, ev.taskID);
            }
        }

        // 3. BARAJAR EL ORDEN DE DECISIÓN DE LOS ROBOTS
        if (!decisionEvents.empty()) {
            std::shuffle(decisionEvents.begin(), decisionEvents.end(), gen);
            
            // Procesar las decisiones en el nuevo orden aleatorio
            for (const auto& ev : decisionEvents) {
                handleRobotDecision(globalTime, ev.robotID);
            }
        }
    }
}

void DistributedSimulator::handleRobotDecision(Time t, RobotID robotID) {
    if (!solvers.count(robotID)) return; // Si no hay IA asignada, el robot se queda quieto

    // 1. Crear la observabilidad local para el solver
    LocalBelief belief;
    belief.currentTime = t;
    belief.currentNode = robotPositions[robotID];
    belief.currentBattery = robotBatteries[robotID];

    // NUEVO: Filtrar las tareas que siguen pendientes
    for (const auto& [tID, taskInfo] : scenario->tasks) {
        if (physicalTasks[tID].status == TaskStatus::PENDING) {
            belief.pendingTasks.push_back(tID);
        }
    }

    // 2. Pedir al cerebro una decisión
    Action action = solvers[robotID]->decideNextAction(belief, scenario);

    // 3. Ejecutar físicamente la decisión
    if (action.type == ActionType::EXECUTE_TASK) {

        // --- NUEVA LÓGICA DE ASIGNACIÓN ---
        TaskID tID = action.targetTask;
        physicalTasks[tID].assignedWorkers++;

        // Si ya hay suficientes robots en camino, la tarea pasa a ASSIGNED
        if (physicalTasks[tID].assignedWorkers >= scenario->tasks.at(tID).requiredWorkers) {
            physicalTasks[tID].status = TaskStatus::ASSIGNED;
        }
        // ----------------------------------

        Time travelTime = calculateTravelTime(robotID, robotPositions[robotID], action.targetNode);
        
        if (travelTime > 0) {
            logger->logNavigation(t, robotID, travelTime, 
                                  scenario->nodes.at(robotPositions[robotID]).coords, 
                                  scenario->nodes.at(action.targetNode).coords);
            consumeBattery(t, robotID, travelTime, scenario->robots.at(robotID).batteryRateWhileNavigating);
        }
        
        // Actualizar posición y programar llegada
        robotPositions[robotID] = action.targetNode;
        eventQueue.push({t + travelTime, EventType::ROBOT_ARRIVES_AT_TASK, robotID, action.targetTask});
        
    } else if (action.type == ActionType::FINISH) {
        Time travelTime = calculateTravelTime(robotID, robotPositions[robotID], action.targetNode);
        if (travelTime > 0) {
            logger->logNavigation(t, robotID, travelTime, 
                                  scenario->nodes.at(robotPositions[robotID]).coords, 
                                  scenario->nodes.at(action.targetNode).coords);
            consumeBattery(t, robotID, travelTime, scenario->robots.at(robotID).batteryRateWhileNavigating);
            robotPositions[robotID] = action.targetNode;
        }
        logger->logRobotFinished(t + travelTime, robotID, scenario->nodes.at(action.targetNode).coords, robotBatteries[robotID]);
    }
}

void DistributedSimulator::handleRobotArrivesAtTask(Time t, RobotID robotID, TaskID taskID) {
    workersAtTask[taskID].push_back(robotID);
    
    // Si han llegado suficientes trabajadores, la tarea se ejecuta inmediatamente
    if (workersAtTask[taskID].size() == scenario->tasks.at(taskID).requiredWorkers) {
        eventQueue.push({t, EventType::TASK_RESOLUTION, -1, taskID});
    }
}

void DistributedSimulator::handleTaskResolution(Time t, TaskID taskID) {
    const auto& taskInfo = scenario->tasks.at(taskID);
    const auto& workers = workersAtTask[taskID];
    
    // Tira los dados para ver si tiene éxito
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    bool success = dis(gen) < taskInfo.successProb;

    // Calcular el tiempo estocástico
    Time mu = success ? taskInfo.averageSuccessTime : taskInfo.averageFailTime;
    Time sigma = success ? taskInfo.stdSuccessTime : taskInfo.stdFailTime;
    std::normal_distribution<> norm(mu, sigma);
    Time execTime = std::max(0.0, norm(gen));

    // Loguear la ejecución y consumir batería de todos los trabajadores involucrados
    for (RobotID workerID : workers) {
        logger->logTaskExecution(t, taskID, workerID, scenario->nodes.at(taskInfo.node).coords, execTime);
        consumeBattery(t, workerID, execTime, scenario->robots.at(workerID).batteryRateWhileExecuting);
        // Liberar al robot para su siguiente decisión después de la tarea
        eventQueue.push({t + execTime, EventType::ROBOT_NEEDS_DECISION, workerID, -1});
    }

    std::string statusStr = success ? "COMPLETED" : "FAILED";
    logger->logTaskResolution(t + execTime, taskID, scenario->nodes.at(taskInfo.node).coords, statusStr, 1);
    
    // --- NUEVO: Actualizar el estado físico ---
    if (success) {
        physicalTasks[taskID].status = TaskStatus::COMPLETED;
    } else {
        physicalTasks[taskID].status = TaskStatus::FAILED;
        
        // Opcional: Si quieres que los robots puedan reintentarla en el futuro (si maxAttempts > 1),
        // aquí podrías resetear physicalTasks[taskID].assignedWorkers = 0 
        // y poner physicalTasks[taskID].status = TaskStatus::PENDING;
    }
    
    workersAtTask[taskID].clear();
}

} // namespace tau