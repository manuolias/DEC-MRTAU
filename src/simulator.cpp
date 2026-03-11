#include "tau/simulator.hpp"
#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>

namespace tau {

DistributedSimulator::DistributedSimulator(std::shared_ptr<const Scenario> scen, std::shared_ptr<Logger> log) 
    : scenario(scen), logger(log), state(scen), globalTime(scen->initialTime) {} 

void DistributedSimulator::registerSolver(RobotID robotID, std::shared_ptr<ISolver> solver) {
    solvers[robotID] = solver;
}

void DistributedSimulator::registerReward(std::shared_ptr<RewardFunction> reward) {
    this->reward = reward;
}

Time DistributedSimulator::calculateTravelTime(RobotID robotID, NodeID src, NodeID dst) {
    if (src == dst) return 0.0;
    Distance dist = scenario->nodes.at(src).neighbors.at(dst);
    return dist / scenario->robots.at(robotID).navigationVelocity;
}

BatteryLevel DistributedSimulator::calculateBatteryConsumption(RobotID robotID, Time duration, BatteryRate rate) {
    BatteryLevel initLvl = state.getRobot(robotID).batteryLevel;
    if (duration <= 0.0) return initLvl;
    BatteryLevel cost = rate * duration;
    BatteryLevel finalLvl = std::max(0.0, initLvl - cost);
    return finalLvl;
}

// NUEVO: Implementación del filtro de observación
Observation DistributedSimulator::generateObservation(RobotID robotID) {
    std::vector<Message> myMessages;
    
    // Filtramos los mensajes: solo cogemos los dirigidos a este robot, o broadcasts (NULL_ID)
    for (const auto& msg : messageBus) {
        if (msg.receiver == robotID || msg.receiver == NULL_ID) {
            myMessages.push_back(msg);
        }
    }
    
    // El robot ve su propio estado interno y el estado público de todas las tareas (totalmente observable)
    return Observation(robotID, state.getRobot(robotID), state.getTasks(), myMessages);
}

// NUEVO: Enrutamiento básico de mensajes
void DistributedSimulator::routeMessages() {
    // Si quisieras procesar latencia o pérdida de paquetes (incertidumbre en comunicaciones), 
    // este sería el lugar perfecto. Por ahora, asumimos comunicación instantánea.
    
    // Limpiamos el bus después de que todos los mensajes hayan sido entregados
    // (Esto dependerá de cómo gestiones el ciclo de turnos de tu simulador, 
    // por ahora lo dejamos como base).
}

void DistributedSimulator::initLogging() {
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
        logger->logRobotSpawn(0.0, id, scenario->nodes.at(info.initialNode).coords, 
                              info.batteryCapacity, info.navigationVelocity, info.batteryRateWhileNavigating);
    }
}

void DistributedSimulator::finalLogging(double reward, double computingTime) {
    const auto& robots = state.getRobots();
    const auto& tasks = state.getTasks();
    const auto& scenario = state.getScenario();

    logger->logMetric("available_agents",tau::getNumberOfRobots(robots, RobotStatus::AVAILABLE));
    logger->logMetric("failed_agents",tau::getNumberOfRobots(robots,RobotStatus::FAILED));
    logger->logMetric("finished_agents",tau::getNumberOfRobots(robots,RobotStatus::FINISHED));

    logger->logMetric("completed_tasks",tau::getNumberOfTasks(tasks,TaskStatus::COMPLETED));
    logger->logMetric("pending_tasks",tau::getNumberOfTasks(tasks,TaskStatus::PENDING));
    logger->logMetric("failed_tasks",tau::getNumberOfTasks(tasks,TaskStatus::FAILED));

    utils::SVector makespans;
    for (const auto& [id, robot] : robots) {
        makespans.push_back(std::max(0.0, robot.time - scenario.initialTime));
    }

    logger->logMetric("average_robot_makespan",makespans.mean());
    logger->logMetric("sd_robot_makespan",makespans.sd());
    logger->logMetric("max_robot_makespan",makespans.max());
    logger->logMetric("min_robot_makespan",makespans.min());

    utils::SVector distances;
    for (const auto& [id, robot] : robots) {
        distances.push_back(robot.travelDistance);
    }

    logger->logMetric("average_robot_travel_distance",distances.mean());
    logger->logMetric("sd_robot_travel_distance",distances.sd());
    logger->logMetric("max_robot_travel_distance",distances.max());
    logger->logMetric("min_robot_travel_distance",distances.min());
    logger->logMetric("sum_robot_travel_distance",distances.sum());

    utils::SVector completed;
    for (const auto& [id, robot] : robots) {
        completed.push_back(static_cast<double>(robot.completedTasks));
    }

    logger->logMetric("average_completed_tasks_by_robot",completed.mean());
    logger->logMetric("sd_completed_tasks_by_robot",completed.sd());
    logger->logMetric("max_completed_tasks_by_robot",completed.max());
    logger->logMetric("min_completed_tasks_by_robot",completed.min());

    logger->logMetric("final_reward",reward);
    logger->logMetric("computing_time", computingTime);
     
}

void DistributedSimulator::run() {

    auto start_time = std::chrono::high_resolution_clock::now();
                        
    initLogging();

    // 1. INICIALIZACIÓN DE LA COLA DE EVENTOS
    // Añadimos la primera decisión para todos los robots disponibles
    for (const auto& [id, robot] : state.getRobots()) {
        scheduleRobotDecision(scenario->initialTime, id);
    }
    // Añadimos el evento de caducidad para todas las tareas
    for (const auto& [id, task] : scenario->getTasks()) {
        eventQueue.push({task.latestStart, EventType::TASK_EXPIRATION, NULL_ID, id});
    }

    // 2. BUCLE PRINCIPAL DE EVENTOS
    while (!eventQueue.empty() && !state.isFinal()) {

        // Extraemos el evento más cercano en el tiempo
        Event currentEvent = eventQueue.top();
        eventQueue.pop();

        // Actualizamos el reloj global (el tiempo nunca retrocede)
        if (currentEvent.time > globalTime) {
            globalTime = currentEvent.time;
        }

        switch (currentEvent.type) {
            
            case EventType::ROBOT_DECISION: {
                auto& robot = state.getRobot(currentEvent.robotID);
                // Si el robot falló o ya terminó, ignoramos este evento
                if (robot.status != RobotStatus::AVAILABLE) break; 

                routeMessages();
                Observation obs = generateObservation(currentEvent.robotID);
                Action action = solvers.at(currentEvent.robotID)->decideNextAction(obs, scenario);
                
                std::vector<Message> outgoing = solvers.at(currentEvent.robotID)->getOutbox();
                messageBus.insert(messageBus.end(), outgoing.begin(), outgoing.end());

                applyAction(currentEvent.robotID, action);
                break;
            }

            case EventType::TASK_RESOLUTION: {
                resolveTask(currentEvent.taskID);
                break;
            }

            case EventType::TASK_EXPIRATION: {
                expireTask(currentEvent.taskID);
                break;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> compute_duration = end_time - start_time;
    double computingTime = compute_duration.count();

    double r = reward->getValue(state);
    finalLogging(r, computingTime);

}

void DistributedSimulator::applyAction(RobotID robotID, const Action& action) {
    // Modificamos el nuevo estado según el tipo de acción
    switch (action.getType()) {
        case Action::Type::EXECUTE_TASK: {
            simulateTask(robotID, action.getTarget());
            break;
        }
        case Action::Type::RECHARGE: {
            simulateRecharge(robotID);
            break;
        }
        case Action::Type::FINISH: {
            simulateFinish(robotID);
            break;
        }
    }
}

void DistributedSimulator::simulateFinish(RobotID robotID) {

    auto& robot = state.getRobot(robotID);
    NodeID nodeStartID = robot.node;
    StationID nearestStationID = scenario->getNodes().at(nodeStartID).nearestStation;
    NodeID nodeEndID = scenario->getStations().at(nearestStationID).node;
    Time travelTime = calculateTravelTime(robotID, nodeStartID, nodeEndID);
    BatteryLevel finalBattery= calculateBatteryConsumption(robotID, travelTime, scenario->robots.at(robotID).batteryRateWhileNavigating);

    if (finalBattery < 0.0){ // FAIL: El robot se ha quedado sin batería
        Time failTime = robot.batteryLevel / scenario->robots.at(robotID).batteryRateWhileNavigating;
        logger->logBatteryConsumption(robot.time, robotID, failTime, robot.batteryLevel, 0.0);
        robot.time = globalTime + failTime;
        logger->logRobotFailed(globalTime + failTime, robotID, scenario->getNodes().at(nodeEndID).coords);
        robot.status = RobotStatus::FAILED; 
        robot.batteryLevel = 0.0;
    } else {
        if (travelTime > 0) {
            logger->logBatteryConsumption(globalTime, robotID, travelTime, robot.batteryLevel, finalBattery);
            logger->logNavigation(globalTime, robotID, travelTime, 
                                    scenario->getNodes().at(nodeStartID).coords, 
                                    scenario->getNodes().at(nodeEndID).coords);
        }
        Distance dist = (nodeStartID == nodeEndID) ? 0.0 : scenario->getNodes().at(nodeStartID).neighbors.at(nodeEndID);
        robot.travelDistance += dist; // <--- Acumulamos la distancia recorrida

        robot.batteryLevel = finalBattery;
        robot.node = nodeEndID;
        robot.time = globalTime + travelTime;
        logger->logRobotFinished(globalTime + travelTime, robotID, scenario->getNodes().at(nodeEndID).coords, robot.batteryLevel);
        robot.status = RobotStatus::FINISHED;
    }    
}

void DistributedSimulator::simulateRecharge(RobotID robotID) {
    auto& robot = state.getRobot(robotID);
    NodeID nodeStartID = robot.node;

    // Buscamos la estación más cercana
    StationID nearestStationID = scenario->getNodes().at(nodeStartID).nearestStation;
    NodeID nodeEndID = scenario->getStations().at(nearestStationID).node;

    // Calculamos viaje y consumo
    Time travelTime = calculateTravelTime(robotID, nodeStartID, nodeEndID);
    BatteryLevel finalBattery= calculateBatteryConsumption(robotID, travelTime, scenario->robots.at(robotID).batteryRateWhileNavigating);

    if (finalBattery < 0.0){ // FAIL: El robot se ha quedado sin batería
        Time failTime = robot.batteryLevel / scenario->robots.at(robotID).batteryRateWhileNavigating;
        logger->logBatteryConsumption(robot.time, robotID, failTime, robot.batteryLevel, 0.0);
        robot.time = globalTime + failTime; // Sincronizamos con el reloj global
        logger->logRobotFailed(globalTime + failTime, robotID, scenario->getNodes().at(nodeEndID).coords);
        robot.status = RobotStatus::FAILED; 
        robot.batteryLevel = 0.0;
    } else {
        if (travelTime > 0) {
            logger->logBatteryConsumption(globalTime, robotID, travelTime, robot.batteryLevel, finalBattery);
            logger->logNavigation(globalTime, robotID, travelTime, 
                                    scenario->getNodes().at(nodeStartID).coords, 
                                    scenario->getNodes().at(nodeEndID).coords);
        }
        Distance dist = (nodeStartID == nodeEndID) ? 0.0 : scenario->getNodes().at(nodeStartID).neighbors.at(nodeEndID);
        robot.travelDistance += dist; // <--- Acumulamos la distancia recorrida

        robot.node = nodeEndID;
        robot.time = globalTime + travelTime; 

        logger->logRobotRecharge(globalTime + travelTime, robotID, nearestStationID,
             scenario->getNodes().at(nodeEndID).coords, scenario->robots.at(robotID).batteryCapacity);

        // El robot recarga y vuelve a estar DISPONIBLE
        robot.status = RobotStatus::AVAILABLE;
        robot.batteryLevel = scenario->robots.at(robotID).batteryCapacity;
        robot.onTask = NULL_ID; // Limpiamos por seguridad

        // NUEVO: Programamos un evento para que el robot tome una nueva decisión
        scheduleRobotDecision(robot.time, robotID);
    }    
}

void DistributedSimulator::simulateTask(RobotID robotID, TaskID taskID) {
    auto& robot = state.getRobot(robotID);
    auto& task = state.getTask(taskID);
    const auto& taskInfo = scenario->getTasks().at(taskID);

    // 1. NAVEGACION
    NodeID nodeStartID = robot.node;
    NodeID nodeEndID = taskInfo.node;
    Time travelTime = calculateTravelTime(robotID, nodeStartID, nodeEndID);
    BatteryLevel finalBattery= calculateBatteryConsumption(robotID, travelTime, scenario->robots.at(robotID).batteryRateWhileNavigating);

    if (finalBattery < 0.0){ // FAIL: El robot se ha quedado sin batería
        Time failTime = robot.batteryLevel / scenario->robots.at(robotID).batteryRateWhileNavigating;
        logger->logBatteryConsumption(robot.time, robotID, failTime, robot.batteryLevel, 0.0);
        robot.time = globalTime + failTime;
        logger->logRobotFailed(globalTime + failTime, robotID, scenario->getNodes().at(nodeEndID).coords);
        robot.status = RobotStatus::FAILED; 
        robot.batteryLevel = 0.0;
    } else { // EXITO: El robot ha llegado con exito
        if (travelTime > 0) {
            logger->logBatteryConsumption(globalTime, robotID, travelTime, robot.batteryLevel, finalBattery);
            logger->logNavigation(globalTime, robotID, travelTime, 
                                    scenario->getNodes().at(nodeStartID).coords, 
                                    scenario->getNodes().at(nodeEndID).coords);
        }                        
        Distance dist = (nodeStartID == nodeEndID) ? 0.0 : scenario->getNodes().at(nodeStartID).neighbors.at(nodeEndID);
        robot.travelDistance += dist; // <--- Acumulamos la distancia recorrida
        
        robot.batteryLevel = finalBattery;
        robot.node = nodeEndID;
        Time arrivalTime = globalTime + travelTime;
        robot.time = arrivalTime;
        robot.status = RobotStatus::WAITING; 
        robot.onTask = taskID;

        task.assignedWorkers++;
        
        // CORRECCIÓN CLAVE DE TIMESTAMPS: 
        // El initTime de la tarea será el máximo entre el arrivalTime de este robot,
        // lo que ya tuviera de antes, o el earliestStart exigido por el escenario.
        task.initTime = std::max({task.initTime, arrivalTime, taskInfo.earliestStart});

        // NOTA: No necesitamos poner robot.time = INFINITY. Al estar en estado WAITING, 
        // el simulador simplemente no lo evaluará para tomar decisiones hasta que expireTask 
        // o resolveTask lo devuelvan a AVAILABLE y creen un nuevo evento en la cola.

        // COMPROBAMOS NUMERO DE ROBOTS EN LA TAREA
        if (task.assignedWorkers == taskInfo.requiredWorkers) { 
            task.status = TaskStatus::ASSIGNED; // Ya no es PENDING, aunque aún no se ha resuelto
            // La tarea se programa para resolverse exactamente en el initTime calculado
            eventQueue.push({task.initTime, EventType::TASK_RESOLUTION, NULL_ID, taskID});
        }
    }
}

// Extrae toda la lógica probabilística que tenías en simulateTask
void DistributedSimulator::resolveTask(TaskID taskID) {
    auto& task = state.getTask(taskID);
    const auto& taskInfo = scenario->getTasks().at(taskID);

    // Si por algún motivo la tarea ya fue cancelada (ej. por expiración), abortamos
    if (task.status == TaskStatus::FAILED || task.status == TaskStatus::COMPLETED) return;

    task.attempts++;
    task.status = TaskStatus::ASSIGNED;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    bool success = dis(gen) < taskInfo.successProb;

    Time mu = success ? taskInfo.averageSuccessTime : taskInfo.averageFailTime;
    Time sigma = success ? taskInfo.stdSuccessTime : taskInfo.stdFailTime;
    std::normal_distribution<> norm(mu, sigma);
    Time execTime = std::max(0.0, norm(gen));

    BatteryLevel averageDemand = success ? taskInfo.averageSuccessDemand : taskInfo.averageFailDemand;
    Time averageTime = success ? taskInfo.averageSuccessTime : taskInfo.averageFailTime;
    // Protección contra división por cero (NaN)
    BatteryRate rate = 0.0;
    if (averageTime > 0.0) {
        rate = averageDemand / averageTime;
    }
    
    BatteryLevel consumptionInitial = rate * execTime;

    std::vector<RobotID> workers;
    
    // Calcular quién sobrevive y quién muere por batería
    for (auto& [workerID, w] : state.getRobots()) {
        if (w.onTask == taskID) {
            workers.emplace_back(workerID);
            BatteryLevel fBat = w.batteryLevel - consumptionInitial;
            if (fBat < 0.0) { 
                success = false;
                Time tFail = w.batteryLevel / rate;
                execTime = (tFail < execTime) ? tFail : execTime;
            }
            logger->logTaskExecution(task.initTime, taskID, workerID, scenario->nodes.at(taskInfo.node).coords, execTime);
        }
    }

    BatteryLevel consumptionFinal = rate * execTime;

    // Actualizar robots y reprogramar sus cerebros
    for (RobotID workerID : workers) {
        auto& w = state.getRobot(workerID);
        logger->logBatteryConsumption(task.initTime, workerID, execTime, w.batteryLevel, w.batteryLevel - consumptionFinal);
        
        w.time = task.initTime + execTime; // Momento en que quedan libres
        w.batteryLevel -= consumptionFinal;
        
        if (w.batteryLevel <= 0.0) { 
            w.batteryLevel = 0.0;
            logger->logRobotFailed(task.initTime, workerID, scenario->getNodes().at(taskInfo.node).coords);
            w.status = RobotStatus::FAILED; 
        } else {
            w.status = RobotStatus::AVAILABLE;
            w.onTask = NULL_ID;
            // NUEVO: ¡El robot está libre! Programamos su próxima decisión
            scheduleRobotDecision(w.time, workerID);
        }
    }

    task.finalTime = task.initTime + execTime;
    // UNA VEZ QUE HEMOS COMPROBADO QUE LOS ROBOTS NO SE HAYAN QUEDADO SIN BATERIA PODEMOS DEFINIR EL RESULTADO
    if (success) {
        task.status = TaskStatus::COMPLETED;
    } else {
        // AQUI VA EL TEMA DEL NUMERO DE INTENTOS
        task.status = TaskStatus::FAILED;
    }

    std::string statusStr = success ? "COMPLETED" : "FAILED";
    logger->logTaskResolution(task.finalTime, taskID, scenario->nodes.at(taskInfo.node).coords, statusStr, 1);
}

// Lógica de caducidad
void DistributedSimulator::expireTask(TaskID taskID) {
    auto& task = state.getTask(taskID);
    
    // Si la tarea ya se resolvió o se está resolviendo, ignoramos el evento
    if (task.status == TaskStatus::COMPLETED || task.status == TaskStatus::FAILED || task.status == TaskStatus::ASSIGNED) return;

    // Si llegamos aquí, la tarea caducó sin suficientes workers
    task.status = TaskStatus::FAILED;
    
    // Liberamos a los robots que estaban WAITING tontamente
    for (auto& [workerID, _] : state.getRobots()) {
        auto& w = state.getRobot(workerID);
        if (w.onTask == taskID) {
            w.status = RobotStatus::AVAILABLE;
            w.onTask = NULL_ID;
            // Despiertan y toman una decisión AHORA (en el tiempo global actual)
            w.time = globalTime;
            scheduleRobotDecision(globalTime, workerID);
        }
    }
}

void DistributedSimulator::scheduleRobotDecision(Time t, RobotID id) {
    static thread_local std::mt19937 generador(std::random_device{}());
    // Un rango amplio para garantizar que las colisiones de empate sean casi imposibles
    std::uniform_int_distribution<int> distribucion(0, 10000000); 

    Event ev;
    ev.time = t;
    ev.type = EventType::ROBOT_DECISION;
    ev.robotID = id;
    ev.taskID = NULL_ID;
    ev.randomTieBreaker = distribucion(generador); // Magia: desempate automático al encolar

    eventQueue.push(ev);
}

} // namespace tau