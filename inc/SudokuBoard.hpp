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

  int importFromBuffers(const uint8_t *values, const uint16_t *cands);

  void exportToBuffers(Digit *values, DigitSet *cands) const;

  // --- values API ---
  Digit getValue(Index idx) const;

  bool isSolved(Index idx) const;

  void setValue(Index idx, Digit digit);

  void clearValue(Index idx);

  // --- candidates API ---
  DigitSet getCandidates(Index idx) const;

  void setCandidates(Index idx, DigitSet candidates);

  bool hasCandidate(Index idx, Digit digit) const;

  int countCandidates(Index idx) const;

  Digit getSingleCandidate(Index idx) const;

  void disableCandidate(Index idx, Digit digit);

  // --- peers API ---
  IndexSet getPeers(Index idx, PeerType peerType = PeerType::ALL) const;

  IndexSet getPeers(const IndexSet &idxSet, PeerType peerType = PeerType::ALL) const;

  // --- events API ---
  void applySetValue(Index idx, Digit digit);

  void applyRemoveCandidate(Index idx, Digit digit);

  // --- other ---
  void autoClearPeersAfterPlacement(Index idx, Digit digit);

  bool isCompletelySolved() const;

  DigitSet getUnsolvedDigits() const;

private:
  // We keep a local copy (owned) so that solver techniques can mutate freely
  SudokuCell cells[81];

  // Keep track of how many times a digit is solved, speeds up advanced techniques
  std::map<Digit, int> counter;

  bool _recalcAllCandidatesFromValues();
};

#endif // SUDOKU_BOARD_H
