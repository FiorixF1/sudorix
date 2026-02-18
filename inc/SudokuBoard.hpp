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
  Digit getValue(Cell idx) const;

  bool isSolved(Cell idx) const;

  void setValue(Cell idx, Digit digit);

  void clearValue(Cell idx);

  // --- candidates API ---
  DigitSet getCandidates(Cell idx) const;

  void setCandidates(Cell idx, DigitSet candidates);

  bool hasCandidate(Cell idx, Digit digit) const;

  int countCandidates(Cell idx) const;

  Digit getSingleCandidate(Cell idx) const;

  void disableCandidate(Cell idx, Digit digit);

  // --- peers API ---
  IndexSet getPeers(Cell idx, PeerType peerType = PeerType::ALL) const;

  IndexSet getPeers(const IndexSet &idxSet, PeerType peerType = PeerType::ALL) const;

  // --- positions API ---

  // given a unit and a digit, tell me which cells contain the digit
  IndexSet getPositionsOfDigit(Unit unit, Digit d) const;

  // given a unit and a cell (as index 0..8), tell me which digits are contained in the cell
  DigitSet getDigitsInPosition(Unit unit, int i) const;

  // --- events API ---
  void applySetValue(Cell idx, Digit digit);

  void applyRemoveCandidate(Cell idx, Digit digit);

  // --- units API ---
  const std::vector<Unit> &getRows() const;

  const std::vector<Unit> &getColumns() const;

  const std::vector<Unit> &getBoxes() const;

  const Unit &getRowByCell(Cell idx) const;

  const Unit &getColumnByCell(Cell idx) const;

  const Unit &getBoxByCell(Cell idx) const;

  const Unit &getRowByIndex08(int idx) const;

  const Unit &getColumnByIndex08(int idx) const;

  const Unit &getBoxByIndex08(int idx) const;

  int getRowIndex08(Cell idx) const;

  int getColumnIndex08(Cell idx) const;

  int getBoxIndex08(Cell idx) const;

  // --- other ---
  void autoClearPeersAfterPlacement(Cell idx, Digit digit);

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
