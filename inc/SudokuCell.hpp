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
  DigitSet getCandidates() const;

  void setCandidates(DigitSet mask);

  bool hasCandidate(Digit digit) const;

  int countCandidates() const;

  Digit getSingleCandidate() const;

  void enableCandidate(Digit digit);

  bool disableCandidate(Digit digit);

  bool toggleCandidate(Digit digit);

private:
  Digit    value;       // 0..9
  DigitSet candidates;  // 9-bit
};

#endif // SUDOKU_CELL_H
