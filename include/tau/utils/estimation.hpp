#pragma once

#include <cmath>
#include <algorithm>
#include "../scenario.hpp"
#include "../definitions.hpp"

// Constantes matemáticas (algunos compiladores no tienen M_PI por defecto sin macros)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

namespace tau {
namespace utils {

// Función de Densidad de Probabilidad (PDF) de la normal estándar
inline double phi(double x) {
    return std::exp(-0.5 * x * x) / (std::sqrt(2.0 * M_PI));
}

// Función de Supervivencia (1 - CDF) de la normal estándar
// Usamos erfc (función de error complementaria) por precisión numérica
inline double survival_Phi(double x) {
    return 0.5 * std::erfc(x / M_SQRT2);
}

// Esperanza de una Normal truncada por la izquierda en 'a'
inline double truncated_normal_mean(double mu, double sigma, double a) {
    if (sigma <= 0.0) return std::max(mu, a);
    
    double alpha = (a - mu) / sigma;
    double denominator = survival_Phi(alpha);
    
    // Si la probabilidad es numéricamente cero, asumimos que termina instantáneamente
    if (denominator < 1e-9) return a; 
    
    return mu + sigma * (phi(alpha) / denominator);
}

// Calcula cuándo se estima que un robot quedará libre
inline Time estimateNextDecisionTime(const Robot& robot, const std::map<TaskID, TaskInfo>& scenarioTasks, const std::map<TaskID, Task>& currentTasks, Time currentTime) {
    
    // Si el robot no está involucrado en una tarea, devolvemos su tiempo programado actual
    if (robot.status != RobotStatus::WAITING && robot.status != RobotStatus::EXECUTING) {
        return std::max(currentTime, robot.time);
    }

    TaskID tId = robot.onTask;
    if (tId == NULL_ID) return std::max(currentTime, robot.time);

    const TaskInfo taskInfo = scenarioTasks.at(tId);
    const Task dynTask = currentTasks.at(tId);

    // CONDICIÓN 1: La tarea está PENDING (no hay suficientes robots asignados)
    // El robot quedará libre en el peor de los casos cuando la tarea caduque.
    if (dynTask.status == TaskStatus::PENDING) {
        return std::max(currentTime, taskInfo.latestStart);
    }

    // CONDICIÓN 2: La tarea está ASSIGNED (suficientes robots asignados, pero aún no ha empezado)
    // El tiempo será initTime + valor esperado no condicionado.
    if (dynTask.status == TaskStatus::ASSIGNED) {
        Time expectedDuration = (taskInfo.successProb * taskInfo.averageSuccessTime) + 
                                ((1.0 - taskInfo.successProb) * taskInfo.averageFailTime);
        // Garantizamos que no devolvemos un tiempo en el pasado
        return std::max(currentTime, dynTask.initTime) + expectedDuration;
    }

    // CONDICIÓN 3: La tarea está EXECUTING (ya comenzó, usamos probabilidad condicionada)
    if (dynTask.status == TaskStatus::EXECUTING) {
        Time elapsed_time = std::max(0.0, currentTime - dynTask.initTime);
        
        // Protecciones de división por cero en caso de que las desviaciones típicas sean 0.0
        double z_success = (taskInfo.stdSuccessTime > 0.0) ? (elapsed_time - taskInfo.averageSuccessTime) / taskInfo.stdSuccessTime : ((elapsed_time < taskInfo.averageSuccessTime) ? -1e9 : 1e9);
        double z_fail = (taskInfo.stdFailTime > 0.0) ? (elapsed_time - taskInfo.averageFailTime) / taskInfo.stdFailTime : ((elapsed_time < taskInfo.averageFailTime) ? -1e9 : 1e9);

        double prob_surv_success = survival_Phi(z_success);
        double prob_surv_fail = survival_Phi(z_fail);
        
        double marginal_surv = (prob_surv_success * taskInfo.successProb) + (prob_surv_fail * (1.0 - taskInfo.successProb));
        
        // Si marginal_surv es ~0, la tarea debería haber acabado. Estimamos que acaba ya.
        if (marginal_surv < 1e-9) return currentTime;

        // Probabilidad a posteriori de éxito (Bayes)
        double posterior_success = (prob_surv_success * taskInfo.successProb) / marginal_surv;
        
        // Esperanzas truncadas
        double expected_success_time = truncated_normal_mean(taskInfo.averageSuccessTime, taskInfo.stdSuccessTime, elapsed_time);
        double expected_fail_time = truncated_normal_mean(taskInfo.averageFailTime, taskInfo.stdFailTime, elapsed_time);
        
        // Esperanza total condicionada al tiempo transcurrido
        double expected_remaining_total = (posterior_success * expected_success_time) + ((1.0 - posterior_success) * expected_fail_time);
        
        return dynTask.initTime + expected_remaining_total;
    }

    // Si la tarea está COMPLETED o FAILED, el robot debería estar a punto de ser liberado por el evento TASK_END
    return std::max(currentTime, robot.time);
}

} // namespace utils
} // namespace tau