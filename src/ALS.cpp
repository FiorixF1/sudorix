#include "ALS.hpp"
#include "encoder.hpp"
#include <sstream>

AlsGraphBuilder::AlsGraphBuilder(const SudokuBoard &board) : board(board) { }

AlsGraph AlsGraphBuilder::build() {
  AlsGraph g;

  build_nodes(g.nodes);

  build_links(g.nodes, g.links);

  return g;
}

void AlsGraphBuilder::build_nodes(std::vector<AlsNode> &nodes) {
  for (const Unit &row : board.getRows()) {
    build_nodes_in_unit(nodes, row);
  }
  for (const Unit &column : board.getColumns()) {
    build_nodes_in_unit(nodes, column);
  }
  for (const Unit &box : board.getBoxes()) {
    build_nodes_in_unit(nodes, box);
  }
}

void AlsGraphBuilder::build_nodes_in_unit(std::vector<AlsNode> &nodes, const Unit &unit) {
  LocationSet unsolved = board.getUnsolvedLocations(unit);
  std::vector<int> unitList = unit.to_vector();

  int maxSubset = std::min<int>(DEFAULT_MAX_ALS_SIZE, unsolved.size());
  for (int subsetSize = 1; subsetSize <= maxSubset; ++subsetSize) {
    std::vector<LocationSet> subsets = unsolved.generate_power_set_of_size(subsetSize);
    for (const LocationSet &subset : subsets) {
      CellSet cells;
      DigitSet digits;

      for (Location l : subset) {
        Cell idx = static_cast<Cell>(unitList[l]);
        DigitSet candidates = board.getCandidates(idx);
        cells.insert(idx);
        digits |= candidates;
      }

      if (!cells.empty() && digits.size() == cells.size() + 1) {
        add_node_if_new(nodes, cells, digits);
      }
    }
  }
}

void AlsGraphBuilder::add_node_if_new(std::vector<AlsNode> &nodes,
                                      const CellSet &cells,
                                      const DigitSet &digits) const {
  AlsNode candidate = get_node_id(digits, cells);
  if (!candidate) {
    return;
  }
  // TODO: change std::vector to std::set? this may explain the performance issues...
  // all bivalue cells are duplicated and also some pairs and triples.
  // Maybe the whole ID system must be redesigned.
  if (std::find(nodes.begin(), nodes.end(), candidate) == nodes.end()) {
    nodes.push_back(candidate);
  }
}

AlsNode AlsGraphBuilder::get_node_id(Digit digit, Cell cell) const {
  return get_node_id(DigitSet({digit}), CellSet({cell}));
}

AlsNode AlsGraphBuilder::get_node_id(Digit digit, const CellSet &cells) const {
  return get_node_id(DigitSet({digit}), cells);
}

AlsNode AlsGraphBuilder::get_node_id(const DigitSet &digits, Cell cell) const {
  return get_node_id(digits, CellSet({cell}));
}

AlsNode AlsGraphBuilder::get_node_id(const DigitSet &digits, const CellSet &cells) const {
  auto codes = serialize_cellset_to_unitcodes(cells);

  // check for invalid input
  if (cells.empty() || codes.size() != 1 || digits.empty()) {
    console_log("Invalid input in get_node_id: cells.size = %d, codes.size = %d, digits.size() = %d", cells.size(), codes.size(), digits.size());
    return 0;
  }

  uint32_t code = codes[0];
  uint32_t shiftedDigits = digits.to_uint32() << 16;
  return shiftedDigits | code;
}

void AlsGraphBuilder::build_links(const std::vector<AlsNode> &nodes,
                                  std::map<AlsNode, std::vector<AlsEdge>> &links) {
  for (size_t i = 0; i < nodes.size(); ++i) {
    bool grouped = false;
    CellSet cellsA;
    DigitSet digitsA;
    deserialize_unitcode(nodes[i], cellsA, digitsA, grouped);

    for (size_t j = i+1; j < nodes.size(); ++j) {
      CellSet cellsB;
      DigitSet digitsB;
      deserialize_unitcode(nodes[j], cellsB, digitsB, grouped);

      if (!(cellsA & cellsB).empty()) {
        continue; // overlapping ALS are normally excluded
      }

      if (board.sees(cellsA, cellsB)) {
        continue; // do not link ALS that completely see each other
      }

      DigitSet common = digitsA & digitsB;
      for (Digit d : common) {
        if (is_rcc(nodes[i], nodes[j], d)) {
          add_edge(links, nodes[i], nodes[j], d);
        }
      }
    }
  }

#if 0
  console_log("ALS GRAPH");
  for (auto &it : links) {
    AlsNode fromID = it.first;
    auto &edges = it.second;

    CellSet fromCellSet;
    DigitSet fromDigitSet;
    bool isGrouped;
    deserialize_unitcode(fromID, fromCellSet, fromDigitSet, isGrouped);

    for (auto &toID : edges) {
      CellSet toCellSet;
      DigitSet toDigitSet;
      bool isGrouped;
      deserialize_unitcode(toID.to, toCellSet, toDigitSet, isGrouped);

      std::stringstream ss;
      ss << "Edge from "   << fromCellSet << " with digits " << fromDigitSet <<
            " to "         << toCellSet   << " with digits " << toDigitSet <<
            " with RCC = " << std::to_string(toID.rcc);
      std::string out = ss.str();

      console_log("%s", out.c_str());
    }
  }
#endif
}

bool AlsGraphBuilder::is_rcc(AlsNode a, AlsNode b, Digit digit) const {
  bool grouped = false;
  CellSet cellsA;
  CellSet cellsB;
  DigitSet digitsA;
  DigitSet digitsB;
  deserialize_unitcode(a, cellsA, digitsA, grouped);
  deserialize_unitcode(b, cellsB, digitsB, grouped);

  if (!digitsA.contains(digit) || !digitsB.contains(digit)) {
    return false;
  }

  CellSet digitCellsA = board.getPositionsOfDigit(cellsA, digit);
  CellSet digitCellsB = board.getPositionsOfDigit(cellsB, digit);

  if (digitCellsA.empty() || digitCellsB.empty()) {
    return false;
  }

  return board.sees(digitCellsA, digitCellsB);
}

void AlsGraphBuilder::add_edge(std::map<AlsNode, std::vector<AlsEdge>> &adj,
                               AlsNode a,
                               AlsNode b,
                               Digit rcc) {
  if (a == b || rcc == 0) {
    return;
  }

  auto add_one_way = [&](AlsNode x, AlsNode y) {
    auto &v = adj[x];
    AlsEdge edge{y, rcc};
    if (std::find(v.begin(), v.end(), edge) == v.end()) {
      v.push_back(edge);
    }
  };

  add_one_way(a, b);
  add_one_way(b, a);
}

/* ---------------------------------------------------------------------- */

AlsSearcher::AlsSearcher(const SudokuBoard &board)
  : board(board) {

}

const AlsConfig &AlsSearcher::setConfigAndReturn(ReasonId reason) {
  config = {};

  /*switch(reason) {
    case ReasonId::ALSXZ:
      config.max_depth = 1;
      break;
    case ReasonId::ALSXYWing:
      config.max_depth = 2;
      break;
    case ReasonId::ALSChain:
      config.max_depth = 6;
      break;
    default:
      config.max_depth = 6;
      break;
  }

  this->reason = reason;*/

  return config;
}

std::optional<Event> AlsSearcher::runSearch(AlsGraph &graph) {
  return als_search_from(graph);
}

std::optional<Event> AlsSearcher::als_search_from(AlsGraph &graph) {
  struct QueueItem {
    AlsNode start;
    AlsNode node;
    Digit last_rcc;
    int depth;
    int state_index;
  };

  std::vector<AlsSearchState> states;
  std::vector<AlsParent> parents;
  std::deque<QueueItem> q;

  auto push_state = [&](AlsNode start,
                        AlsNode node,
                        Digit last_rcc,
                        int depth,
                        int prev_idx,
                        AlsNode prev_node,
                        Digit via_rcc) {
    states.push_back({node, last_rcc});
    parents.push_back({prev_idx, prev_node, via_rcc});
    int idx = static_cast<int>(states.size()) - 1;
    q.push_back({start, node, last_rcc, depth, idx});
    return idx;
  };

  for (auto it = graph.links.begin(); it != graph.links.end(); ++it) {
    AlsNode start = it->first;
    push_state(start, start, 0, 0, -1, start, 0);
  }

  while (!q.empty()) {
    QueueItem cur = q.front();
    q.pop_front();

    if (cur.depth >= DEFAULT_MAX_DEPTH) {
      continue;
    }

    for (const AlsEdge &nb : graph.links[cur.node]) {
      if (path_contains_node(cur.state_index, nb.to, states, parents)) {
        continue;
      }
      if (config.require_distinct_rcc && cur.last_rcc != 0 && nb.rcc == cur.last_rcc) {
        continue;
      }

      int next_idx = push_state(cur.start,
                                nb.to,
                                nb.rcc,
                                cur.depth + 1,
                                cur.state_index,
                                cur.node,
                                nb.rcc);

      // Look for possible rings
      for (const AlsEdge &nc : graph.links[nb.to]) {
        if (nc.to == cur.start) {
          if (config.require_distinct_rcc && nb.rcc != 0 && nc.rcc == nb.rcc) {
            continue;
          }

          // Ring detected, give priority
          int double_next_idx = push_state(cur.start,
                                           nc.to,
                                           nc.rcc,
                                           cur.depth + 2,
                                           next_idx,
                                           nb.to,
                                           nc.rcc);

          std::optional<Event> event = execute_als_rules(cur.start, nc.to, double_next_idx, states, parents);
          if (event) {
            return event;
          }
          // Restore state as if nothing had happened :)
          states.pop_back();
          parents.pop_back();
          q.pop_back();
        }
      }

      // Look for eliminations according to ALS rules
      std::optional<Event> event = execute_als_rules(cur.start, nb.to, next_idx, states, parents);
      if (event) {
        return event;
      }
    }
  }

  return {};
}

bool AlsSearcher::path_contains_node(int state_idx,
                                     AlsNode node,
                                     const std::vector<AlsSearchState> &states,
                                     const std::vector<AlsParent> &parents) const {
  int idx = state_idx;
  while (idx >= 0) {
    if (states[idx].node == node) {
      return true;
    }
    idx = parents[idx].prev_state_index;
  }
  return false;
}

AlsPath AlsSearcher::reconstruct_path(int end_state_idx,
                                      const std::vector<AlsSearchState> &states,
                                      const std::vector<AlsParent> &parents) const {
  AlsPath p;
  std::vector<FullAlsNode> rev_nodes;
  std::vector<AlsEdge> rev_edges;

  int idx = end_state_idx;
  while (idx >= 0) {
    FullAlsNode node;
    deserialize_unitcode(states[idx].node, node.cellSet, node.digitSet, node.isGrouped);
    
    rev_nodes.push_back(node);
    if (parents[idx].prev_state_index >= 0) {
      rev_edges.push_back({states[idx].node, parents[idx].rcc});
    }
    idx = parents[idx].prev_state_index;
  }

  p.nodes.assign(rev_nodes.rbegin(), rev_nodes.rend());
  p.edges.assign(rev_edges.rbegin(), rev_edges.rend());
  return p;
}

DigitSet AlsSearcher::get_rcc_set(const AlsPath &path) const {
  DigitSet rccs;
  for (const AlsEdge &edge : path.edges) {
    if (edge.rcc != 0) {
      rccs.insert(edge.rcc);
    }
  }
  return rccs;
}

CellSet AlsSearcher::get_common_non_rcc_digits(const AlsPath &path,
                                               const DigitSet &startDigits,
                                               const DigitSet &endDigits) const {
  DigitSet rccs = get_rcc_set(path);
  DigitSet common = startDigits & endDigits;
  DigitSet usable = common - rccs;

  CellSet result;
  for (Digit d : usable) {
    result.insert(static_cast<Cell>(d));
  }
  return result;
}

std::optional<Event> AlsSearcher::build_circular_elimination_event(const AlsPath &path,
                                                                   ReasonId detailedReason) const {
  if (path.nodes.size() < 2) {
    return {};
  }

  Event event(EventType::RemoveCandidate, detailedReason);

  // sources: list of ALSs
  for (size_t i = 0; i < path.nodes.size(); ++i) {
    FullAlsNode node = path.nodes[i];
    event.addSource(node.cellSet, node.digitSet);
  }

  event.addDelimiter();

  // sources: list of RCCs
  for (size_t i = 0; i < path.nodes.size(); ++i) {
    FullAlsNode fromNode = path.nodes[i];

    if (i < path.edges.size()) {
      FullAlsNode toNode;
      Digit RCC = path.edges[i].rcc;
      deserialize_unitcode(path.edges[i].to, toNode.cellSet, toNode.digitSet, toNode.isGrouped);
      event.addSource(board.getPositionsOfDigit(fromNode.cellSet, RCC), RCC);
      event.addSource(board.getPositionsOfDigit(toNode.cellSet, RCC), RCC);
    }
  }

  event.addDelimiter();

  for (size_t i = 0; i < path.nodes.size(); ++i) {
    FullAlsNode prevNode = path.nodes[i == 0 ? path.nodes.size()-1 : i-1];
    FullAlsNode node = path.nodes[i];
    FullAlsNode nextNode = path.nodes[(i+1)%path.nodes.size()];

    // RCC between prev and curr set
    Digit rccA = path.edges[i == 0 ? path.nodes.size()-1 : i-1].rcc;
    CellSet rccAcells = board.getPositionsOfDigit(prevNode.cellSet | node.cellSet, rccA);
    // RCC between curr and next set
    Digit rccB = path.edges[i].rcc;
    CellSet rccBcells = board.getPositionsOfDigit(node.cellSet | nextNode.cellSet, rccB);

    // operations: remove the RCCs from peers
    CellSet victimsA = board.getPeersContaining(rccAcells, rccA);
    for (Cell idx : victimsA) {
      event.addOperation(idx, rccA);
    }
    CellSet victimsB = board.getPeersContaining(rccBcells, rccB);
    for (Cell idx : victimsB) {
      event.addOperation(idx, rccB);
    }

    // operations: remove the non-RCCs from peers
    DigitSet eliminations = node.digitSet - DigitSet({rccA, rccB});
    for (Digit d : eliminations) {
      CellSet sourceSet = board.getPositionsOfDigit(node.cellSet, d);
      CellSet victims = board.getPeersContaining(sourceSet, d);
      for (Cell c : victims) {
        event.addOperation(c, d);
      }
    }
  }

  if (event.getNumberOfOperations() > 0) {
    return event;
  }

  return {};
}

std::optional<Event> AlsSearcher::build_endpoint_elimination_event(const AlsPath &path,
                                                                   Digit z,
                                                                   ReasonId detailedReason) const {
  if (path.nodes.size() < 2) {
    return {};
  }

  CellSet startZ = board.getPositionsOfDigit(path.nodes.front().cellSet, z);
  CellSet endZ = board.getPositionsOfDigit(path.nodes.back().cellSet, z);
  if (startZ.empty() || endZ.empty()) {
    return {};
  }

  CellSet victims = board.getPeersContaining(startZ | endZ, z);

  if (!victims.empty()) {
    Event event(EventType::RemoveCandidate, detailedReason);

    // sources: list of ALSs
    for (size_t i = 0; i < path.nodes.size(); ++i) {
      FullAlsNode node = path.nodes[i];
      event.addSource(node.cellSet, node.digitSet);
    }

    event.addDelimiter();

    // sources: list of RCCs
    for (size_t i = 0; i < path.nodes.size()-1; ++i) {
      FullAlsNode fromNode = path.nodes[i];
      FullAlsNode toNode = path.nodes[i+1];
      Digit RCC = path.edges[i].rcc;

      if (i < path.edges.size()) {
        event.addSource(board.getPositionsOfDigit(fromNode.cellSet, RCC), RCC);
        event.addSource(board.getPositionsOfDigit(toNode.cellSet, RCC), RCC);
      }
    }

    event.addDelimiter();

    // sources: list of Z digits
    event.addSource(startZ, z);
    event.addSource(endZ, z);

    // operation: remove Z from peers
    for (Cell c : victims) {
      event.addOperation(c, z);
    }

    if (event.getNumberOfOperations() > 0) {
      return event;
    }
  }

  return {};
}

std::optional<Event> AlsSearcher::execute_als_rules(
  AlsNode start,
  AlsNode end,
  int end_state_idx,
  const std::vector<AlsSearchState> &states,
  const std::vector<AlsParent> &parents) const
{
  AlsPath path = reconstruct_path(end_state_idx, states, parents);
  if (path.nodes.size() < 2) {
    return {};
  }

  bool grouped;
  CellSet startCells;
  DigitSet startDigits;
  deserialize_unitcode(start, startCells, startDigits, grouped);
  CellSet endCells;
  DigitSet endDigits;
  deserialize_unitcode(end, endCells, endDigits, grouped);

  if (startCells == endCells && startDigits == endDigits) {
    // Ring
    ReasonId detailedReason;// = reason;
    if (path.edges.size() == 2) {
      detailedReason = ReasonId::ALSXZDoublyLinked;
    } else if (path.edges.size() == 3) {
      detailedReason = ReasonId::ALSXYRing;
    } else {
      detailedReason = ReasonId::ALSRing;
    }

    // remove the last node since it is a duplicate of the first one, it simplifies things
    path.nodes.pop_back();

    std::optional<Event> event = build_circular_elimination_event(path, detailedReason);
    if (event) {
      return event;
    }
  } else {
    // Wing
    DigitSet rccs = get_rcc_set(path);
    DigitSet common = startDigits & endDigits;
    DigitSet endpointDigits = common - rccs;

    if (endpointDigits.empty()) {
      return {};
    }

    ReasonId detailedReason; // = reason;
    if (path.edges.size() == 1) {
      detailedReason = ReasonId::ALSXZSinglyLinked;
    } else if (path.edges.size() == 2) {
      detailedReason = ReasonId::ALSXYWing;
    } else {
      detailedReason = ReasonId::ALSChain;
    }

    for (Digit z : endpointDigits) {
      std::optional<Event> event = build_endpoint_elimination_event(path, z, detailedReason);
      if (event) {
        return event;
      }
    }
  }

  return {};
}
