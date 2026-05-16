#include "Forcing.hpp"

#include <algorithm>
#include <queue>

namespace {
constexpr int kCells = 81;
constexpr int kDigitsPerCellSlot = 10; // digits are addressed as 1..9; slot 0 unused
constexpr int kPolaritySlots = 2;
constexpr int kLiteralCount = kCells * kDigitsPerCellSlot * kPolaritySlots;

bool sameLiteral(const ForcingLiteral &a, const ForcingLiteral &b) {
  return a.cell == b.cell && a.digit == b.digit && a.on == b.on;
}

bool sameCandidate(const ForcingLiteral &a, const ForcingLiteral &b) {
  return a.cell == b.cell && a.digit == b.digit;
}

} // namespace

ForcingChainBuilder::ForcingChainBuilder(const SudokuBoard &baseBoard, const ForcingConfig &cfg)
  : base(baseBoard), config(cfg), graph(kLiteralCount) { }

int ForcingChainBuilder::literalIndex(Cell cell, Digit digit, bool on) {
  return ((static_cast<int>(cell) * kDigitsPerCellSlot + static_cast<int>(digit)) * kPolaritySlots) + (on ? 1 : 0);
}

int ForcingChainBuilder::literalIndex(ForcingLiteral literal) {
  return literalIndex(literal.cell, literal.digit, literal.on);
}

ForcingLiteral ForcingChainBuilder::literalFromIndex(int index) {
  const bool on = (index % kPolaritySlots) != 0;
  index /= kPolaritySlots;
  const Digit digit = static_cast<Digit>(index % kDigitsPerCellSlot);
  const Cell cell = static_cast<Cell>(index / kDigitsPerCellSlot);
  return ForcingLiteral{cell, digit, on};
}

ForcingLiteral ForcingChainBuilder::complement(ForcingLiteral literal) {
  literal.on = !literal.on;
  return literal;
}

bool ForcingChainBuilder::candidateExists(Cell cell, Digit digit) const {
  return cell >= 0 && cell < 81 && digit >= 1 && digit <= 9 && !base.isSolved(cell) && base.hasCandidate(cell, digit);
}

bool ForcingChainBuilder::operationStillUseful(ForcingLiteral literal) const {
  if (!literal.valid() || !candidateExists(literal.cell, literal.digit)) {
    return false;
  }
  return true;
}

void ForcingChainBuilder::addImplication(ForcingLiteral from, ForcingLiteral to) {
  if (!operationStillUseful(from) || !operationStillUseful(to)) {
    return;
  }
  graph[literalIndex(from)].push_back(Edge{literalIndex(to)});
}

void ForcingChainBuilder::build() {
  for (auto &edges : graph) {
    edges.clear();
  }

  addCellImplications();
  addPeerImplications();
  addUnitStrongLinkImplications();
}

void ForcingChainBuilder::addCellImplications() {
  for (Cell cell = 0; cell < 81; ++cell) {
    if (base.isSolved(cell)) {
      continue;
    }

    const DigitSet candidates = base.getCandidates(cell);

    // If a candidate is true in a cell, all other candidates in that cell are false.
    for (Digit digit : candidates) {
      for (Digit other : candidates) {
        if (other != digit) {
          addImplication(ForcingLiteral{cell, digit, true}, ForcingLiteral{cell, other, false});
        }
      }
    }

    // If a bivalue cell loses one candidate, the other one is forced true.
    if (candidates.size() == 2) {
      std::vector<int> ds = candidates.to_vector();
      const Digit a = static_cast<Digit>(ds[0]);
      const Digit b = static_cast<Digit>(ds[1]);
      addImplication(ForcingLiteral{cell, a, false}, ForcingLiteral{cell, b, true});
      addImplication(ForcingLiteral{cell, b, false}, ForcingLiteral{cell, a, true});
    }
  }
}

void ForcingChainBuilder::addPeerImplications() {
  for (Cell cell = 0; cell < 81; ++cell) {
    if (base.isSolved(cell)) {
      continue;
    }

    for (Digit digit : base.getCandidates(cell)) {
      for (Cell peer : base.getPeers(cell)) {
        if (candidateExists(peer, digit)) {
          addImplication(ForcingLiteral{cell, digit, true}, ForcingLiteral{peer, digit, false});
        }
      }
    }
  }
}

void ForcingChainBuilder::addUnitStrongLinkImplications() {
  auto addForUnit = [this](const Unit &unit) {
    for (Digit digit = 1; digit <= 9; ++digit) {
      std::vector<Cell> cells;
      for (Cell cell : unit) {
        if (candidateExists(cell, digit)) {
          cells.push_back(cell);
        }
      }

      // If digit can be in only two places in a unit, removing one forces the other.
      if (cells.size() == 2) {
        addImplication(ForcingLiteral{cells[0], digit, false}, ForcingLiteral{cells[1], digit, true});
        addImplication(ForcingLiteral{cells[1], digit, false}, ForcingLiteral{cells[0], digit, true});
      }
    }
  };

  for (const Unit &unit : SudokuBoard::getRows()) addForUnit(unit);
  for (const Unit &unit : SudokuBoard::getColumns()) addForUnit(unit);
  for (const Unit &unit : SudokuBoard::getBoxes()) addForUnit(unit);
}

std::optional<ForcingPath> ForcingChainBuilder::findPath(ForcingLiteral from, ForcingLiteral to) const {
  if (!operationStillUseful(from) || !operationStillUseful(to)) {
    return std::nullopt;
  }

  const int start = literalIndex(from);
  const int target = literalIndex(to);

  std::vector<int> parent(kLiteralCount, -1);
  std::vector<int> depth(kLiteralCount, -1);
  std::queue<int> q;

  parent[start] = start;
  depth[start] = 0;
  q.push(start);

  while (!q.empty()) {
    const int current = q.front();
    q.pop();

    if (current == target) {
      break;
    }
    if (depth[current] >= config.maxChainDepth) {
      continue;
    }

    for (const Edge &edge : graph[current]) {
      if (edge.to < 0 || edge.to >= kLiteralCount || parent[edge.to] != -1) {
        continue;
      }
      parent[edge.to] = current;
      depth[edge.to] = depth[current] + 1;
      q.push(edge.to);
    }
  }

  if (parent[target] == -1) {
    return std::nullopt;
  }

  std::vector<ForcingLiteral> reversed;
  for (int at = target; at != start; at = parent[at]) {
    reversed.push_back(literalFromIndex(at));
  }
  reversed.push_back(literalFromIndex(start));
  std::reverse(reversed.begin(), reversed.end());

  return ForcingPath{reversed};
}

std::vector<int> ForcingChainBuilder::reachableFrom(ForcingLiteral from) const {
  std::vector<int> depth(kLiteralCount, -1);
  std::queue<int> q;
  const int start = literalIndex(from);

  if (!operationStillUseful(from)) {
    return depth;
  }

  depth[start] = 0;
  q.push(start);

  while (!q.empty()) {
    const int current = q.front();
    q.pop();

    if (depth[current] >= config.maxChainDepth) {
      continue;
    }

    for (const Edge &edge : graph[current]) {
      if (edge.to < 0 || edge.to >= kLiteralCount || depth[edge.to] != -1) {
        continue;
      }
      depth[edge.to] = depth[current] + 1;
      q.push(edge.to);
    }
  }

  return depth;
}

bool ForcingChainBuilder::find(Event &outEvent) const {
  return findContradiction(outEvent) || findCommonConsequences(outEvent);
}

bool ForcingChainBuilder::findContradiction(Event &outEvent) const {
  return false;

  // Nishio Forcing Chain
  // Supported cases:
  // - The assumption is false because it leads a candidate being both ON and OFF.
  // Not yet supported:
  // - The assumption is false because it leads to a bi-value cell being emptied.
  // - The assumption is false because it leads to the last remaining candidates in a unit to be both ON.
  // - The assumption in false because it leads to the last remaining candidates in a unit to be both OFF.
  //
  // WARNING: the current implementation gives the exact same results of a Digit Forcing Chain
  // but they harder to visualize on the grid, so Nishio is currently disabled.
  //
  for (Cell assumptionCell = 0; assumptionCell < 81; ++assumptionCell) {
    for (Digit assumptionDigit = 1; assumptionDigit <= 9; ++assumptionDigit) {
      if (!candidateExists(assumptionCell, assumptionDigit)) {
        continue;
      }

      // store the initial implication: the candidate is ON
      const ForcingLiteral rootOn{assumptionCell, assumptionDigit, true};

      // find the reachable consequences from the assumption
      const std::vector<int> reach = reachableFrom(rootOn);

      // look for consequences that are contradictory
      for (Cell cell = 0; cell < 81; ++cell) {
        for (Digit digit = 1; digit <= 9; ++digit) {
          if (!candidateExists(cell, digit)) {
            continue;
          }

          if (cell == assumptionCell) {
            continue;
          }

          const ForcingLiteral onConsequence{cell, digit, true};
          const ForcingLiteral offConsequence{cell, digit, false};

          // a consequence is both true and false -> the assumption is false
          const int onIdx = literalIndex(onConsequence);
          const int offIdx = literalIndex(offConsequence);
          if (reach[onIdx] != -1 && reach[offIdx] != -1) {
            auto pathA = findPath(rootOn, onConsequence);
            auto pathB = findPath(rootOn, offConsequence);
            if (pathA && pathB) {
              // Nishio Forcing Chain spotted: source is the two chains starting from the assumption
              Event event(EventType::RemoveCandidate, ReasonId::ForcingChain, ReasonId::NishioForcingChain);
              addPathSources(event, *pathA);
              event.addDelimiter();
              addPathSources(event, *pathB);
              event.addOperation(assumptionCell, assumptionDigit);
              outEvent = event;
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

bool ForcingChainBuilder::findCommonConsequences(Event &outEvent) const {
  // Digit Forcing Chain
  // Supported cases:
  // - One candidate is ON either the assumption is true or false. It must be the solution.
  // - One candidate is OFF either the assumption is true or false. It can be removed.
  // Not yet supported:
  // - Two candidates in a cell are both ON, all other numbers can be removed.
  // - Two candidates on the same unit are both ON, all other numbers can be removed on that unit.
  for (Cell assumptionCell = 0; assumptionCell < 81; ++assumptionCell) {
    for (Digit assumptionDigit = 1; assumptionDigit <= 9; ++assumptionDigit) {
      if (!candidateExists(assumptionCell, assumptionDigit)) {
        continue;
      }

      // store the initial implications: the candidate can be ON or OFF
      const ForcingLiteral rootOn{assumptionCell, assumptionDigit, true};
      const ForcingLiteral rootOff{assumptionCell, assumptionDigit, false};

      // find the reachable consequences from each assumption
      const std::vector<int> onReach = reachableFrom(rootOn);
      const std::vector<int> offReach = reachableFrom(rootOff);

      // look for consequences that are common for all assumptions
      for (Cell cell = 0; cell < 81; ++cell) {
        for (Digit digit = 1; digit <= 9; ++digit) {
          if (!candidateExists(cell, digit)) {
            continue;
          }

          for (bool on : {true, false}) {
            // corner case: a consequence is an initial assumption
            const ForcingLiteral consequence{cell, digit, on};
            if (sameCandidate(consequence, rootOn)) {
              continue;
            }

            // corner case: the consequence is not reachable by every assumption
            const int idx = literalIndex(consequence);
            if (onReach[idx] == -1 || offReach[idx] == -1) {
              continue;
            }

            // corner case: there is no path for the desired consequence
            auto pathA = findPath(rootOn, consequence);
            auto pathB = findPath(rootOff, consequence);
            if (!pathA || !pathB) {
              continue;
            }

            // Digit Forcing Chain spotted: source is the ON-chain and the OFF-chain
            Event event(on ? EventType::SetValue : EventType::RemoveCandidate, ReasonId::ForcingChain, ReasonId::DigitForcingChain);
            addPathSources(event, *pathA);
            event.addDelimiter();
            addPathSources(event, *pathB);
            event.addOperation(cell, digit);
            outEvent = event;
            return true;
          }
        }
      }
    }
  }

  // Cell Forcing Chain
  // Supported cases:
  // - One candidate is ON for each possible solution of the starting cell. It must be the solution.
  // - One candidate is OFF for each possible solution of the starting cell. It can be removed.
  // Not yet supported:
  // - Two candidates in a cell are both ON, all other numbers can be removed.
  // - All candidates that can see all ends of the chain can be removed.
  for (Cell assumptionCell = 0; assumptionCell < 81; ++assumptionCell) {
    if (!base.isSolved(assumptionCell)) {
      std::vector<int> assumptionDigits = base.getCandidates(assumptionCell).to_vector();
      if (assumptionDigits.size() >= 2 && assumptionDigits.size() <= 4) {
        // store the initial implications: each candidate in the cell is true
        std::vector<ForcingLiteral> rootOnList;
        for (int i = 0; i < assumptionDigits.size(); ++i) {
          Digit digit = assumptionDigits[i];
          rootOnList.push_back(ForcingLiteral{assumptionCell, digit, true});
        }

        // find the reachable consequences from each assumption
        std::vector<std::vector<int>> onReachList;
        for (const ForcingLiteral &assumption : rootOnList) {
          onReachList.push_back(reachableFrom(assumption));
        }

        // look for consequences that are common for all assumptions
        for (Cell cell = 0; cell < 81; ++cell) {
          for (Digit digit = 1; digit <= 9; ++digit) {
            if (!candidateExists(cell, digit)) {
              continue;
            }

            for (bool on : {true, false}) {
              // corner case: a consequence is an initial assumption
              const ForcingLiteral consequence{cell, digit, on};
              bool consequence_equals_assumption = false;
              for (const ForcingLiteral &assumption : rootOnList) {
                if (sameCandidate(consequence, assumption)) {
                  consequence_equals_assumption = true;
                }
              }
              if (consequence_equals_assumption) {
                continue;
              }

              // corner case: the consequence is not reachable by every assumption
              const int idx = literalIndex(consequence);
              bool consequence_not_reachable = false;
              for (auto &consequences : onReachList) {
                if (consequences[idx] == -1) {
                  consequence_not_reachable = true;
                }
              }
              if (consequence_not_reachable) {
                continue;
              }

              // corner case: there is no path for the desired consequence
              std::vector<std::optional<ForcingPath>> pathList;
              bool path_not_found = false;
              for (const ForcingLiteral &assumption : rootOnList) {
                auto path = findPath(assumption, consequence);
                pathList.push_back(path);
                if (!path) {
                  path_not_found = true;
                }
              }
              if (path_not_found) {
                continue;
              }

              // Cell Forcing Chain spotted: source is the list of chains
              Event event(on ? EventType::SetValue : EventType::RemoveCandidate, ReasonId::ForcingChain, ReasonId::CellForcingChain);
              for (auto &path : pathList) {
                addPathSources(event, *path);
                event.addDelimiter();
              }
              event.addOperation(cell, digit);
              outEvent = event;
              return true;
            }
          }
        }
      }
    }
  }

  // Unit Forcing Chain
  // Supported cases:
  // - One candidate is ON for each possible position of starting unit. It must be the solution.
  // - One candidate is OFF for each possible position of the starting unit. It can be removed.
  // Not yet supported:
  // - Two candidates in a cell are both ON, all other numbers can be removed.
  // - All candidates that can see all ends of the chain can be removed.
  auto unitForcingChain = [&](const Unit &unit) -> bool
  {
    for (Digit assumptionDigit = 1; assumptionDigit <= 9; ++assumptionDigit) {
      std::vector<int> assumptionCells = base.getPositionsOfDigit(unit, assumptionDigit).to_vector();
      if (assumptionCells.size() >= 2 && assumptionCells.size() <= 4) {
        // store the initial implications: each candidate in the unit is true
        std::vector<ForcingLiteral> rootOnList;
        for (int i = 0; i < assumptionCells.size(); ++i) {
          Cell cell = assumptionCells[i];
          rootOnList.push_back(ForcingLiteral{cell, assumptionDigit, true});
        }

        // find the reachable consequences from each assumption
        std::vector<std::vector<int>> onReachList;
        for (const ForcingLiteral &assumption : rootOnList) {
          onReachList.push_back(reachableFrom(assumption));
        }

        // look for consequences that are common for all assumptions
        for (Cell cell = 0; cell < 81; ++cell) {
          for (Digit digit = 1; digit <= 9; ++digit) {
            if (!candidateExists(cell, digit)) {
              continue;
            }

            for (bool on : {true, false}) {
              // corner case: a consequence is an initial assumption
              const ForcingLiteral consequence{cell, digit, on};
              bool consequence_equals_assumption = false;
              for (const ForcingLiteral &assumption : rootOnList) {
                if (sameCandidate(consequence, assumption)) {
                  consequence_equals_assumption = true;
                }
              }
              if (consequence_equals_assumption) {
                continue;
              }

              // corner case: the consequence is not reachable by every assumption
              const int idx = literalIndex(consequence);
              bool consequence_not_reachable = false;
              for (auto &consequences : onReachList) {
                if (consequences[idx] == -1) {
                  consequence_not_reachable = true;
                }
              }
              if (consequence_not_reachable) {
                continue;
              }

              // corner case: there is no path for the desired consequence
              std::vector<std::optional<ForcingPath>> pathList;
              bool path_not_found = false;
              for (const ForcingLiteral &assumption : rootOnList) {
                auto path = findPath(assumption, consequence);
                pathList.push_back(path);
                if (!path) {
                  path_not_found = true;
                }
              }
              if (path_not_found) {
                continue;
              }

              // Unit Forcing Chain spotted: source is the list of chains
              Event event(on ? EventType::SetValue : EventType::RemoveCandidate, ReasonId::ForcingChain, ReasonId::UnitForcingChain);
              for (auto &path : pathList) {
                addPathSources(event, *path);
                event.addDelimiter();
              }
              event.addOperation(cell, digit);
              outEvent = event;
              return true;
            }
          }
        }
      }
    }
    return false;
  };

  for (const Unit &row : base.getRows()) {
    if (unitForcingChain(row)) return true;
  }
  for (const Unit &column: base.getColumns()) {
    if (unitForcingChain(column)) return true;
  }
  for (const Unit &box : base.getBoxes()) {
    if (unitForcingChain(box)) return true;
  }

  return false;
}

void ForcingChainBuilder::addPathSources(Event &event, const ForcingPath &path) {
  for (const ForcingLiteral &node : path.nodes) {
    event.addSource(node.cell, node.digit);
  }
}
