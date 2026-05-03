#include <iostream>
#include <memory>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>


#include "tau/scenario.hpp"
#include "tau/logger.hpp"
#include "tau/simulator.hpp"
#include "tau/solver_interface.hpp"
#include "tau/solver_greedy.hpp"
#include "tau/solver_random.hpp"
#include "tau/solver_CBAA.hpp"
#include "tau/solver_CBBA.hpp"
#include "tau/reward_00.hpp"

namespace fs = std::filesystem;

// Estructura para almacenar la configuración del experimento
struct ExperimentConfig {
    std::vector<std::string> solvers;
    std::vector<std::string> reward_functions;
    int replicas;
};

// Función para leer el experiment_config.yaml
ExperimentConfig loadExperimentConfig(const std::string& path) {
    YAML::Node configNode = YAML::LoadFile(path);
    ExperimentConfig config;
    
    for (const auto& solver : configNode["solvers"]) {
        config.solvers.push_back(solver.as<std::string>());
    }
    for (const auto& reward : configNode["reward_functions"]) {
        config.reward_functions.push_back(reward.as<std::string>());
    }
    config.replicas = configNode["replicas"].as<int>();
    
    return config;
}

// Función para obtener todos los archivos .yaml de una ruta (archivo o directorio)
std::vector<std::string> getScenarioFiles(const std::string& path) {
    std::vector<std::string> files;
    if (fs::is_directory(path)) {
        for (const auto& entry : fs::directory_iterator(path)) {
            // Solo procesar archivos .yaml y excluir el propio archivo de configuración si estuviera ahí
            if (entry.path().extension() == ".yaml" && entry.path().filename() != "experiment_config.yaml") {
                files.push_back(entry.path().string());
            }
        }
    } else {
        files.push_back(path); // Es un archivo suelto
    }
    return files;
}

int main() {
    try {
        // Rutas de entrada y salida (Puedes cambiarlas según tu estructura)
        std::string configPath = "../data/experiment_config.yaml";
        std::string scenariosPath = "../data/"; // Puede ser carpeta o archivo (ej. "../data/scenario_12_06_01.yaml")
        std::string logsDir = "../logs/prueba";       // Carpeta de destino para los logs

        // Asegurarnos de que el directorio de logs existe
        if (!fs::exists(logsDir)) {
            fs::create_directories(logsDir);
        }

        std::cout << "--- Iniciando Framework de Experimentación ---\n";
        
        // 1. Cargar configuración del experimento
        ExperimentConfig config = loadExperimentConfig(configPath);
        std::cout << "Solvers a evaluar: " << config.solvers.size() << "\n";
        std::cout << "Recompensas a evaluar: " << config.reward_functions.size() << "\n";
        std::cout << "Réplicas por test: " << config.replicas << "\n";

        // 2. Obtener escenarios
        std::vector<std::string> scenarioFiles = getScenarioFiles(scenariosPath);
        std::cout << "Escenarios encontrados: " << scenarioFiles.size() << "\n\n";

        // 3. Ejecutar las simulaciones (Bucles anidados)
        int totalSimulations = scenarioFiles.size() * config.solvers.size() * config.reward_functions.size() * config.replicas;
        int currentSim = 1;

        for (const std::string& scenarioFile : scenarioFiles) {
            auto baseScenario = tau::Scenario::loadFromYAML(scenarioFile);
            std::string scenarioName = baseScenario->name; // Asumiendo que has puesto "name" en el yaml del escenario

            for (const std::string& solverName : config.solvers) {
                for (const std::string& rewardName : config.reward_functions) {
                    for (int rep = 1; rep <= config.replicas; ++rep) {
                        
                        // Generar el nombre del log: ej. scenario_12_06_01_greedy_reward01_001.log
                        std::ostringstream logFilename;
                        logFilename << logsDir << "/" << scenarioName 
                                    << "_" << solverName 
                                    << "_" << rewardName 
                                    << "_" << std::setw(3) << std::setfill('0') << rep << ".log";

                        std::cout << "[" << currentSim << "/" << totalSimulations << "] Ejecutando: " 
                                  << scenarioName << " | " << solverName << " | " << rewardName << " | Rep: " << rep << "...\n";

                        // Recargar el escenario para cada réplica (así restauramos baterías, tiempos, etc.)
                        auto scenario = tau::Scenario::loadFromYAML(scenarioFile);
                        auto logger = std::make_shared<tau::Logger>(logFilename.str());
                        
                        // Opcional: Escribir la info de la función de recompensa y solver en el log
                        logger->logInfo("solver_name", solverName);
                        logger->logInfo("reward_function", rewardName);
                        logger->logInfo("replica", std::to_string(rep));

                        tau::DistributedSimulator simulator(scenario, logger);

                        // --- NUEVO: Instanciación Distribuida de Solvers ---
                        // Iteramos sobre todos los robots definidos en el escenario
                        for (const auto& [robotID, robotInfo] : scenario->getRobots()) {
                            std::shared_ptr<tau::ISolver> solver;
                            
                            if (solverName == "greedy") {
                                solver = std::make_shared<tau::GreedySolver>();
                            } else if (solverName == "random") {
                                solver = std::make_shared<tau::RandomSolver>();
                            } else if (solverName == "cbaa") {
                                solver = std::make_shared<tau::CBAASolver>(robotID);
                            } else if (solverName == "cbba") { 
                                solver = std::make_shared<tau::CBBASolver>(robotID); 
                            }
                            // Aquí añadirás otros en el futuro:
                            // else if (solverName == "cbaa") { 
                            //     solver = std::make_shared<tau::CBAASolver>(robotID); 
                            // }
                            else {
                                throw std::runtime_error("Solver no reconocido: " + solverName);
                            }
                            
                            // Registramos este cerebro ÚNICO para este robot específico
                            simulator.registerSolver(robotID, solver);
                        }
                    
                        // 3. Registrar el Reward
                        std::shared_ptr<tau::RewardFunction> rewardFunc;
                        if (rewardName == "reward00") {
                            rewardFunc = std::make_shared<tau::RewardFunction00>();
                        } else {
                            throw std::runtime_error("Reward no reconocido: " + rewardName);
                        }

                        simulator.registerReward(rewardFunc);
                        
                        // Ejecutar la simulación para esta réplica
                        simulator.run();
                        currentSim++;
                        
                    }
                }
            }
        }

        std::cout << "\n--- ¡Todos los experimentos finalizados con éxito! ---\n";

    } catch (const std::exception& e) {
        std::cerr << "Error crítico durante la experimentación: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
