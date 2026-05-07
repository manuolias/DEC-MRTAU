#include "tau/scenario.hpp"
#include "tau/definitions.hpp"
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>

namespace tau {

// Función auxiliar interna (static) para calcular la distancia euclidiana
static Distance calculateDistance(const Position& p1, const Position& p2) {
    return std::sqrt(std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2));
}

std::shared_ptr<Scenario> Scenario::loadFromYAML(const std::string& filepath) {
    // make_shared reserva memoria en el "heap" y devuelve un puntero inteligente
    auto scenario = std::make_shared<Scenario>();
    
    try {
        YAML::Node config = YAML::LoadFile(filepath); // Abre y lee el archivo

        // 1. Leer propiedades globales (ya no leemos ntasks ni nrobots)
        scenario->name = config["name"].as<std::string>();
        scenario->height = config["height"].as<int>();
        scenario->width = config["width"].as<int>();
        scenario->initialTime = config["initial_time"].as<Time>();

        // 2. Leer Nodos
        // rawNeighbors nos sirve para guardar las conexiones antes de saber las coordenadas de todos
        std::map<NodeID, std::vector<NodeID>> rawNeighbors;
        
        // Bucle for-each (recorre todos los elementos de "graph")
        for (const auto& yamlNode : config["graph"]) {
            Node node;
            node.id = yamlNode["node"].as<NodeID>();
            node.description = yamlNode["description"].as<std::string>();
            node.coords.x = yamlNode["coords"][0].as<double>();
            node.coords.y = yamlNode["coords"][1].as<double>();
            
            scenario->nodes[node.id] = node;
            
            // Guardamos los vecinos de este nodo para procesarlos después
            for (const auto& neighborID : yamlNode["neighbors"]) {
                rawNeighbors[node.id].push_back(neighborID.as<NodeID>());
            }
        }

        // 3. Leer Tareas
        for (const auto& yamlTask : config["tasks"]) {
            TaskInfo task;
            task.id = yamlTask["id"].as<TaskID>();
            task.node = yamlTask["node"].as<NodeID>();
            task.description = yamlTask["description"].as<std::string>();
            task.earliestStart = yamlTask["time_window"][0].as<Time>();
            task.latestStart = yamlTask["time_window"][1].as<Time>();
            task.successProb = yamlTask["success_prob"].as<double>();
            task.averageSuccessTime = yamlTask["success_time"][0].as<Time>();
            task.stdSuccessTime = yamlTask["success_time"][1].as<Time>();
            task.averageFailTime = yamlTask["fail_time"][0].as<Time>();
            task.stdFailTime = yamlTask["fail_time"][1].as<Time>();
            task.requiredWorkers = yamlTask["required_workers"].as<int>();
            task.averageSuccessDemand = yamlTask["demand"][0].as<BatteryLevel>();
            task.averageFailDemand = yamlTask["demand"][1].as<BatteryLevel>();
            
            scenario->tasks[task.id] = task;
        }

        // 4. Leer Robots
        for (const auto& yamlRobot : config["robots"]) {
            RobotInfo robot;
            robot.id = yamlRobot["id"].as<RobotID>();
            robot.initialNode = yamlRobot["initial_node"].as<NodeID>();
            robot.description = yamlRobot["description"].as<std::string>();
            robot.initialBatteryLevel = yamlRobot["initial_battery_level"].as<BatteryLevel>();
            robot.batteryCapacity = yamlRobot["battery_capacity"].as<BatteryLevel>();
            robot.navigationVelocity = yamlRobot["navigation_velocity"].as<Velocity>();
            robot.batteryRateWhileNavigating = yamlRobot["battery_rate_while_navigating"].as<BatteryRate>();
            
            for (const auto& cap : yamlRobot["capabilities"]) {
                robot.capabilities.insert(cap.as<TaskID>());
            }
            
            scenario->robots[robot.id] = robot;
        }

        // 5. Leer Estaciones de Recarga
        if (config["recharging_stations"]) {
            for (const auto& yamlStation : config["recharging_stations"]) {
                StationInfo station;
                station.id = yamlStation["id"].as<StationID>();
                station.node = yamlStation["node"].as<NodeID>();
                station.description = yamlStation["description"].as<std::string>();
                
                scenario->stations[station.id] = station;
            }
        }

        // Segunda pasada: ahora que todos los nodos existen, calculamos las distancias euclidias 
        // Tambien almacenaremos para cada nodo su estacion de recarga mas cercana
        for (auto& [nodeID, node] : scenario->nodes) {
            node.nearestStation = NULL_ID;
            Distance minDistanceStation = std::numeric_limits<Distance>::infinity();
            for (NodeID neighborID : rawNeighbors[nodeID]) {
                if (scenario->nodes.count(neighborID)) { // Comprueba si el vecino existe
                    Distance dist = calculateDistance(node.coords, scenario->nodes[neighborID].coords);
                    node.neighbors[neighborID] = dist; // Guarda la conexión y la distancia física
                }
            }

            // Calculamos la estación de recarga más cercana entre todas las estaciones
            for (const auto& [stationID, station] : scenario->stations) {
                if (scenario->nodes.count(station.node)) {
                    Distance d = calculateDistance(node.coords, scenario->nodes[station.node].coords);
                    if (d < minDistanceStation) {
                        minDistanceStation = d;
                        node.nearestStation = stationID;
                    }
                }
            }
        }

    } catch (const YAML::Exception& e) {
        std::cerr << "Error parseando YAML: " << e.what() << std::endl;
        throw; // Escupe el error para que el programa se detenga
    }

    return scenario;
}

Distance Scenario::distanceBetween(NodeID a, NodeID b) const {
    if (a == b) return 0.0;
    auto itA = nodes.find(a);
    auto itB = nodes.find(b);
    if (itA == nodes.end() || itB == nodes.end()) {
        throw std::out_of_range("Scenario::distanceBetween: node id not found");
    }
    const auto& neigh = itA->second.neighbors;
    auto nit = neigh.find(b);
    if (nit != neigh.end()) return nit->second;
    // Fallback: distancia euclidiana directa entre coordenadas
    return calculateDistance(itA->second.coords, itB->second.coords);
}

} // namespace tau