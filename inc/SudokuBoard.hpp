#ifndef SUDOKU_BOARD_H
#define SUDOKU_BOARD_H

#include <cstdint>
#include "SudokuCell.hpp"

class SudokuBoard
{
public:
  SudokuBoard();

  int importFromString(const char *values);

  int importFromBuffers(const Digit *values, const Mask *cands);

  void exportToBuffers(Digit *values, Mask *cands) const;

  // --- values API ---
  Digit getValue(Index idx) const;

  bool isSolved(Index idx) const;

  void setValue(Index idx, Digit digit);

  void clearValue(Index idx);

  // --- candidates API ---
  Mask getCandidateMask(Index idx) const;

  void setCandidateMask(Index idx, Mask mask);

  bool hasCandidate(Index idx, Digit digit) const;

  size_t countCandidates(Index idx) const;

  Digit getSingleCandidate(Index idx) const;

  void disableCandidate(Index idx, Digit digit) ;

  // --- events API ---
  void applySetValue(Index idx, Digit digit);

  void applyRemoveCandidate(Index idx, Digit digit);

  void autoClearPeersAfterPlacement(Index idx, Digit digit);

  bool isCompletelySolved() const;

private:
  // We keep a local copy (owned) so that solver techniques can mutate freely
  SudokuCell cells[81];

  static inline bool isValidIndex(Index idx);

  bool _recalcAllCandidatesFromValues();
};

#endif // SUDOKU_BOARD_H
