#include <cstdint>
#include <cstddef>
#include <cstring>
#include <functional>
#include <set>
#include <vector>

#include "solver.hpp"
#include "SudokuBoard.hpp"
#include "EventQueue.hpp"
#include "utils.hpp"

static SudokuBoard g_sudokuBoard;
static EventQueue g_eventQueue;

// =========================================================
// Techniques
// =========================================================

static void techFullHouse(SudokuBoard &board) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    Cell emptyIdx = -1;
    Digit missingDigit = 0;
    DigitSet present;

    for (Cell idx : unit) {
      const Digit value = board.getValue(idx);
      if (value == 0) {
        if (emptyIdx != -1) {
          emptyIdx = -2; // more than one empty
          break;
        }
        emptyIdx = idx;
      } else {
        present.insert(value);
      }
    }

    if (emptyIdx >= 0) {
      const DigitSet missingDigits = ALL_DIGITS - present;
      if (missingDigits.size() == 1) {
        missingDigit = *missingDigits.begin();
        Event event(EventType::SetValue, ReasonId::FullHouse);
        event.addOperation(emptyIdx, missingDigit);
        g_eventQueue.enqueue(board, event);
      }
    }
  };

  for (const Unit &box : board.getBoxes()) {
    scanUnit(box);
  }
  for (const Unit &row : board.getRows()) {
    scanUnit(row);
  }
  for (const Unit &column: board.getColumns()) {
    scanUnit(column);
  }
}

static void techNakedSingles(SudokuBoard &board) {
  for (Cell i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }
    const Digit d = board.getSingleCandidate(i);
    if (d != 0) {
      Event event(EventType::SetValue, ReasonId::NakedSingle);
      event.addOperation(i, d);
      g_eventQueue.enqueue(board, event);
    }
  }
}

static void techNakedPairs(SudokuBoard &board) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    // map each location to its digits in this unit
    DigitSet digitsOfLocation[9];
    std::vector<int> unitList = unit.to_vector();
    for (Location i = 0; i < 9; ++i) {
      digitsOfLocation[i] = board.getDigitsInLocation(unit, i);
    }

    // iterate over all locations that have exactly two digits in the unit
    for (Location a = 0; a < 8; ++a) {
      if (digitsOfLocation[a].size() == 2) {
        for (Location b = a+1; b < 9; ++b) {
          if (digitsOfLocation[a] == digitsOfLocation[b]) {
            // naked pair spotted
            Event event(EventType::RemoveCandidate, ReasonId::NakedPair);
            // the source is the two cells containing the pair
            CellSet sourceSet = CellSet({unitList[a], unitList[b]});
            event.addSource(sourceSet, digitsOfLocation[a]);
            for (Cell idx : board.getPeers(sourceSet)) {
              // remove digits present in a and b from other cells they can see
              event.addOperation(idx, digitsOfLocation[a]);
            }
            g_eventQueue.enqueue(board, event);
          }
        }
      }
    }
  };

  for (const Unit &row : board.getRows()) {
    scanUnit(row);
  }
  for (const Unit &column: board.getColumns()) {
    scanUnit(column);
  }
  for (const Unit &box : board.getBoxes()) {
    scanUnit(box);
  }
}

static void techNakedTriples(SudokuBoard &board) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    // map each location to its digits in this unit
    DigitSet digitsOfLocation[9];
    for (Location i = 0; i < 9; ++i) {
      digitsOfLocation[i] = board.getDigitsInLocation(unit, i);
    }

    // iterate over all locations that have up to three digits in the unit
    std::vector<int> unitList = unit.to_vector();
    for (Location a = 0; a < 7; ++a) {
      if (!digitsOfLocation[a].empty() && digitsOfLocation[a].size() <= 3) {
        for (Location b = a+1; b < 8; ++b) {
          if (!digitsOfLocation[b].empty() && (digitsOfLocation[a] | digitsOfLocation[b]).size() <= 3) {
            for (Location c = b+1; c < 9; ++c) {
              if (!digitsOfLocation[c].empty() && (digitsOfLocation[a] | digitsOfLocation[b] | digitsOfLocation[c]).size() == 3) {
                // naked triple spotted
                Event event(EventType::RemoveCandidate, ReasonId::NakedTriple);
                DigitSet lockedSet = digitsOfLocation[a] | digitsOfLocation[b] | digitsOfLocation[c];
                // the source is the three cells containing the triple
                CellSet sourceSet = CellSet({unitList[a], unitList[b], unitList[c]});
                event.addSource(sourceSet, lockedSet);
                for (Cell idx : board.getPeers(sourceSet)) {
                  // remove digits present in a, b, c from other cells they can see
                  event.addOperation(idx, lockedSet);
                }
                g_eventQueue.enqueue(board, event);
              }
            }
          }
        }
      }
    }
  };

  for (const Unit &row : board.getRows()) {
    scanUnit(row);
  }
  for (const Unit &column: board.getColumns()) {
    scanUnit(column);
  }
  for (const Unit &box : board.getBoxes()) {
    scanUnit(box);
  }
}

static void techHiddenSingles(SudokuBoard &board, const Unit &unit) {
  // map each digit to its positions in this unit
  CellSet positionsOfDigit[10];
  for (Digit d : board.getUnsolvedDigits()) {
    positionsOfDigit[d] = board.getPositionsOfDigit(unit, d);
  }

  // iterate over all digits that appear exactly once in the unit
  for (Digit a = 1; a <= 9; ++a) {
    if (positionsOfDigit[a].size() == 1) {
      // hidden single spotted
      Event event(EventType::SetValue, ReasonId::HiddenSingle);
      event.addOperation(*positionsOfDigit[a].begin(), a);
      g_eventQueue.enqueue(board, event);
    }
  }
}

static void techHiddenSinglesBox(SudokuBoard &board) {
  for (const Unit &box : board.getBoxes()) {
    techHiddenSingles(board, box);
  }
}

static void techHiddenSinglesRowColumn(SudokuBoard &board) {
  for (const Unit &row : board.getRows()) {
    techHiddenSingles(board, row);
  }
  for (const Unit &column: board.getColumns()) {
    techHiddenSingles(board, column);
  }
}

static void techHiddenPairs(SudokuBoard &board, const Unit &unit) {
  // map each digit to its positions in this unit
  CellSet positionsOfDigit[10];
  for (Digit d : board.getUnsolvedDigits()) {
    positionsOfDigit[d] = board.getPositionsOfDigit(unit, d);
  }

  // iterate over all pairs of digits that appear exactly twice in the unit
  for (Digit a = 1; a <= 8; ++a) {
    if (positionsOfDigit[a].size() == 2) {
      for (Digit b = a+1; b <= 9; ++b) {
        if (positionsOfDigit[a] == positionsOfDigit[b]) {
          // hidden pair spotted
          Event event(EventType::RemoveCandidate, ReasonId::HiddenPair);
          // the source is the two cells containing the pair
          CellSet lockedSet = positionsOfDigit[a];
          CellSet sourceSet = lockedSet;
          event.addSource(sourceSet, DigitSet({a, b}));
          for (Cell idx : lockedSet) {
            // remove digits different from a and b from the cells of the pair
            event.addOperation(idx, board.getUnsolvedDigits() - DigitSet({a, b}));
          }
          g_eventQueue.enqueue(board, event);
        }
      }
    }
  }
}

static void techHiddenPairsBox(SudokuBoard &board) {
  for (const Unit &box : board.getBoxes()) {
    techHiddenPairs(board, box);
  }
}

static void techHiddenPairsRowColumn(SudokuBoard &board) {
  for (const Unit &row : board.getRows()) {
    techHiddenPairs(board, row);
  }
  for (const Unit &column: board.getColumns()) {
    techHiddenPairs(board, column);
  }
}

static void techHiddenTriples(SudokuBoard &board) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    // map each digit to its positions in this unit
    CellSet positionsOfDigit[10];
    for (Digit d : board.getUnsolvedDigits()) {
      positionsOfDigit[d] = board.getPositionsOfDigit(unit, d);
    }

    // iterate over all digits that appear up to three times in the unit
    for (Digit a = 1; a <= 7; ++a) {
      if (!positionsOfDigit[a].empty() && positionsOfDigit[a].size() <= 3) {
        for (Digit b = a+1; b <= 8; ++b) {
          if (!positionsOfDigit[b].empty() && (positionsOfDigit[a] | positionsOfDigit[b]).size() <= 3) {
            for (Digit c = b+1; c <= 9; ++c) {
              if (!positionsOfDigit[c].empty() && (positionsOfDigit[a] | positionsOfDigit[b] | positionsOfDigit[c]).size() == 3) {
                // hidden triple spotted
                Event event(EventType::RemoveCandidate, ReasonId::HiddenTriple);
                // the source is the three cells containing the triple
                CellSet lockedSet = positionsOfDigit[a] | positionsOfDigit[b] | positionsOfDigit[c];
                CellSet sourceSet = lockedSet;
                event.addSource(sourceSet, DigitSet({a, b, c}));
                for (Cell idx : lockedSet) {
                  // remove digits different from a, b, c from the cells of the triple
                  event.addOperation(idx, board.getCandidates(idx) - DigitSet({a, b, c}));
                }
                g_eventQueue.enqueue(board, event);
              }
            }
          }
        }
      }
    }
  };

  for (const Unit &row : board.getRows()) {
    scanUnit(row);
  }
  for (const Unit &column: board.getColumns()) {
    scanUnit(column);
  }
  for (const Unit &box : board.getBoxes()) {
    scanUnit(box);
  }
}

static void techPointingSet(SudokuBoard &board) {
  // For each box and digit:
  //  - if all candidates are confined to a single row within the box,
  //    remove the digit from that row outside the box
  //  - same for a single column
  for (const Unit &box : board.getBoxes()) {
    for (Digit digit : board.getUnsolvedDigits()) {
      // get cells where the digit is present in the box
      CellSet positions = board.getPositionsOfDigit(box, digit);

      int posCount = positions.size();
      if (posCount < 2 || posCount > 3) {
        continue; // pointing set is about confinement with 2 or 3
      }

      ReasonId reasonId;
      if (posCount == 2) {
        reasonId = ReasonId::PointingPair;
      } else if (posCount == 3) {
        reasonId = ReasonId::PointingTriple;
      }

      const Location r0 = SudokuBoard::getRowLocation(*positions.begin());
      bool sameRow = true;
      for (Cell pos : positions) {
        if (SudokuBoard::getRowLocation(pos) != r0) {
          sameRow = false;
          break;
        }
      }

      if (sameRow) {
        Event event(EventType::RemoveCandidate, reasonId);
        // the source is the cells containing the digit
        CellSet sourceSet = positions;
        event.addSource(sourceSet, digit);
        // remove digit from row r0, excluding cells in this box
        CellSet set = SudokuBoard::getRowByLocation(r0).difference_with(box);
        for (Cell idx : set) {
          event.addOperation(idx, digit);
        }
        g_eventQueue.enqueue(board, event);
      }

      const Location c0 = SudokuBoard::getColumnLocation(*positions.begin());
      bool sameCol = true;
      for (Cell pos : positions) {
        if (SudokuBoard::getColumnLocation(pos) != c0) {
          sameCol = false;
          break;
        }
      }

      if (sameCol) {
        Event event(EventType::RemoveCandidate, reasonId);
        // the source is the cells containing the digit
        CellSet sourceSet = positions;
        event.addSource(sourceSet, digit);
        // remove digit from column c0, excluding cells in this box
        CellSet set = SudokuBoard::getColumnByLocation(c0).difference_with(box);
        for (Cell idx : set) {
          event.addOperation(idx, digit);
        }
        g_eventQueue.enqueue(board, event);
      }
    }
  }
}

static void techBoxLineReduction(SudokuBoard &board) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    for (Digit digit : board.getUnsolvedDigits()) {
      // get cells where the digit is present in the unit
      CellSet positions = board.getPositionsOfDigit(unit, digit);

      int posCount = positions.size();
      if (posCount < 2 || posCount > 3) {
        continue; // box line reduction is about confinement with 2 or 3
      }

      ReasonId reasonId;
      if (posCount == 2) {
        reasonId = ReasonId::ClaimingPair;
      } else if (posCount == 3) {
        reasonId = ReasonId::ClaimingTriple;
      }

      LocationSet boxes;
      bool sameBlock = true;
      for (Cell pos : positions) {
        boxes.insert(SudokuBoard::getBoxLocation(pos));
      }

      if (boxes.size() == 1) {
        Location boxIdx = *boxes.begin();
        Event event(EventType::RemoveCandidate, reasonId);
        // the source is the cells containing the digit
        CellSet sourceSet = positions;
        event.addSource(sourceSet, digit);
        // remove digit from this box, excluding cells in this row/column
        CellSet set = SudokuBoard::getBoxByLocation(boxIdx).difference_with(unit);
        for (Cell idx : set) {
          event.addOperation(idx, digit);
        }
        g_eventQueue.enqueue(board, event);
      }
    }
  };

  for (const Unit &row : board.getRows()) {
    scanUnit(row);
  }
  for (const Unit &column : board.getColumns()) {
    scanUnit(column);
  }
}

static void techBUGPlusOne(SudokuBoard &board) {
  // Condition 1: only bivalue cells except for one trivalue cell
  Cell trivalueCell = -1;
  for (Cell i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }

    DigitSet candidates = board.getCandidates(i);
    if (candidates.size() == 3) {
      if (trivalueCell != -1) {
        // not applicable
        return;
      }
      trivalueCell = i;
    } else if (candidates.size() != 2) {
      // not applicable
      return;
    }
  }

  if (trivalueCell != -1) {
    // Condition 2: each digit appears twice, except for one digit that appears three times
    Digit trilocationDigitRowValue = 0;
    Location trilocationDigitRowLocation = -1;
    for (Digit d : board.getUnsolvedDigits()) {
      for (Location l = 0; l < 9; ++l) {
        const Unit &row = SudokuBoard::getRowByLocation(l);
        const CellSet &tmp = board.getPositionsOfDigit(row, d);
        if (!tmp.empty()) {
          if (tmp.size() == 3) {
            if (trilocationDigitRowLocation != -1) {
              // not applicable
              return;
            }
            trilocationDigitRowValue = d;
            trilocationDigitRowLocation = l;
          } else if (tmp.size() != 2) {
            // not applicable
            return;
          }
        }
      }
    }

    Digit trilocationDigitColumnValue = 0;
    Location trilocationDigitColumnLocation = -1;
    for (Digit d : board.getUnsolvedDigits()) {
      for (Location l = 0; l < 9; ++l) {
        const Unit &column = SudokuBoard::getColumnByLocation(l);
        const CellSet &tmp = board.getPositionsOfDigit(column, d);
        if (!tmp.empty()) {
          if (tmp.size() == 3) {
            if (trilocationDigitColumnLocation != -1) {
              // not applicable
              return;
            }
            trilocationDigitColumnValue = d;
            trilocationDigitColumnLocation = l;
          } else if (tmp.size() != 2) {
            // not applicable
            return;
          }
        }
      }
    }

    if (trilocationDigitRowValue == trilocationDigitColumnValue &&
        SudokuBoard::getRowLocation(trivalueCell) == trilocationDigitRowLocation &&
        SudokuBoard::getColumnLocation(trivalueCell) == trilocationDigitColumnLocation) {
      // BUG+1 spotted
      Digit solution = trilocationDigitRowValue;
      Event event(EventType::SetValue, ReasonId::BUGPlusOne);
      // the source is the trivalue cell and its peers containing the BUG value
      event.addSource(trivalueCell, solution);
      CellSet sourceSet = board.getPeersContaining(trivalueCell, solution);
      event.addSource(sourceSet, solution);
      // set the BUG value in the trivalue cell
      event.addOperation(trivalueCell, solution);
      g_eventQueue.enqueue(board, event);
    }
  }
}

static void techXWing(SudokuBoard &board) {
  auto scanDigit = [&](Digit digit) -> void
  {
    // rows
    const std::vector<Unit> &rows = board.getRows();
    for (Location a = 0; a < 8; ++a) {
      // get cells where the digit is present in the row
      const Unit &row = rows[a];
      CellSet positions = board.getPositionsOfDigit(row, digit);

      int posCount = positions.size();
      if (posCount != 2) {
        continue; // X-Wing requires exactly two positions
      }

      // get the columns corresponding to the positions of the digit
      std::vector<int> positionsList = positions.to_vector();
      Location ca0 = SudokuBoard::getColumnLocation(positionsList[0]);
      Location ca1 = SudokuBoard::getColumnLocation(positionsList[1]);

      for (Location b = a+1; b < 9; ++b) {
        const Unit &row = rows[b];
        CellSet positionsInner = board.getPositionsOfDigit(row, digit);

        int posCount = positionsInner.size();
        if (posCount != 2) {
          continue;
        }
      
        std::vector<int> positionsInnerList = positionsInner.to_vector();
        Location cb0 = SudokuBoard::getColumnLocation(positionsInnerList[0]);
        Location cb1 = SudokuBoard::getColumnLocation(positionsInnerList[1]);
        if (ca0 == cb0 && ca1 == cb1) {
          // X-Wing spotted
          Event event(EventType::RemoveCandidate, ReasonId::XWing);
          // the source is the four cells forming the X-Wing, unit by unit
          event.addSource(positions, digit);
          event.addSource(positionsInner, digit);
          // remove instances of the digit from the two columns, excluding the two rows
          CellSet set = (SudokuBoard::getColumnByLocation(ca0) | SudokuBoard::getColumnByLocation(ca1)) - 
                        (rows[a] | rows[b]);
          for (Cell idx : set) {
            event.addOperation(idx, digit);
          }
          g_eventQueue.enqueue(board, event);
        }

        // look for skyscrapers, A and D are the ends of the chain
        Cell A = -1;
        Cell B = -1;
        Cell C = -1;
        Cell D = -1;
        if (ca0 == cb0 && ca1 != cb1) {
          A = positionsList[1];
          B = positionsList[0];
          C = positionsInnerList[0];
          D = positionsInnerList[1];
        } else if (ca0 != cb0 && ca1 == cb1) {
          A = positionsList[0];
          B = positionsList[1];
          C = positionsInnerList[1];
          D = positionsInnerList[0];
        }

        if (A != -1 && D != -1) {
          // Skyscraper spotted
          Event event(EventType::RemoveCandidate, ReasonId::Skyscraper);
          // the source is the four cells forming the skyscraper, following the chain
          event.addSource(A, digit);
          event.addSource(B, digit);
          event.addSource(C, digit);
          event.addSource(D, digit);
          // remove instances of the digit from peers of the ends
          CellSet set = board.getPeersContaining(CellSet({A, D}), digit);
          for (Cell idx : set) {
            event.addOperation(idx, digit);
          }
          g_eventQueue.enqueue(board, event);
        }
      }
    }

    // columns
    const std::vector<Unit> &columns = board.getColumns();
    for (Location a = 0; a < 8; ++a) {
      // get cells where the digit is present in the column
      const Unit &column = columns[a];
      CellSet positions = board.getPositionsOfDigit(column, digit);

      int posCount = positions.size();
      if (posCount != 2) {
        continue; // X-Wing requires exactly two positions
      }

      // get the rows corresponding to the positions of the digit
      std::vector<int> positionsList = positions.to_vector();
      Location ra0 = SudokuBoard::getRowLocation(positionsList[0]);
      Location ra1 = SudokuBoard::getRowLocation(positionsList[1]);

      for (Location b = a+1; b < 9; ++b) {
        const Unit &column = columns[b];
        CellSet positionsInner = board.getPositionsOfDigit(column, digit);

        int posCount = positionsInner.size();
        if (posCount != 2) {
          continue;
        }
      
        std::vector<int> positionsInnerList = positionsInner.to_vector();
        Location rb0 = SudokuBoard::getRowLocation(positionsInnerList[0]);
        Location rb1 = SudokuBoard::getRowLocation(positionsInnerList[1]);
        if (ra0 == rb0 && ra1 == rb1) {
          // X-Wing spotted
          Event event(EventType::RemoveCandidate, ReasonId::XWing);
          // the source is the four cells forming the X-Wing, unit by unit
          event.addSource(positions, digit);
          event.addSource(positionsInner, digit);
          // remove instances of the digit from the two rows, excluding the two columns
          CellSet set = (SudokuBoard::getRowByLocation(ra0) | SudokuBoard::getRowByLocation(ra1)) - 
                        (columns[a] | columns[b]);
          for (Cell idx : set) {
            event.addOperation(idx, digit);
          }
          g_eventQueue.enqueue(board, event);
        }

        // look for skyscrapers, A and D are the ends of the chain
        Cell A = -1;
        Cell B = -1;
        Cell C = -1;
        Cell D = -1;
        if (ra0 == rb0 && ra1 != rb1) {
          A = positionsList[1];
          B = positionsList[0];
          C = positionsInnerList[0];
          D = positionsInnerList[1];
        } else if (ra0 != rb0 && ra1 == rb1) {
          A = positionsList[0];
          B = positionsList[1];
          C = positionsInnerList[1];
          D = positionsInnerList[0];
        }

        if (A != -1 && D != -1) {
          // Skyscraper spotted
          Event event(EventType::RemoveCandidate, ReasonId::Skyscraper);
          // the source is the four cells forming the skyscraper, following the chain
          event.addSource(A, digit);
          event.addSource(B, digit);
          event.addSource(C, digit);
          event.addSource(D, digit);
          // remove instances of the digit from peers of the ends
          CellSet set = board.getPeersContaining(CellSet({A, D}), digit);
          for (Cell idx : set) {
            event.addOperation(idx, digit);
          }
          g_eventQueue.enqueue(board, event);
        }
      }
    }
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

static void techXYWing(SudokuBoard &board) {
  CellSet bivalues = board.getBivalues();
  for (Cell a : bivalues) {
    // extreme a has XZ
    DigitSet xz = board.getCandidates(a);
    for (Cell b : board.getPeers(a) & bivalues) {
      // wing b must contain XY
      DigitSet xy = board.getCandidates(b);
      if ((xz & xy).size() == 1) {
        Digit x = *(xz & xy).begin();
        Digit y = *(xy - xz).begin();
        Digit z = *(xz - xy).begin();
        for (Cell c : (board.getPeers(b) & bivalues) - CellSet({a})) {
          // extreme c has YZ
          DigitSet yz = board.getCandidates(c);
          if (yz.contains(y) && yz.contains(z)) {
            // XY-Wing spotted
            Event event(EventType::RemoveCandidate, ReasonId::XYWing);
            // the source is the three cells forming the XY-Wing
            event.addSource(a, DigitSet({x, z}));  // wing
            event.addSource(b, DigitSet({x, y}));  // hinge
            event.addSource(c, DigitSet({y, z}));  // wing
            event.addDelimiter();
            event.addSource(a, DigitSet({z}));     // mark Z for UI
            event.addSource(c, DigitSet({z}));
            // remove instances of Z from peers of extreme cells
            CellSet set = board.getPeersContaining(CellSet({a, c}), z);
            for (Cell idx : set) {
              event.addOperation(idx, z);
            }
            g_eventQueue.enqueue(board, event);
          }
        }
      }
    }
  }
}

static void techXYZWing(SudokuBoard &board) {
  CellSet bivalues = board.getBivalues();
  for (Cell a : bivalues) {
    // extreme a has XZ
    DigitSet xz = board.getCandidates(a);
    for (Cell b : board.getPeers(a)) {
      // wing b must contain XYZ
      DigitSet xyz = board.getCandidates(b);
      Digit y = *(xyz - xz).begin();
      if ((xz & xyz).size() == 2 && xyz.size() == 3) {
        for (Cell c : (board.getPeers(b) & bivalues) - CellSet({a})) {
          // extreme c has YZ
          DigitSet yz = board.getCandidates(c);
          if (yz.contains(y) && (xyz - yz).size() == 1) {
            // XYZ-Wing spotted
            Digit x = *(xyz - yz).begin();
            Digit z = *(xz & yz).begin();
            Event event(EventType::RemoveCandidate, ReasonId::XYZWing);
            // the source is the three cells forming the XYZ-Wing
            event.addSource(a, DigitSet({x, z}));     // wing
            event.addSource(b, DigitSet({x, y, z}));  // hinge
            event.addSource(c, DigitSet({y, z}));     // wing
            event.addDelimiter();
            event.addSource(a, DigitSet({z}));        // mark Z for UI
            event.addSource(b, DigitSet({z}));
            event.addSource(c, DigitSet({z}));
            // remove instances of Z from peers of all cells
            CellSet set = board.getPeersContaining(CellSet({a, b, c}), z);
            for (Cell idx : set) {
              event.addOperation(idx, z);
            }
            g_eventQueue.enqueue(board, event);
          }
        }
      }
    }
  }
}

static void techSwordfish(SudokuBoard &board) {
  auto scanDigit = [&](Digit digit) -> void
  {
    // rows
    const std::vector<Unit> &rows = board.getRows();
    for (Location a = 0; a < 7; ++a) {
      // get cells where the digit is present in the row
      const Unit &row = rows[a];
      CellSet positions = board.getPositionsOfDigit(row, digit);

      int posCount = positions.size();
      if (posCount < 2 || posCount > 3) {
        continue; // Swordfish requires up to three positions
      }

      // get the columns corresponding to the positions of the digit
      LocationSet locationSet;
      for (Cell idx : positions) {
        locationSet.insert(SudokuBoard::getColumnLocation(idx));
      }

      for (Location b = a+1; b < 8; ++b) {
        const Unit &row = rows[b];
        CellSet positionsInner = board.getPositionsOfDigit(row, digit);

        int posCount = positionsInner.size();
        if (posCount < 2 || posCount > 3) {
          continue;
        }

        LocationSet locationInnerSet;
        for (Cell idx : positionsInner) {
          locationInnerSet.insert(SudokuBoard::getColumnLocation(idx));
        }

        if ((locationSet | locationInnerSet).size() == 3) {
          for (Location c = b+1; c < 9; ++c) {
            const Unit &row = rows[c];
            CellSet positionsInner2 = board.getPositionsOfDigit(row, digit);

            int posCount = positionsInner2.size();
            if (posCount < 2 || posCount > 3) {
              continue;
            }

            LocationSet locationInner2Set;
            for (Cell idx : positionsInner2) {
              locationInner2Set.insert(SudokuBoard::getColumnLocation(idx));
            }

            if ((locationSet | locationInnerSet | locationInner2Set).size() == 3) {
              // Swordfish spotted
              Event event(EventType::RemoveCandidate, ReasonId::Swordfish);
              // the source is the cells forming the Swordfish, unit by unit
              CellSet sourceSet = positions | positionsInner | positionsInner2;
              event.addSource(positions, digit);
              event.addSource(positionsInner, digit);
              event.addSource(positionsInner2, digit);
              // remove instances of the digit from the three columns, excluding the three rows
              CellSet set;
              for (Cell idx : sourceSet) {
                set |= board.getColumnByCell(idx);
              }
              set -= sourceSet;
              for (Cell idx : set) {
                event.addOperation(idx, digit);
              }
              g_eventQueue.enqueue(board, event);
            }
          }
        }
      }
    }

    // columns
    const std::vector<Unit> &columns = board.getColumns();
    for (Location a = 0; a < 7; ++a) {
      // get cells where the digit is present in the column
      const Unit &column = columns[a];
      CellSet positions = board.getPositionsOfDigit(column, digit);

      int posCount = positions.size();
      if (posCount < 2 || posCount > 3) {
        continue; // Swordfish requires up to three positions
      }

      // get the rows corresponding to the positions of the digit
      LocationSet locationSet;
      for (Cell idx : positions) {
        locationSet.insert(SudokuBoard::getRowLocation(idx));
      }

      for (Location b = a+1; b < 8; ++b) {
        const Unit &column = columns[b];
        CellSet positionsInner = board.getPositionsOfDigit(column, digit);

        int posCount = positionsInner.size();
        if (posCount < 2 || posCount > 3) {
          continue;
        }

        LocationSet locationInnerSet;
        for (Cell idx : positionsInner) {
          locationInnerSet.insert(SudokuBoard::getRowLocation(idx));
        }

        if ((locationSet | locationInnerSet).size() == 3) {
          for (Location c = b+1; c < 9; ++c) {
            const Unit &column = columns[c];
            CellSet positionsInner2 = board.getPositionsOfDigit(column, digit);

            int posCount = positionsInner2.size();
            if (posCount < 2 || posCount > 3) {
              continue;
            }

            LocationSet locationInner2Set;
            for (Cell idx : positionsInner2) {
              locationInner2Set.insert(SudokuBoard::getRowLocation(idx));
            }

            if ((locationSet | locationInnerSet | locationInner2Set).size() == 3) {
              // Swordfish spotted
              Event event(EventType::RemoveCandidate, ReasonId::Swordfish);
              // the source is the cells forming the Swordfish, unit by unit
              CellSet sourceSet = positions | positionsInner | positionsInner2;
              event.addSource(positions, digit);
              event.addSource(positionsInner, digit);
              event.addSource(positionsInner2, digit);
              // remove instances of the digit from the three rows, excluding the three columns
              CellSet set;
              for (Cell idx : sourceSet) {
                set |= board.getRowByCell(idx);
              }
              set -= sourceSet;
              for (Cell idx : set) {
                event.addOperation(idx, digit);
              }
              g_eventQueue.enqueue(board, event);
            }
          }
        }
      }
    }
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

typedef void (*TechniqueFn)(SudokuBoard &);

// nCr(9, 2) = 36
// nCr(9, 3) = 84
// nCr(9, 4) = 126

static constexpr TechniqueFn EASY_TECHNIQUES_SPARSE[] = {
  techFullHouse,
  techHiddenSinglesBox,
  techPointingSet,
  techBoxLineReduction,
  techHiddenSinglesRowColumn,
  techHiddenPairsBox,
  techHiddenPairsRowColumn,
  techNakedSingles,
  techNakedPairs,
  techNakedTriples,
  techHiddenTriples,
};

static constexpr TechniqueFn EASY_TECHNIQUES_DENSE[] = {
  techFullHouse,
  techNakedSingles,
  techHiddenSinglesBox,
  techHiddenSinglesRowColumn,
  techNakedPairs,
  techNakedTriples,
  techHiddenPairsBox,
  techHiddenPairsRowColumn,
  techHiddenTriples,
  techPointingSet,
  techBoxLineReduction,
};

static constexpr TechniqueFn TECHNIQUES[] = {
  techBUGPlusOne,
  techXWing,  // includes skyscrapers
  techXYWing,
  techXYZWing,
  techSwordfish,
};

static bool is_operation_applicable(SudokuBoard &board, EventType type, Operation &op) {
  // you can set only one digit in an unsolved cell
  if (type == EventType::SetValue) {
    return !board.isSolved(op.idx) && op.mask.size() == 1;
  }
  // you can remove only existing candidates from an unsolved cell
  if (type == EventType::RemoveCandidate) {
    op.mask = op.mask & board.getCandidates(op.idx); // remove not available candidates
    return !board.isSolved(op.idx) && !op.mask.empty();
  }
  return false;
}

// ---- Source CellSet serialization (CellSet -> uint32_t) ----
// Encoding (uint32):
//   bits[0..4]   : unitId (0..26)
//   bits[5..13]  : 9-bit mask of cells inside the unit
// unitId mapping:
//   0..8   rows
//   9..17  cols
//   18..26 boxes
// Special case when bits[5..13] are all zero: delimiter of group of sources or digit-only source
static inline uint32_t encode_unit_cells(uint32_t unitId, uint32_t mask9) {
  return ((mask9 & 0x1FFu) << 5) | (unitId & 0x1Fu);
}

static bool cellset_common_row(const std::vector<int> &cells, int &rowOut, uint32_t &mask9Out) {
  if (cells.empty()) return false;
  int r = SudokuBoard::getRowLocation(cells[0]);
  uint32_t m = 0;
  for (int idx : cells) {
    if (SudokuBoard::getRowLocation(idx) != r) return false;
    m |= (1u << (uint32_t)SudokuBoard::getColumnLocation(idx));
  }
  rowOut = r;
  mask9Out = m;
  return true;
}

static bool cellset_common_col(const std::vector<int> &cells, int &colOut, uint32_t &mask9Out) {
  if (cells.empty()) return false;
  int c = SudokuBoard::getColumnLocation(cells[0]);
  uint32_t m = 0;
  for (int idx : cells) {
    if (SudokuBoard::getColumnLocation(idx) != c) return false;
    m |= (1u << (uint32_t)SudokuBoard::getRowLocation(idx));
  }
  colOut = c;
  mask9Out = m;
  return true;
}

static bool cellset_common_box(const std::vector<int> &cells, int &boxOut, uint32_t &mask9Out) {
  if (cells.empty()) return false;
  int b = SudokuBoard::getBoxLocation(cells[0]);
  uint32_t m = 0;
  for (int idx : cells) {
    if (SudokuBoard::getBoxLocation(idx) != b) return false;
    int r = SudokuBoard::getRowLocation(idx) % 3;
    int c = SudokuBoard::getColumnLocation(idx) % 3;
    int pos = r * 3 + c;
    m |= (1u << (uint32_t)pos);
  }
  boxOut = b;
  mask9Out = m;
  return true;
}

static std::vector<uint32_t> serialize_cellset_to_unitcodes(const CellSet &cells) {
  std::vector<uint32_t> out;
  const std::vector<int> v = cells.to_vector();
  if (v.empty()) {
    // special case: encode the empty set as a string of zeroes
    out.push_back(encode_unit_cells(0, 0));
    return out;
  }

  int r = -1, c = -1, b = -1;
  uint32_t mask9 = 0;

  if (cellset_common_row(v, r, mask9)) {
    out.push_back(encode_unit_cells((uint32_t)r, mask9));
    return out;
  }
  if (cellset_common_col(v, c, mask9)) {
    out.push_back(encode_unit_cells((uint32_t)(9 + c), mask9));
    return out;
  }
  if (cellset_common_box(v, b, mask9)) {
    out.push_back(encode_unit_cells((uint32_t)(18 + b), mask9));
    return out;
  }

  // No single common unit: split by box (always possible).
  uint32_t boxMasks[9] = {0};
  for (int idx : v) {
    int bb = SudokuBoard::getBoxLocation(idx);
    int rr = SudokuBoard::getRowLocation(idx) % 3;
    int cc = SudokuBoard::getColumnLocation(idx) % 3;
    int pos = rr * 3 + cc;
    boxMasks[bb] |= (1u << (uint32_t)pos);
  }
  for (int bb = 0; bb < 9; bb++) {
    if (boxMasks[bb]) {
      out.push_back(encode_unit_cells((uint32_t)(18 + bb), boxMasks[bb]));
    }
  }
  return out;
}

// Drain the next event and serialize the operations into out[] as described by API.
// The function returns only events and operations that are applicable to the current 
// state of the board. This implies that some events in queue could be discarded.
// The function will continue the search until the queue is empty.
static int drain_event(SudokuBoard &board,
                       uint32_t *out,
                       uint32_t out_words,
                       uint32_t fromPrev) {
  if (!out || out_words < 5) {
    return 0;
  }

  Event first;
  if (!g_eventQueue.peek(first)) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    out[4] = 0;
    return 0;
  }

  const EventType type = first.type;
  const ReasonId reason = first.reason;

  uint32_t srcChunks = 0;
  for (const Source &src : first.getSources()) {
    srcChunks += (uint32_t)serialize_cellset_to_unitcodes(src.cells).size();
  }
  const uint32_t need_words = 5u + 2u * (uint32_t)first.getNumberOfOperations() + 2u * srcChunks;
  if (need_words > out_words) {
    // Not enough space in output buffer. TODO notify caller
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    out[4] = 0;
    return 0;
  }

  g_eventQueue.dequeue(first);

  out[0] = (uint32_t)type;
  out[1] = (uint32_t)reason;
  out[2] = fromPrev;
  out[3] = 0;
  out[4] = 0;

  // Serialize sources first (may split CellSet into multiple unit-codes).
  uint32_t srcCount = 0;
  for (const Source &src : first.getSources()) {
    const std::vector<uint32_t> codes = serialize_cellset_to_unitcodes(src.cells);
    for (uint32_t code : codes) {
      out[5 + 2 * srcCount + 0] = code;
      out[5 + 2 * srcCount + 1] = src.mask.to_uint32();
      srcCount++;
    }
  }
  out[4] = srcCount;

  const uint32_t opsBase = 5u + 2u * srcCount;

  // Serialize operations (with applicability filter)
  uint32_t opCount = 0;
  for (Operation &op : first.getOperations()) {
    if (is_operation_applicable(board, type, op)) {
      out[opsBase + 2 * opCount + 0] = (uint32_t)op.idx;
      out[opsBase + 2 * opCount + 1] = op.mask.to_uint32();
      opCount++;

      if (type == EventType::SetValue) {
        // Set + Look for immediate naked singles
        board.applySetValue(op.idx, *op.mask.begin());
        for (Cell idx : board.getPeers(op.idx)) {
          int only = board.getSingleCandidate(op.idx);
          if (only) {
            Event event(EventType::SetValue, ReasonId::NakedSingle);
            event.addOperation(op.idx, only);
            g_eventQueue.enqueue(board, event);
          }
        }
      } else if (type == EventType::RemoveCandidate) {
        for (Digit d : op.mask) {
          // Remove + Auto place if applicable
          board.applyRemoveCandidate(op.idx, d);
          int only = board.getSingleCandidate(op.idx);
          if (only) {
            Event event(EventType::SetValue, ReasonId::NakedSingle);
            event.addOperation(op.idx, only);
            g_eventQueue.enqueue(board, event);
          }
        }
      }
    } // else discard invalid operations
  }
  out[3] = opCount;

  // If opCount == 0, discard and continue draining.
  return (opCount > 0) ? 1 : drain_event(board, out, out_words, fromPrev);
}

// Run techniques to fill the queue if needed, then return a single event.
// The drained operations are applied to 'board'.
static int compute_next_event(SudokuBoard &board,
                              uint32_t *out,
                              uint32_t out_words) {
  // If we already have pending events, return them immediately.
  if (drain_event(board, out, out_words, 1u)) {
    return 1;
  }

  // Run techniques in priority order; stop at the first technique that enqueues anything,
  // then verify, apply and return.
  if (board.getNumberOfSolvedCells() < 36) {
    // Cross-hatching order when the grid is mostly empty.
    for (TechniqueFn tech : EASY_TECHNIQUES_SPARSE) {
      tech(board);
      // If something has been generated, drain as "fromPrev=0".
      if (drain_event(board, out, out_words, 0u)) {
        return 1;
      }
    }
  } else {
    // Classic order when the grid is typically filled with candidates.
    for (TechniqueFn tech : EASY_TECHNIQUES_DENSE) {
      tech(board);
      if (drain_event(board, out, out_words, 0u)) {
        return 1;
      }
    }
  }
  // Advanced techniques.
  for (TechniqueFn tech : TECHNIQUES) {
    tech(board);
    if (drain_event(board, out, out_words, 0u)) {
      return 1;
    }
  }

  // No events produced.
  if (out && out_words >= 5) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    out[4] = 0;
  }
  return 0;
}

static void count_solutions_impl(SudokuBoard &board, Cell current_cell, int &found_solutions) {
  if (board.isCompletelySolved()) {
    // validate rows
    for (const Unit &row : board.getRows()) {
      DigitSet set;
      for (Cell idx : row) {
        set.insert(board.getValue(idx));
      }
      if (set != ALL_DIGITS) {
        return;
      }
    }

    // validate columns
    for (const Unit &column : board.getColumns()) {
      DigitSet set;
      for (Cell idx : column) {
        set.insert(board.getValue(idx));
      }
      if (set != ALL_DIGITS) {
        return;
      }
    }

    // validate boxes
    for (const Unit &box : board.getBoxes()) {
      DigitSet set;
      for (Cell idx : box) {
        set.insert(board.getValue(idx));
      }
      if (set != ALL_DIGITS) {
        return;
      }
    }

    // solution found
    ++found_solutions;
    return;
  }

  if (board.isSolved(current_cell)) {
    // this cell is already filled, go on with the next one
    return count_solutions_impl(board, current_cell+1, found_solutions);
  }

  for (Digit d = 1; d <= 9; ++d) {
    const CellSet &peers = board.getRowByCell(current_cell) |
                           board.getColumnByCell(current_cell) |
                           board.getBoxByCell(current_cell);

    bool valid = true;
    for (Cell idx : peers) {
      if (board.isSolved(idx) && board.getValue(idx) == d) {
        // not valid digit, test next one
        valid = false;
        break;
      }
    }
    if (!valid) continue;

    // d is a valid digit
    board.setValue(current_cell, d);
    count_solutions_impl(board, current_cell+1, found_solutions);
    // restore previous status
    board.clearValue(current_cell);
  }
}

static int count_solutions(SudokuBoard &board) {
  int solutions = 0;
  count_solutions_impl(board, 0, solutions);
  return solutions;
}

//
// FOR DEBUGGING compile with -DDEBUG and use this function:
// console_log("Queue has %d elements", g_eventQueue.size());
//

// =========================================================
// Public API exported to JS
// =========================================================

extern "C"
{
  // Calculates the number of solutions of a Sudoku given its initial representation.
  // Returns -1 in case of error, else the number of solutions.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_count_solutions(const char *in81) {
    if (in81 == nullptr) {
      return -1;
    }

    // Import Sudoku from string
    SudokuBoard board;
    if (!board.importFromString(in81)) {
      return -1;
    }

    // It is proven that a Sudoku needs at least 17 clues to have a unique solution
    if (board.getNumberOfSolvedCells() < 17) {
      return 999;
    }

    // Count solutions
    int result = count_solutions(board);

    return result;
  }

  // Solves an entire Sudoku given its initial representation in one shot.
  // Returns 0 in case of error, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_full(const char *in81, char *out81) {
    if (in81 == nullptr || out81 == nullptr) {
      return 0;
    }

    // Import Sudoku from string
    SudokuBoard board;
    if (!board.importFromString(in81)) {
      return 0;
    }

    // Reset queue
    g_eventQueue = EventQueue();

    // Solve loop using existing stepper:
    // repeatedly compute one event, apply it locally, and continue until stuck.
    uint32_t tmp[1024];
    int guard = 0;
    const int guardMax = 200000;

    while (guard++ < guardMax) {
      const int ok = compute_next_event(board, tmp, 1024);
      if (!ok) {
        break;
      }
    }

    // Export
    for (int i = 0; i < 81; i++) {
      const Digit value = board.getValue(i);
      out81[i] = value ? (char)('0' + value) : '.';
    }
    out81[81] = '\0';

    return 1;
  }

  // Initializes the board for a step-by-step solution.
  // Returns 0 in case of error, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_init_board(const char *in81) {
    if (in81 == nullptr) {
      return 0;
    }

    // Import Sudoku from string (WASM is the source of truth)
    if (!g_sudokuBoard.importFromString(in81)) {
      return 0;
    }

    // Reset queue
    g_eventQueue = EventQueue();

    return 1;
  }

  // Performs and returns one step to solve the currently loaded board.
  // Returns 0 in case of error or no event is produced, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_next_step(uint32_t *out, uint32_t out_words) {
    if (out == nullptr || out_words < 4) {
      return 0;
    }

    // Compute one event, apply it locally and return it to the caller.
    const int ok = compute_next_event(g_sudokuBoard, out, out_words);
    return ok ? 1 : 0;
  }

  // Calculate and return one step to solve the board given as input (both values and candidates are given).
  // Returns 0 in case of error or no event is produced, else 1.
  EMSCRIPTEN_KEEPALIVE
  int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out, uint32_t out_words) {
    if (values == nullptr || cands == nullptr || out == nullptr || out_words < 4) {
      return 0;
    }

    // Build a temporary board owned by the caller (JS is the source of truth here).
    SudokuBoard board;
    if (!board.importFromBuffers(values, cands)) {
      out[0] = 0;
      out[1] = 0;
      out[2] = 0;
      out[3] = 0;
      return 0;
    }

    // Clear internal queue state for this hint computation.
    g_eventQueue = EventQueue();

    const int ok = compute_next_event(board, out, out_words);
    return ok ? 1 : 0;
  }
} // extern "C"
