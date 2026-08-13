#ifndef SUDOKU_BOARD_H
#define SUDOKU_BOARD_H

#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include "SudokuCell.hpp"
#include "AIC.hpp"
#include "ALS.hpp"

class SudokuBoard
{
public:
  SudokuBoard();

  void clear();

  int importValues(const std::string &values);

  int importCandidates(const std::vector<std::vector<int>> &candidates);

  int importPuzzle(const std::string &values);

  json to_json();

  int from_json(const json &j);

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

  // --- subsets API ---

  // Given a unit and a digit, tell me which cells contain the digit
  CellSet getPositionsOfDigit(const Unit &unit, Digit d) const;

  // Given a unit and a set of digits, tell me which cells contain any of the digits
  // Hint: N digits exactly in N cells = hidden subset
  CellSet getPositionsOfDigitsAny(const Unit &unit, DigitSet set) const;

  // Given a unit and a set of digits, tell me which cells contain only those digits
  CellSet getPositionsOfDigitsStrict(const Unit &unit, DigitSet set) const;

  // Given a unit and a cell (as index 0..8), tell me which digits are contained in the cell
  DigitSet getDigitsInLocation(const Unit &unit, Location i) const;

  // Given a unit and a set of cells (as index 0..8), tell me which digits are contained in the cells
  // Hint: N cells containing exactly N digits = naked subset
  DigitSet getDigitsInLocations(const Unit &unit, LocationSet set) const;

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

  // --- AIC & ALS ---

  AicGraph getPrunedAicGraph(const AicConfig &config);

  AlsGraph &getAlsGraph(const AlsConfig &config);

  // --- other ---

  void autoClearPeersAfterPlacement(Cell idx, Digit digit);

  bool isCompletelySolved() const;

  int countSolutions();

  // Return the digits that are not completely solved yet in the grid
  DigitSet getUnsolvedDigits() const;

  // Return the digits that are not solved yet in the unit
  DigitSet getUnsolvedDigits(const Unit &unit) const;

  // Return the cells of a unit (as index 0..8) that are not solved yet
  LocationSet getUnsolvedLocations(const Unit &unit) const;

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

  // Graph for AIC
  AicGraphBuilder aicGraphBuilder;
  AicGraph aicGraph;
  bool aic_graph_valid = false;

  // Graph for ALS
  AlsGraphBuilder alsGraphBuilder;
  AlsGraph alsGraph;
  bool als_graph_valid = false;

  bool _recalcAllCandidatesFromValues();
  void _invalidateCache();
  void _countSolutionsImpl(Cell current_cell, int &found_solutions);
};

#endif // SUDOKU_BOARD_H
