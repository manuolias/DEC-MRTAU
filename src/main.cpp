#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>
#include <cstdlib>


#include "tau/scenario.hpp"
#include "tau/logger.hpp"
#include "tau/simulator.hpp"
#include "tau/solver_interface.hpp"
#include "tau/solver_greedy.hpp"
#include "tau/solver_random.hpp"
#include "tau/solver_CBAA.hpp"
#include "tau/solver_CBBA.hpp"
#include "tau/solver_DecMCTS_v1.hpp"
#include "tau/solver_DecMCTS_v2.hpp"
#include "tau/solver_DecMCTS_v3.hpp"
#include "tau/solver_DecMCTS_v4.hpp"
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

int main(int argc, char** argv) {
    try {
        // Rutas de entrada y salida. Pueden pasarse por argumentos para no
        // recompilar entre tandas:  ./simulador <scenariosPath> <logsDir> [configPath]
        std::string configPath    = (argc > 3) ? argv[3] : "../data/experiment_config.yaml";
        std::string scenariosPath = (argc > 1) ? argv[1] : "../data/";
        std::string logsDir       = (argc > 2) ? argv[2] : "../logs/fixed_sim";

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

            // std::cout << "Escenarios encontrados: " << scenarioFile << "\n";

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

                        // Saltar si el log ya existe y está completo
                        bool alreadyDone = false;
                        if (fs::exists(logFilename.str())) {
                            std::ifstream logCheck(logFilename.str());
                            std::string lastLine, line;
                            while (std::getline(logCheck, line)) {
                                if (!line.empty()) lastLine = line;
                            }
                            if (lastLine.find("metric: computing_time;") != std::string::npos) {
                                alreadyDone = true;
                            }
                        }

                        if (alreadyDone) {
                            std::cout << "[" << currentSim << "/" << totalSimulations << "] Saltando (ya completado): "
                                      << scenarioName << " | " << solverName << " | " << rewardName << " | Rep: " << rep << "\n";
                            currentSim++;
                            continue;
                        }

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
                        for (const auto& [robotID, _] : scenario->getRobots()) {
                            std::shared_ptr<tau::ISolver> solver;
                            
                            if (solverName == "greedy") {
                                solver = std::make_shared<tau::GreedySolver>();
                            } else if (solverName == "random") {
                                solver = std::make_shared<tau::RandomSolver>();
                            } else if (solverName == "cbaa") {
                                solver = std::make_shared<tau::CBAASolver>(robotID);
                            } else if (solverName == "cbba") {
                                solver = std::make_shared<tau::CBBASolver>(robotID);
                            } else if (solverName == "dec-mcts-v1") {
                                solver = std::make_shared<tau::DecMCTSSolverV1>(robotID);
                            } else if (solverName == "dec-mcts-v2") {
                                solver = std::make_shared<tau::DecMCTSSolverV2>(robotID); // gamma=0.999, useDiffReward=false
                            } else if (solverName == "dec-mcts-nodiff") {
                                // Ablación H1: sin doble rollout (usa reward_with directo)
                                solver = std::make_shared<tau::DecMCTSSolverV2>(robotID, 0.999, false);
                            } else if (solverName == "dec-mcts-gamma99") {
                                // Ablación H4: GAMMA=0.99 (descuento más agresivo)
                                solver = std::make_shared<tau::DecMCTSSolverV2>(robotID, 0.99, true);
                            } else if (solverName == "dec-mcts-v3") {
                                // V3: chance nodes + estadística doble n_disc/n_avail_disc
                                solver = std::make_shared<tau::DecMCTSSolverV3>(robotID);
                            } else if (solverName == "dec-mcts-v3.2") {
                                // V3: chance nodes + estadística doble n_disc/n_avail_disc
                                solver = std::make_shared<tau::DecMCTSSolverV3>(robotID, 0.999, true);
                            } else if (solverName == "dec-mcts-v4") {
                                // V4: rollout consciente de ventanas (urgencia), widening progresivo
                                // + expansión ordenada por heurística, selección filtrada por
                                // factibilidad, poda de hijos stale y tie-break de earliness
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID);
                            } else if (solverName == "dec-mcts-v4-c035") {
                                // Ablación V4: exploración más explotadora (C=0.35)
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID, 0.999, false, 0.35);
                            } else if (solverName == "dec-mcts-v4-g9999") {
                                // Ablación V4: descuento más lento (gamma=0.9999, más memoria)
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID, 0.9999, false);
                            } else if (solverName == "dec-mcts-v4-nocomm") {
                                // Ablación C1: idéntico a dec-mcts-v4-g9999 pero IGNORANDO las
                                // distribuciones comunicadas por los vecinos. Mide cuánto aporta
                                // realmente el canal de comunicación de Dec-MCTS.
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID, 0.9999, false,
                                                                                0.7, 30, 300, false);
                            } else if (solverName == "dec-mcts-v4-b4") {
                                // Mejora B4: idéntico a dec-mcts-v4-g9999 pero con blockingProb
                                // PROBABILÍSTICA en vez de binaria — descuenta una tarea según la
                                // probabilidad de que los vecinos lleguen realmente a cubrirla,
                                // atendiendo a su posición en el bundle y a la varianza acumulada.
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID, 0.9999, false,
                                                                                0.7, 30, 300, true, true);
                            } else if (solverName == "dec-mcts-v4-d8") {
                                // D-08: bundle comunicado construido siguiendo la cadena más
                                // visitada RESTRINGIDA a EXECUTE_TASK. Corrige que el 69% de
                                // las cadenas se cortaran en un nodo FINISH, dejando los planes
                                // comunicados con longitud media 1.25.
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID, 0.9999, false,
                                                                                0.7, 30, 300, true, false, true);
                            } else if (solverName == "dec-mcts-v4-d8b4") {
                                // D-08 + B4 combinadas: con bundles profundos, la blockingProb
                                // probabilística sí tiene profundidad sobre la que operar.
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID, 0.9999, false,
                                                                                0.7, 30, 300, true, true, true);
                            } else if (solverName == "dec-mcts-v4-g9999-hc") {
                                // V4 g9999 con ALTO CÓMPUTO. iteraciones por latido y de
                                // emergencia configurables vía env DECMCTS_ITERS / DECMCTS_EMERG
                                // (por defecto 1200 / 12000) para calibrar ~5 min/run.
                                int iters = 1200, emerg = 12000;
                                if (const char* e = std::getenv("DECMCTS_ITERS"))  iters = std::atoi(e);
                                if (const char* e = std::getenv("DECMCTS_EMERG")) emerg = std::atoi(e);
                                solver = std::make_shared<tau::DecMCTSSolverV4>(robotID, 0.9999, false, 0.7, iters, emerg);
                            } else {
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
