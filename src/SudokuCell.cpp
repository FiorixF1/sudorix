#include "SudokuCell.hpp"
#include "types.hpp"

// =========================================================
// SudokuCell
// =========================================================

SudokuCell::SudokuCell() = default;

// --- value ---
Digit SudokuCell::getValue() const {
  return value;
}

bool SudokuCell::isSolved() const {
  return value != 0;
}

void SudokuCell::setValue(Digit digit) {
  value = digit;
  if (digit != 0) {
    // When solved, keep only the digit bit as candidates.
    candidates = DigitSet({digit});
  }
}

void SudokuCell::clearValue() {
  value = 0;
}

// --- candidates ---
DigitSet SudokuCell::getCandidates() const {
  return candidates;
}

void SudokuCell::setCandidates(DigitSet mask) {
  candidates = mask;
}

bool SudokuCell::hasCandidate(Digit digit) const {
  return candidates.contains(digit);
}

int SudokuCell::countCandidates() const {
  return candidates.size();
}

Digit SudokuCell::getSingleCandidate() const {
  if (candidates.size() == 1) {
    return *candidates.begin();
  }
  return 0;
}

void SudokuCell::enableCandidate(Digit digit) {
  candidates.insert(digit);
}

bool SudokuCell::disableCandidate(Digit digit) {
  bool result = candidates.contains(digit);
  candidates.erase(digit);
  return result;
}

bool SudokuCell::toggleCandidate(Digit digit) {
  bool result = candidates.contains(digit);
  candidates.toggle(digit);
  return result;
}
