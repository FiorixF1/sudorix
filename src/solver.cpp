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

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <functional>
#include <set>
#include <vector>

#include "solver.hpp"
#include "SudokuBoard.hpp"
#include "EventQueue.hpp"
#include "utils.hpp"

static SudokuBoard g_sudokuBoard;
static EventQueue g_eventQueue;

// =========================================================
// Techniques
// =========================================================

static void techFullHouse(SudokuBoard &board) {
  auto scanUnit = [&](const Index unitCells[9]) -> void
  {
    Index emptyIdx = -1;
    Digit missingDigit = 0;
    Mask present = 0;

    for (int k = 0; k < 9; k++) {
      const Index idx = unitCells[k];
      const Digit v = board.getValue(idx);
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
      const Mask missingMask = (Mask)(0x1FFu & ~present);
      if (countBits9(missingMask) == 1) {
        missingDigit = bitToDigitSingle(missingMask);
        Event event(EventType::SetValue, ReasonId::FullHouse);
        event.addOperation(emptyIdx, missingDigit);
        g_eventQueue.enqueue(board, event);
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
  auto scanUnit = [&](const Index unitCells[9]) -> void
  {
    for (Digit digit = 1; digit <= 9; digit++) {
      Index foundIdx = -1;
      for (int k = 0; k < 9; k++) {
        const Index idx = unitCells[k];
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
        Event event(EventType::SetValue, ReasonId::HiddenSingle);
        event.addOperation(foundIdx, digit);
        g_eventQueue.enqueue(board, event);
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
    for (Digit digit = 1; digit <= 9; digit++) {
      std::vector<Index> positions;
      for (int k = 0; k < 9; k++) {
        const Index idx = BOX_CELLS[b][k];
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          positions.push_back(idx);
        }
      }

      size_t posCount = positions.size();
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

      const int r0 = idxRow(positions[0]);
      bool sameRow = true;
      for (Index pos : positions) {
        if (idxRow(pos) != r0) {
          sameRow = false;
          break;
        }
      }

      if (sameRow) {
        // remove digit from row r0, excluding cells in this box
        Event event(EventType::RemoveCandidate, reasonId);
        for (int k = 0; k < 9; k++) {
          const Index idx = ROW_CELLS[r0][k];
          if (idxBox(idx) == b) {
            continue;
          }
          if (!board.isSolved(idx) && board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
        g_eventQueue.enqueue(board, event);
      }

      const int c0 = idxCol(positions[0]);
      bool sameCol = true;
      for (Index pos : positions) {
        if (idxCol(pos) != c0) {
          sameCol = false;
          break;
        }
      }

      if (sameCol) {
        // remove digit from column c0, excluding cells in this box
        Event event(EventType::RemoveCandidate, reasonId);
        for (int k = 0; k < 9; k++) {
          const Index idx = COL_CELLS[c0][k];
          if (idxBox(idx) == b) {
            continue;
          }
          if (!board.isSolved(idx) && board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
        g_eventQueue.enqueue(board, event);
      }
    }
  }
}

static void techBoxLineReduction(SudokuBoard &board) {
  // rows
  for (int r = 0; r < 9; r++) {
    for (Digit digit = 1; digit <= 9; digit++) {
      std::vector<Digit> positions;
      for (int k = 0; k < 9; k++) {
        const Index idx = ROW_CELLS[r][k];
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          positions.push_back(idx);
        }
      }

      size_t posCount = positions.size();
      if (posCount < 2 || posCount > 3) {
        continue; // box line reduction is about confinement with 2 or 3
      }

      ReasonId reasonId = ReasonId::BoxLineReduction;

      std::set<int> boxes;
      bool sameBlock = true;
      for (Index pos : positions) {
        boxes.insert(idxBox(pos));
      }

      if (boxes.size() == 1) {
        // remove digit from this box, excluding cells in this row/column
        int boxIdx = *boxes.begin();

        Event event(EventType::RemoveCandidate, reasonId);
        for (int k = 0; k < 9; k++) {
          const Index idx = BOX_CELLS[boxIdx][k];
          if (idxRow(idx) == r) {
            continue;
          }
          if (!board.isSolved(idx) && board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
        g_eventQueue.enqueue(board, event);
      }
    }
  }

  // columns
  for (int c = 0; c < 9; c++) {
    for (Digit digit = 1; digit <= 9; digit++) {
      std::vector<Digit> positions;
      for (int k = 0; k < 9; k++) {
        const Index idx = COL_CELLS[c][k];
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          positions.push_back(idx);
        }
      }

      size_t posCount = positions.size();
      if (posCount < 2 || posCount > 3) {
        continue; // box line reduction is about confinement with 2 or 3
      }

      ReasonId reasonId = ReasonId::BoxLineReduction;

      std::set<Index> boxes;
      bool sameBlock = true;
      for (Index pos : positions) {
        boxes.insert(idxBox(pos));
      }

      if (boxes.size() == 1) {
        // remove digit from this box, excluding cells in this row/column
        Index boxIdx = *boxes.begin();

        Event event(EventType::RemoveCandidate, reasonId);
        for (int k = 0; k < 9; k++) {
          const Index idx = BOX_CELLS[boxIdx][k];
          if (idxCol(idx) == c) {
            continue;
          }
          if (!board.isSolved(idx) && board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
        g_eventQueue.enqueue(board, event);
      }
    }
  }
}

static void techNakedSingles(SudokuBoard &board) {
  for (int i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }
    const Digit d = board.getSingleCandidate(i);
    if (d != 0) {
      Event event(EventType::SetValue, ReasonId::NakedSingle);
      event.addOperation(i, d);
      g_eventQueue.enqueue(board, event);
    }
  }
}

typedef void (*TechniqueFn)(SudokuBoard &);

static constexpr TechniqueFn TECHNIQUES[] =
{
  techFullHouse,
  techHiddenSingles,
  techLockedCandidates,
  techNakedSingles,
  techBoxLineReduction
};

static bool is_operation_applicable(SudokuBoard &board, EventType type, Index idx, Digit digit) {
  // you can set only an unsolved cell
  if (type == EventType::SetValue) {
    return !board.isSolved(idx) && digit != 0;
  }
  // you can remove only existing candidates from an unsolved cell
  if (type == EventType::RemoveCandidate) {
    return !board.isSolved(idx) && board.hasCandidate(idx, digit) && digit != 0;
  }
  return false;
}

// Drain the next event and serialize the operations into out[].
// Layout (out_words is the capacity in uint32_t):
//   out[0] = eventType (0 none, 1 setValue, 2 removeCandidate)
//   out[1] = reasonId
//   out[2] = fromPrev (1 if coming from a previous iteration queue, 0 otherwise)
//   out[3] = count (number of operations)
//   then payload pairs (idx, digit) repeated count times.
//
// The function returns only events and operations that are applicable to the current 
// state of the board. This implies that some events in queue could be discarded.
// The function will continue the search until the queue is empty.
static int drain_event(SudokuBoard &board,
                       uint32_t *out,
                       uint32_t out_words,
                       uint32_t fromPrev,
                       bool apply_to_board) {
  if (!out || out_words < 4) {
    return 0;
  }

  Event first;
  if (!g_eventQueue.peek(first)) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    return 0;
  }

  const EventType type = first.type;
  const ReasonId reason = first.reason;

  const uint32_t max_ops = (out_words - 4u) / 2u;
  if (first.getNumberOfOperations() > max_ops) {
    // no space remaining in output buffer, TODO notify caller
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    return 0;
  }

  g_eventQueue.dequeue(first);
  out[0] = (uint32_t)type;
  out[1] = (uint32_t)reason;
  out[2] = fromPrev;
  out[3] = 0;

  uint32_t count = 0;
  for (const Operation &op : first.getOperations()) {
    // anti-duplication filter
    if (is_operation_applicable(board, type, op.idx, op.digit)) {
      out[4 + 2 * count + 0] = (uint32_t)op.idx;
      out[4 + 2 * count + 1] = (uint32_t)op.digit;
      count++;

      if (apply_to_board) {
        if (type == EventType::SetValue) {
          board.applySetValue(op.idx, op.digit);
        }
        if (type == EventType::RemoveCandidate) {
          board.applyRemoveCandidate(op.idx, op.digit);
        }
      }
    } // else discard invalid operations
  }
  out[3] = count;

  // if count equals 0, the entire event is discarded, continue draining
  return (count > 0) ? 1 : drain_event(board, out, out_words, fromPrev, apply_to_board);
}

// Run techniques to fill the queue if needed, then return a single event.
// If apply_to_board is true, the drained operations are also applied to 'board'.
static int compute_next_event(SudokuBoard &board,
                              uint32_t *out,
                              uint32_t out_words,
                              bool apply_to_board) {
  // 1) if we already have pending events, return them immediately.
  if (drain_event(board, out, out_words, 1u, apply_to_board)) {
    return 1;
  }

  // 2) run techniques in priority order; stop at the first technique that enqueues anything.
  for (size_t i = 0; i < (sizeof(TECHNIQUES) / sizeof(TECHNIQUES[0])); i++) {
    const size_t before = g_eventQueue.size();
    TECHNIQUES[i](board);
    if (g_eventQueue.size() != before) {
      break;
    }
  }

  // 3) if something has been generated, drain as "fromPrev=0".
  if (drain_event(board, out, out_words, 0u, apply_to_board)) {
    return 1;
  }

  // No events produced.
  if (out && out_words >= 4) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
  }
  return 0;
}

//
// FOR DEBUGGING compile with -DDEBUG and use this function:
// debug_log("Queue has %d elements", g_eventQueue.size());
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
    g_eventQueue = EventQueue();

    // Solve loop using existing stepper:
    // repeatedly compute one event, apply it locally, and continue until stuck.
    uint32_t tmp[1024];
    int guard = 0;
    const int guardMax = 200000;

    while (guard++ < guardMax) {
      const int ok = compute_next_event(board, tmp, 1024, true);
      if (!ok) {
        break;
      }
    }

    // Export
    for (int i = 0; i < 81; i++) {
      const Digit value = board.getValue(i);
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
    g_eventQueue = EventQueue();

    return 1;
  }

  // Performs and returns one step to solve the currently loaded board.
  // Returns 0 in case of error or no event is produced, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_next_step(uint32_t *out, uint32_t out_words) {
    if (out == nullptr || out_words < 4) {
      return 0;
    }

    // Compute one event, apply it locally and return it to the caller.
    const int ok = compute_next_event(g_sudokuBoard, out, out_words, true);
    return ok ? 1 : 0;
  }

  // Calculate and return one step to solve the board given as input (both values and candidates are given).
  // Returns 0 in case of error or no event is produced, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out, uint32_t out_words) {
    if (values == nullptr || cands == nullptr || out == nullptr || out_words < 4) {
      return 0;
    }

    // Build a temporary board owned by the caller (JS is the source of truth here).
    SudokuBoard board;
    if (!board.importFromBuffers(values, cands)) {
      out[0] = 0;
      out[1] = 0;
      out[2] = 0;
      out[3] = 0;
      return 0;
    }

    // Clear internal queue state for this hint computation.
    g_eventQueue = EventQueue();

    const int ok = compute_next_event(board, out, out_words, false);
    return ok ? 1 : 0;
  }
} // extern "C"
