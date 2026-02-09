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
// State is managed by WASM for 
// sudorix_solver_next_step requires an initial call to sudorix_solver_init_board.
//
// Notes:
//   - The event queue is stored in WASM as persistent state (g_eventQueue contains unique events).
//   - JS must provide a consistent board (values and candidates) before calling sudorix_solver_hint.
//   - JS must initialize the board with sudorix_solver_init_board before using sudorix_solver_next_step.
//   - JS does not need to manage the state when using sudorix_solver_full and sudorix_solver_next_step other than UI purpose.

#include "solver.hpp"

#include <cstdint>
#include <cstddef>
#include <cstring>

#include <queue>
#include <deque>
#include <set>
#include <functional>
#include <stdexcept>

#ifdef __EMSCRIPTEN__
  #include <emscripten/emscripten.h>
#else
  #include <cstdio>
  #define EMSCRIPTEN_KEEPALIVE
  #define emscripten_log(x, fmt, ...) printf(fmt, ##__VA_ARGS__);
#endif

// =========================================================
// Precomputed indices (rows / cols / boxes)
// =========================================================

static constexpr int ROW_CELLS[9][9] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17 },
    { 18, 19, 20, 21, 22, 23, 24, 25, 26 },
    { 27, 28, 29, 30, 31, 32, 33, 34, 35 },
    { 36, 37, 38, 39, 40, 41, 42, 43, 44 },
    { 45, 46, 47, 48, 49, 50, 51, 52, 53 },
    { 54, 55, 56, 57, 58, 59, 60, 61, 62 },
    { 63, 64, 65, 66, 67, 68, 69, 70, 71 },
    { 72, 73, 74, 75, 76, 77, 78, 79, 80 }
  };

static constexpr int COL_CELLS[9][9] = {
    { 0, 9, 18, 27, 36, 45, 54, 63, 72 },
    { 1, 10, 19, 28, 37, 46, 55, 64, 73 },
    { 2, 11, 20, 29, 38, 47, 56, 65, 74 },
    { 3, 12, 21, 30, 39, 48, 57, 66, 75 },
    { 4, 13, 22, 31, 40, 49, 58, 67, 76 },
    { 5, 14, 23, 32, 41, 50, 59, 68, 77 },
    { 6, 15, 24, 33, 42, 51, 60, 69, 78 },
    { 7, 16, 25, 34, 43, 52, 61, 70, 79 },
    { 8, 17, 26, 35, 44, 53, 62, 71, 80 }
  };

static constexpr int BOX_CELLS[9][9] = {
    { 0, 1, 2, 9, 10, 11, 18, 19, 20 },
    { 3, 4, 5, 12, 13, 14, 21, 22, 23 },
    { 6, 7, 8, 15, 16, 17, 24, 25, 26 },
    { 27, 28, 29, 36, 37, 38, 45, 46, 47 },
    { 30, 31, 32, 39, 40, 41, 48, 49, 50 },
    { 33, 34, 35, 42, 43, 44, 51, 52, 53 },
    { 54, 55, 56, 63, 64, 65, 72, 73, 74 },
    { 57, 58, 59, 66, 67, 68, 75, 76, 77 },
    { 60, 61, 62, 69, 70, 71, 78, 79, 80 }
  };

static inline uint8_t idxRow(int idx) {
  return (uint8_t)(idx / 9);
}

static inline uint8_t idxCol(int idx) {
  return (uint8_t)(idx % 9);
}

static inline uint8_t idxBox(int idx) {
  const uint8_t r = idxRow(idx);
  const uint8_t c = idxCol(idx);
  return (uint8_t)((r / 3) * 3 + (c / 3));
}

// =========================================================
// Helpers (bitmasks)
// =========================================================

static inline uint16_t digitToBit(uint8_t d) {
  // d: 1..9 -> bit (d-1)
  return (uint16_t)(1u << (d - 1u));
}

static inline uint8_t countBits9(uint16_t mask) {
  mask &= 0x1FFu;
  // builtin popcount if available
#if defined(__GNUC__) || defined(__clang__)
  return (uint8_t)__builtin_popcount((unsigned)mask);
#else
  uint8_t c = 0;
  while (mask) {
    c += (mask & 1u);
    mask >>= 1u;
  }
  return c;
#endif
}

static inline uint8_t bitToDigitSingle(uint16_t mask) {
  // assumes exactly one bit set (1..9)
#if defined(__GNUC__) || defined(__clang__)
  return (uint8_t)(__builtin_ctz((unsigned)mask) + 1u);
#else
  for (uint8_t d = 1; d <= 9; d++) {
    if (mask & digitToBit(d)) {
      return d;
    }
  }
  return 0;
#endif
}

// =========================================================
// Events
// =========================================================

enum class EventType : uint8_t {
  None = 0,
  SetValue = 1,
  RemoveCandidate = 2
};

enum class ReasonId : uint8_t {
  Solver = 0,
  FullHouse = 1,
  NakedSingle = 2,
  HiddenSingle = 3,
  PointingPair = 4,
  PointingTriple = 5,
  LockedCandidates = 6
};

class Event
{
public:
  EventType type;
  uint8_t   idx;
  uint8_t   digit;
  ReasonId  reason;

  bool operator<(const Event &other) const {
    return this->getEventId() < other.getEventId();
  }

private:
  friend class EventQueue;
  uint32_t getEventId() const {
    // make the event comparable by mapping it to a unique integer
    return (static_cast<uint32_t>(this->type) & 0xFFu) |
          ((static_cast<uint32_t>(this->idx) & 0xFFu) << 8) |
          ((static_cast<uint32_t>(this->digit) & 0xFFu) << 16) ;
  }
};

// =========================================================
// Event queue (implementation avoids duplicates)
// =========================================================

class EventQueue
{
public:
  EventQueue() = default;

  // push solo se l'elemento non è già presente
  // evita eventi duplicati nella stessa iterazione
  bool push(const Event &x) {
    if (s.find(x.getEventId()) != s.end()) {
      return false;
    }

    q.push(x);
    s.insert(x.getEventId());
    return true;
  }

  // rimozione coerente queue + set
  void pop() {
    if (q.empty()) {
      throw std::logic_error("EventQueue::pop() on empty queue");
    }

    const Event &front = q.front();
    s.erase(front.getEventId());
    q.pop();
  }

  const Event &front() const {
    if (q.empty()) {
      throw std::logic_error("EventQueue::front() on empty queue");
    }

    return q.front();
  }

  bool contains(const Event &x) const {
    return s.find(x.getEventId()) != s.end();
  }

  std::size_t size() const noexcept {
    return q.size();
  }

  bool empty() const noexcept {
    return q.empty();
  }

private:
  std::queue<Event> q;
  std::set<uint32_t> s;
};

static EventQueue g_eventQueue;

// =========================================================
// SudokuCell / SudokuBoard
// =========================================================

class SudokuCell
{
public:
  SudokuCell() : value(0), candMask(0) { }

  // --- value ---
  uint8_t getValue() const {
    return value;
  }

  bool isSolved() const {
    return value != 0;
  }

  void setValue(uint8_t digit) {
    value = digit;
    if (digit != 0) {
      // When solved, keep only the digit bit as candidates.
      candMask = digitToBit(digit);
    }
  }

  void clearValue() {
    value = 0;
  }

  // --- candidates ---
  uint16_t getCandidateMask() const {
    return (uint16_t)(candMask & 0x1FFu);
  }

  void setCandidateMask(uint16_t mask) {
    candMask = (uint16_t)(mask & 0x1FFu);
  }

  bool hasCandidate(uint8_t digit) const {
    return (getCandidateMask() & digitToBit(digit)) != 0;
  }

  uint8_t countCandidates() const {
    return countBits9(getCandidateMask());
  }

  uint8_t getSingleCandidate() const {
    const uint16_t m = getCandidateMask();
    if (countBits9(m) == 1) {
      return bitToDigitSingle(m);
    }
    return 0;
  }

  void enableCandidate(uint8_t digit) {
    candMask |= digitToBit(digit);
  }

  bool disableCandidate(uint8_t digit) {
    const uint16_t bit = digitToBit(digit);
    const uint16_t before = getCandidateMask();
    const uint16_t after = (uint16_t)(before & ~bit);
    if (after != before) {
      candMask = after;
      return true;
    }
    return false;
  }

  bool toggleCandidate(uint8_t digit) {
    const uint16_t bit = digitToBit(digit);
    const bool wasOn = (candMask & bit) != 0;
    candMask ^= bit;
    return !wasOn;
  }

private:
  uint8_t  value;     // 0..9
  uint16_t candMask;  // 9-bit
};

class SudokuBoard
{
public:
  // empty board
  SudokuBoard() = default;

  // only values, candidates are calculated automatically
  int importFromString(const char *values) {
    // parse: digits 1..9 are values; 0 or '.' are empty; ignore others
    int tokens = 0;
    for (int i = 0; values[i] != '\0'; i++) {
      const char ch = values[i];
      if (ch >= '1' && ch <= '9') {
        // given
        cells[i].setValue(ch - '0');
        ++tokens;
      } else if (ch == '0' || ch == '.') {
        // empty
        cells[i].setValue(0);
        ++tokens;
      } else {
        // skip character
        continue;
      }

      if (tokens == 81) {
        break;
      }
    }

    /* Sudoku incompleto se non ho 81 simboli riconosciuti (0-9 o '.') */
    if (tokens < 81) {
      return 0;
    }

    // calculate candidates
    _recalcAllCandidatesFromValues();

    return 1;
  }

  // values and candidates
  int importFromBuffers(const uint8_t *values, const uint16_t *cands) {
    // TODO: error handling
    for (int i = 0; i < 81; i++) {
      cells[i].setValue(values[i]);
      // If JS provides candidates for solved cells too, keep them consistent anyway.
      if (values[i] == 0) {
        cells[i].setCandidateMask(cands[i]);
      } else {
        cells[i].setCandidateMask(digitToBit(values[i]));
      }
    }
    return 1;
  }

  void exportToBuffers(uint8_t *values, uint16_t *cands) const {
    for (int i = 0; i < 81; i++) {
      values[i] = cells[i].getValue();
      cands[i]  = cells[i].getCandidateMask();
    }
  }

  // --- values API ---
  uint8_t getValue(int idx) const {
    return cells[idx].getValue();
  }

  bool isSolved(int idx) const {
    return cells[idx].isSolved();
  }

  void setValue(int idx, uint8_t digit) {
    cells[idx].setValue(digit);
  }

  void clearValue(int idx) {
    cells[idx].clearValue();
  }

  // --- candidates API ---
  uint16_t getCandidateMask(int idx) const {
    return cells[idx].getCandidateMask();
  }

  void setCandidateMask(int idx, uint16_t mask) {
    cells[idx].setCandidateMask(mask);
  }

  bool hasCandidate(int idx, uint8_t digit) const {
    return cells[idx].hasCandidate(digit);
  }

  uint8_t countCandidates(int idx) const {
    return cells[idx].countCandidates();
  }

  uint8_t getSingleCandidate(int idx) const {
    return cells[idx].getSingleCandidate();
  }

  void disableCandidate(int idx, uint8_t digit) {
    cells[idx].disableCandidate(digit);
  }

  // --- events API ---
  void applySetValue(int idx, int digit) {
    // Set + Auto clear 
    setValue(idx, digit);
    autoClearPeersAfterPlacement(idx, digit);
  }

  void applyRemoveCandidate(int idx, int digit) {
    // Remove + Auto place if applicable
    disableCandidate(idx, digit);
    int only = getSingleCandidate(idx);
    if (only) {
      return applySetValue(idx, only);
    }
  }

  void autoClearPeersAfterPlacement(int idx, int digit) {
    int r = idxRow(idx);
    int c = idxCol(idx);
    int b = idxBox(idx);

    // Rimuove digit dai candidati dei peers non risolti.
    for (int k = 0; k < 9; k++) {
      int ir = ROW_CELLS[r][k];
      if (ir != idx && !isSolved(ir)) {
        disableCandidate(ir, digit);
      }
      int ic = COL_CELLS[c][k];
      if (ic != idx && !isSolved(ic)) {
        disableCandidate(ic, digit);
      }
      int ib = BOX_CELLS[b][k];
      if (ib != idx && !isSolved(ib)) {
        disableCandidate(ib, digit);
      }
    }
  }

  bool isCompletelySolved() {
    for (const SudokuCell &cell : cells) {
      if (!cell.isSolved()) {
        return false;
      }
    }
    return true;
  }

private:
  // We keep a local copy (owned) so that solver techniques can mutate freely.
  SudokuCell cells[81];

  static inline bool isValidIndex(int idx) {
    return idx >= 0 && idx < 81;
  }

  bool _recalcAllCandidatesFromValues() {
    // Reset completo
    for (int i = 0; i < 81; i++) {
      setCandidateMask(i, 0);
    }

    // Precompute delle mask "used" per ogni unità
    uint16_t rowUsed[9] = {0};
    uint16_t colUsed[9] = {0};
    uint16_t boxUsed[9] = {0};

    // 1) Scansione valori e costruzione used masks + verifica conflitti
    for (int idx = 0; idx < 81; idx++) {
      int value = getValue(idx);
      if (value == 0) {
        continue;
      }
      if (value < 1 || value > 9) {
        return false;
      }

      uint16_t mask = digitToBit(value);
      int r = idxRow(idx);
      int c = idxCol(idx);
      int b = idxBox(idx);

      if ((rowUsed[r] & mask) != 0) {
        return false;
      }
      if ((colUsed[c] & mask) != 0) {
        return false;
      }
      if ((boxUsed[b] & mask) != 0) {
        return false;
      }

      rowUsed[r] = static_cast<uint16_t>(rowUsed[r] | mask);
      colUsed[c] = static_cast<uint16_t>(colUsed[c] | mask);
      boxUsed[b] = static_cast<uint16_t>(boxUsed[b] | mask);

      // Cella risolta: candidato unico
      setCandidateMask(idx, mask);
    }

    // 2) Celle vuote: candidati = NOT(used in row/col/box)
    constexpr uint16_t ALL = 0x01FF; // 9 bit a 1
    for (int idx = 0; idx < 81; idx++) {
      if (isSolved(idx)) {
        continue;
      }

      int r = idxRow(idx);
      int c = idxCol(idx);
      int b = idxBox(idx);

      uint16_t used = static_cast<uint16_t>(rowUsed[r] | colUsed[c] | boxUsed[b]);
      uint16_t allowed = static_cast<uint16_t>(ALL & ~used);

      // Se una cella vuota non ha candidati, griglia inconsistente
      if (allowed == 0) {
        return false;
      }

      setCandidateMask(idx, allowed);
    }

    return true;
  }
};

static SudokuBoard g_sudokuBoard;

// =========================================================
// Technique helpers
// =========================================================

static void enqueueSetValue(SudokuBoard &board, int idx, uint8_t digit, ReasonId reason) {
  if (digit == 0) {
    return;
  }
  if (board.isSolved(idx)) {
    return;
  }

  Event ev;
  ev.type = EventType::SetValue;
  ev.idx = (uint32_t)idx;
  ev.digit = (uint32_t)digit;
  ev.reason = reason;
  g_eventQueue.push(ev);
}

static void enqueueRemoveCandidate(SudokuBoard &board, int idx, uint8_t digit, ReasonId reason) {
  if (digit == 0) {
    return;
  }
  if (board.isSolved(idx)) {
    return;
  }
  if (!board.hasCandidate(idx, digit)) {
    return;
  }

  Event ev;
  ev.type = EventType::RemoveCandidate;
  ev.idx = (uint32_t)idx;
  ev.digit = (uint32_t)digit;
  ev.reason = reason;
  g_eventQueue.push(ev);
}

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
        enqueueSetValue(board, emptyIdx, missingDigit, ReasonId::FullHouse);
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
        enqueueSetValue(board, foundIdx, digit, ReasonId::HiddenSingle);
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
          enqueueRemoveCandidate(board, idx, digit, reasonId);
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
          enqueueRemoveCandidate(board, idx, digit, reasonId);
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
      enqueueSetValue(board, i, d, ReasonId::NakedSingle);
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
  // Solves an entire Sudoku given its initial representation in one shot
  // Returns 0 in case of error, else 1
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

  // Initializes the board for a step-by-step solution
  // Returns 0 in case of error, else 1
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

  // Performs and returns one step to solve the currently loaded board
  // Returns 0 in case of error or no event is produced, else 1
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

  // Calculate and return one step to solve the board given as input (both values and candidates are given)
  // Returns 0 in case of error or no event is produced, else 1
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
