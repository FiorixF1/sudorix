#ifndef SUDOKU_BOARD_H
#define SUDOKU_BOARD_H

#include <cstdint>
#include "SudokuCell.hpp"

class SudokuBoard
{
public:
  SudokuBoard();

  int importFromString(const char *values);

  int importFromBuffers(const uint8_t *values, const uint16_t *cands);

  void exportToBuffers(uint8_t *values, uint16_t *cands) const;

  // --- values API ---
  uint8_t getValue(int idx) const;

  bool isSolved(int idx) const;

  void setValue(int idx, uint8_t digit);

  void clearValue(int idx);

  // --- candidates API ---
  uint16_t getCandidateMask(int idx) const;

  void setCandidateMask(int idx, uint16_t mask);

  bool hasCandidate(int idx, uint8_t digit) const;

  uint8_t countCandidates(int idx) const;

  uint8_t getSingleCandidate(int idx) const;

  void disableCandidate(int idx, uint8_t digit) ;

  // --- events API ---
  void applySetValue(int idx, int digit);

  void applyRemoveCandidate(int idx, int digit);

  void autoClearPeersAfterPlacement(int idx, int digit);

  bool isCompletelySolved() const;

private:
  // We keep a local copy (owned) so that solver techniques can mutate freely
  SudokuCell cells[81];

  static inline bool isValidIndex(int idx);

  bool _recalcAllCandidatesFromValues();
};

#endif // SUDOKU_BOARD_H
