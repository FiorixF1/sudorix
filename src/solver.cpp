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
  auto scanUnit = [&](const IndexSet &unit) -> void
  {
    Index emptyIdx = -1;
    Digit missingDigit = 0;
    DigitSet present;

    for (Index idx : unit) {
      const Digit value = board.getValue(idx);
      if (value == 0) {
        if (emptyIdx != -1) {
          emptyIdx = -2; // more than one empty
          break;
        }
        emptyIdx = idx;
      } else {
        present.insert(value);
      }
    }

    if (emptyIdx >= 0) {
      const DigitSet missingDigits = ALL_DIGITS - present;
      if (missingDigits.size() == 1) {
        missingDigit = *missingDigits.begin();
        Event event(EventType::SetValue, ReasonId::FullHouse);
        event.addOperation(emptyIdx, missingDigit);
        g_eventQueue.enqueue(board, event);
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(BOX_UNITS[u]);
    scanUnit(ROW_UNITS[u]);
    scanUnit(COL_UNITS[u]);
  }
}

static void techNakedSingles(SudokuBoard &board) {
  for (Index i = 0; i < 81; i++) {
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

static void techNakedPairs(SudokuBoard &board) {
  auto scanUnit = [&](const IndexSet &unit) -> void
  {
    // map each position to its digits in this unit
    DigitSet digitsOfPosition[9];

    std::vector<int> unitList = unit.to_vector();
    for (int i = 0; i < 9; ++i) {
      Index idx = unitList[i];
      if (board.isSolved(idx)) {
        continue;
      }
      digitsOfPosition[i] = board.getCandidates(idx);
    }

    // iterate over all positions that have exactly two digits in the unit
    for (int a = 0; a < 8; ++a) {
      if (digitsOfPosition[a].size() == 2) {
        for (int b = a+1; b < 9; ++b) {
          if (digitsOfPosition[a] == digitsOfPosition[b]) {
            // naked pair spotted
            Event event(EventType::RemoveCandidate, ReasonId::NakedPair);
            for (Index idx : board.getPeers(IndexSet({unitList[a], unitList[b]}))) {
              // remove digits present in a and b from other cells they can see
              for (Digit digit : digitsOfPosition[a]) {
                event.addOperation(idx, digit);
              }
            }
            g_eventQueue.enqueue(board, event);
          }
        }
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(ROW_UNITS[u]);
    scanUnit(COL_UNITS[u]);
    scanUnit(BOX_UNITS[u]);
  }
}

static void techNakedTriples(SudokuBoard &board) {
  auto scanUnit = [&](const IndexSet &unit) -> void
  {
    // map each position to its digits in this unit
    DigitSet digitsOfPosition[9];

    std::vector<int> unitList = unit.to_vector();
    for (int i = 0; i < 9; ++i) {
      Index idx = unitList[i];
      if (board.isSolved(idx)) {
        continue;
      }
      digitsOfPosition[i] = board.getCandidates(idx);
    }

    // iterate over all positions that have up to three digits in the unit
    for (int a = 0; a < 7; ++a) {
      DigitSet lockedSet;
      if (!digitsOfPosition[a].empty() && digitsOfPosition[a].size() <= 3) {
        for (int b = a+1; b < 8; ++b) {
          if (!digitsOfPosition[b].empty() && (digitsOfPosition[a] | digitsOfPosition[b]).size() <= 3) {
            for (int c = b+1; c < 9; ++c) {
              if (!digitsOfPosition[c].empty() && (digitsOfPosition[a] | digitsOfPosition[b] | digitsOfPosition[c]).size() == 3) {
                // naked triple spotted
                Event event(EventType::RemoveCandidate, ReasonId::NakedTriple);
                DigitSet lockedSet = digitsOfPosition[a] | digitsOfPosition[b] | digitsOfPosition[c];
                for (Index idx : board.getPeers(IndexSet({unitList[a], unitList[b], unitList[c]}))) {
                  // remove digits present in a, b, c from other cells they can see
                  for (Digit digit : lockedSet) {
                    event.addOperation(idx, digit);
                  }
                }
                g_eventQueue.enqueue(board, event);
              }
            }
          }
        }
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(ROW_UNITS[u]);
    scanUnit(COL_UNITS[u]);
    scanUnit(BOX_UNITS[u]);
  }
}

static void techHiddenSingles(SudokuBoard &board) {
  auto scanUnit = [&](const IndexSet &unit) -> void
  {
    for (Digit digit : board.getUnsolvedDigits()) {
      Index foundIdx = -1;
      for (Index idx : unit) {
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
    scanUnit(BOX_UNITS[u]);
  }
  for (int u = 0; u < 9; u++) {
    scanUnit(ROW_UNITS[u]);
  }
  for (int u = 0; u < 9; u++) {
    scanUnit(COL_UNITS[u]);
  }
}

static void techHiddenPairs(SudokuBoard &board) {
  auto scanUnit = [&](const IndexSet &unit) -> void
  {
    // map each digit to its positions in this unit
    IndexSet positionsOfDigit[10];

    for (Digit digit : board.getUnsolvedDigits()) {
      IndexSet thisPosition;
      for (Index idx : unit) {
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          thisPosition.insert(idx);
        }
      }
      positionsOfDigit[digit] = thisPosition;
    }

    // iterate over all pairs of digits that appear exactly twice in the unit
    for (Digit a = 1; a <= 8; ++a) {
      if (positionsOfDigit[a].size() == 2) {
        for (Digit b = a+1; b <= 9; ++b) {
          if (positionsOfDigit[a] == positionsOfDigit[b]) {
            // hidden pair spotted
            Event event(EventType::RemoveCandidate, ReasonId::HiddenPair);
            for (Index idx : positionsOfDigit[a]) {
              // remove digits different from a and b from the cells of the pair
              for (Digit digit : board.getUnsolvedDigits() - DigitSet({a, b})) {
                event.addOperation(idx, digit);
              }
            }
            g_eventQueue.enqueue(board, event);
          }
        }
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(ROW_UNITS[u]);
    scanUnit(COL_UNITS[u]);
    scanUnit(BOX_UNITS[u]);
  }
}

static void techHiddenTriples(SudokuBoard &board) {
  auto scanUnit = [&](const IndexSet &unit) -> void
  {
    // map each digit to its positions in this unit
    IndexSet positionsOfDigit[10];

    for (Digit digit : board.getUnsolvedDigits()) {
      IndexSet thisPosition;
      for (Index idx : unit) {
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          thisPosition.insert(idx);
        }
      }
      positionsOfDigit[digit] = thisPosition;
    }

    // iterate over all digits that appear up to three times in the unit
    for (Digit a = 1; a <= 7; ++a) {
      if (!positionsOfDigit[a].empty() && positionsOfDigit[a].size() <= 3) {
        for (Digit b = a+1; b <= 8; ++b) {
          if (!positionsOfDigit[b].empty() && (positionsOfDigit[a] | positionsOfDigit[b]).size() <= 3) {
            for (Digit c = b+1; c <= 9; ++c) {
              if (!positionsOfDigit[c].empty() && (positionsOfDigit[a] | positionsOfDigit[b] | positionsOfDigit[c]).size() == 3) {
                // hidden triple spotted
                Event event(EventType::RemoveCandidate, ReasonId::HiddenTriple);
                IndexSet lockedSet = positionsOfDigit[a] | positionsOfDigit[b] | positionsOfDigit[c];
                for (Index idx : lockedSet) {
                  // remove digits different from a, b, c from the cells of the triple
                  for (Digit digit : board.getCandidates(idx) - DigitSet({a, b, c})) {
                    event.addOperation(idx, digit);
                  }
                }
                g_eventQueue.enqueue(board, event);
              }
            }
          }
        }
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(ROW_UNITS[u]);
    scanUnit(COL_UNITS[u]);
    scanUnit(BOX_UNITS[u]);
  }
}

static void techLockedCandidates(SudokuBoard &board) {
  // For each box and digit:
  //  - if all candidates are confined to a single row within the box,
  //    remove the digit from that row outside the box
  //  - same for a single column
  for (const IndexSet &box : BOX_UNITS) {
    for (Digit digit : board.getUnsolvedDigits()) {
      IndexSet positions;
      for (Index idx : box) {
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          positions.insert(idx);
        }
      }

      int posCount = positions.size();
      if (posCount < 2) {
        continue; // locked candidates is about confinement with at least 2
      }

      ReasonId reasonId;
      if (posCount == 2) {
        reasonId = ReasonId::PointingPair;
      } else if (posCount == 3) {
        reasonId = ReasonId::PointingTriple;
      }

      const int r0 = idxRow(*positions.begin());
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
        IndexSet set = ROW_UNITS[r0].difference_with(box);
        for (Index idx : set) {
          if (!board.isSolved(idx) && board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
        g_eventQueue.enqueue(board, event);
      }

      const int c0 = idxCol(*positions.begin());
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
        IndexSet set = COL_UNITS[c0].difference_with(box);
        for (Index idx : set) {
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
  auto scanUnit = [&](const IndexSet &unit) -> void
  {
    for (Digit digit : board.getUnsolvedDigits()) {
      IndexSet positions;
      for (Index idx : unit) {
        if (board.isSolved(idx)) {
          continue;
        }
        if (board.hasCandidate(idx, digit)) {
          positions.insert(idx);
        }
      }

      int posCount = positions.size();
      if (posCount < 2 || posCount > 3) {
        continue; // box line reduction is about confinement with 2 or 3
      }

      ReasonId reasonId;
      if (posCount == 2) {
        reasonId = ReasonId::ClaimingPair;
      } else if (posCount == 3) {
        reasonId = ReasonId::ClaimingTriple;
      }

      IndexSet boxes;
      bool sameBlock = true;
      for (Index pos : positions) {
        boxes.insert(idxBox(pos));
      }

      if (boxes.size() == 1) {
        // remove digit from this box, excluding cells in this row/column
        int boxIdx = *boxes.begin();
        Event event(EventType::RemoveCandidate, reasonId);
        IndexSet set = BOX_UNITS[boxIdx].difference_with(unit);
        for (Index idx : set) {
          if (!board.isSolved(idx) && board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
        g_eventQueue.enqueue(board, event);
      }
    }
  };

  for (int u = 0; u < 9; u++) {
    scanUnit(ROW_UNITS[u]);
  }
  for (int u = 0; u < 9; u++) {
    scanUnit(COL_UNITS[u]);
  }
}

typedef void (*TechniqueFn)(SudokuBoard &);

// nCr(9, 2) = 36
// nCr(9, 3) = 84
// nCr(9, 4) = 126

static constexpr TechniqueFn TECHNIQUES[] =
{
  techFullHouse,
  techHiddenSingles,
  techLockedCandidates,
  techBoxLineReduction,
  techHiddenPairs,
  techNakedSingles,
  techHiddenTriples,
  techNakedPairs,
  techNakedTriples,
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

// Drain the next event and serialize the operations into out[] as described by API.
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
  for (TechniqueFn tech : TECHNIQUES) {
    const size_t before = g_eventQueue.size();
    tech(board);
    if (g_eventQueue.size() != before) {
      // validate event
      Event tmp;
      g_eventQueue.peek(tmp);
      int counter = 0;
      for (const Operation &op : tmp.getOperations()) {
        counter += is_operation_applicable(board, tmp.type, op.idx, op.digit);
      }
      if (counter == 0) {
        // discard event and go on
        g_eventQueue.dequeue(tmp);
      } else {
        // found valid event, break loop
        break;
      }
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
