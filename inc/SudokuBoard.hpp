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

  CellSet getPeers(Cell idx) const;

  CellSet getPeers(const CellSet &idxSet) const;

  CellSet getPeersContaining(Cell idx, Digit digit) const;

  CellSet getPeersContaining(const CellSet &idxSet, Digit digit) const;

  bool sees(Cell a, Cell b) const;

  bool sees(CellSet a, CellSet b) const;

  // --- positions API ---

  // given a unit and a digit, tell me which cells contain the digit
  CellSet getPositionsOfDigit(Unit unit, Digit d) const;

  // given a unit and a cell (as index 0..8), tell me which digits are contained in the cell
  DigitSet getDigitsInLocation(Unit unit, Location i) const;

  // --- events API ---

  void applySetValue(Cell idx, Digit digit);

  void applyRemoveCandidate(Cell idx, Digit digit);

  // --- utility API ---

  static const std::vector<Unit> &getRows();

  static const std::vector<Unit> &getColumns();

  static const std::vector<Unit> &getBoxes();

  static const Unit &getRowByCell(Cell idx);

  static const Unit &getColumnByCell(Cell idx);

  static const Unit &getBoxByCell(Cell idx);

  static const Unit &getRowByLocation(Location idx);

  static const Unit &getColumnByLocation(Location idx);

  static const Unit &getBoxByLocation(Location idx);

  static Location getRowLocation(Cell idx);

  static Location getColumnLocation(Cell idx);

  static Location getBoxLocation(Cell idx);

  // --- other ---

  void autoClearPeersAfterPlacement(Cell idx, Digit digit);

  bool isCompletelySolved() const;

  DigitSet getUnsolvedDigits() const;

  int getNumberOfSolvedCells() const;

  CellSet getBivalues() const;

  bool areRemotePair(Cell a, Cell b) const;

private:
  // We keep a local copy (owned) so that solver techniques can mutate freely
  SudokuCell cells[81];

  // Keep track of how many times a digit is solved, speeds up advanced techniques
  std::map<Digit, int> counter;

  // How many cells are solved
  int solvedCells = 0;

  bool _recalcAllCandidatesFromValues();
};

#endif // SUDOKU_BOARD_H
