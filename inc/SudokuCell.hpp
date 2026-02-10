#ifndef SUDOKU_CELL_H
#define SUDOKU_CELL_H

#include <cstdint>

class SudokuCell
{
public:
  SudokuCell();

  // --- value ---
  uint8_t getValue() const;

  bool isSolved() const;

  void setValue(uint8_t digit);

  void clearValue();

  // --- candidates ---
  uint16_t getCandidateMask() const;

  void setCandidateMask(uint16_t mask);

  bool hasCandidate(uint8_t digit) const;

  uint8_t countCandidates() const;

  uint8_t getSingleCandidate() const;

  void enableCandidate(uint8_t digit);

  bool disableCandidate(uint8_t digit);

  bool toggleCandidate(uint8_t digit);

private:
  uint8_t  value;     // 0..9
  uint16_t candMask;  // 9-bit
};

#endif // SUDOKU_CELL_H
