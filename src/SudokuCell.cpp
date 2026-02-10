#include "SudokuCell.hpp"
#include "utils.hpp"

// =========================================================
// SudokuCell
// =========================================================

SudokuCell::SudokuCell() : value(0), candMask(0) { }

// --- value ---
uint8_t SudokuCell::getValue() const {
  return value;
}

bool SudokuCell::isSolved() const {
  return value != 0;
}

void SudokuCell::setValue(uint8_t digit) {
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
uint16_t SudokuCell::getCandidateMask() const {
  return (uint16_t)(candMask & 0x1FFu);
}

void SudokuCell::setCandidateMask(uint16_t mask) {
  candMask = (uint16_t)(mask & 0x1FFu);
}

bool SudokuCell::hasCandidate(uint8_t digit) const {
  return (getCandidateMask() & digitToBit(digit)) != 0;
}

uint8_t SudokuCell::countCandidates() const {
  return countBits9(getCandidateMask());
}

uint8_t SudokuCell::getSingleCandidate() const {
  const uint16_t m = getCandidateMask();
  if (countBits9(m) == 1) {
    return bitToDigitSingle(m);
  }
  return 0;
}

void SudokuCell::enableCandidate(uint8_t digit) {
  candMask |= digitToBit(digit);
}

bool SudokuCell::disableCandidate(uint8_t digit) {
  const uint16_t bit = digitToBit(digit);
  const uint16_t before = getCandidateMask();
  const uint16_t after = (uint16_t)(before & ~bit);
  if (after != before) {
    candMask = after;
    return true;
  }
  return false;
}

bool SudokuCell::toggleCandidate(uint8_t digit) {
  const uint16_t bit = digitToBit(digit);
  const bool wasOn = (candMask & bit) != 0;
  candMask ^= bit;
  return !wasOn;
}
