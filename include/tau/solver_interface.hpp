#ifndef TAU_SOLVER_INTERFACE_HPP
#define TAU_SOLVER_INTERFACE_HPP

#include <memory>
#include <optional> // <--- NUEVO: Necesario para std::optional
#include "scenario.hpp"
#include "observation.hpp"
#include "action.hpp"

namespace tau {

// La Interfaz pura que todos tus algoritmos heredarán
class ISolver {
public:
    virtual ~ISolver() = default;
        
    // El algoritmo decide basándose ÚNICAMENTE en su observación local y pública
    virtual Action decideNextAction(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) = 0;
    
    // El simulador preguntará esto antes de encolar eventos
    virtual bool requiresContinuousPlanning() const { 
        return false; 
    }

    // Método para algoritmos Anytime (como Dec-MCTS)
    // Permite al robot computar en segundo plano y devolver su nueva distribución de creencias.
    // Por defecto devuelve std::nullopt (útil para que CBAA/CBBA no tengan que implementarla obligatoriamente).
    virtual std::optional<Distribution> performBackgroundPlanning(const Observation& obs, const std::shared_ptr<const Scenario>& scenario) {
        return std::nullopt;
    }
};

} // namespace tau

#endif // TAU_SOLVER_INTERFACE_HPP