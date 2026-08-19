#ifndef SOLVER_H
#define SOLVER_H

#include <cstdint>
#include "types.hpp"

extern "C"
{
  const char *sudorix_solver_api(const char *requestJson);
} // extern "C"

// higher-level interface
json sudorix_solver_api(const json &request);

#endif // SOLVER_H
