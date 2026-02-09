#ifndef SOLVER_H
#define SOLVER_H

#include <cstdint>

extern "C"
{
  int sudorix_solver_full(const char *in81, char *out81);
  
  int sudorix_solver_init_board(const char *in81);
  
  int sudorix_solver_next_step(uint32_t *out);

  int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out);
} // extern "C"

#endif // SOLVER_H