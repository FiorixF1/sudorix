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

void AlsGraphBuilder::build_nodes(AlsGraphNodes &nodes) {
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

void AlsGraphBuilder::build_nodes_in_unit(AlsGraphNodes &nodes, const Unit &unit) {
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

void AlsGraphBuilder::add_node_if_new(AlsGraphNodes &nodes,
                                      const CellSet &cells,
                                      const DigitSet &digits) const {
  AlsNodeID id = get_node_id(digits, cells);
  if (!id) return;
  AlsNode candidate{
    .id = id,
    .cellSet = cells,
    .digitSet = digits,
    .isGrouped = true
  };
  nodes[id] = candidate;
}

AlsNodeID AlsGraphBuilder::get_node_id(Digit digit, Cell cell) const {
  return get_node_id(DigitSet({digit}), CellSet({cell}));
}

AlsNodeID AlsGraphBuilder::get_node_id(Digit digit, const CellSet &cells) const {
  return get_node_id(DigitSet({digit}), cells);
}

AlsNodeID AlsGraphBuilder::get_node_id(const DigitSet &digits, Cell cell) const {
  return get_node_id(digits, CellSet({cell}));
}

AlsNodeID AlsGraphBuilder::get_node_id(const DigitSet &digits, const CellSet &cells) const {
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

void AlsGraphBuilder::build_links(AlsGraphNodes &nodes,
                                  AlsGraphEdges &links) {
  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    AlsNode &A = it->second;
    for (auto ot = nodes.begin(); ot != nodes.end(); ++ot) {
      AlsNode &B = ot->second;

      if (A.id > B.id) continue;

      if (!(A.cellSet & B.cellSet).empty()) {
        continue; // overlapping ALS are normally excluded
      }

      if (board.sees(A.cellSet, B.cellSet)) {
        continue; // do not link ALS that completely see each other
      }

      DigitSet common = A.digitSet & B.digitSet;
      for (Digit d : common) {
        if (is_rcc(A, B, d)) {
          add_edge(links, A.id, B.id, d);
        }
      }
    }
  }

#if 0
  console_log("ALS GRAPH");
  console_log("Number of nodes: %d", nodes.size());
  console_log("Number of edges: %d", links.size());
  for (auto &it : links) {
    AlsNode &from = nodes[it.first];

    auto &edges = it.second;
    for (auto &edge : edges) {
      AlsNode &to = nodes[edge.to];

      std::stringstream ss;
      ss << "Edge from "   << from.cellSet << " with digits " << from.digitSet <<
            " to "         << to.cellSet   << " with digits " << to.digitSet <<
            " with RCC = " << std::to_string(edge.rcc);
      std::string out = ss.str();

      console_log("%s", out.c_str());
    }
  }
#endif
}

bool AlsGraphBuilder::is_rcc(AlsNode &a, AlsNode &b, Digit digit) const {
  if (!a.digitSet.contains(digit) || !b.digitSet.contains(digit)) {
    return false;
  }

  CellSet digitCellsA = board.getPositionsOfDigit(a.cellSet, digit);
  CellSet digitCellsB = board.getPositionsOfDigit(b.cellSet, digit);

  if (digitCellsA.empty() || digitCellsB.empty()) {
    return false;
  }

  return board.sees(digitCellsA, digitCellsB);
}

void AlsGraphBuilder::add_edge(AlsGraphEdges &adj,
                               AlsNodeID a,
                               AlsNodeID b,
                               Digit rcc) {
  if (a == b || rcc == 0) {
    return;
  }

  auto add_one_way = [&](AlsNodeID x, AlsNodeID y) {
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

static void retain(AlsSearchNode *n) {
  if (n) {
    ++n->refcount;
  }
}

static void release(AlsSearchNode *n) {
  while (n) {
    --n->refcount;
    if (n->refcount > 0) {
      return;
    }

    AlsSearchNode *parent = n->parent;
    delete n;
    n = parent;
  }
}

AlsSearchNode *make_node(AlsNodeID start,
                         AlsNodeID node,
                         Digit last_rcc,
                         int depth,
                         AlsSearchNode *parent) {
  AlsSearchNode *s = new AlsSearchNode{
    .start = start,
    .node = node,
    .last_rcc = last_rcc,
    .depth = depth,
    .parent = parent,
    .refcount = 1
  };

  if (parent) {
    retain(parent);
  }

  return s;
}

/* ---------------------------------------------------------------------- */

AlsSearcher::AlsSearcher(const SudokuBoard &board)
  : board(board) {

}

const AlsConfig &AlsSearcher::setConfigAndReturn(ReasonId reason) {
  config = {};

  switch(reason) {
    case ReasonId::ALSXZ:
      config.max_depth = 1;
      break;
    case ReasonId::ALSXYWing:
      config.max_depth = 2;
      break;
    case ReasonId::ALSChain:
      config.max_depth = DEFAULT_MAX_DEPTH;
      break;
    default:
      config.max_depth = DEFAULT_MAX_DEPTH;
      break;
  }

  this->reason = reason;

  return config;
}

std::optional<Event> AlsSearcher::runSearch(AlsGraph &graph) {
  std::optional<Event> maybeEvent;
  maybeEvent = als_search_from(graph);

  if (maybeEvent) {
    return maybeEvent;
  }

  return {};
}

std::optional<Event> AlsSearcher::als_search_from(AlsGraph &graph) {
  std::deque<AlsSearchNode *> q;

  for (auto it = graph.links.begin(); it != graph.links.end(); ++it) {
    AlsNodeID start = it->first;
    q.push_back(make_node(start, start, 0, 0, nullptr));
  }

  while (!q.empty()) {
    AlsSearchNode *cur = q.front();
    q.pop_front();

    if (cur->depth > config.max_depth) {
      release(cur);
      continue;
    }

    for (const AlsEdge &nb : graph.links[cur->node]) {
      bool IS_RING = (nb.to == cur->start);

      // only with rings one more level is allowed
      if (!IS_RING && cur->depth >= config.max_depth) {
        continue;
      }

      if (path_contains_node(cur->start, cur, nb.to)) {
        continue;
      }
      if (cur->last_rcc != 0 && nb.rcc == cur->last_rcc) {
        continue;
      }

      AlsSearchNode *child = make_node(cur->start,
                                       nb.to,
                                       nb.rcc,
                                       cur->depth + 1,
                                       cur);

      // Look for eliminations according to ALS rules
      std::optional<Event> event = execute_als_rules(graph, cur->start, nb.to, child);
      if (event) {
        release(child);

        while (!q.empty()) {
          release(q.front());
          q.pop_front();
        }

        release(cur);
        return event;
      }

      q.push_back(child);
    }

    release(cur);
  }

  return {};
}

bool AlsSearcher::path_contains_node(AlsNodeID start, AlsSearchNode *cur, AlsNodeID node) const {
  // allow rings
  if (node == start) {
    return false;
  }

  while (cur) {
    if (cur->node == node) {
      return true;
    }
    cur = cur->parent;
  }
  return false;
}

AlsPath AlsSearcher::reconstruct_path(AlsGraph &graph, AlsSearchNode *end) const {
  AlsPath p;
  std::vector<AlsNode> rev_nodes;
  std::vector<AlsEdge> rev_edges;

  AlsSearchNode *cur = end;
  while (cur) {
    rev_nodes.push_back(graph.nodes[cur->node]);

    if (cur->parent) {
      rev_edges.push_back({cur->node, cur->last_rcc});
    }

    cur = cur->parent;
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

std::optional<Event> AlsSearcher::build_circular_elimination_event(AlsPath &path,
                                                                   ReasonId detailedReason) const {
  if (path.nodes.size() < 2) {
    return {};
  }

  Event event(EventType::RemoveCandidate, detailedReason);

  // sources: list of ALSs
  for (size_t i = 0; i < path.nodes.size(); ++i) {
    AlsNode &node = path.nodes[i];
    event.addSource(node.cellSet, node.digitSet);
  }

  event.addDelimiter();

  // sources: list of RCCs
  for (size_t i = 0; i < path.nodes.size(); ++i) {
    AlsNode &fromNode = path.nodes[i];

    if (i < path.edges.size()) {
      AlsNode toNode;
      Digit RCC = path.edges[i].rcc;
      deserialize_unitcode(path.edges[i].to, toNode.cellSet, toNode.digitSet, toNode.isGrouped);
      event.addSource(board.getPositionsOfDigit(fromNode.cellSet, RCC), RCC);
      event.addSource(board.getPositionsOfDigit(toNode.cellSet, RCC), RCC);
      
      //AlsNode &toNode = path.nodes[(i+1)%path.nodes.size()];
      //Digit RCC = path.edges[i].rcc;
      //event.addSource(board.getPositionsOfDigit(fromNode.cellSet, RCC), RCC);
      //event.addSource(board.getPositionsOfDigit(toNode.cellSet, RCC), RCC);
    }
  }

  event.addDelimiter();

  for (size_t i = 0; i < path.nodes.size(); ++i) {
    AlsNode &prevNode = path.nodes[i == 0 ? path.nodes.size()-1 : i-1];
    AlsNode &node = path.nodes[i];
    AlsNode &nextNode = path.nodes[(i+1)%path.nodes.size()];

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

std::optional<Event> AlsSearcher::build_endpoint_elimination_event(AlsPath &path,
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
      AlsNode &node = path.nodes[i];
      event.addSource(node.cellSet, node.digitSet);
    }

    event.addDelimiter();

    // sources: list of RCCs
    for (size_t i = 0; i < path.nodes.size()-1; ++i) {
      AlsNode &fromNode = path.nodes[i];
      AlsNode &toNode = path.nodes[i+1];
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

std::optional<Event> AlsSearcher::execute_als_rules(AlsGraph &graph,
                                                    AlsNodeID start,
                                                    AlsNodeID end,
                                                    AlsSearchNode *end_state) const {
  AlsPath path = reconstruct_path(graph, end_state);
  if (path.nodes.size() < 2) {
    return {};
  }

  AlsNode &Start = graph.nodes[start];
  AlsNode &End = graph.nodes[end];

  if (Start.cellSet == End.cellSet && Start.digitSet == End.digitSet) {
    // Ring
    ReasonId detailedReason;
    if (path.edges.size() == 2) {
      detailedReason = ReasonId::ALSXZDoublyLinked;
    } else if (path.edges.size() == 3) {
      detailedReason = ReasonId::ALSXYRing;
    } else {
      detailedReason = ReasonId::ALSRing;
    }

    // ensure that the ring is not starting and finishing on the same RCC
    if ((*path.edges.begin()).rcc == (*path.edges.rbegin()).rcc) {
      return {};
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
    DigitSet common = Start.digitSet & End.digitSet;
    DigitSet endpointDigits = common - rccs;

    if (endpointDigits.empty()) {
      return {};
    }

    ReasonId detailedReason;
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
