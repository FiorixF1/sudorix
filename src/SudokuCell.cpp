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
    candMask = DigitSet({digit});
  }
}

void SudokuCell::clearValue() {
  value = 0;
}

// --- candidates ---
DigitSet SudokuCell::getCandidates() const {
  return candMask;
}

void SudokuCell::setCandidates(DigitSet mask) {
  candMask = mask;
}

bool SudokuCell::hasCandidate(Digit digit) const {
  return candMask.contains(digit);
}

int SudokuCell::countCandidates() const {
  return candMask.size();
}

Digit SudokuCell::getSingleCandidate() const {
  if (candMask.size() == 1) {
    return *candMask.begin();
  }
  return 0;
}

void SudokuCell::enableCandidate(Digit digit) {
  candMask.insert(digit);
}

bool SudokuCell::disableCandidate(Digit digit) {
  bool result = candMask.contains(digit);
  candMask.erase(digit);
  return result;
}

bool SudokuCell::toggleCandidate(Digit digit) {
  bool result = candMask.contains(digit);
  candMask.toggle(digit);
  return result;
}
