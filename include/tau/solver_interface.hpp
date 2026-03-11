#ifndef TAU_SOLVER_INTERFACE_HPP
#define TAU_SOLVER_INTERFACE_HPP

#include <memory>
#include <vector>
#include "scenario.hpp"
#include "observation.hpp"
#include "action.hpp"


namespace tau {

// La Interfaz pura que todos tus algoritmos heredarán
class ISolver {
public:
    virtual ~ISolver() = default;
        
    // El algoritmo decide basándose ÚNICAMENTE en su observación local
    virtual Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) = 0;
    
    // Método para que el simulador inyecte mensajes en el "cerebro" del robot
    virtual void receiveMessage(const Message& msg) = 0;

    // Opcional: Método para que el robot emita mensajes hacia el simulador
    virtual std::vector<Message> getOutbox() = 0; 
};

} // namespace tau

#endif // TAU_SOLVER_INTERFACE_HPP