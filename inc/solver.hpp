// Sudorix WASM Solver Core (C++)
// C++/WASM implements the solver engine; UI/gameplay stays in JS.
//
// Exported functions:
//   int sudorix_solver_full(const char *in81, char *out81);
//   int sudorix_solver_init_board(const char *in81);
//   int sudorix_solver_next_step(uint32_t *out, uint32_t out_words);
//   int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out, uint32_t out_words);
//
// JS -> WASM contract:
//   in81[81]   : char      (0 = empty, 1..9 = digit)
//   values[81] : uint8_t   (0 = empty, 1..9 = digit)
//   cands[81]  : uint16_t  (bit0..bit8 correspond to digits 1..9)
//
// Output string (out81[81] as char):
//   out81[81]  : char      (. = not solved, 1..9 = digit)
//
// Output buffer (out[5] as uint32_t):
//   out[0] = type     (0 = none, 1 = setValue, 2 = removeCandidate)
//   out[1] = reasonId (implementation-defined; mapped to label in JS)
//   out[2] = fromPrev (1 = popped from previously-filled queue, 0 = generated this iteration)
//   out[3] = count    (number of operations)
//   out[4..]          (operations as 'count' pairs of cell and value)
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

#ifndef SOLVER_H
#define SOLVER_H

#include <cstdint>

extern "C"
{
  int sudorix_solver_full(const char *in81, char *out81);
  
  int sudorix_solver_init_board(const char *in81);
  
  int sudorix_solver_next_step(uint32_t *out, uint32_t out_words);

  int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out, uint32_t out_words);
} // extern "C"

#endif // SOLVER_H
