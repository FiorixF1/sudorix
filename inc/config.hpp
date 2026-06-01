#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "Event.hpp"

struct SolverConfig {
  bool enabledTechniques[256] = {};
  bool allPossibleSteps = false;
};

extern SolverConfig g_solverConfig;

void set_default_solver_config();

void ensure_solver_config_initialized();

bool is_technique_enabled(ReasonId reason);

#endif // CONFIG_HPP
