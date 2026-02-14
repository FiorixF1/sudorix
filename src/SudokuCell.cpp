#include "SudokuCell.hpp"
#include "utils.hpp"

// =========================================================
// SudokuCell
// =========================================================

SudokuCell::SudokuCell() : value(0), candMask(0) { }

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
    candMask = digitToBit(digit);
  }
}

void SudokuCell::clearValue() {
  value = 0;
}

// --- candidates ---
Mask SudokuCell::getCandidateMask() const {
  return (Mask)(candMask & 0x1FFu);
}

void SudokuCell::setCandidateMask(Mask mask) {
  candMask = (Mask)(mask & 0x1FFu);
}

bool SudokuCell::hasCandidate(Digit digit) const {
  return (getCandidateMask() & digitToBit(digit)) != 0;
}

size_t SudokuCell::countCandidates() const {
  return countBits9(getCandidateMask());
}

Digit SudokuCell::getSingleCandidate() const {
  const Mask m = getCandidateMask();
  if (countBits9(m) == 1) {
    return bitToDigitSingle(m);
  }
  return 0;
}

void SudokuCell::enableCandidate(Digit digit) {
  candMask |= digitToBit(digit);
}

bool SudokuCell::disableCandidate(Digit digit) {
  const Mask bit = digitToBit(digit);
  const Mask before = getCandidateMask();
  const Mask after = (uint16_t)(before & ~bit);
  if (after != before) {
    candMask = after;
    return true;
  }
  return false;
}

bool SudokuCell::toggleCandidate(Digit digit) {
  const Mask bit = digitToBit(digit);
  const bool wasOn = (candMask & bit) != 0;
  candMask ^= bit;
  return !wasOn;
}
