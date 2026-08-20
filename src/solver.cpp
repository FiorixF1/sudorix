#include <cstdint>
#include <cstddef>
#include <cstring>
#include <functional>
#include <set>
#include <vector>

#include "solver.hpp"
#include "SudokuBoard.hpp"
#include "EventQueue.hpp"
#include "AIC.hpp"
#include "ALS.hpp"
#include "encoder.hpp"
#include "types.hpp"
#include "config.hpp"

static SudokuBoard g_sudokuBoard;
static EventQueue g_eventQueue;

// =========================================================
// Techniques
// =========================================================

static void techFullHouse(SudokuBoard &board, EventQueue &eventQueue) {
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
        eventQueue.enqueue(board, event);
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

static void techNakedSingles(SudokuBoard &board, EventQueue &eventQueue) {
  for (Cell i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }
    const Digit d = board.getSingleCandidate(i);
    if (d != 0) {
      Event event(EventType::SetValue, ReasonId::NakedSingle);
      event.addOperation(i, d);
      eventQueue.enqueue(board, event);
    }
  }
}

static void techNakedPairs(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    std::vector<LocationSet> subsets = board.getUnsolvedLocations(unit).generate_power_set_of_size(2);
    for (LocationSet &subset : subsets) {
      DigitSet digitSet = board.getDigitsInLocations(unit, subset);
      if (digitSet.size() == 2) {
        std::vector<int> unitList = unit.to_vector();
        std::vector<int> subsetList = subset.to_vector();
        // naked pair spotted
        Event event(EventType::RemoveCandidate, ReasonId::NakedPair);
        // the source is the two cells containing the pair
        CellSet sourceSet = CellSet({unitList[subsetList[0]], unitList[subsetList[1]]});
        event.addSource("basic", sourceSet, digitSet);
        for (Cell idx : board.getPeers(sourceSet)) {
          // remove digits from other cells in the unit
          event.addOperation(idx, digitSet);
        }
        eventQueue.enqueue(board, event);
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

static void techNakedTriples(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    std::vector<LocationSet> subsets = board.getUnsolvedLocations(unit).generate_power_set_of_size(3);
    for (LocationSet &subset : subsets) {
      DigitSet digitSet = board.getDigitsInLocations(unit, subset);
      if (digitSet.size() == 3) {
        std::vector<int> unitList = unit.to_vector();
        std::vector<int> subsetList = subset.to_vector();
        // naked triple spotted
        Event event(EventType::RemoveCandidate, ReasonId::NakedTriple);
        // the source is the three cells containing the triple
        CellSet sourceSet = CellSet({unitList[subsetList[0]], unitList[subsetList[1]], unitList[subsetList[2]]});
        event.addSource("basic", sourceSet, digitSet);
        for (Cell idx : board.getPeers(sourceSet)) {
          // remove digits from other cells in the unit
          event.addOperation(idx, digitSet);
        }
        eventQueue.enqueue(board, event);
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

static void techHiddenSingles(SudokuBoard &board, EventQueue &eventQueue, const Unit &unit) {
  for (Digit d : board.getUnsolvedDigits()) {
    CellSet cellSet = board.getPositionsOfDigit(unit, d);
    if (cellSet.size() == 1) {
      Event event(EventType::SetValue, ReasonId::HiddenSingle);
      event.addOperation(*cellSet.begin(), d);
      eventQueue.enqueue(board, event);
    }
  }
}

static void techHiddenSinglesBox(SudokuBoard &board, EventQueue &eventQueue) {
  for (const Unit &box : board.getBoxes()) {
    techHiddenSingles(board, eventQueue, box);
  }
}

static void techHiddenSinglesRowColumn(SudokuBoard &board, EventQueue &eventQueue) {
  for (const Unit &row : board.getRows()) {
    techHiddenSingles(board, eventQueue, row);
  }
  for (const Unit &column: board.getColumns()) {
    techHiddenSingles(board, eventQueue, column);
  }
}

static void techHiddenPairs(SudokuBoard &board, EventQueue &eventQueue, const Unit &unit) {
  std::vector<DigitSet> subsets = board.getUnsolvedDigits(unit).generate_power_set_of_size(2);
  for (const DigitSet &subset : subsets) {
    CellSet cellSet = board.getPositionsOfDigitsAny(unit, subset);
    if (cellSet.size() == 2) {
      // hidden pair spotted
      Event event(EventType::RemoveCandidate, ReasonId::HiddenPair);
      // the source is the two cells containing the pair
      CellSet sourceSet = cellSet;
      event.addSource("basic", sourceSet, subset);
      for (Cell idx : cellSet) {
        // remove other digits from the cells of the pair
        event.addOperation(idx, board.getUnsolvedDigits() - subset);
      }
      eventQueue.enqueue(board, event);
    }
  }
}

static void techHiddenPairsBox(SudokuBoard &board, EventQueue &eventQueue) {
  for (const Unit &box : board.getBoxes()) {
    techHiddenPairs(board, eventQueue, box);
  }
}

static void techHiddenPairsRowColumn(SudokuBoard &board, EventQueue &eventQueue) {
  for (const Unit &row : board.getRows()) {
    techHiddenPairs(board, eventQueue, row);
  }
  for (const Unit &column : board.getColumns()) {
    techHiddenPairs(board, eventQueue, column);
  }
}

static void techHiddenTriples(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanUnit = [&](const Unit &unit) -> void
  {
    std::vector<DigitSet> subsets = board.getUnsolvedDigits(unit).generate_power_set_of_size(3);
    for (const DigitSet &subset : subsets) {
      CellSet cellSet = board.getPositionsOfDigitsAny(unit, subset);
      if (cellSet.size() == 3) {
        // hidden triple spotted
        Event event(EventType::RemoveCandidate, ReasonId::HiddenTriple);
        // the source is the three cells containing the triple
        CellSet sourceSet = cellSet;
        event.addSource("basic", sourceSet, subset);
        for (Cell idx : cellSet) {
          // remove other digits from the cells of the triple
          event.addOperation(idx, board.getUnsolvedDigits() - subset);
        }
        eventQueue.enqueue(board, event);
      }
    }
  };

  for (const Unit &row : board.getRows()) {
    scanUnit(row);
  }
  for (const Unit &column : board.getColumns()) {
    scanUnit(column);
  }
  for (const Unit &box : board.getBoxes()) {
    scanUnit(box);
  }
}

static void techPointingSet(SudokuBoard &board, EventQueue &eventQueue) {
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
        Event event(EventType::RemoveCandidate, ReasonId::PointingSet, reasonId);
        // the source is the cells containing the digit
        CellSet sourceSet = positions;
        event.addSource("basic", sourceSet, digit);
        // remove digit from row r0, excluding cells in this box
        CellSet set = SudokuBoard::getRowByLocation(r0).difference_with(box);
        for (Cell idx : set) {
          event.addOperation(idx, digit);
        }
        eventQueue.enqueue(board, event);
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
        Event event(EventType::RemoveCandidate, ReasonId::PointingSet, reasonId);
        // the source is the cells containing the digit
        CellSet sourceSet = positions;
        event.addSource("basic", sourceSet, digit);
        // remove digit from column c0, excluding cells in this box
        CellSet set = SudokuBoard::getColumnByLocation(c0).difference_with(box);
        for (Cell idx : set) {
          event.addOperation(idx, digit);
        }
        eventQueue.enqueue(board, event);
      }
    }
  }
}

static void techBoxLineReduction(SudokuBoard &board, EventQueue &eventQueue) {
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
        Event event(EventType::RemoveCandidate, ReasonId::BoxLineReduction, reasonId);
        // the source is the cells containing the digit
        CellSet sourceSet = positions;
        event.addSource("basic", sourceSet, digit);
        // remove digit from this box, excluding cells in this row/column
        CellSet set = SudokuBoard::getBoxByLocation(boxIdx).difference_with(unit);
        for (Cell idx : set) {
          event.addOperation(idx, digit);
        }
        eventQueue.enqueue(board, event);
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

static void techXWing(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanDigit = [&](Digit digit) -> void
  {
    const int FISH_SIZE = 2;

    auto fishSearcher = [&](const std::vector<Unit> &units,
                            const Unit &(*getBaseByLocation)(Location),
                            const Unit &(*getCoverByLocation)(Location),
                            Location (*getCoverLocation)(Cell)) -> void
    {
      LocationSet possibleUnits;
      for (Location i = 0; i < 9; ++i) {
        const Unit &unit = units[i];
        if (!board.getPositionsOfDigit(unit, digit).empty()) {
          possibleUnits.insert(i);
        }
      }

      const std::vector<LocationSet> allBaseSets = possibleUnits.generate_power_set_of_size(FISH_SIZE);
      for (const LocationSet &baseSets : allBaseSets) {
        std::vector<int> baseSetsList = baseSets.to_vector();

        const Unit &baseA = getBaseByLocation(baseSetsList[0]);
        const Unit &baseB = getBaseByLocation(baseSetsList[1]);
        CellSet positionsA = board.getPositionsOfDigit(baseA, digit);
        CellSet positionsB = board.getPositionsOfDigit(baseB, digit);

        if (positionsA.size() <= FISH_SIZE && positionsB.size() <= FISH_SIZE) {
          LocationSet locationsA; for (Cell idx : positionsA) { locationsA.insert(getCoverLocation(idx)); }
          LocationSet locationsB; for (Cell idx : positionsB) { locationsB.insert(getCoverLocation(idx)); }
          LocationSet coverSets = locationsA | locationsB;
          if (coverSets.size() == FISH_SIZE) {
            // X-Wing spotted
            std::vector<int> coverSetsList = coverSets.to_vector();
            const Unit &coverA = getCoverByLocation(coverSetsList[0]);
            const Unit &coverB = getCoverByLocation(coverSetsList[1]);
            Event event(EventType::RemoveCandidate, ReasonId::XWing);
            // the source is the base sets forming the X-Wing
            event.addSource("base", positionsA, digit);
            event.addSource("base", positionsB, digit);
            // remove instances of the digit from cover sets, excluding the base sets
            CellSet set = (coverA | coverB) - (baseA | baseB);
            for (Cell idx : set) {
              event.addOperation(idx, digit);
            }
            if (eventQueue.enqueue(board, event)) return;
          }
        }
      }
    };

    // base sets: rows | cover sets: columns
    fishSearcher(board.getRows(), SudokuBoard::getRowByLocation, SudokuBoard::getColumnByLocation, SudokuBoard::getColumnLocation);
    // base sets: columns | cover sets: rows
    fishSearcher(board.getColumns(), SudokuBoard::getColumnByLocation, SudokuBoard::getRowByLocation, SudokuBoard::getRowLocation);
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

static void techSwordfish(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanDigit = [&](Digit digit) -> void
  {
    const int FISH_SIZE = 3;

    auto fishSearcher = [&](const std::vector<Unit> &units,
                            const Unit &(*getBaseByLocation)(Location),
                            const Unit &(*getCoverByLocation)(Location),
                            Location (*getCoverLocation)(Cell)) -> void
    {
      LocationSet possibleUnits;
      for (Location i = 0; i < 9; ++i) {
        const Unit &unit = units[i];
        if (!board.getPositionsOfDigit(unit, digit).empty()) {
          possibleUnits.insert(i);
        }
      }

      const std::vector<LocationSet> allBaseSets = possibleUnits.generate_power_set_of_size(FISH_SIZE);
      for (const LocationSet &baseSets : allBaseSets) {
        std::vector<int> baseSetsList = baseSets.to_vector();

        const Unit &baseA = getBaseByLocation(baseSetsList[0]);
        const Unit &baseB = getBaseByLocation(baseSetsList[1]);
        const Unit &baseC = getBaseByLocation(baseSetsList[2]);
        CellSet positionsA = board.getPositionsOfDigit(baseA, digit);
        CellSet positionsB = board.getPositionsOfDigit(baseB, digit);
        CellSet positionsC = board.getPositionsOfDigit(baseC, digit);

        if (positionsA.size() <= FISH_SIZE && positionsB.size() <= FISH_SIZE && positionsC.size() <= FISH_SIZE) {
          LocationSet locationsA; for (Cell idx : positionsA) { locationsA.insert(getCoverLocation(idx)); }
          LocationSet locationsB; for (Cell idx : positionsB) { locationsB.insert(getCoverLocation(idx)); }
          LocationSet locationsC; for (Cell idx : positionsC) { locationsC.insert(getCoverLocation(idx)); }
          LocationSet coverSets = locationsA | locationsB | locationsC;
          if (coverSets.size() == FISH_SIZE) {
            // Swordfish spotted
            std::vector<int> coverSetsList = coverSets.to_vector();
            const Unit &coverA = getCoverByLocation(coverSetsList[0]);
            const Unit &coverB = getCoverByLocation(coverSetsList[1]);
            const Unit &coverC = getCoverByLocation(coverSetsList[2]);
            Event event(EventType::RemoveCandidate, ReasonId::Swordfish);
            // the source is the base sets forming the Swordfish
            event.addSource("base", positionsA, digit);
            event.addSource("base", positionsB, digit);
            event.addSource("base", positionsC, digit);
            // remove instances of the digit from cover sets, excluding the base sets
            CellSet set = (coverA | coverB | coverC) - (baseA | baseB | baseC);
            for (Cell idx : set) {
              event.addOperation(idx, digit);
            }
            if (eventQueue.enqueue(board, event)) return;
          }
        }
      }
    };

    // base sets: rows | cover sets: columns
    fishSearcher(board.getRows(), SudokuBoard::getRowByLocation, SudokuBoard::getColumnByLocation, SudokuBoard::getColumnLocation);
    // base sets: columns | cover sets: rows
    fishSearcher(board.getColumns(), SudokuBoard::getColumnByLocation, SudokuBoard::getRowByLocation, SudokuBoard::getRowLocation);
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

static void techJellyfish(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanDigit = [&](Digit digit) -> void
  {
    const int FISH_SIZE = 4;

    auto fishSearcher = [&](const std::vector<Unit> &units,
                            const Unit &(*getBaseByLocation)(Location),
                            const Unit &(*getCoverByLocation)(Location),
                            Location (*getCoverLocation)(Cell)) -> void
    {
      LocationSet possibleUnits;
      for (Location i = 0; i < 9; ++i) {
        const Unit &unit = units[i];
        if (!board.getPositionsOfDigit(unit, digit).empty()) {
          possibleUnits.insert(i);
        }
      }

      const std::vector<LocationSet> allBaseSets = possibleUnits.generate_power_set_of_size(FISH_SIZE);
      for (const LocationSet &baseSets : allBaseSets) {
        std::vector<int> baseSetsList = baseSets.to_vector();

        const Unit &baseA = getBaseByLocation(baseSetsList[0]);
        const Unit &baseB = getBaseByLocation(baseSetsList[1]);
        const Unit &baseC = getBaseByLocation(baseSetsList[2]);
        const Unit &baseD = getBaseByLocation(baseSetsList[3]);
        CellSet positionsA = board.getPositionsOfDigit(baseA, digit);
        CellSet positionsB = board.getPositionsOfDigit(baseB, digit);
        CellSet positionsC = board.getPositionsOfDigit(baseC, digit);
        CellSet positionsD = board.getPositionsOfDigit(baseD, digit);

        if (positionsA.size() <= FISH_SIZE && positionsB.size() <= FISH_SIZE && positionsC.size() <= FISH_SIZE && positionsD.size() <= FISH_SIZE) {
          LocationSet locationsA; for (Cell idx : positionsA) { locationsA.insert(getCoverLocation(idx)); }
          LocationSet locationsB; for (Cell idx : positionsB) { locationsB.insert(getCoverLocation(idx)); }
          LocationSet locationsC; for (Cell idx : positionsC) { locationsC.insert(getCoverLocation(idx)); }
          LocationSet locationsD; for (Cell idx : positionsD) { locationsD.insert(getCoverLocation(idx)); }
          LocationSet coverSets = locationsA | locationsB | locationsC | locationsD;
          if (coverSets.size() == FISH_SIZE) {
            // Jellyfish spotted
            std::vector<int> coverSetsList = coverSets.to_vector();
            const Unit &coverA = getCoverByLocation(coverSetsList[0]);
            const Unit &coverB = getCoverByLocation(coverSetsList[1]);
            const Unit &coverC = getCoverByLocation(coverSetsList[2]);
            const Unit &coverD = getCoverByLocation(coverSetsList[3]);
            Event event(EventType::RemoveCandidate, ReasonId::Jellyfish);
            // the source is the base sets forming the Jellyfish
            event.addSource("base", positionsA, digit);
            event.addSource("base", positionsB, digit);
            event.addSource("base", positionsC, digit);
            event.addSource("base", positionsD, digit);
            // remove instances of the digit from cover sets, excluding the base sets
            CellSet set = (coverA | coverB | coverC | coverD) - (baseA | baseB | baseC | baseD);
            for (Cell idx : set) {
              event.addOperation(idx, digit);
            }
            if (eventQueue.enqueue(board, event)) return;
          }
        }
      }
    };

    // base sets: rows | cover sets: columns
    fishSearcher(board.getRows(), SudokuBoard::getRowByLocation, SudokuBoard::getColumnByLocation, SudokuBoard::getColumnLocation);
    // base sets: columns | cover sets: rows
    fishSearcher(board.getColumns(), SudokuBoard::getColumnByLocation, SudokuBoard::getRowByLocation, SudokuBoard::getRowLocation);
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

static void techFinnedXWing(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanDigit = [&](Digit digit) -> void
  {
    const int FISH_SIZE = 2;

    auto fishSearcher = [&](const std::vector<Unit> &units,
                            const Unit &(*getBaseByLocation)(Location),
                            const Unit &(*getCoverByLocation)(Location),
                            Location (*getCoverLocation)(Cell)) -> bool
    {
      LocationSet possibleUnits;
      for (Location i = 0; i < 9; ++i) {
        const Unit &unit = units[i];
        if (!board.getPositionsOfDigit(unit, digit).empty()) {
          possibleUnits.insert(i);
        }
      }

      const std::vector<LocationSet> allBaseSets = possibleUnits.generate_power_set_of_size(FISH_SIZE);
      for (const LocationSet &baseSets : allBaseSets) {
        std::vector<int> baseSetsList = baseSets.to_vector();

        const Unit &baseA = getBaseByLocation(baseSetsList[0]);
        const Unit &baseB = getBaseByLocation(baseSetsList[1]);
        CellSet positionsA = board.getPositionsOfDigit(baseA, digit);
        CellSet positionsB = board.getPositionsOfDigit(baseB, digit);
        CellSet vertices = positionsA | positionsB;

        LocationSet locationsA; for (Cell idx : positionsA) { locationsA.insert(getCoverLocation(idx)); }
        LocationSet locationsB; for (Cell idx : positionsB) { locationsB.insert(getCoverLocation(idx)); }
        LocationSet coverSets = locationsA | locationsB;

        if (coverSets.size() <= FISH_SIZE+2) {
          for (const LocationSet &coverSet : coverSets.generate_power_set_of_size(FISH_SIZE)) {
            // look for the real cover sets
            std::vector<int> coverSetList = coverSet.to_vector();
            const Unit &coverA = getCoverByLocation(coverSetList[0]);
            const Unit &coverB = getCoverByLocation(coverSetList[1]);
            CellSet covered = vertices & (coverA | coverB);
            CellSet fins = vertices - covered;
            // verify sashiminess
            bool sashiminess = false;
            if ((positionsA - fins).size() <= 1 || (positionsB - fins).size() <= 1) {
              sashiminess = true;
            }
            // look for fins and check they belong to a single box
            LocationSet boxes;
            for (Cell fin : fins) { boxes.insert(SudokuBoard::getBoxLocation(fin)); }
            if (boxes.size() == 1 && fins.size() <= 2) {
              // check for possible eliminations
              const Unit &finBox = SudokuBoard::getBoxByLocation(*boxes.begin());
              CellSet set = ((coverA | coverB) - (baseA | baseB)) & finBox;
              if (!set.empty()) {
                // Finned X-Wing spotted
                Event event(EventType::RemoveCandidate, ReasonId::FinnedXWing, sashiminess ? ReasonId::SashimiXWing : ReasonId::FinnedXWing);
                // base sets without the fins
                event.addSource("base", positionsA - fins, digit);
                event.addSource("base", positionsB - fins, digit);
                // fins
                for (Cell idx : fins) {
                  event.addSource("fin", idx, digit);
                }
                // eliminations
                for (Cell idx : set) {
                  event.addOperation(idx, digit);
                }
                if (eventQueue.enqueue(board, event)) return true;
              }
            }
          }
        }
      }
      return false;
    };

    // base sets: rows | cover sets: columns
    if (fishSearcher(board.getRows(), SudokuBoard::getRowByLocation, SudokuBoard::getColumnByLocation, SudokuBoard::getColumnLocation)) return;
    // base sets: columns | cover sets: rows
    if (fishSearcher(board.getColumns(), SudokuBoard::getColumnByLocation, SudokuBoard::getRowByLocation, SudokuBoard::getRowLocation)) return;
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

static void techFinnedSwordfish(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanDigit = [&](Digit digit) -> void
  {
    const int FISH_SIZE = 3;

    auto fishSearcher = [&](const std::vector<Unit> &units,
                            const Unit &(*getBaseByLocation)(Location),
                            const Unit &(*getCoverByLocation)(Location),
                            Location (*getCoverLocation)(Cell)) -> bool
    {
      LocationSet possibleUnits;
      for (Location i = 0; i < 9; ++i) {
        const Unit &unit = units[i];
        if (!board.getPositionsOfDigit(unit, digit).empty()) {
          possibleUnits.insert(i);
        }
      }

      const std::vector<LocationSet> allBaseSets = possibleUnits.generate_power_set_of_size(FISH_SIZE);
      for (const LocationSet &baseSets : allBaseSets) {
        std::vector<int> baseSetsList = baseSets.to_vector();

        const Unit &baseA = getBaseByLocation(baseSetsList[0]);
        const Unit &baseB = getBaseByLocation(baseSetsList[1]);
        const Unit &baseC = getBaseByLocation(baseSetsList[2]);
        CellSet positionsA = board.getPositionsOfDigit(baseA, digit);
        CellSet positionsB = board.getPositionsOfDigit(baseB, digit);
        CellSet positionsC = board.getPositionsOfDigit(baseC, digit);
        CellSet vertices = positionsA | positionsB | positionsC;

        LocationSet locationsA; for (Cell idx : positionsA) { locationsA.insert(getCoverLocation(idx)); }
        LocationSet locationsB; for (Cell idx : positionsB) { locationsB.insert(getCoverLocation(idx)); }
        LocationSet locationsC; for (Cell idx : positionsC) { locationsC.insert(getCoverLocation(idx)); }
        LocationSet coverSets = locationsA | locationsB | locationsC;

        if (coverSets.size() <= FISH_SIZE+2) {
          for (const LocationSet &coverSet : coverSets.generate_power_set_of_size(FISH_SIZE)) {
            // look for the real cover sets
            std::vector<int> coverSetList = coverSet.to_vector();
            const Unit &coverA = getCoverByLocation(coverSetList[0]);
            const Unit &coverB = getCoverByLocation(coverSetList[1]);
            const Unit &coverC = getCoverByLocation(coverSetList[2]);
            CellSet covered = vertices & (coverA | coverB | coverC);
            CellSet fins = vertices - covered;
            // verify sashiminess
            bool sashiminess = false;
            if ((positionsA - fins).size() <= 1 || (positionsB - fins).size() <= 1 || (positionsC - fins).size() <= 1) {
              sashiminess = true;
            }
            // look for fins and check they belong to a single box
            LocationSet boxes;
            for (Cell fin : fins) { boxes.insert(SudokuBoard::getBoxLocation(fin)); }
            if (boxes.size() == 1 && fins.size() <= 2) {
              // check for possible eliminations
              const Unit &finBox = SudokuBoard::getBoxByLocation(*boxes.begin());
              CellSet set = ((coverA | coverB | coverC) - (baseA | baseB | baseC)) & finBox;
              if (!set.empty()) {
                // Finned Swordfish spotted
                Event event(EventType::RemoveCandidate, ReasonId::FinnedSwordfish, sashiminess ? ReasonId::SashimiSwordfish : ReasonId::FinnedSwordfish);
                // base sets without the fins
                event.addSource("base", positionsA - fins, digit);
                event.addSource("base", positionsB - fins, digit);
                event.addSource("base", positionsC - fins, digit);
                // fins
                for (Cell idx : fins) {
                  event.addSource("fin", idx, digit);
                }
                // eliminations
                for (Cell idx : set) {
                  event.addOperation(idx, digit);
                }
                if (eventQueue.enqueue(board, event)) return true;
              }
            }
          }
        }
      }
      return false;
    };

    // base sets: rows | cover sets: columns
    if (fishSearcher(board.getRows(), SudokuBoard::getRowByLocation, SudokuBoard::getColumnByLocation, SudokuBoard::getColumnLocation)) return;
    // base sets: columns | cover sets: rows
    if (fishSearcher(board.getColumns(), SudokuBoard::getColumnByLocation, SudokuBoard::getRowByLocation, SudokuBoard::getRowLocation)) return;
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

static void techFinnedJellyfish(SudokuBoard &board, EventQueue &eventQueue) {
  auto scanDigit = [&](Digit digit) -> void
  {
    const int FISH_SIZE = 4;

    auto fishSearcher = [&](const std::vector<Unit> &units,
                            const Unit &(*getBaseByLocation)(Location),
                            const Unit &(*getCoverByLocation)(Location),
                            Location (*getCoverLocation)(Cell)) -> bool
    {
      LocationSet possibleUnits;
      for (Location i = 0; i < 9; ++i) {
        const Unit &unit = units[i];
        if (!board.getPositionsOfDigit(unit, digit).empty()) {
          possibleUnits.insert(i);
        }
      }

      const std::vector<LocationSet> allBaseSets = possibleUnits.generate_power_set_of_size(FISH_SIZE);
      for (const LocationSet &baseSets : allBaseSets) {
        std::vector<int> baseSetsList = baseSets.to_vector();

        const Unit &baseA = getBaseByLocation(baseSetsList[0]);
        const Unit &baseB = getBaseByLocation(baseSetsList[1]);
        const Unit &baseC = getBaseByLocation(baseSetsList[2]);
        const Unit &baseD = getBaseByLocation(baseSetsList[3]);
        CellSet positionsA = board.getPositionsOfDigit(baseA, digit);
        CellSet positionsB = board.getPositionsOfDigit(baseB, digit);
        CellSet positionsC = board.getPositionsOfDigit(baseC, digit);
        CellSet positionsD = board.getPositionsOfDigit(baseD, digit);
        CellSet vertices = positionsA | positionsB | positionsC | positionsD;

        LocationSet locationsA; for (Cell idx : positionsA) { locationsA.insert(getCoverLocation(idx)); }
        LocationSet locationsB; for (Cell idx : positionsB) { locationsB.insert(getCoverLocation(idx)); }
        LocationSet locationsC; for (Cell idx : positionsC) { locationsC.insert(getCoverLocation(idx)); }
        LocationSet locationsD; for (Cell idx : positionsD) { locationsD.insert(getCoverLocation(idx)); }
        LocationSet coverSets = locationsA | locationsB | locationsC | locationsD;

        if (coverSets.size() <= FISH_SIZE+2) {
          for (const LocationSet &coverSet : coverSets.generate_power_set_of_size(FISH_SIZE)) {
            // look for the real cover sets
            std::vector<int> coverSetList = coverSet.to_vector();
            const Unit &coverA = getCoverByLocation(coverSetList[0]);
            const Unit &coverB = getCoverByLocation(coverSetList[1]);
            const Unit &coverC = getCoverByLocation(coverSetList[2]);
            const Unit &coverD = getCoverByLocation(coverSetList[3]);
            CellSet covered = vertices & (coverA | coverB | coverC | coverD);
            CellSet fins = vertices - covered;
            // verify sashiminess
            bool sashiminess = false;
            if ((positionsA - fins).size() <= 1 || (positionsB - fins).size() <= 1 || (positionsC - fins).size() <= 1 || (positionsD - fins).size() <= 1) {
              sashiminess = true;
            }
            // look for fins and check they belong to a single box
            LocationSet boxes;
            for (Cell fin : fins) { boxes.insert(SudokuBoard::getBoxLocation(fin)); }
            if (boxes.size() == 1 && fins.size() <= 2) {
              // check for possible eliminations
              const Unit &finBox = SudokuBoard::getBoxByLocation(*boxes.begin());
              CellSet set = ((coverA | coverB | coverC | coverD) - (baseA | baseB | baseC | baseD)) & finBox;
              if (!set.empty()) {
                // Finned Jellyfish spotted
                Event event(EventType::RemoveCandidate, ReasonId::FinnedJellyfish, sashiminess ? ReasonId::SashimiJellyfish : ReasonId::FinnedJellyfish);
                // base sets without the fins
                event.addSource("base", positionsA - fins, digit);
                event.addSource("base", positionsB - fins, digit);
                event.addSource("base", positionsC - fins, digit);
                event.addSource("base", positionsD - fins, digit);
                // fins
                for (Cell idx : fins) {
                  event.addSource("fin", idx, digit);
                }
                // eliminations
                for (Cell idx : set) {
                  event.addOperation(idx, digit);
                }
                if (eventQueue.enqueue(board, event)) return true;
              }
            }
          }
        }
      }
      return false;
    };

    // base sets: rows | cover sets: columns
    if (fishSearcher(board.getRows(), SudokuBoard::getRowByLocation, SudokuBoard::getColumnByLocation, SudokuBoard::getColumnLocation)) return;
    // base sets: columns | cover sets: rows
    if (fishSearcher(board.getColumns(), SudokuBoard::getColumnByLocation, SudokuBoard::getRowByLocation, SudokuBoard::getRowLocation)) return;
  };

  for (Digit digit : board.getUnsolvedDigits()) {
    scanDigit(digit);
  }
}

static void techUniqueRectangle(SudokuBoard &board, EventQueue &eventQueue) {
  std::vector<Event> type1;
  std::vector<Event> type2;
  std::vector<Event> type3;
  std::vector<Event> type4;
  std::vector<Event> type5;
  std::vector<Event> type6;

  // Look for the four vertices of the rectangle, defined as:
  // - Main vertex: the main bivalue cell
  // - Box vertex: the vertex in the same box (and line) of the main vertex
  // - Line vertex: the vertex in the same line of the main vertex, but different box
  // - Opposite vertex: the vertex on the opposite side of the main vertex
  CellSet bivalues = board.getBivalues();
  for (Cell mainVertex : bivalues) {
    DigitSet xy = board.getCandidates(mainVertex);
    Digit x = *xy.begin();
    Digit y = *(++xy.begin());
    const Unit &box = SudokuBoard::getBoxByCell(mainVertex);
    // Box vertex: look for peers in the same box and same row/column
    for (Cell boxVertex : (box - CellSet({mainVertex})) &
                          (SudokuBoard::getRowByCell(mainVertex) | SudokuBoard::getColumnByCell(mainVertex))) {
      DigitSet boxVertexDigits = board.getCandidates(boxVertex);
      if (boxVertexDigits.is_superset_of(xy)) {
        // Line vertex: look for peers in the same row/column but different box (not visible by box vertex)
        for (Cell lineVertex : board.getPeers(mainVertex) - board.getPeers(boxVertex) - CellSet({boxVertex})) {
          DigitSet lineVertexDigits = board.getCandidates(lineVertex);
          if (lineVertexDigits.is_superset_of(xy)) {
            // Opposite vertex: the only *aligned* cell visible by both box vertex and line vertex
            CellSet tmp = board.getBoxByCell(lineVertex) &
                          board.getPeers(boxVertex) &
                          (SudokuBoard::getRowByCell(lineVertex) | SudokuBoard::getColumnByCell(lineVertex));
            // be careful, it could be a solved cell
            if (tmp.empty()) continue;
            Cell oppositeVertex = *tmp.begin();
            DigitSet oppositeVertexDigits = board.getCandidates(oppositeVertex);
            if (oppositeVertexDigits.is_superset_of(xy)) {
              // rectangle found
              Location rowMin = std::min({SudokuBoard::getRowLocation(mainVertex), 
                                          SudokuBoard::getRowLocation(boxVertex),
                                          SudokuBoard::getRowLocation(lineVertex),
                                          SudokuBoard::getRowLocation(oppositeVertex)});
              Location rowMax = std::max({SudokuBoard::getRowLocation(mainVertex), 
                                          SudokuBoard::getRowLocation(boxVertex),
                                          SudokuBoard::getRowLocation(lineVertex),
                                          SudokuBoard::getRowLocation(oppositeVertex)});
              Location colMin = std::min({SudokuBoard::getColumnLocation(mainVertex), 
                                          SudokuBoard::getColumnLocation(boxVertex),
                                          SudokuBoard::getColumnLocation(lineVertex),
                                          SudokuBoard::getColumnLocation(oppositeVertex)});
              Location colMax = std::max({SudokuBoard::getColumnLocation(mainVertex), 
                                          SudokuBoard::getColumnLocation(boxVertex),
                                          SudokuBoard::getColumnLocation(lineVertex),
                                          SudokuBoard::getColumnLocation(oppositeVertex)});
              const Unit &rowMinUnit = SudokuBoard::getRowByLocation(rowMin);
              const Unit &rowMaxUnit = SudokuBoard::getRowByLocation(rowMax);
              const Unit &colMinUnit = SudokuBoard::getRowByLocation(colMin);
              const Unit &colMaxUnit = SudokuBoard::getRowByLocation(colMax);
              Cell a = rowMin*9 + colMin;
              Cell b = rowMin*9 + colMax;
              Cell c = rowMax*9 + colMin;
              Cell d = rowMax*9 + colMax;
              CellSet rectangleUpper({a, b});
              CellSet rectangleLower({c, d});
              if (xy == boxVertexDigits) {
                // UR types 1, 2, 3, 4
                if (xy == lineVertexDigits || xy == oppositeVertexDigits) {
                  // Type 1
                  Event event(EventType::RemoveCandidate, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType1);
                  event.addSource("UR", rectangleUpper, xy);
                  event.addSource("UR", rectangleLower, xy);
                  // remove xy from the vertex with more candidates
                  event.addOperation(xy == lineVertexDigits ? oppositeVertex : lineVertex, xy);
                  type1.push_back(event);
                }
                if (lineVertexDigits.size() == 3 && lineVertexDigits == oppositeVertexDigits) {
                  // Type 2
                  Digit z = *(lineVertexDigits - xy).begin();
                  Event event(EventType::RemoveCandidate, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType2);
                  event.addSource("UR", rectangleUpper, xy);
                  event.addSource("UR", rectangleLower, xy);
                  event.addSource("guardian", {lineVertex, oppositeVertex}, z);
                  // remove z from peers of lineVertex and oppositeVertex
                  for (Cell idx : board.getPeers({lineVertex, oppositeVertex})) {
                    event.addOperation(idx, z);
                  }
                  type2.push_back(event);
                }
                if (lineVertexDigits != oppositeVertexDigits) {
                  // Type 3
                  DigitSet extraDigits = (lineVertexDigits | oppositeVertexDigits) - xy;
                  CellSet peers = board.getPeers({lineVertex, oppositeVertex});
                  CellSet virtualSubset = board.getPositionsOfDigitsStrict(peers, extraDigits);
                  // make sure all digits are present in the virtual subset
                  DigitSet foundDigits;
                  for (Cell idx : virtualSubset) foundDigits |= board.getCandidates(idx);
                  // make sure the virtual subset is inside a single house
                  bool seeEachOther = true;
                  for (Cell idx : virtualSubset) seeEachOther &= board.sees(CellSet({idx}), virtualSubset - CellSet({idx}));
                  // let's go
                  if (virtualSubset.size() == extraDigits.size()-1 && foundDigits == extraDigits && seeEachOther) {
                    Event event(EventType::RemoveCandidate, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType3);
                    event.addSource("UR", rectangleUpper, xy);
                    event.addSource("UR", rectangleLower, xy);
                    event.addSource("guardian", {lineVertex, oppositeVertex}, extraDigits);
                    // remove the extra digits from peers of lineVertex, oppositeVertex and the found cells
                    for (Cell idx : board.getPeers(CellSet({lineVertex, oppositeVertex}) | virtualSubset)) {
                      event.addOperation(idx, extraDigits);
                    }
                    type3.push_back(event);
                  }
                  // Type 4
                  CellSet xPositions = board.getPositionsOfDigit(peers, x);
                  CellSet yPositions = board.getPositionsOfDigit(peers, y);
                  if (xPositions.empty()) {
                    Event event(EventType::RemoveCandidate, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType4);
                    event.addSource("UR", rectangleUpper, xy);
                    event.addSource("UR", rectangleLower, xy);
                    // remove the other digit from lineVertex and oppositeVertex
                    event.addOperation(lineVertex, y);
                    event.addOperation(oppositeVertex, y);
                    type4.push_back(event);
                  } else if (yPositions.empty()) {
                    Event event(EventType::RemoveCandidate, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType4);
                    event.addSource("UR", rectangleUpper, xy);
                    event.addSource("UR", rectangleLower, xy);
                    // remove the other digit from lineVertex and oppositeVertex
                    event.addOperation(lineVertex, x);
                    event.addOperation(oppositeVertex, x);
                    type4.push_back(event);
                  }
                }
              } else if (xy == oppositeVertexDigits) {
                // UR types 5, 6
                if (lineVertexDigits.size() == 3 && lineVertexDigits == boxVertexDigits) {
                  // Type 5 (two cells)
                  Digit z = *(lineVertexDigits - xy).begin();
                  Event event(EventType::RemoveCandidate, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType5);
                  event.addSource("UR", rectangleUpper, xy);
                  event.addSource("UR", rectangleLower, xy);
                  event.addSource("guardian", {lineVertex, boxVertex}, z);
                  // remove z from peers of lineVertex and boxVertex
                  for (Cell idx : board.getPeers({lineVertex, boxVertex})) {
                    event.addOperation(idx, z);
                  }
                  type5.push_back(event);
                }
                if (lineVertexDigits != boxVertexDigits) {
                  // Type 6
                  if (board.getPositionsOfDigit(rowMinUnit, x).size() == 2 &&
                      board.getPositionsOfDigit(rowMaxUnit, x).size() == 2 &&
                      board.getPositionsOfDigit(colMinUnit, x).size() == 2 &&
                      board.getPositionsOfDigit(colMaxUnit, x).size() == 2) {
                    Event event(EventType::SetValue, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType6);
                    event.addSource("UR", rectangleUpper, xy);
                    event.addSource("UR", rectangleLower, xy);
                    // set the X-Wing digit in the bivalue cells of the UR
                    event.addOperation(mainVertex, x);
                    event.addOperation(oppositeVertex, x);
                    type6.push_back(event);
                  }
                  if (board.getPositionsOfDigit(rowMinUnit, y).size() == 2 &&
                      board.getPositionsOfDigit(rowMaxUnit, y).size() == 2 &&
                      board.getPositionsOfDigit(colMinUnit, y).size() == 2 &&
                      board.getPositionsOfDigit(colMaxUnit, y).size() == 2) {
                    Event event(EventType::SetValue, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType6);
                    event.addSource("UR", rectangleUpper, xy);
                    event.addSource("UR", rectangleLower, xy);
                    // set the X-Wing digit in the bivalue cells of the UR
                    event.addOperation(mainVertex, y);
                    event.addOperation(oppositeVertex, y);
                    type6.push_back(event);
                  }
                }
              } else if (xy.is_subset_of(oppositeVertexDigits)) {
                // Specific instance of type 5
                if (lineVertexDigits.size() == 3 && lineVertexDigits == boxVertexDigits && lineVertexDigits == oppositeVertexDigits) {
                  // Type 5 (three cells)
                  Digit z = *(lineVertexDigits - xy).begin();
                  Event event(EventType::RemoveCandidate, ReasonId::UniqueRectangle, ReasonId::UniqueRectangleType5);
                  event.addSource("UR", rectangleUpper, xy);
                  event.addSource("UR", rectangleLower, xy);
                  event.addSource("guardian", {lineVertex, boxVertex}, z);
                  // remove z from peers of lineVertex and boxVertex and oppositeVertex
                  for (Cell idx : board.getPeers({lineVertex, boxVertex, oppositeVertex})) {
                    event.addOperation(idx, z);
                  }
                  type5.push_back(event);
                }
              }
            }
          }
        }
      }
    }
  }

  // return by priority
  for (Event &event : type1) { if (eventQueue.enqueue(board, event)) return; }
  for (Event &event : type2) { if (eventQueue.enqueue(board, event)) return; }
  for (Event &event : type3) { if (eventQueue.enqueue(board, event)) return; }
  for (Event &event : type4) { if (eventQueue.enqueue(board, event)) return; }
  for (Event &event : type5) { if (eventQueue.enqueue(board, event)) return; }
  for (Event &event : type6) { if (eventQueue.enqueue(board, event)) return; }
}

static void techHiddenRectangle(SudokuBoard &board, EventQueue &eventQueue) {
  std::vector<Event> hidden;

  // Look for the four vertices of the rectangle, defined as:
  // - Main vertex: the main bivalue cell
  // - Box vertex: the vertex in the same box (and line) of the main vertex
  // - Line vertex: the vertex in the same line of the main vertex, but different box
  // - Opposite vertex: the vertex on the opposite side of the main vertex
  CellSet bivalues = board.getBivalues();
  for (Cell mainVertex : bivalues) {
    DigitSet xy = board.getCandidates(mainVertex);
    Digit x = *xy.begin();
    Digit y = *(++xy.begin());
    const Unit &box = SudokuBoard::getBoxByCell(mainVertex);
    // Box vertex: look for peers in the same box and same row/column
    for (Cell boxVertex : (box - CellSet({mainVertex})) &
                          (SudokuBoard::getRowByCell(mainVertex) | SudokuBoard::getColumnByCell(mainVertex))) {
      DigitSet boxVertexDigits = board.getCandidates(boxVertex);
      if (boxVertexDigits.is_superset_of(xy)) {
        // Line vertex: look for peers in the same row/column but different box (not visible by box vertex)
        for (Cell lineVertex : board.getPeers(mainVertex) - board.getPeers(boxVertex) - CellSet({boxVertex})) {
          DigitSet lineVertexDigits = board.getCandidates(lineVertex);
          if (lineVertexDigits.is_superset_of(xy)) {
            // Opposite vertex: the only *aligned* cell visible by both box vertex and line vertex
            CellSet tmp = board.getBoxByCell(lineVertex) &
                          board.getPeers(boxVertex) &
                          (SudokuBoard::getRowByCell(lineVertex) | SudokuBoard::getColumnByCell(lineVertex));
            // be careful, it could be a solved cell
            if (tmp.empty()) continue;
            Cell oppositeVertex = *tmp.begin();
            DigitSet oppositeVertexDigits = board.getCandidates(oppositeVertex);
            if (oppositeVertexDigits.is_superset_of(xy)) {
              // rectangle found
              Location rowMin = std::min({SudokuBoard::getRowLocation(mainVertex), 
                                          SudokuBoard::getRowLocation(boxVertex),
                                          SudokuBoard::getRowLocation(lineVertex),
                                          SudokuBoard::getRowLocation(oppositeVertex)});
              Location rowMax = std::max({SudokuBoard::getRowLocation(mainVertex), 
                                          SudokuBoard::getRowLocation(boxVertex),
                                          SudokuBoard::getRowLocation(lineVertex),
                                          SudokuBoard::getRowLocation(oppositeVertex)});
              Location colMin = std::min({SudokuBoard::getColumnLocation(mainVertex), 
                                          SudokuBoard::getColumnLocation(boxVertex),
                                          SudokuBoard::getColumnLocation(lineVertex),
                                          SudokuBoard::getColumnLocation(oppositeVertex)});
              Location colMax = std::max({SudokuBoard::getColumnLocation(mainVertex), 
                                          SudokuBoard::getColumnLocation(boxVertex),
                                          SudokuBoard::getColumnLocation(lineVertex),
                                          SudokuBoard::getColumnLocation(oppositeVertex)});
              const Unit &rowMinUnit = SudokuBoard::getRowByLocation(rowMin);
              const Unit &rowMaxUnit = SudokuBoard::getRowByLocation(rowMax);
              const Unit &colMinUnit = SudokuBoard::getRowByLocation(colMin);
              const Unit &colMaxUnit = SudokuBoard::getRowByLocation(colMax);
              Cell a = rowMin*9 + colMin;
              Cell b = rowMin*9 + colMax;
              Cell c = rowMax*9 + colMin;
              Cell d = rowMax*9 + colMax;
              CellSet rectangleUpper({a, b});
              CellSet rectangleLower({c, d});
              if (xy.is_subset_of(oppositeVertexDigits)) {
                {
                  // hidden rectangle
                  const Unit &row = SudokuBoard::getRowByCell(oppositeVertex);
                  const Unit &column = SudokuBoard::getColumnByCell(oppositeVertex);
                  if (board.getPositionsOfDigit(row | column, x).size() == 3) {
                    Event event(EventType::RemoveCandidate, ReasonId::HiddenRectangle);
                    event.addSource("UR", rectangleUpper, xy);
                    event.addSource("UR", rectangleLower, xy);
                    // remove the other UR digit from oppositeVertex
                    event.addOperation(oppositeVertex, y);
                    hidden.push_back(event);
                  } else if (board.getPositionsOfDigit(row | column, y).size() == 3) {
                    Event event(EventType::RemoveCandidate, ReasonId::HiddenRectangle);
                    event.addSource("UR", rectangleUpper, xy);
                    event.addSource("UR", rectangleLower, xy);
                    // remove the other UR digit from oppositeVertex
                    event.addOperation(oppositeVertex, x);
                    hidden.push_back(event);
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  for (Event &event : hidden) { if (eventQueue.enqueue(board, event)) return; }
}

static void techBUGPlusOne(SudokuBoard &board, EventQueue &eventQueue) {
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
      event.addSource("BUG", trivalueCell, solution);
      CellSet sourceSet = board.getPeersContaining(trivalueCell, solution);
      event.addSource("peers", sourceSet, solution);
      // set the BUG value in the trivalue cell
      event.addOperation(trivalueCell, solution);
      eventQueue.enqueue(board, event);
    }
  }
}

static void techXYWing(SudokuBoard &board, EventQueue &eventQueue) {
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
            event.addSource("Wing", a, DigitSet({x, z}));
            event.addSource("Wing", b, DigitSet({x, y}));
            event.addSource("Wing", c, DigitSet({y, z}));
            event.addSource("Z", a, DigitSet({z}));
            event.addSource("Z", c, DigitSet({z}));
            // remove instances of Z from peers of extreme cells
            CellSet set = board.getPeersContaining(CellSet({a, c}), z);
            for (Cell idx : set) {
              event.addOperation(idx, z);
            }
            if (eventQueue.enqueue(board, event)) return;
          }
        }
      }
    }
  }
}

static void techXYZWing(SudokuBoard &board, EventQueue &eventQueue) {
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
            event.addSource("Wing", a, DigitSet({x, z}));
            event.addSource("Wing", b, DigitSet({x, y, z}));
            event.addSource("Wing", c, DigitSet({y, z}));
            event.addSource("Z", a, DigitSet({z}));
            event.addSource("Z", b, DigitSet({z}));
            event.addSource("Z", c, DigitSet({z}));
            // remove instances of Z from peers of all cells
            CellSet set = board.getPeersContaining(CellSet({a, b, c}), z);
            for (Cell idx : set) {
              event.addOperation(idx, z);
            }
            if (eventQueue.enqueue(board, event)) return;
          }
        }
      }
    }
  }
}

static void techWWing(SudokuBoard &board, EventQueue &eventQueue) {
  CellSet bivalues = board.getBivalues();
  for (Cell a : bivalues) {
    DigitSet xy = board.getCandidates(a);
    CellSet peers_of_a = board.getPeers(a);
    for (Cell b : bivalues) {
      // we are looking for a remote pair
      if (board.areRemotePair(a, b)) {
        CellSet peers_of_b = board.getPeers(b);
        for (Cell peer_of_a : peers_of_a) {
          for (Cell peer_of_b : peers_of_b) {
            // we are looking for the two cells containing the bilocation
            if (peer_of_a != peer_of_b && board.sees(peer_of_a, peer_of_b)) {
              CellSet bilocationCandidates = CellSet({peer_of_a, peer_of_b});
              auto v = xy.to_vector();
              Digit x = v[0];
              Digit y = v[1];

              // look for the common unit of the two cells
              const Unit *common_unit = nullptr;
              if (SudokuBoard::getRowLocation(peer_of_a) == SudokuBoard::getRowLocation(peer_of_b)) {
                // they are in the same row
                common_unit = &SudokuBoard::getRowByCell(peer_of_a);
              } else if (SudokuBoard::getColumnLocation(peer_of_a) == SudokuBoard::getColumnLocation(peer_of_b)) {
                // they are in the same column
                common_unit = &SudokuBoard::getColumnByCell(peer_of_a);
              } else if (SudokuBoard::getBoxLocation(peer_of_a) == SudokuBoard::getBoxLocation(peer_of_b)) {
                // they are in the same box
                common_unit = &SudokuBoard::getBoxByCell(peer_of_a);
              }

              if (common_unit) {
                // trick to find grouped W-Wings
                CellSet xInUnit = board.getPositionsOfDigit(*common_unit, x);
                CellSet yInUnit = board.getPositionsOfDigit(*common_unit, y);

                if (xInUnit.is_subset_of(peers_of_a | peers_of_b) && !(xInUnit & peers_of_a).empty() && !(xInUnit & peers_of_b).empty()) {
                  // W-Wing spotted on digit x
                  Event event(EventType::RemoveCandidate, ReasonId::WWing);
                  // the source is the four (or more) cells forming the W-Wing
                  event.addSource("Wing", a, DigitSet({x, y}));
                  event.addSource("Wing", xInUnit & peers_of_a, DigitSet({x}));
                  event.addSource("Wing", xInUnit & peers_of_b, DigitSet({x}));
                  event.addSource("Wing", b, DigitSet({x, y}));
                  event.addSource("Z", a, DigitSet({y}));
                  event.addSource("Z", b, DigitSet({y}));
                  // remove instances of Y from peers of extreme cells
                  CellSet set = board.getPeersContaining(CellSet({a, b}), y);
                  for (Cell idx : set) {
                    event.addOperation(idx, y);
                  }
                  if (eventQueue.enqueue(board, event)) return;
                } else if (yInUnit.is_subset_of(peers_of_a | peers_of_b) && !(yInUnit & peers_of_a).empty() && !(yInUnit & peers_of_b).empty()) {
                  // W-Wing spotted on digit y
                  Event event(EventType::RemoveCandidate, ReasonId::WWing);
                  // the source is the four (or more) cells forming the W-Wing
                  event.addSource("Wing", a, DigitSet({x, y}));
                  event.addSource("Wing", yInUnit & peers_of_a, DigitSet({y}));
                  event.addSource("Wing", yInUnit & peers_of_b, DigitSet({y}));
                  event.addSource("Wing", b, DigitSet({x, y}));
                  event.addSource("Z", a, DigitSet({x}));
                  event.addSource("Z", b, DigitSet({x}));
                  // remove instances of X from peers of extreme cells
                  CellSet set = board.getPeersContaining(CellSet({a, b}), x);
                  for (Cell idx : set) {
                    event.addOperation(idx, x);
                  }
                  if (eventQueue.enqueue(board, event)) return;
                }
              }
            }
          }
        }
      }
    }
  }
}

/* -------------------- AIC WORLD -------------------- */
static void techGenericAIC(SudokuBoard &board,
                           EventQueue &eventQueue,
                           ReasonId reason) {
  AicSearcher searcher(board, eventQueue);
  const AicConfig &config = searcher.setConfigAndReturn(reason);
  AicGraph prunedGraph = board.getPrunedAicGraph(config);
  bool event = searcher.runSearch(prunedGraph);
}

static void techRemotePair(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::RemotePair);
}

static void techSingleDigitPattern(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::SingleDigitPattern);
}

static void techEmptyRectangle(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::EmptyRectangle);
}

static void techSimpleColoring(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::SimpleColoring);
}

static void tech3DMedusa(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::_3DMedusa);
}

static void techXChain(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::XChain);
}

static void techXYChain(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::XYChain);
}

static void techAIC(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::AIC);
}

static void techGroupedXChain(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::GroupedXChain);
}

static void techGroupedAIC(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::GroupedAIC);
}

static void techForcingChain(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericAIC(board, eventQueue, ReasonId::ForcingChain);
}
/* ------------------ END AIC WORLD ------------------ */

/* -------------------- ALS WORLD -------------------- */
static void techGenericALS(SudokuBoard &board,
                           EventQueue &eventQueue,
                           ReasonId reason) {
  AlsGraphBuilder builder(board);
  AlsSearcher searcher(board, eventQueue);
  const AlsConfig &config = searcher.setConfigAndReturn(reason);
  AlsGraph &graph = board.getAlsGraph(config);
  bool event = searcher.runSearch(graph);
}

static void techALSXZ(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericALS(board, eventQueue, ReasonId::ALSXZ);
}

static void techALSXYWing(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericALS(board, eventQueue, ReasonId::ALSXYWing);
}

static void techALSChain(SudokuBoard &board, EventQueue &eventQueue) {
  techGenericALS(board, eventQueue, ReasonId::ALSChain);
}
/* ------------------ END ALS WORLD ------------------ */

static void techSueDeCoq(SudokuBoard &board, EventQueue &eventQueue) {
  // basic Sue-de-Coq
  auto scanBasicSDC = [&](const Unit &box, const Unit &line) -> bool
  {
    CellSet intersection = (box & line).filter([&](Cell i){ return !board.isSolved(i); });
    DigitSet intersectionDigits;
    for (Cell idx : intersection) {
      intersectionDigits |= board.getCandidates(idx);
    }
    int intersectionSize = intersection.size();
    // basic Sue-de-Coq
    if (intersectionSize == 2 && intersectionDigits.size() == 4 ||
        intersectionSize == 3 && intersectionDigits.size() == 5) {
      CellSet li = line - intersection;
      CellSet bi = box - intersection;
      for (Cell lidx : li) {
        DigitSet liDigits = board.getCandidates(lidx);
        for (Cell bidx : bi) {
          DigitSet biDigits = board.getCandidates(bidx);
          // intersection size 2
          if ((liDigits & biDigits).empty() && (liDigits | biDigits) == intersectionDigits) {
            // basic Sue-de-Coq spotted
            Event event(EventType::RemoveCandidate, ReasonId::SueDeCoq);
            // the source is the cells in intersection, line and box
            event.addSource("line", CellSet({lidx}) | intersection, liDigits);
            event.addSource("box", CellSet({bidx}) | intersection, biDigits);
            // eliminate along line and inside the box
            for (Cell idx : line - intersection - li) {
              event.addOperation(idx, liDigits);
            }
            for (Cell idx : box - intersection - bi) {
              event.addOperation(idx, biDigits);
            }
            if (eventQueue.enqueue(board, event)) return true;
          }
          // intersection size 3
          if ((liDigits & biDigits).empty() && (intersectionDigits - liDigits - biDigits).size() == 1) {
            DigitSet extraDigit = intersectionDigits - liDigits - biDigits;
            // basic Sue-de-Coq spotted
            Event event(EventType::RemoveCandidate, ReasonId::SueDeCoq);
            // the source is the cells in intersection, line and box
            event.addSource("line", CellSet({lidx}) | intersection, liDigits);
            event.addSource("box", CellSet({bidx}) | intersection, biDigits);
            event.addSource("extra", intersection, extraDigit);
            // eliminate along line and inside the box
            for (Cell idx : line - intersection - li) {
              event.addOperation(idx, liDigits | extraDigit);
            }
            for (Cell idx : box - intersection - bi) {
              event.addOperation(idx, biDigits | extraDigit);
            }
            if (eventQueue.enqueue(board, event)) return true;
          }
        }
      }
    }
    return false;
  };

  for (const Unit &b : SudokuBoard::getBoxes()) {
    for (const Unit &r : SudokuBoard::getRows()) {
      if (scanBasicSDC(b, r)) return;
    }
  }

  for (const Unit &b : SudokuBoard::getBoxes()) {
    for (const Unit &c : SudokuBoard::getColumns()) {
      if (scanBasicSDC(b, c)) return;
    }
  }

  // extended Sue-de-Coq
  auto scanExtendedSDC = [&](const Unit &box, const Unit &line) -> bool
  {
    std::vector<CellSet> intersections;
    CellSet mainIntersection = (box & line).filter([&](Cell i){ return !board.isSolved(i); });
    intersections.push_back(mainIntersection);

    // add the possibility to consider only 2 cells when the box-line intersection contains 3 cells
    if (mainIntersection.size() == 3) {
      auto first  = mainIntersection.begin();
      auto second = ++first;
      auto third  = ++second;

      intersections.push_back(CellSet({*first}) | CellSet({*second}));
      intersections.push_back(CellSet({*first}) | CellSet({*third}));
      intersections.push_back(CellSet({*second}) | CellSet({*third}));
    }

    // then test every possible combination
    for (CellSet intersection : intersections) {
      DigitSet intersectionDigits;
      for (Cell idx : intersection) {
        intersectionDigits |= board.getCandidates(idx);
      }
      int intersectionSize = intersection.size();

      if (intersectionDigits.size() >= intersectionSize + 2) {
        // get unsolved locations in box outside intersection
        std::vector<int> boxList = box.to_vector();
        LocationSet boxUnsolved;
        for (Location i = 0; i < 9; ++i) {
          if (!board.isSolved(boxList[i]) && !intersection.contains(boxList[i])) {
            boxUnsolved.insert(i);
          }
        }

        // get unsolved locations in line outside intersection
        std::vector<int> lineList = line.to_vector();
        LocationSet lineUnsolved;
        for (Location i = 0; i < 9; ++i) {
          if (!board.isSolved(lineList[i]) && !intersection.contains(lineList[i])) {
            lineUnsolved.insert(i);
          }
        }

        for (int boxSubsetSize = 1; boxSubsetSize <= boxUnsolved.size(); ++boxSubsetSize) {
          std::vector<LocationSet> subsets = boxUnsolved.generate_power_set_of_size(boxSubsetSize);
          for (const LocationSet &boxSubset : subsets) {
            CellSet boxCells;
            DigitSet boxDigits;

            for (Location l : boxSubset) {
              Cell idx = static_cast<Cell>(boxList[l]);
              DigitSet candidates = board.getCandidates(idx);
              boxCells.insert(idx);
              boxDigits |= candidates;
            }

            for (int lineSubsetSize = 1; lineSubsetSize <= lineUnsolved.size(); ++lineSubsetSize) {
              std::vector<LocationSet> subsets = lineUnsolved.generate_power_set_of_size(lineSubsetSize);
              for (const LocationSet &lineSubset : subsets) {
                CellSet lineCells;
                DigitSet lineDigits;

                for (Location l : lineSubset) {
                  Cell idx = static_cast<Cell>(lineList[l]);
                  DigitSet candidates = board.getCandidates(idx);
                  lineCells.insert(idx);
                  lineDigits |= candidates;
                }

                // Sue-de-Coq test
                if ((boxDigits & lineDigits).empty()) {
                  if ((intersection | boxCells | lineCells).size() == (intersectionDigits | boxDigits | lineDigits).size()) {
                    DigitSet allDigits = intersectionDigits | boxDigits | lineDigits;
                    DigitSet extraDigit = intersectionDigits - boxDigits - lineDigits;
                    // extended Sue-de-Coq spotted
                    Event event(EventType::RemoveCandidate, ReasonId::SueDeCoq);
                    // the source is the cells in intersection, line and box
                    event.addSource("line", lineCells | intersection, lineDigits);
                    event.addSource("box", boxCells | intersection, boxDigits);
                    if (!extraDigit.empty()) event.addSource("extra", intersection, extraDigit);
                    // eliminate along line and inside the box
                    for (Cell idx : line - intersection - lineCells) {
                      event.addOperation(idx, lineDigits | extraDigit);
                    }
                    for (Cell idx : box - intersection - boxCells) {
                      event.addOperation(idx, boxDigits | extraDigit);
                    }
                    if (eventQueue.enqueue(board, event)) return true;
                  }
                }
              }
            }
          }
        }
      }
    }

    return false;
  };

  for (const Unit &b : SudokuBoard::getBoxes()) {
    for (const Unit &r : SudokuBoard::getRows()) {
      if (scanExtendedSDC(b, r)) return;
    }
  }

  for (const Unit &b : SudokuBoard::getBoxes()) {
    for (const Unit &c : SudokuBoard::getColumns()) {
      if (scanExtendedSDC(b, c)) return;
    }
  }
}

static void techDeathBlossom(SudokuBoard &board, EventQueue &eventQueue) {
  /* for each cell, register:
   * - its candidates
   * - which candidates are RCC for a given ALS
   * - the corresponding ALS(s) for each RCC
   * */
  struct BlossomNode {
    DigitSet candidates;                     // candidates of this cell
    DigitSet RCCs;                           // candidates that are RCC
    std::vector<AlsNodeID> setsByDigit[10];  // the corresponding ALS(s) for each RCC
  };

  AlsGraphBuilder builder(board);
  AlsSearcher searcher(board, eventQueue);
  const AlsConfig &config = searcher.setConfigAndReturn(ReasonId::Solver);  // not important
  AlsGraph &graph = board.getAlsGraph(config);

  // syntax hell due to the function being recursive, can't use 'auto'
  std::function<bool (BlossomNode &,
                      int,
                      Digit,
                      DigitSet,
                      std::vector<const AlsNode *> &)>
    search_death_blossom = [&](BlossomNode &stem,
                    int index,
                    Digit RCC,
                    DigitSet accumulator,
                    std::vector<const AlsNode *> &petals) -> bool
  {
    if (RCC == 10) {
      // final, non-recursive step: remove RCCs from potential eliminations
      accumulator -= stem.candidates;

      // initialize Death Blossom event
      Event event(EventType::RemoveCandidate, ReasonId::DeathBlossom);
      // the source is the stem and the petals
      event.addSource("stem", index, stem.candidates);
      for (const AlsNode *petal : petals) {
        event.addSource("petal", petal->cellSet, petal->digitSet);
      }

      // look for eliminations
      for (Digit elimination : accumulator) {
        // common digit found, possible elimination ahead
        CellSet source;
        for (const AlsNode *petal : petals) {
          source |= petal->cellSet.filter([&](Cell i){ return board.hasCandidate(i, elimination); });
        }
        CellSet target = board.getPeersContaining(source, elimination);
        if (!target.empty()) {
          // Death Blossom found - eliminate the common digit
          for (Cell idx : target) {
            event.addOperation(idx, elimination);
          }
        }
      }
      if (eventQueue.enqueue(board, event)) return true;
    } else {
      // check if there are ALSs for the current RCC
      if (stem.setsByDigit[RCC].empty()) {
        // go on with the next RCC
        return search_death_blossom(stem, index, RCC+1, accumulator, petals);
      }
      // for each ALS in the current RCC
      for (AlsNodeID id : stem.setsByDigit[RCC]) {
        const AlsNode &als = graph.nodes[id];
        // check if the ALS of this RCC is already in the set of petals
        bool already_present = false;
        for (const AlsNode *petal : petals) {
          if (petal->id == id) {
            already_present = true;
          }
        }
        // then add if new
        std::vector<const AlsNode *> new_petals = petals;
        if (!already_present) new_petals.push_back(&als);
        // go on with the next RCC with updated accumulator and petals
        if (search_death_blossom(stem, index, RCC+1, accumulator & als.digitSet, new_petals)) return true;
      }
    }
    return false;
  };

  // initialize database
  BlossomNode database[81];
  for (Cell idx = 0; idx < 81; ++idx) {
    database[idx] = {
      .candidates = board.getCandidates(idx),
      .RCCs = DigitSet(0)
    };
  }

  /* For each ALS:
   *   For each digit:
   *     Get peers outside the ALS containing that digit
   *     Store those peers in a Blossom Node 
   */
  for (auto &[nodeID, node] : graph.nodes) {
    AlsNodeID id = nodeID;
    for (Digit digit : node.digitSet) {
      CellSet source = node.cellSet.filter([&](Cell i){ return board.hasCandidate(i, digit); });
      CellSet target = board.getPeersContaining(source, digit);
      for (Cell idx : target) {
        database[idx].RCCs.insert(digit);
        database[idx].setsByDigit[digit].push_back(id);
      }
    }
  }

  // search for Death Blossom
  for (Cell i = 0; i < 81; ++i) {
    BlossomNode &stem = database[i];
    // a cell where each candidate is an RCC
    if (stem.candidates.size() > 1 && stem.candidates.size() == stem.RCCs.size()) {
      // since each RCC can have more than one ALS associated, we need a recursive function
      int index = i;
      DigitSet accumulator = ALL_DIGITS;
      Digit RCC = 0;
      std::vector<const AlsNode *> petals;
      if (search_death_blossom(stem, index, RCC, accumulator, petals)) return;
    }
  }
}

static void techFireworks(SudokuBoard &board, EventQueue &eventQueue) {
  // represent a single firework for a digit centered in a cell
  struct Firework {
    Digit digit;
    Cell baseCell;
    CellSet externalCells;
    bool valid = false;
  };

  // map each cell-digit pair to a possible firework
  std::map<std::pair<Cell, Digit>, Firework> fireworks;

  // look for single fireworks
  for (Cell i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }

    const Unit &box = board.getBoxByCell(i);
    const CellSet &row_external = board.getRowByCell(i) - box;
    const CellSet &column_external = board.getColumnByCell(i) - box;

    for (Digit d : board.getCandidates(i)) {
      const CellSet source_peers = board.getPeersContaining(i, d);
      const CellSet row_external_peers = row_external & source_peers;
      const CellSet column_external_peers = column_external & source_peers;
      // a firework exists when the digit appears as candidate at most once along
      // the row and column, without considering the initial box
      if (row_external_peers.size() <= 1 && column_external_peers.size() <= 1 &&
          row_external_peers.size() + column_external_peers.size() >= 1) {
        // single firework identified
        CellSet externalCells = row_external_peers | column_external_peers;
        Firework firework{d, i, externalCells, true};
        fireworks[{i, d}] = firework;
      }
    }
  }

  // look for triple fireworks
  for (Cell i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }
    DigitSet candidates = board.getCandidates(i);
    // check all subsets of three candidates in a cell
    for (const DigitSet &triple : candidates.generate_power_set_of_size(3)) {
      std::vector<int> digits = triple.to_vector();
      Digit a = digits[0];
      Digit b = digits[1];
      Digit c = digits[2];
      // TODO: improve implementation to avoid iterating on non-existing fireworks
      const Firework &fireA = fireworks[{i, a}];
      const Firework &fireB = fireworks[{i, b}];
      const Firework &fireC = fireworks[{i, c}];
      bool validity = fireA.valid && fireB.valid && fireC.valid;
      CellSet totalExternalCells = validity ? (fireA.externalCells | fireB.externalCells | fireC.externalCells)
                                            : CellSet();
      // if every firework involves the same cells, then we can apply eliminations
      if (totalExternalCells.size() == 2) {
        // Triple Fireworks spotted
        Event event(EventType::RemoveCandidate, ReasonId::Fireworks, ReasonId::TripleFireworks);
        // the source is the three cells forming the pattern and their digits
        event.addSource("fireworks_A", i, triple);
        for (Cell idx : totalExternalCells) {
          event.addSource("fireworks_A", idx, board.getCandidates(idx) & triple);
        }
        // remove any digit outside of the triple from the involved cells
        event.addOperation(i, candidates - triple);
        for (Cell idx : totalExternalCells) {
          event.addOperation(idx, board.getCandidates(idx) - triple);
        }
        // remove instances of the triple inside the box (excluding row and column)
        CellSet boxEliminationCells = SudokuBoard::getBoxByCell(i) - SudokuBoard::getRowByCell(i) - SudokuBoard::getColumnByCell(i);
        for (Cell idx : boxEliminationCells) {
          event.addOperation(idx, board.getCandidates(idx) & triple);
        }
        if (eventQueue.enqueue(board, event)) return;
      }
    }
  }

  // look for quadruple fireworks
  for (Cell i = 0; i < 81; i++) {
    if (board.isSolved(i)) {
      continue;
    }
    for (Cell j = i+1; j < 81; j++) {
      if (board.isSolved(j) || board.sees(i, j)) {
        continue;
      }
      DigitSet A_candidates = board.getCandidates(i);
      // check all subsets of two candidates in the two cells
      for (const DigitSet &A_pair : A_candidates.generate_power_set_of_size(2)) {
        DigitSet B_candidates = board.getCandidates(j);
        for (const DigitSet &B_pair : B_candidates.generate_power_set_of_size(2)) {
          // the two sets must be disjoint
          if ((A_pair & B_pair).empty()) {
            std::vector<int> A_digits = A_pair.to_vector();
            Digit a = A_digits[0];
            Digit b = A_digits[1];
            std::vector<int> B_digits = B_pair.to_vector();
            Digit c = B_digits[0];
            Digit d = B_digits[1];
            // TODO: improve implementation to avoid iterating on non-existing fireworks
            const Firework &fireA = fireworks[{i, a}];
            const Firework &fireB = fireworks[{i, b}];
            const Firework &fireC = fireworks[{j, c}];
            const Firework &fireD = fireworks[{j, d}];
            bool validity = fireA.valid && fireB.valid && fireC.valid && fireD.valid;
            CellSet totalExternalCells = validity ? (fireA.externalCells | fireB.externalCells | fireC.externalCells | fireD.externalCells)
                                                  : CellSet();
            // if every firework involves the same cells, then we can apply eliminations
            if (totalExternalCells.size() == 2) {
              // Quadruple Fireworks spotted
              Event event(EventType::RemoveCandidate, ReasonId::Fireworks, ReasonId::QuadrupleFireworks);
              // the source is the two sets of three cells forming the pattern and their digits
              event.addSource("fireworks_A", i, A_pair);
              for (Cell idx : (fireA.externalCells | fireB.externalCells)) {
                event.addSource("fireworks_A", idx, board.getCandidates(idx) & A_pair);
              }
              event.addSource("fireworks_B", j, B_pair);
              for (Cell idx : (fireC.externalCells | fireD.externalCells)) {
                event.addSource("fireworks_B", idx, board.getCandidates(idx) & B_pair);
              }
              // remove any digit outside of the pair from the base cells
              event.addOperation(i, A_candidates - A_pair);
              event.addOperation(j, B_candidates - B_pair);
              // remove any digit outside of the two pairs from the external cells
              for (Cell idx : totalExternalCells) {
                event.addOperation(idx, board.getCandidates(idx) - A_pair - B_pair);
              }
              // remove instances of the pairs inside each box (excluding row and column)
              CellSet firstBoxEliminationCells = SudokuBoard::getBoxByCell(i) - SudokuBoard::getRowByCell(i) - SudokuBoard::getColumnByCell(i);
              for (Cell idx : firstBoxEliminationCells) {
                event.addOperation(idx, board.getCandidates(idx) & A_pair);
              }
              CellSet secondBoxEliminationCells = SudokuBoard::getBoxByCell(j) - SudokuBoard::getRowByCell(j) - SudokuBoard::getColumnByCell(j);
              for (Cell idx : secondBoxEliminationCells) {
                event.addOperation(idx, board.getCandidates(idx) & B_pair);
              }
              if (eventQueue.enqueue(board, event)) return;
            }
          }
        }
      }
    }
  }
}

// nCr(9, 2) = 36
// nCr(9, 3) = 84
// nCr(9, 4) = 126

typedef void (*TechniqueFn)(SudokuBoard &, EventQueue &);

struct TechniqueEntry {
  TechniqueFn fn;
  ReasonId reason;
  Category category;
};

static const TechniqueEntry TECHNIQUES[] = {
  {techFullHouse,              ReasonId::FullHouse,           Category::Single},
  {techHiddenSinglesBox,       ReasonId::HiddenSingle,        Category::Single},
  {techPointingSet,            ReasonId::PointingSet,         Category::Intersection},
  {techBoxLineReduction,       ReasonId::BoxLineReduction,    Category::Intersection},
  {techHiddenSinglesRowColumn, ReasonId::HiddenSingle,        Category::Single},
  {techHiddenPairsBox,         ReasonId::HiddenPair,          Category::NakedHiddenSet},
  {techHiddenPairsRowColumn,   ReasonId::HiddenPair,          Category::NakedHiddenSet},
  {techNakedSingles,           ReasonId::NakedSingle,         Category::Single},
  {techNakedPairs,             ReasonId::NakedPair,           Category::NakedHiddenSet},
  {techNakedTriples,           ReasonId::NakedTriple,         Category::NakedHiddenSet},
  {techHiddenTriples,          ReasonId::HiddenTriple,        Category::NakedHiddenSet},
  {techBUGPlusOne,             ReasonId::BUGPlusOne,          Category::BUG},
  {techXWing,                  ReasonId::XWing,               Category::Fish},
  {techXYWing,                 ReasonId::XYWing,              Category::Wing},
  {techSwordfish,              ReasonId::Swordfish,           Category::Fish},
  {techRemotePair,             ReasonId::RemotePair,          Category::Coloring},
  {techUniqueRectangle,        ReasonId::UniqueRectangle,     Category::UR},
  {techWWing,                  ReasonId::WWing,               Category::Wing},
  {techSingleDigitPattern,     ReasonId::SingleDigitPattern,  Category::AIC},
  {techFinnedXWing,            ReasonId::FinnedXWing,         Category::Fish},
  {techEmptyRectangle,         ReasonId::EmptyRectangle,      Category::AIC},
  {techXYZWing,                ReasonId::XYZWing,             Category::Wing},
  {techSimpleColoring,         ReasonId::SimpleColoring,      Category::Coloring}, 
  {tech3DMedusa,               ReasonId::_3DMedusa,           Category::Coloring},
  {techXChain,                 ReasonId::XChain,              Category::AIC},
  {techFinnedSwordfish,        ReasonId::FinnedSwordfish,     Category::Fish},
  {techFireworks,              ReasonId::Fireworks,           Category::Fireworks},
  {techHiddenRectangle,        ReasonId::HiddenRectangle,     Category::UR},
  {techJellyfish,              ReasonId::Jellyfish,           Category::Fish},
  {techXYChain,                ReasonId::XYChain,             Category::AIC},
  {techGroupedXChain,          ReasonId::GroupedXChain,       Category::AIC},
  {techFinnedJellyfish,        ReasonId::FinnedJellyfish,     Category::Fish},
  {techAIC,                    ReasonId::AIC,                 Category::AIC},
  {techGroupedAIC,             ReasonId::GroupedAIC,          Category::AIC},
  {techSueDeCoq,               ReasonId::SueDeCoq,            Category::SDC},
  {techALSXZ,                  ReasonId::ALSXZ,               Category::ALS},
  {techALSXYWing,              ReasonId::ALSXYWing,           Category::ALS},
  {techALSChain,               ReasonId::ALSChain,            Category::ALS},
  {techDeathBlossom,           ReasonId::DeathBlossom,        Category::Blossom},
  {techForcingChain,           ReasonId::ForcingChain,        Category::FC},
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

// Drain the next event and serialize the operations into response as described by API.
// The function returns only events and operations that are applicable to the current 
// state of the board. This implies that some events in queue could be discarded.
// The function will continue the search until the queue is empty.
static int drain_event(SudokuBoard &board,
                       EventQueue &eventQueue,
                       json &response,
                       bool apply) {
  Event first;
  if (!eventQueue.peek(first)) {
    response = api_error(ApiError::NoStep, "No step found");
    return 0;
  }

  const EventType type = first.type;
  const ReasonId reason = first.reason;
  const ReasonId detailedReason = first.detailedReason;

  eventQueue.dequeue(first);

  json out;
  out["type"] = type == EventType::SetValue ? "setValue" : "removeCandidate";
  out["reason"] = reason;
  out["detailedReason"] = detailedReason;

  // Serialize sources first
  out["sources"] = json::array();
  for (auto &[name, sourceList] : first.getSources()) {
    json this_source = json::object();

    json this_list = json::array();
    if (!sourceList.empty()) {
      for (auto &source : sourceList) {
        json this_source;
        auto cells = source.cells.to_vector();
        auto mask = source.mask.to_vector();
        this_source["cells"] = cells;
        this_source["digits"] = mask;
        this_source["eureka"] = cellset_to_eureka(source.cells);
        this_list.push_back(this_source);
      }
    }

    this_source["name"] = name;
    this_source["list"] = this_list;

    out["sources"].push_back(this_source);
  }

  // Serialize operations (with applicability filter)
  bool found_operations = false;
  out["operations"] = json::array();
  for (auto &op : first.getOperations()) {
    if (is_operation_applicable(board, type, op)) {
      json this_op;
      this_op["cell"] = op.idx;
      this_op["digits"] = op.mask.to_vector();
      out["operations"].push_back(this_op);
      found_operations = true;
      
      if (apply) {
        if (type == EventType::SetValue) {
          // Set + Look for immediate naked singles
          board.applySetValue(op.idx, *op.mask.begin());
          for (Cell idx : board.getPeers(op.idx)) {
            int only = board.getSingleCandidate(idx);
            if (only) {
              Event event(EventType::SetValue, ReasonId::NakedSingle);
              event.addOperation(idx, only);
              eventQueue.enqueue(board, event);
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
              eventQueue.enqueue(board, event);
            }
          }
        }
      }
    }
  }

  // If no ops found, discard and continue draining.
  if (found_operations) {
    response["status"] = "ok";
    response["step"] = out;
    return 1;
  } else {
    return drain_event(board, eventQueue, response, apply);
  }
}

// Run techniques to fill the queue if needed, then return a single event.
// The drained operations are applied to 'board'.
static int compute_next_event(SudokuBoard &board,
                              EventQueue &eventQueue,
                              json &response) {
  // If we already have pending events, return them immediately.
  if (drain_event(board, eventQueue, response, true)) {
    return 1;
  }

  // Run enabled techniques in priority order; stop at the first technique that enqueues anything,
  // then verify, apply and return.
  for (const TechniqueEntry &tech : TECHNIQUES) {
    if (!is_technique_enabled(tech.reason)) {
      continue;
    }
    tech.fn(board, eventQueue);
    if (drain_event(board, eventQueue, response, true)) {
      return 1;
    }
  }

  // No events produced.
  return 0;
}

// =========================================================
// API implementation
// =========================================================

static json json_sudorix_solver_count_solutions(const json &request);
static json json_sudorix_solver_full(const json &request);
static json json_sudorix_solver_init_board(const json &request);
static json json_sudorix_solver_next_step(const json &request);
static json json_sudorix_solver_export_board(const json &request);
static json json_sudorix_solver_hint(const json &request);
static json json_sudorix_solver_all_possible_steps_for_technique(const json &request);
static json json_sudorix_solver_set_enabled_techniques(const json &request);
static json json_sudorix_solver_get_techniques(const json &request);

static json json_sudorix_solver_count_solutions(const json &request) {
  json response;
  std::string puzzle = request.at("puzzle");

  // Import Sudoku from string
  SudokuBoard board;
  if (!board.importPuzzle(puzzle)) {
    return api_error(ApiError::InvalidPuzzle, "Invalid puzzle");
  }

  // Count solutions
  int result = board.countSolutions();

  response["status"] = "ok";
  response["solutions"] = result;
  return response;
}

static json json_sudorix_solver_full(const json &request) {
  ensure_solver_config_initialized();

  json response;
  std::string puzzle = request.at("puzzle");

  // Import Sudoku from string
  SudokuBoard board;
  if (!board.importPuzzle(puzzle)) {
    return api_error(ApiError::InvalidPuzzle, "Invalid puzzle");
  }

  // Instantiate queue
  EventQueue queue;

  // Solve loop using existing stepper:
  // repeatedly compute one event, apply it locally, and continue until stuck.
  int guard = 0;
  const int guardMax = 200000;

  json fake;
  while (guard++ < guardMax) {
    const int ok = compute_next_event(board, queue, fake);
    if (!ok) {
      break;
    }
  }

  // Export
  std::string solution = board.to_json()["values"];

  response["status"] = "ok";
  response["solution"] = solution;
  return response;
}

static json json_sudorix_solver_init_board(const json &request) {
  ensure_solver_config_initialized();

  json response;
  std::string puzzle = request.at("puzzle");

  // Import Sudoku from string (solver is the source of truth)
  if (!g_sudokuBoard.importPuzzle(puzzle)) {
    return api_error(ApiError::InvalidPuzzle, "Invalid puzzle");
  }

  // Reset queue
  g_eventQueue = EventQueue();

  return api_ok();
}

static json json_sudorix_solver_next_step(const json &request) {
  json response;

  // Compute one event, apply it locally and return it to the caller.
  const int ok = compute_next_event(g_sudokuBoard, g_eventQueue, response);
  if (ok) {
    return response;
  } else {
    return api_error(ApiError::NoStep, "No step found");
  }
}

static json json_sudorix_solver_export_board(const json &request) {
  json response;

  // Export Sudoku
  response["status"] = "ok";
  response["board"] = g_sudokuBoard.to_json();

  return response;
}

static json json_sudorix_solver_hint(const json &request) {
  ensure_solver_config_initialized();

  json response;

  // Build a temporary board owned by the caller (UI is the source of truth here).
  SudokuBoard board;
  if (!board.from_json(request.at("board"))) {
    return api_error(ApiError::InvalidBoard, "Invalid board");
  }

  // Instantiate a new queue for this hint computation.
  EventQueue queue;

  const int ok = compute_next_event(board, queue, response);
  if (ok) {
    return response;
  } else {
    return api_error(ApiError::NoStep, "No step found");
  }
}

static json json_sudorix_solver_all_possible_steps_for_technique(const json &request) {
  ensure_solver_config_initialized();

  json response;
  ReasonId technique = request.at("technique");

  // Build a temporary board owned by the caller (JS is the source of truth here).
  SudokuBoard board;
  if (!board.from_json(request.at("board"))) {
    return api_error(ApiError::InvalidBoard, "Invalid board");
  }

  // Instantiate a new queue for steps computation.
  EventQueue queue;

  // Backup global config and update for all possible steps computation.
  const bool oldAllPossibleSteps = g_solverConfig.allPossibleSteps;
  const bool oldEnabled = g_solverConfig.enabledTechniques[(uint8_t)technique];
  g_solverConfig.allPossibleSteps = true;
  g_solverConfig.enabledTechniques[(uint8_t)technique] = true;

  bool matched = false;
  for (const TechniqueEntry &tech : TECHNIQUES) {
    if (tech.reason != technique) {
      continue;
    }
    matched = true;
    tech.fn(board, queue);
  }

  // Restore global config
  g_solverConfig.enabledTechniques[(uint8_t)technique] = oldEnabled;
  g_solverConfig.allPossibleSteps = oldAllPossibleSteps;

  if (!matched) {
    response["status"] = "ok";
    response["instances"] = 0;
    return response;
  }

  response["steps"] = json::array();

  do {
    json step;
    bool found = drain_event(board, queue, step, false);
    if (found) {
      response["steps"].push_back(step["step"]);
    }
  } while (!queue.empty());

  response["status"] = "ok";
  response["instances"] = response["steps"].size();
  return response;
}

static json json_sudorix_solver_set_enabled_techniques(const json &request) {
  ensure_solver_config_initialized();

  json response;
  std::vector<ReasonId> techniques = request.at("techniques").get<std::vector<ReasonId>>();

  // validate techniques
  for (auto &tech : techniques) {
    try {
      uint32_t id = static_cast<uint32_t>(tech);
    } catch (const json::exception &e) {
      return api_error(ApiError::InvalidTechnique, e.what());
    }
  }

  for (bool &enabled : g_solverConfig.enabledTechniques) {
    enabled = false;
  }

  for (auto &tech : techniques) {
    uint32_t id = static_cast<uint32_t>(tech);
    g_solverConfig.enabledTechniques[id] = true;
  }

  return api_ok();
}

static json json_sudorix_solver_get_techniques(const json &request) {
  json response;

  json techniques = json::array();
  for (auto &tech : TECHNIQUES) {
    json technique = {
      {"name", tech.reason},
      {"category", tech.category}
    };
    techniques.push_back(technique);
  }

  response["status"] = "ok";
  response["techniques"] = techniques;
  return response;
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
  EMSCRIPTEN_KEEPALIVE
  const char *sudorix_solver_api(const char *requestJson) {
    thread_local std::string output;

    try {
      json request = json::parse(requestJson);
      json response;

#if 1
      console_log("JSON request: %s", request.dump(2).c_str());
#endif

      ApiCommand cmd;

      try {
        cmd = request.at("command").get<ApiCommand>();
      } catch (const json::exception &e) {
        response = api_error(ApiError::InvalidCommand, e.what());

        output = response.dump();
        return output.c_str();
      }
      
      switch (cmd) {
        case ApiCommand::CountSolutions:
          response = json_sudorix_solver_count_solutions(request);
          break;
        case ApiCommand::FullSolve:
          response = json_sudorix_solver_full(request);
          break;
        case ApiCommand::InitBoard:
          response = json_sudorix_solver_init_board(request);
          break;
        case ApiCommand::NextStep:
          response = json_sudorix_solver_next_step(request);
          break;
        case ApiCommand::ExportBoard:
          response = json_sudorix_solver_export_board(request);
          break;
        case ApiCommand::Hint:
          response = json_sudorix_solver_hint(request);
          break;
        case ApiCommand::AllPossibleSteps:
          response = json_sudorix_solver_all_possible_steps_for_technique(request);
          break;
        case ApiCommand::SetEnabledTechniques:
          response = json_sudorix_solver_set_enabled_techniques(request);
          break;
        case ApiCommand::GetTechniques:
          response = json_sudorix_solver_get_techniques(request);
          break;
      }

#if 1
      console_log("JSON response: %s", response.dump(2).c_str());
#endif

      output = response.dump();
      return output.c_str();
    } catch (const json::exception &e) {
      output = api_error(ApiError::InvalidRequest, e.what()).dump();
      return output.c_str();
    } catch (const std::exception &e) {
      output = api_error(ApiError::InternalError, e.what()).dump();
      return output.c_str();
    } catch (...) {
      output = api_error(ApiError::InternalError, "Unknown exception").dump();
      return output.c_str();
    }
  }
} // extern "C"

// higher-level interface
json sudorix_solver_api(const json &request) {
  const char *responseStr = sudorix_solver_api(request.dump().c_str());
  json response = json::parse(responseStr);
  return response;
}
