#include "SudokuBoard.hpp"
#include "utils.hpp"

// =========================================================
// Precomputed indices
// =========================================================

// tables to get units as bitmasks
static const std::vector<Unit> ROW_UNITS = {
  CellSet({ 0,  1,  2,  3,  4,  5,  6,  7,  8 }),
  CellSet({ 9, 10, 11, 12, 13, 14, 15, 16, 17 }),
  CellSet({18, 19, 20, 21, 22, 23, 24, 25, 26 }),
  CellSet({27, 28, 29, 30, 31, 32, 33, 34, 35 }),
  CellSet({36, 37, 38, 39, 40, 41, 42, 43, 44 }),
  CellSet({45, 46, 47, 48, 49, 50, 51, 52, 53 }),
  CellSet({54, 55, 56, 57, 58, 59, 60, 61, 62 }),
  CellSet({63, 64, 65, 66, 67, 68, 69, 70, 71 }),
  CellSet({72, 73, 74, 75, 76, 77, 78, 79, 80 })
};

static const std::vector<Unit> COL_UNITS = {
  CellSet({ 0,  9, 18, 27, 36, 45, 54, 63, 72 }),
  CellSet({ 1, 10, 19, 28, 37, 46, 55, 64, 73 }),
  CellSet({ 2, 11, 20, 29, 38, 47, 56, 65, 74 }),
  CellSet({ 3, 12, 21, 30, 39, 48, 57, 66, 75 }),
  CellSet({ 4, 13, 22, 31, 40, 49, 58, 67, 76 }),
  CellSet({ 5, 14, 23, 32, 41, 50, 59, 68, 77 }),
  CellSet({ 6, 15, 24, 33, 42, 51, 60, 69, 78 }),
  CellSet({ 7, 16, 25, 34, 43, 52, 61, 70, 79 }),
  CellSet({ 8, 17, 26, 35, 44, 53, 62, 71, 80 })
};

static const std::vector<Unit> BOX_UNITS = {
  CellSet({ 0,  1,  2,  9, 10, 11, 18, 19, 20 }),
  CellSet({ 3,  4,  5, 12, 13, 14, 21, 22, 23 }),
  CellSet({ 6,  7,  8, 15, 16, 17, 24, 25, 26 }),
  CellSet({27, 28, 29, 36, 37, 38, 45, 46, 47 }),
  CellSet({30, 31, 32, 39, 40, 41, 48, 49, 50 }),
  CellSet({33, 34, 35, 42, 43, 44, 51, 52, 53 }),
  CellSet({54, 55, 56, 63, 64, 65, 72, 73, 74 }),
  CellSet({57, 58, 59, 66, 67, 68, 75, 76, 77 }),
  CellSet({60, 61, 62, 69, 70, 71, 78, 79, 80 })
};

// tables to get units as indexable arrays
static constexpr std::array<std::array<Cell,9>,9> ROW_ARRAY = {{
  { 0,  1,  2,  3,  4,  5,  6,  7,  8 },
  { 9, 10, 11, 12, 13, 14, 15, 16, 17 },
  {18, 19, 20, 21, 22, 23, 24, 25, 26 },
  {27, 28, 29, 30, 31, 32, 33, 34, 35 },
  {36, 37, 38, 39, 40, 41, 42, 43, 44 },
  {45, 46, 47, 48, 49, 50, 51, 52, 53 },
  {54, 55, 56, 57, 58, 59, 60, 61, 62 },
  {63, 64, 65, 66, 67, 68, 69, 70, 71 },
  {72, 73, 74, 75, 76, 77, 78, 79, 80 }
}};

static constexpr std::array<std::array<Cell,9>,9> COL_ARRAY = {{
  { 0,  9, 18, 27, 36, 45, 54, 63, 72 },
  { 1, 10, 19, 28, 37, 46, 55, 64, 73 },
  { 2, 11, 20, 29, 38, 47, 56, 65, 74 },
  { 3, 12, 21, 30, 39, 48, 57, 66, 75 },
  { 4, 13, 22, 31, 40, 49, 58, 67, 76 },
  { 5, 14, 23, 32, 41, 50, 59, 68, 77 },
  { 6, 15, 24, 33, 42, 51, 60, 69, 78 },
  { 7, 16, 25, 34, 43, 52, 61, 70, 79 },
  { 8, 17, 26, 35, 44, 53, 62, 71, 80 }
}};

static constexpr std::array<std::array<Cell,9>,9> BOX_ARRAY = {{
  { 0,  1,  2,  9, 10, 11, 18, 19, 20 },
  { 3,  4,  5, 12, 13, 14, 21, 22, 23 },
  { 6,  7,  8, 15, 16, 17, 24, 25, 26 },
  {27, 28, 29, 36, 37, 38, 45, 46, 47 },
  {30, 31, 32, 39, 40, 41, 48, 49, 50 },
  {33, 34, 35, 42, 43, 44, 51, 52, 53 },
  {54, 55, 56, 63, 64, 65, 72, 73, 74 },
  {57, 58, 59, 66, 67, 68, 75, 76, 77 },
  {60, 61, 62, 69, 70, 71, 78, 79, 80 }
}};

// peers table
static const std::vector<CellSet> PEERS = {
  CellSet({ 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 18, 19, 20, 27, 36, 45, 54, 63, 72}),
  CellSet({ 0,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 18, 19, 20, 28, 37, 46, 55, 64, 73}),
  CellSet({ 0,  1,  3,  4,  5,  6,  7,  8,  9, 10, 11, 18, 19, 20, 29, 38, 47, 56, 65, 74}),
  CellSet({ 0,  1,  2,  4,  5,  6,  7,  8, 12, 13, 14, 21, 22, 23, 30, 39, 48, 57, 66, 75}),
  CellSet({ 0,  1,  2,  3,  5,  6,  7,  8, 12, 13, 14, 21, 22, 23, 31, 40, 49, 58, 67, 76}),
  CellSet({ 0,  1,  2,  3,  4,  6,  7,  8, 12, 13, 14, 21, 22, 23, 32, 41, 50, 59, 68, 77}),
  CellSet({ 0,  1,  2,  3,  4,  5,  7,  8, 15, 16, 17, 24, 25, 26, 33, 42, 51, 60, 69, 78}),
  CellSet({ 0,  1,  2,  3,  4,  5,  6,  8, 15, 16, 17, 24, 25, 26, 34, 43, 52, 61, 70, 79}),
  CellSet({ 0,  1,  2,  3,  4,  5,  6,  7, 15, 16, 17, 24, 25, 26, 35, 44, 53, 62, 71, 80}),
  CellSet({ 0,  1,  2, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 27, 36, 45, 54, 63, 72}),
  CellSet({ 0,  1,  2,  9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 28, 37, 46, 55, 64, 73}),
  CellSet({ 0,  1,  2,  9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 29, 38, 47, 56, 65, 74}),
  CellSet({ 3,  4,  5,  9, 10, 11, 13, 14, 15, 16, 17, 21, 22, 23, 30, 39, 48, 57, 66, 75}),
  CellSet({ 3,  4,  5,  9, 10, 11, 12, 14, 15, 16, 17, 21, 22, 23, 31, 40, 49, 58, 67, 76}),
  CellSet({ 3,  4,  5,  9, 10, 11, 12, 13, 15, 16, 17, 21, 22, 23, 32, 41, 50, 59, 68, 77}),
  CellSet({ 6,  7,  8,  9, 10, 11, 12, 13, 14, 16, 17, 24, 25, 26, 33, 42, 51, 60, 69, 78}),
  CellSet({ 6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 17, 24, 25, 26, 34, 43, 52, 61, 70, 79}),
  CellSet({ 6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 24, 25, 26, 35, 44, 53, 62, 71, 80}),
  CellSet({ 0,  1,  2,  9, 10, 11, 19, 20, 21, 22, 23, 24, 25, 26, 27, 36, 45, 54, 63, 72}),
  CellSet({ 0,  1,  2,  9, 10, 11, 18, 20, 21, 22, 23, 24, 25, 26, 28, 37, 46, 55, 64, 73}),
  CellSet({ 0,  1,  2,  9, 10, 11, 18, 19, 21, 22, 23, 24, 25, 26, 29, 38, 47, 56, 65, 74}),
  CellSet({ 3,  4,  5, 12, 13, 14, 18, 19, 20, 22, 23, 24, 25, 26, 30, 39, 48, 57, 66, 75}),
  CellSet({ 3,  4,  5, 12, 13, 14, 18, 19, 20, 21, 23, 24, 25, 26, 31, 40, 49, 58, 67, 76}),
  CellSet({ 3,  4,  5, 12, 13, 14, 18, 19, 20, 21, 22, 24, 25, 26, 32, 41, 50, 59, 68, 77}),
  CellSet({ 6,  7,  8, 15, 16, 17, 18, 19, 20, 21, 22, 23, 25, 26, 33, 42, 51, 60, 69, 78}),
  CellSet({ 6,  7,  8, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 26, 34, 43, 52, 61, 70, 79}),
  CellSet({ 6,  7,  8, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 35, 44, 53, 62, 71, 80}),
  CellSet({ 0,  9, 18, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 45, 46, 47, 54, 63, 72}),
  CellSet({ 1, 10, 19, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 45, 46, 47, 55, 64, 73}),
  CellSet({ 2, 11, 20, 27, 28, 30, 31, 32, 33, 34, 35, 36, 37, 38, 45, 46, 47, 56, 65, 74}),
  CellSet({ 3, 12, 21, 27, 28, 29, 31, 32, 33, 34, 35, 39, 40, 41, 48, 49, 50, 57, 66, 75}),
  CellSet({ 4, 13, 22, 27, 28, 29, 30, 32, 33, 34, 35, 39, 40, 41, 48, 49, 50, 58, 67, 76}),
  CellSet({ 5, 14, 23, 27, 28, 29, 30, 31, 33, 34, 35, 39, 40, 41, 48, 49, 50, 59, 68, 77}),
  CellSet({ 6, 15, 24, 27, 28, 29, 30, 31, 32, 34, 35, 42, 43, 44, 51, 52, 53, 60, 69, 78}),
  CellSet({ 7, 16, 25, 27, 28, 29, 30, 31, 32, 33, 35, 42, 43, 44, 51, 52, 53, 61, 70, 79}),
  CellSet({ 8, 17, 26, 27, 28, 29, 30, 31, 32, 33, 34, 42, 43, 44, 51, 52, 53, 62, 71, 80}),
  CellSet({ 0,  9, 18, 27, 28, 29, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 54, 63, 72}),
  CellSet({ 1, 10, 19, 27, 28, 29, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 55, 64, 73}),
  CellSet({ 2, 11, 20, 27, 28, 29, 36, 37, 39, 40, 41, 42, 43, 44, 45, 46, 47, 56, 65, 74}),
  CellSet({ 3, 12, 21, 30, 31, 32, 36, 37, 38, 40, 41, 42, 43, 44, 48, 49, 50, 57, 66, 75}),
  CellSet({ 4, 13, 22, 30, 31, 32, 36, 37, 38, 39, 41, 42, 43, 44, 48, 49, 50, 58, 67, 76}),
  CellSet({ 5, 14, 23, 30, 31, 32, 36, 37, 38, 39, 40, 42, 43, 44, 48, 49, 50, 59, 68, 77}),
  CellSet({ 6, 15, 24, 33, 34, 35, 36, 37, 38, 39, 40, 41, 43, 44, 51, 52, 53, 60, 69, 78}),
  CellSet({ 7, 16, 25, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 44, 51, 52, 53, 61, 70, 79}),
  CellSet({ 8, 17, 26, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 51, 52, 53, 62, 71, 80}),
  CellSet({ 0,  9, 18, 27, 28, 29, 36, 37, 38, 46, 47, 48, 49, 50, 51, 52, 53, 54, 63, 72}),
  CellSet({ 1, 10, 19, 27, 28, 29, 36, 37, 38, 45, 47, 48, 49, 50, 51, 52, 53, 55, 64, 73}),
  CellSet({ 2, 11, 20, 27, 28, 29, 36, 37, 38, 45, 46, 48, 49, 50, 51, 52, 53, 56, 65, 74}),
  CellSet({ 3, 12, 21, 30, 31, 32, 39, 40, 41, 45, 46, 47, 49, 50, 51, 52, 53, 57, 66, 75}),
  CellSet({ 4, 13, 22, 30, 31, 32, 39, 40, 41, 45, 46, 47, 48, 50, 51, 52, 53, 58, 67, 76}),
  CellSet({ 5, 14, 23, 30, 31, 32, 39, 40, 41, 45, 46, 47, 48, 49, 51, 52, 53, 59, 68, 77}),
  CellSet({ 6, 15, 24, 33, 34, 35, 42, 43, 44, 45, 46, 47, 48, 49, 50, 52, 53, 60, 69, 78}),
  CellSet({ 7, 16, 25, 33, 34, 35, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 53, 61, 70, 79}),
  CellSet({ 8, 17, 26, 33, 34, 35, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 62, 71, 80}),
  CellSet({ 0,  9, 18, 27, 36, 45, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 72, 73, 74}),
  CellSet({ 1, 10, 19, 28, 37, 46, 54, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 72, 73, 74}),
  CellSet({ 2, 11, 20, 29, 38, 47, 54, 55, 57, 58, 59, 60, 61, 62, 63, 64, 65, 72, 73, 74}),
  CellSet({ 3, 12, 21, 30, 39, 48, 54, 55, 56, 58, 59, 60, 61, 62, 66, 67, 68, 75, 76, 77}),
  CellSet({ 4, 13, 22, 31, 40, 49, 54, 55, 56, 57, 59, 60, 61, 62, 66, 67, 68, 75, 76, 77}),
  CellSet({ 5, 14, 23, 32, 41, 50, 54, 55, 56, 57, 58, 60, 61, 62, 66, 67, 68, 75, 76, 77}),
  CellSet({ 6, 15, 24, 33, 42, 51, 54, 55, 56, 57, 58, 59, 61, 62, 69, 70, 71, 78, 79, 80}),
  CellSet({ 7, 16, 25, 34, 43, 52, 54, 55, 56, 57, 58, 59, 60, 62, 69, 70, 71, 78, 79, 80}),
  CellSet({ 8, 17, 26, 35, 44, 53, 54, 55, 56, 57, 58, 59, 60, 61, 69, 70, 71, 78, 79, 80}),
  CellSet({ 0,  9, 18, 27, 36, 45, 54, 55, 56, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74}),
  CellSet({ 1, 10, 19, 28, 37, 46, 54, 55, 56, 63, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74}),
  CellSet({ 2, 11, 20, 29, 38, 47, 54, 55, 56, 63, 64, 66, 67, 68, 69, 70, 71, 72, 73, 74}),
  CellSet({ 3, 12, 21, 30, 39, 48, 57, 58, 59, 63, 64, 65, 67, 68, 69, 70, 71, 75, 76, 77}),
  CellSet({ 4, 13, 22, 31, 40, 49, 57, 58, 59, 63, 64, 65, 66, 68, 69, 70, 71, 75, 76, 77}),
  CellSet({ 5, 14, 23, 32, 41, 50, 57, 58, 59, 63, 64, 65, 66, 67, 69, 70, 71, 75, 76, 77}),
  CellSet({ 6, 15, 24, 33, 42, 51, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70, 71, 78, 79, 80}),
  CellSet({ 7, 16, 25, 34, 43, 52, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 71, 78, 79, 80}),
  CellSet({ 8, 17, 26, 35, 44, 53, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 78, 79, 80}),
  CellSet({ 0,  9, 18, 27, 36, 45, 54, 55, 56, 63, 64, 65, 73, 74, 75, 76, 77, 78, 79, 80}),
  CellSet({ 1, 10, 19, 28, 37, 46, 54, 55, 56, 63, 64, 65, 72, 74, 75, 76, 77, 78, 79, 80}),
  CellSet({ 2, 11, 20, 29, 38, 47, 54, 55, 56, 63, 64, 65, 72, 73, 75, 76, 77, 78, 79, 80}),
  CellSet({ 3, 12, 21, 30, 39, 48, 57, 58, 59, 66, 67, 68, 72, 73, 74, 76, 77, 78, 79, 80}),
  CellSet({ 4, 13, 22, 31, 40, 49, 57, 58, 59, 66, 67, 68, 72, 73, 74, 75, 77, 78, 79, 80}),
  CellSet({ 5, 14, 23, 32, 41, 50, 57, 58, 59, 66, 67, 68, 72, 73, 74, 75, 76, 78, 79, 80}),
  CellSet({ 6, 15, 24, 33, 42, 51, 60, 61, 62, 69, 70, 71, 72, 73, 74, 75, 76, 77, 79, 80}),
  CellSet({ 7, 16, 25, 34, 43, 52, 60, 61, 62, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 80}),
  CellSet({ 8, 17, 26, 35, 44, 53, 60, 61, 62, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79}),
};

// =========================================================
// SudokuBoard
// =========================================================

// empty board
SudokuBoard::SudokuBoard() = default;

// only values, candidates are calculated automatically
int SudokuBoard::importFromString(const char *values) {
  // clear cache
  for (Digit x = 1; x <= 9; ++x) {
    counter[x] = 0;
  }
  // parse: digits 1..9 are values; 0 or '.' are empty; ignore others
  int tokens = 0;
  for (int i = 0; values[i] != '\0'; i++) {
    const char ch = values[i];
    if (ch >= '1' && ch <= '9') {
      // given
      cells[i].setValue(ch - '0');
      ++counter[ch - '0'];
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
int SudokuBoard::importFromBuffers(const uint8_t *values, const uint16_t *cands) {
  // clear cache
  for (Digit x = 1; x <= 9; ++x) {
    counter[x] = 0;
  }
  // TODO: error handling
  for (int i = 0; i < 81; i++) {
    cells[i].setValue(values[i]);
    // If JS provides candidates for solved cells too, keep them consistent anyway.
    if (values[i] == 0) {
      cells[i].setCandidates(DigitSet(  cands[i]   ));  // treat candidates as mask
    } else {
      ++counter[values[i]];
      cells[i].setCandidates(DigitSet( {values[i]} ));  // single value, use { ... }
    }
  }
  return 1;
 }

void SudokuBoard::exportToBuffers(Digit *values, DigitSet *cands) const {
  for (int i = 0; i < 81; i++) {
    values[i] = cells[i].getValue();
    cands[i]  = cells[i].getCandidates();
  }
}

// --- values API ---
Digit SudokuBoard::getValue(Cell idx) const {
  return cells[idx].getValue();
}

bool SudokuBoard::isSolved(Cell idx) const {
  return cells[idx].isSolved();
}

void SudokuBoard::setValue(Cell idx, Digit digit) {
  ++counter[digit];
  cells[idx].setValue(digit);
}

void SudokuBoard::clearValue(Cell idx) {
  --counter[cells[idx].getValue()];
  cells[idx].clearValue();
}

// --- candidates API ---
DigitSet SudokuBoard::getCandidates(Cell idx) const {
  return cells[idx].getCandidates();
}

void SudokuBoard::setCandidates(Cell idx, DigitSet candidates) {
  cells[idx].setCandidates(candidates);
}

bool SudokuBoard::hasCandidate(Cell idx, Digit digit) const {
  return cells[idx].hasCandidate(digit);
}

int SudokuBoard::countCandidates(Cell idx) const {
  return cells[idx].countCandidates();
}

Digit SudokuBoard::getSingleCandidate(Cell idx) const {
  return cells[idx].getSingleCandidate();
}

void SudokuBoard::disableCandidate(Cell idx, Digit digit) {
  cells[idx].disableCandidate(digit);
}

// --- peers API ---
CellSet SudokuBoard::getPeers(Cell idx) const {
  return PEERS[idx];
}

CellSet SudokuBoard::getPeers(const CellSet &idxSet) const {
  CellSet result;
  for (auto it = idxSet.begin(); it != idxSet.end(); ++it) {
    CellSet tmp = this->getPeers(*it);
    if (it == idxSet.begin()) {
      // first iteration, the set is empty
      result.union_assign(tmp);
    } else {
      result.intersect_assign(tmp);
    }
  }
  return result;
}

bool SudokuBoard::sees(Cell a, Cell b) const {
  return PEERS[a].contains(b);
}

// --- positions API ---
const std::vector<Unit> &SudokuBoard::getRows() const {
  return ROW_UNITS;
} 

const std::vector<Unit> &SudokuBoard::getColumns() const {
  return COL_UNITS;
}

const std::vector<Unit> &SudokuBoard::getBoxes() const {
  return BOX_UNITS;
}

const Unit &SudokuBoard::getRowByCell(Cell idx) const {
  return ROW_UNITS[getRowLocation(idx)];
} 

const Unit &SudokuBoard::getColumnByCell(Cell idx) const {
  return COL_UNITS[getColumnLocation(idx)];
}

const Unit &SudokuBoard::getBoxByCell(Cell idx) const {
  return BOX_UNITS[getBoxLocation(idx)];
}

const Unit &SudokuBoard::getRowByLocation(Location idx) const {
  return ROW_UNITS[idx];
} 

const Unit &SudokuBoard::getColumnByLocation(Location idx) const {
  return COL_UNITS[idx];
}

const Unit &SudokuBoard::getBoxByLocation(Location idx) const {
  return BOX_UNITS[idx];
}

Location SudokuBoard::getRowLocation(Cell idx) const {
  return (int)(idx / 9);
}

Location SudokuBoard::getColumnLocation(Cell idx) const {
  return (int)(idx % 9);
}

Location SudokuBoard::getBoxLocation(Cell idx) const {
  const int r = getRowLocation(idx);
  const int c = getColumnLocation(idx);
  return (int)((r / 3) * 3 + (c / 3));
}

CellSet SudokuBoard::getPositionsOfDigit(Unit unit, Digit d) const {
  CellSet positions;
  for (Cell idx : unit) {
    if (this->isSolved(idx)) {
      continue;
    }
    if (this->hasCandidate(idx, d)) {
      positions.insert(idx);
    }
  }
  return positions;
}
  
DigitSet SudokuBoard::getDigitsInLocation(Unit unit, Location i) const {
  std::vector<int> unitList = unit.to_vector();
  Cell idx = unitList[i];
  if (this->isSolved(idx)) {
    return DigitSet(0);
  }
  return this->getCandidates(idx);
}

// --- events API ---
void SudokuBoard::applySetValue(Cell idx, Digit digit) {
  // Set + Auto clear 
  setValue(idx, digit);
  autoClearPeersAfterPlacement(idx, digit);
}

void SudokuBoard::applyRemoveCandidate(Cell idx, Digit digit) {
  disableCandidate(idx, digit);
}

void SudokuBoard::autoClearPeersAfterPlacement(Cell idx, Digit digit) {
  for (Cell i : this->getPeers(idx)) {
    disableCandidate(i, digit);
  }
}

bool SudokuBoard::isCompletelySolved() const {
  for (const SudokuCell &cell : cells) {
    if (!cell.isSolved()) {
      return false;
    }
  }
  return true;
}

DigitSet SudokuBoard::getUnsolvedDigits() const {
  DigitSet result;
  for (auto it = counter.begin(); it != counter.end(); ++it) {
    if (it->second != 9) {
      result.insert(it->first);
    }
  }
  return result;
}

CellSet SudokuBoard::getBivalues() const {
  CellSet result;
  for (Cell i = 0; i < 81; i++) {
    if (countCandidates(i) == 2) {
      result.insert(i);
    }
  }
  return result;
}

bool SudokuBoard::_recalcAllCandidatesFromValues() {
  // Reset completo
  for (Cell i = 0; i < 81; i++) {
    setCandidates(i, DigitSet());
  }

  // Precompute delle mask "used" per ogni unità
  DigitSet rowUsed[9];
  DigitSet colUsed[9];
  DigitSet boxUsed[9];

  // 1) Scansione valori e costruzione used masks + verifica conflitti
  for (Cell idx = 0; idx < 81; idx++) {
    Digit value = getValue(idx);
    if (value == 0) {
      continue;
    }

    int r = getRowLocation(idx);
    int c = getColumnLocation(idx);
    int b = getBoxLocation(idx);

    if ((rowUsed[r].contains(value))) {
      return false;
    }
    if ((colUsed[c].contains(value))) {
      return false;
    }
    if ((boxUsed[b].contains(value))) {
      return false;
    }

    rowUsed[r].insert(value);
    colUsed[c].insert(value);
    boxUsed[b].insert(value);

    // Cella risolta: candidato unico
    setCandidates(idx, DigitSet({value}));
  }

  // 2) Celle vuote: candidati = NOT(used in row/col/box)
  for (Cell idx = 0; idx < 81; idx++) {
    if (isSolved(idx)) {
      continue;
    }

    int r = getRowLocation(idx);
    int c = getColumnLocation(idx);
    int b = getBoxLocation(idx);

    DigitSet used = rowUsed[r] | colUsed[c] | boxUsed[b];
    DigitSet allowed = ALL_DIGITS - used;

    // Se una cella vuota non ha candidati, griglia inconsistente
    if (allowed.empty()) {
      return false;
    }

    setCandidates(idx, allowed);
  }

  return true;
}
