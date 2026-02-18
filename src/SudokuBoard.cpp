#include "SudokuBoard.hpp"
#include "utils.hpp"

// =========================================================
// Precomputed indices (rows / cols / boxes)
// =========================================================

static const std::vector<Unit> ROW_UNITS = {
    IndexSet({ 0,  1,  2,  3,  4,  5,  6,  7,  8 }), 
    IndexSet({ 9, 10, 11, 12, 13, 14, 15, 16, 17 }),
    IndexSet({18, 19, 20, 21, 22, 23, 24, 25, 26 }),
    IndexSet({27, 28, 29, 30, 31, 32, 33, 34, 35 }),
    IndexSet({36, 37, 38, 39, 40, 41, 42, 43, 44 }),
    IndexSet({45, 46, 47, 48, 49, 50, 51, 52, 53 }),
    IndexSet({54, 55, 56, 57, 58, 59, 60, 61, 62 }),
    IndexSet({63, 64, 65, 66, 67, 68, 69, 70, 71 }),
    IndexSet({72, 73, 74, 75, 76, 77, 78, 79, 80 })
  };

static const std::vector<Unit> COL_UNITS = {
    IndexSet({ 0,  9, 18, 27, 36, 45, 54, 63, 72 }),
    IndexSet({ 1, 10, 19, 28, 37, 46, 55, 64, 73 }),
    IndexSet({ 2, 11, 20, 29, 38, 47, 56, 65, 74 }),
    IndexSet({ 3, 12, 21, 30, 39, 48, 57, 66, 75 }),
    IndexSet({ 4, 13, 22, 31, 40, 49, 58, 67, 76 }),
    IndexSet({ 5, 14, 23, 32, 41, 50, 59, 68, 77 }),
    IndexSet({ 6, 15, 24, 33, 42, 51, 60, 69, 78 }),
    IndexSet({ 7, 16, 25, 34, 43, 52, 61, 70, 79 }),
    IndexSet({ 8, 17, 26, 35, 44, 53, 62, 71, 80 })
  };

static const std::vector<Unit> BOX_UNITS = {
    IndexSet({ 0,  1,  2,  9, 10, 11, 18, 19, 20 }),
    IndexSet({ 3,  4,  5, 12, 13, 14, 21, 22, 23 }),
    IndexSet({ 6,  7,  8, 15, 16, 17, 24, 25, 26 }),
    IndexSet({27, 28, 29, 36, 37, 38, 45, 46, 47 }),
    IndexSet({30, 31, 32, 39, 40, 41, 48, 49, 50 }),
    IndexSet({33, 34, 35, 42, 43, 44, 51, 52, 53 }),
    IndexSet({54, 55, 56, 63, 64, 65, 72, 73, 74 }),
    IndexSet({57, 58, 59, 66, 67, 68, 75, 76, 77 }),
    IndexSet({60, 61, 62, 69, 70, 71, 78, 79, 80 })
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
IndexSet SudokuBoard::getPeers(Cell idx, PeerType peerType) const {
  IndexSet peers;

  if (peerType & PeerType::ROWS) {
    peers.union_assign(getRowByCell(idx));
  }
  
  if (peerType & PeerType::COLUMNS) {
    peers.union_assign(getColumnByCell(idx));
  }

  if (peerType & PeerType::BOXES) {
    peers.union_assign(getBoxByCell(idx));
  }
  
  // Consider only unsolved cells and exclude the input cell
  IndexSet result = peers.filter([&](Cell i){ return i != idx && !isSolved(i); });

  return result;
}

IndexSet SudokuBoard::getPeers(const IndexSet &idxSet, PeerType peerType) const {
  IndexSet result;
  for (auto it = idxSet.begin(); it != idxSet.end(); ++it) {
    IndexSet tmp = this->getPeers(*it, peerType);
    if (it == idxSet.begin()) {
      // first iteration, the set is empty
      result.union_assign(tmp);
    } else {
      result.intersect_assign(tmp);
    }
  }
  return result;
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
  return ROW_UNITS[getRowIndex08(idx)];
} 

const Unit &SudokuBoard::getColumnByCell(Cell idx) const {
  return COL_UNITS[getColumnIndex08(idx)];
}

const Unit &SudokuBoard::getBoxByCell(Cell idx) const {
  return BOX_UNITS[getBoxIndex08(idx)];
}

const Unit &SudokuBoard::getRowByIndex08(int idx) const {
  return ROW_UNITS[idx];
} 

const Unit &SudokuBoard::getColumnByIndex08(int idx) const {
  return COL_UNITS[idx];
}

const Unit &SudokuBoard::getBoxByIndex08(int idx) const {
  return BOX_UNITS[idx];
}

int SudokuBoard::getRowIndex08(Cell idx) const {
  return (int)(idx / 9);
}

int SudokuBoard::getColumnIndex08(Cell idx) const {
  return (int)(idx % 9);
}

int SudokuBoard::getBoxIndex08(Cell idx) const {
  const int r = getRowIndex08(idx);
  const int c = getColumnIndex08(idx);
  return (int)((r / 3) * 3 + (c / 3));
}

IndexSet SudokuBoard::getPositionsOfDigit(Unit unit, Digit d) const {
  IndexSet positions;
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
  
DigitSet SudokuBoard::getDigitsInPosition(Unit unit, int i) const {
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
  // Remove + Auto place if applicable
  disableCandidate(idx, digit);
  // auto place not supported in UI
  //int only = getSingleCandidate(idx);
  //if (only) {
  //  return applySetValue(idx, only);
  //}
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

    int r = getRowIndex08(idx);
    int c = getColumnIndex08(idx);
    int b = getBoxIndex08(idx);

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

    int r = getRowIndex08(idx);
    int c = getColumnIndex08(idx);
    int b = getBoxIndex08(idx);

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
