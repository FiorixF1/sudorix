#ifndef SOLVER_H
#define SOLVER_H

#include <cstdint>

// Sudorix Solver Core API
//
// Exported functions:
//   int sudorix_solver_count_solutions(const char *in81);
//   int sudorix_solver_full(const char *in81, char *out81);
//   int sudorix_solver_init_board(const char *in81);
//   int sudorix_solver_next_step(uint32_t *out, uint32_t out_words);
//   int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out, uint32_t out_words);
//   int sudorix_solver_set_enabled_techniques(const uint32_t *reasons, uint32_t count);
//
// Input contract:
//   in81[81]   : char      ('.' or '0' = empty, '1'..'9' = digit)
//   values[81] : uint8_t   (0 = empty, 1..9 = digit)
//   cands[81]  : uint16_t  (bit0..bit8 correspond to digits 1..9)
//
// Output string:
//   out81[81]  : char      ('.' = not solved, '1'..'9' = digit), null-terminated at [81].
//
// Output buffer (uint32_t words, caller-allocated):
//   Layout:
//     out[0] = type      (0 = none, 1 = setValue, 2 = removeCandidate)
//     out[1] = reasonId         (macro technique, see Event.hpp)
//     out[2] = detailedReasonId (specific sub-technique, see Event.hpp)
//     out[3] = opCount          (#operations)
//     out[4] = srcCount         (#sources)
//     then srcCount pairs: (cells, mask)
//     then opCount  pairs: (idx, mask)
//
//   mask is 9-bit: bit0=digit1 .. bit8=digit9.
//   idx is a cell identified by an index in range 0..80.
//   cells identifies more cells inside a unit in 14 bits:
//     bit0..bit4=unitId (0..8 rows, 9..17 columns, 18..26 boxes)
//     bit5=cell1 .. bit13 = cell9
//
//   Minimum out_words is 5.
//   NOTE: For SetValue, mask must contain exactly one bit.
//
// State is managed by the caller for sudorix_solver_hint.
// State is managed by WASM for sudorix_solver_full and sudorix_solver_next_step.
// sudorix_solver_next_step requires an initial call to sudorix_solver_init_board.
//
// Notes:
//   - The event queue is stored in WASM as persistent state (g_eventQueue contains unique events).
//   - JS must provide a consistent board (values and candidates) before calling sudorix_solver_hint.
//   - JS must initialize the board with sudorix_solver_init_board before using sudorix_solver_next_step.
//   - JS does not need to manage the state when using sudorix_solver_full and sudorix_solver_next_step other than UI purpose.

extern "C"
{
  int sudorix_solver_count_solutions(const char *in81);

  int sudorix_solver_full(const char *in81, char *out81);
  
  int sudorix_solver_init_board(const char *in81);
  
  int sudorix_solver_next_step(uint32_t *out, uint32_t out_words);

  int sudorix_solver_export_board(uint8_t *values, uint16_t *cands);

  int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out, uint32_t out_words);

  int sudorix_solver_set_enabled_techniques(const uint32_t *reasons, uint32_t count);
} // extern "C"

#endif // SOLVER_H
