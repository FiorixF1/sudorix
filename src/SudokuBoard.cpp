#include "SudokuBoard.hpp"
#include "utils.hpp"

// =========================================================
// SudokuBoard
// =========================================================

// empty board
SudokuBoard::SudokuBoard() = default;

// only values, candidates are calculated automatically
int SudokuBoard::importFromString(const char *values) {
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
int SudokuBoard::importFromBuffers(const uint8_t *values, const uint16_t *cands) {
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

void SudokuBoard::exportToBuffers(uint8_t *values, uint16_t *cands) const {
  for (int i = 0; i < 81; i++) {
    values[i] = cells[i].getValue();
    cands[i]  = cells[i].getCandidateMask();
  }
}

// --- values API ---
Digit SudokuBoard::getValue(Index idx) const {
  return cells[idx].getValue();
}

bool SudokuBoard::isSolved(Index idx) const {
  return cells[idx].isSolved();
}

void SudokuBoard::setValue(Index idx, Digit digit) {
  cells[idx].setValue(digit);
}

void SudokuBoard::clearValue(Index idx) {
  cells[idx].clearValue();
}

// --- candidates API ---
Mask SudokuBoard::getCandidateMask(Index idx) const {
  return cells[idx].getCandidateMask();
}

void SudokuBoard::setCandidateMask(Index idx, Mask mask) {
  cells[idx].setCandidateMask(mask);
}

bool SudokuBoard::hasCandidate(Index idx, Digit digit) const {
  return cells[idx].hasCandidate(digit);
}

size_t SudokuBoard::countCandidates(Index idx) const {
  return cells[idx].countCandidates();
}

Digit SudokuBoard::getSingleCandidate(Index idx) const {
  return cells[idx].getSingleCandidate();
}

void SudokuBoard::disableCandidate(Index idx, Digit digit) {
  cells[idx].disableCandidate(digit);
}

// --- events API ---
void SudokuBoard::applySetValue(Index idx, Digit digit) {
  // Set + Auto clear 
  setValue(idx, digit);
  autoClearPeersAfterPlacement(idx, digit);
}

void SudokuBoard::applyRemoveCandidate(Index idx, Digit digit) {
  // Remove + Auto place if applicable
  disableCandidate(idx, digit);
  int only = getSingleCandidate(idx);
  if (only) {
    return applySetValue(idx, only);
  }
}

void SudokuBoard::autoClearPeersAfterPlacement(Index idx, Digit digit) {
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

bool SudokuBoard::isCompletelySolved() const {
  for (const SudokuCell &cell : cells) {
    if (!cell.isSolved()) {
      return false;
    }
  }
  return true;
}

inline bool SudokuBoard::isValidIndex(Index idx) {
  return idx >= 0 && idx < 81;
}

bool SudokuBoard::_recalcAllCandidatesFromValues() {
  // Reset completo
  for (int i = 0; i < 81; i++) {
    setCandidateMask(i, 0);
  }

  // Precompute delle mask "used" per ogni unità
  uint16_t rowUsed[9] = {0};
  uint16_t colUsed[9] = {0};
  uint16_t boxUsed[9] = {0};

  // 1) Scansione valori e costruzione used masks + verifica conflitti
  for (Index idx = 0; idx < 81; idx++) {
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
  for (Index idx = 0; idx < 81; idx++) {
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
