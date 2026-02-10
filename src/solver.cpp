// Sudorix WASM Solver Core (C++)
// C++/WASM implements the solver engine; UI/gameplay stays in JS.
//
// Exported functions:
//   int sudorix_solver_full(const char *in81, char *out81);
//   int sudorix_solver_init_board(const char *in81);
//   int sudorix_solver_next_step(uint32_t *out);
//   int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out);
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
//   out[1] = idx      (0..80)
//   out[2] = digit    (1..9)
//   out[3] = reasonId (implementation-defined; mapped to label in JS)
//   out[4] = fromPrev (1 = popped from previously-filled queue, 0 = generated this iteration)
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

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <functional>

#include "solver.hpp"
#include "SudokuBoard.hpp"
#include "EventQueue.hpp"
#include "utils.hpp"

#ifdef __EMSCRIPTEN__
  // WASM
  #include <emscripten/emscripten.h>
#else
  // native
  #include <cstdio>
  #define EMSCRIPTEN_KEEPALIVE
  #define emscripten_log(x, fmt, ...) printf(fmt, ##__VA_ARGS__);
#endif

static SudokuBoard g_sudokuBoard;
static EventQueue g_eventQueue;

// =========================================================
// Techniques
// =========================================================

static void techFullHouse(SudokuBoard &board) {
  auto scanUnit = [&](const int unitCells[9]) -> void
  {
    int emptyIdx = -1;
    uint8_t missingDigit = 0;
    uint16_t present = 0;

    for (int k = 0; k < 9; k++) {
      const int idx = unitCells[k];
      const uint8_t v = board.getValue(idx);
      if (v == 0) {
        if (emptyIdx != -1) {
          emptyIdx = -2; // more than one empty
          break;
        }
        emptyIdx = idx;
      } else {
        present |= digitToBit(v);
      }
    }

    if (emptyIdx >= 0) {
      const uint16_t missingMask = (uint16_t)(0x1FFu & ~present);
      if (countBits9(missingMask) == 1) {
        missingDigit = bitToDigitSingle(missingMask);
        g_eventQueue.enqueueSetValue(board, emptyIdx, missingDigit, ReasonId::FullHouse);
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(BOX_CELLS[u]);
    scanUnit(ROW_CELLS[u]);
    scanUnit(COL_CELLS[u]);
  }
}

static void techHiddenSingles(SudokuBoard &board) {
  auto scanUnit = [&](const int unitCells[9]) -> void
  {
    for (uint8_t digit = 1; digit <= 9; digit++) {
      int foundIdx = -1;
      for (int k = 0; k < 9; k++) {
        const int idx = unitCells[k];
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          if (foundIdx != -1) {
            foundIdx = -2; // multiple places
            break;
          }
          foundIdx = idx;
        }
      }
      if (foundIdx >= 0) {
        g_eventQueue.enqueueSetValue(board, foundIdx, digit, ReasonId::HiddenSingle);
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(BOX_CELLS[u]);
  }
  for (int u = 0; u < 9; u++) {
    scanUnit(ROW_CELLS[u]);
  }
  for (int u = 0; u < 9; u++) {
    scanUnit(COL_CELLS[u]);
  }
}

static void techLockedCandidates(SudokuBoard &board) {
  // For each box and digit:
  //  - if all candidates are confined to a single row within the box,
  //    remove the digit from that row outside the box
  //  - same for a single column
  for (int b = 0; b < 9; b++) {
    for (uint8_t digit = 1; digit <= 9; digit++) {
      int positions[9];
      int posCount = 0;

      for (int k = 0; k < 9; k++) {
        const int idx = BOX_CELLS[b][k];
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          positions[posCount++] = idx;
        }
      }

      if (posCount < 2) {
        continue; // locked candidates is about confinement with at least 2
      }

      ReasonId reasonId;
      if (posCount == 2) {
        reasonId = ReasonId::PointingPair;
      } else if (posCount == 3) {
        reasonId = ReasonId::PointingTriple;
      } else {
        // generic name
        reasonId = ReasonId::LockedCandidates;
      }

      const uint8_t r0 = idxRow(positions[0]);
      bool sameRow = true;
      for (int i = 1; i < posCount; i++) {
        if (idxRow(positions[i]) != r0) {
          sameRow = false;
          break;
        }
      }

      if (sameRow) {
        // remove digit from row r0, excluding cells in this box
        for (int k = 0; k < 9; k++) {
          const int idx = ROW_CELLS[r0][k];
          if (idxBox(idx) == (uint8_t)b) {
            continue;
          }
          g_eventQueue.enqueueRemoveCandidate(board, idx, digit, reasonId);
        }
      }

      const uint8_t c0 = idxCol(positions[0]);
      bool sameCol = true;
      for (int i = 1; i < posCount; i++) {
        if (idxCol(positions[i]) != c0) {
          sameCol = false;
          break;
        }
      }

      if (sameCol) {
        // remove digit from column c0, excluding cells in this box
        for (int k = 0; k < 9; k++) {
          const int idx = COL_CELLS[c0][k];
          if (idxBox(idx) == (uint8_t)b) {
            continue;
          }
          g_eventQueue.enqueueRemoveCandidate(board, idx, digit, reasonId);
        }
      }
    }
  }
}

static void techNakedSingles(SudokuBoard &board) {
  for (int i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }
    const uint8_t d = board.getSingleCandidate(i);
    if (d != 0) {
      g_eventQueue.enqueueSetValue(board, i, d, ReasonId::NakedSingle);
    }
  }
}

typedef void (*TechniqueFn)(SudokuBoard &);

static constexpr TechniqueFn TECHNIQUES[] =
{
  &techFullHouse,
  &techHiddenSingles,
  &techLockedCandidates,
  &techNakedSingles
};

// shared by all interface functions
static int dequeue_event(SudokuBoard &board, Event &out, uint32_t &fromPrev) {
  // 1) If queue already has pending events from a previous iteration, return one immediately.
  if (!g_eventQueue.empty()) {
    const Event ev = g_eventQueue.front();
    g_eventQueue.pop();

    out.type = ev.type;
    out.idx = ev.idx;
    out.digit = ev.digit;
    out.reason = ev.reason;
    fromPrev = 1u; // fromPrev
    return 1;
  }

  // 2) Run techniques in priority order. Stop at the first technique that enqueues events.
  for (size_t i = 0; i < (sizeof(TECHNIQUES) / sizeof(TECHNIQUES[0])); i++) {
    const size_t before = g_eventQueue.size();
    TECHNIQUES[i](board);
    const bool produced = g_eventQueue.size() != before;
    if (produced) {
      break;
    }
  }

  // 3) Pop one event generated by this iteration (if any).
  if (!g_eventQueue.empty()) {
    const Event ev = g_eventQueue.front();
    g_eventQueue.pop();

    out.type = ev.type;
    out.idx = ev.idx;
    out.digit = ev.digit;
    out.reason = ev.reason;
    fromPrev = 0u; // generated this iteration
    return 1;
  }

  // nothing has been produced
  return 0;
}

//
// FOR DEBUGGING
// emscripten_log(EM_LOG_CONSOLE, "Queue has %d elements", g_eventQueue.size());
//

// =========================================================
// Public API exported to JS
// =========================================================

extern "C"
{
  // Solves an entire Sudoku given its initial representation in one shot.
  // Returns 0 in case of error, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_full(const char *in81, char *out81) {
    if (in81 == nullptr || out81 == nullptr) {
      return 0;
    }

    // Import Sudoku from string
    SudokuBoard board;
    if (!board.importFromString(in81)) {
      return 0;
    }

    // Reset queue
    while (!g_eventQueue.empty()) {
      g_eventQueue.pop();
    }

    // Solve loop using existing stepper:
    // repeatedly compute one event, apply it locally, and continue until stuck.
    Event ev;
    uint32_t fromPrev;
    int guard = 0;
    const int guardMax = 200000;

    while (guard++ < guardMax) {
      if (!dequeue_event(board, ev, fromPrev)) {
        break;
      }

      if (ev.type == EventType::None) {
        break;
      }

      if (ev.type == EventType::SetValue) {
        board.applySetValue(ev.idx, ev.digit);
      } else if (ev.type == EventType::RemoveCandidate) {
        board.applyRemoveCandidate(ev.idx, ev.digit);
      } else {
        break;
      }
    }

    // Export
    for (int i = 0; i < 81; i++) {
      const uint8_t value = board.getValue(i);
      out81[i] = value ? (char)('0' + value) : '.';
    }
    out81[81] = '\0';

    return 1;
  }

  // Initializes the board for a step-by-step solution.
  // Returns 0 in case of error, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_init_board(const char *in81) {
    if (in81 == nullptr) {
      return 0;
    }

    // Import Sudoku from string (WASM is the source of truth)
    if (!g_sudokuBoard.importFromString(in81)) {
      return 0;
    }

    // Reset queue
    while (!g_eventQueue.empty()) {
      g_eventQueue.pop();
    }

    return 1;
  }

  // Performs and returns one step to solve the currently loaded board.
  // Returns 0 in case of error or no event is produced, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_next_step(uint32_t *out) {
    if (out == nullptr) {
      return 0;
    }

    // Compute one event, apply it locally and return it to the caller.
    Event ev;
    uint32_t fromPrev;
    if (!dequeue_event(g_sudokuBoard, ev, fromPrev)) {
      // nothing has been produced
      out[0] = 0;
      out[1] = 0;
      out[2] = 0;
      out[3] = 0;
      out[4] = 0;
      return 0;
    } else {
      // serialize event and send it back
      out[0] = (uint32_t)ev.type;
      out[1] = ev.idx;
      out[2] = ev.digit;
      out[3] = (uint32_t)ev.reason;
      out[4] = 0u; // generated this iteration

      // apply event to local copy
      if (ev.type == EventType::SetValue) {
        g_sudokuBoard.applySetValue(ev.idx, ev.digit);
      } else if (ev.type == EventType::RemoveCandidate) {
        g_sudokuBoard.applyRemoveCandidate(ev.idx, ev.digit);
      }

      return 1;
    }
  }

  // Calculate and return one step to solve the board given as input (both values and candidates are given).
  // Returns 0 in case of error or no event is produced, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out) {
    if (values == nullptr || cands == nullptr || out == nullptr) {
      return 0;
    }

    // Load board snapshot (JS is the source of truth)
    SudokuBoard board;
    if (!board.importFromBuffers(values, cands)) {
      return 0;
    }

    // Reset queue
    while (!g_eventQueue.empty()) {
      g_eventQueue.pop();
    }

    Event ev;
    uint32_t fromPrev;
    if (!dequeue_event(board, ev, fromPrev)) {
      // nothing has been produced
      out[0] = 0;
      out[1] = 0;
      out[2] = 0;
      out[3] = 0;
      out[4] = 0;
      return 0;
    } else {
      // serialize event and send it back
      out[0] = (uint32_t)ev.type;
      out[1] = ev.idx;
      out[2] = ev.digit;
      out[3] = (uint32_t)ev.reason;
      out[4] = 0u; // generated this iteration
      return 1;
    }
  }
} // extern "C"
