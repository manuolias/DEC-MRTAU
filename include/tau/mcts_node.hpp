#ifndef TAU_MCTS_NODE_HPP
#define TAU_MCTS_NODE_HPP

#include <vector>
#include <memory>
#include "action.hpp"
#include "definitions.hpp"

namespace tau {

class MCTSNode : public std::enable_shared_from_this<MCTSNode> {
private:
    std::weak_ptr<MCTSNode> parent;
    std::vector<std::shared_ptr<MCTSNode>> children;

    Action actionToGetHere;
    std::vector<Action> untriedActions;

    // --- Variables del D-UCT (Discounted UCB) ---
    // Según Best et al., las visitas y recompensas antiguas valen menos
    double discountedVisits;
    double discountedReward;

    // --- Estado Virtual ---
    // Representa cómo estará el robot si llega hasta este nodo
    Robot virtualState;
    Time virtualTime;

    // Profundidad del nodo (para limitar el horizonte)
    int depth;

public:
    // Constructor para el nodo RAÍZ
    MCTSNode(const Robot& state, Time time, const std::vector<Action>& possibleActions)
        : actionToGetHere(Action(Action::Type::START)), // Acción ficticia para la raíz
          untriedActions(possibleActions),
          discountedVisits(0.0), discountedReward(0.0),
          virtualState(state), virtualTime(time),
          depth(0) {}

    // Constructor para nodos HIJO
    MCTSNode(std::shared_ptr<MCTSNode> parentNode, const Action& action, const Robot& state, Time time, const std::vector<Action>& possibleActions)
        : parent(parentNode), actionToGetHere(action),
          untriedActions(possibleActions),
          discountedVisits(0.0), discountedReward(0.0),
          virtualState(state), virtualTime(time),
          depth(parentNode->getDepth() + 1) {}

    // --- Métodos de Topología del Árbol ---
    void addChild(std::shared_ptr<MCTSNode> child) {
        children.push_back(child);
    }

    std::shared_ptr<MCTSNode> getParent() const { return parent.lock(); }
    void detachFromParent() { parent.reset(); }
    const std::vector<std::shared_ptr<MCTSNode>>& getChildren() const { return children; }
    const Action& getAction() const { return actionToGetHere; }
    
    // --- Métodos de Estado ---
    const Robot& getVirtualState() const { return virtualState; }
    Time getVirtualTime() const { return virtualTime; }

    // --- Métodos de Expansión ---
    bool isFullyExpanded() const { return untriedActions.empty(); }

    bool isTerminal() const { return actionToGetHere.getType() == Action::Type::FINISH; }

    Action popUntriedAction() {
        if (untriedActions.empty()) return Action(Action::Type::FINISH);
        Action a = untriedActions.back();
        untriedActions.pop_back();
        return a;
    }

    // --- Lógica D-UCT del Artículo ---
    void update(double reward, double gamma) {
        // Implementación iterativa del descuento temporal:
        // N_{new} = N_{old} * gamma + 1
        // Q_{new} = Q_{old} * gamma + reward
        discountedVisits = (discountedVisits * gamma) + 1.0;
        discountedReward = (discountedReward * gamma) + reward;
    }

    double getExpectedReward() const {
        if (discountedVisits <= 0.0) return 0.0;
        return discountedReward / discountedVisits;
    }

    double getDiscountedVisits() const { return discountedVisits; }

    int getDepth() const { return depth; }
};

} // namespace tau

#endif // TAU_MCTS_NODE_HPP