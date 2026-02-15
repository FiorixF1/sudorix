#ifndef SUDOKU_BOARD_H
#define SUDOKU_BOARD_H

#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include "SudokuCell.hpp"

enum PeerType : uint8_t {
  ROWS = 1,
  COLUMNS = 2,
  BOXES = 4,
  ALL = 7
};

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

  void disableCandidate(Index idx, Digit digit);

  // --- peers API ---
  Set<Index> getPeers(PeerType peerType, Index idx) const;

  Set<Index> getPeers(PeerType peerType, const Set<Index> &idxSet) const;

  // --- events API ---
  void applySetValue(Index idx, Digit digit);

  void applyRemoveCandidate(Index idx, Digit digit);

  // --- other ---
  void autoClearPeersAfterPlacement(Index idx, Digit digit);

  bool isCompletelySolved() const;

  Set<Digit> getAvailableDigits() const;

private:
  // We keep a local copy (owned) so that solver techniques can mutate freely
  SudokuCell cells[81];

  // Keep track of how many times a digit is solved, speeds up advanced techniques
  std::map<Digit, int> counter;

  static inline bool isValidIndex(Index idx);

  bool _recalcAllCandidatesFromValues();
};

#endif // SUDOKU_BOARD_H
