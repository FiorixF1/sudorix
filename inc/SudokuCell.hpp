#ifndef SUDOKU_CELL_H
#define SUDOKU_CELL_H

#include <cstdint>
#include "Event.hpp"

class SudokuCell
{
public:
  SudokuCell();

  // --- value ---
  Digit getValue() const;

  bool isSolved() const;

  void setValue(Digit digit);

  void clearValue();

  // --- candidates ---
  Mask getCandidateMask() const;

  void setCandidateMask(Mask mask);

  bool hasCandidate(Digit digit) const;

  size_t countCandidates() const;

  Digit getSingleCandidate() const;

  void enableCandidate(Digit digit);

  bool disableCandidate(Digit digit);

  bool toggleCandidate(Digit digit);

private:
  Digit  value;     // 0..9
  Mask   candMask;  // 9-bit
};

#endif // SUDOKU_CELL_H
