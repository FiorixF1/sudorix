#include "config.hpp"

SolverConfig g_solverConfig;

void set_default_solver_config() {
  for (bool &enabled : g_solverConfig.enabledTechniques) {
    enabled = true;
  }
  g_solverConfig.allPossibleSteps = false;
}

void ensure_solver_config_initialized() {
  static bool initialized = false;
  if (!initialized) {
    set_default_solver_config();
    initialized = true;
  }
}

bool is_technique_enabled(ReasonId reason) {
  return g_solverConfig.enabledTechniques[(uint8_t)reason];
}
