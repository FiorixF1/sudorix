#include "AIC.hpp"

AicGraphBuilder::AicGraphBuilder(const SudokuBoard &board) : board(board) { }

AicGraph AicGraphBuilder::build() {
  AicGraph g;

  build_singleton_nodes(g.nodes);
  //build_grouped_nodes(g.nodes);

  build_strong_links(g.nodes, g.strong_links);

  return g;
}

AicGraph AicGraphBuilder::prune(const AicGraph &graph, const AicConfig &config) {
  AicGraph prunedGraph;

  // build edges according to config
  for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
    AicNode source = it->first;

    bool isGrouped;
    CellSet cellSet;
    DigitSet digitSet;
    deserialize_unitcode(source, cellSet, digitSet, isGrouped);
    if (isGrouped && !config.useGroupedCells) {
      continue;
    }
    if (isGrouped && !config.useWeakLinks) {
      // groups are not allowed in coloring techniques
      continue;
    }
    // be careful: graph nodes have only one digit, use SudokuBoard to get the digits of the pair
    DigitSet remotePair = board.getCandidates(*cellSet.begin());
    if (config.useRemotePairs && (isGrouped || remotePair.size() != 2)) {
      continue;
    }

    auto &edges = it->second;
    for (AicNode target : edges) {
      bool targetIsGrouped;
      CellSet targetCellSet;
      DigitSet targetDigitSet;
      deserialize_unitcode(target, targetCellSet, targetDigitSet, targetIsGrouped);
      if (targetIsGrouped && !config.useGroupedCells) {
        continue;
      }
      if (targetIsGrouped && !config.useWeakLinks) {
        // groups are not allowed in coloring techniques
        continue;
      }
      if (targetDigitSet != digitSet && (!config.multiDigit || !config.useStrongBivalues)) {
        continue;
      }
      if (targetCellSet != cellSet && !config.useStrongBilocations) {
        continue;
      }
      // be careful: graph nodes have only one digit, use SudokuBoard to get the digits of the pair
      // however ensure that the single digit between two nodes is the same
      DigitSet targetRemotePair = board.getCandidates(*targetCellSet.begin());
      if (config.useRemotePairs && (targetIsGrouped || targetRemotePair != remotePair || targetDigitSet != digitSet)) {
        continue;
      }
      add_strong_edge(prunedGraph.strong_links, source, target);
    }
  }

  // build nodes from filtered edges
  for (auto it = prunedGraph.strong_links.begin(); it != prunedGraph.strong_links.end(); ++it) {
    prunedGraph.nodes.push_back(it->first);
  }

#ifdef DEBUG
  console_log("AIC GRAPH");
  for (auto &it : prunedGraph.strong_links) {
    AicNode fromID = it.first;
    auto &edges = it.second;

    CellSet fromCellSet;
    DigitSet fromDigitSet;
    bool isGrouped;
    deserialize_unitcode(fromID, fromCellSet, fromDigitSet, isGrouped);
    Cell fromCell = *fromCellSet.begin();
    Digit fromDigit = *fromDigitSet.begin();
    if (isGrouped) continue;  // debug only singletons

    for (auto &toID : edges) {
      CellSet toCellSet;
      DigitSet toDigitSet;
      bool isGrouped;
      deserialize_unitcode(toID, toCellSet, toDigitSet, isGrouped);
      Cell toCell = *toCellSet.begin();
      Digit toDigit = *toDigitSet.begin();
      if (isGrouped) continue;  // debug only singletons

      console_log("Edge from r%dc%d (%d) to r%dc%d (%d)", SudokuBoard::getRowLocation(fromCell)+1,
                                                          SudokuBoard::getColumnLocation(fromCell)+1,
                                                          fromDigit,
                                                          SudokuBoard::getRowLocation(toCell)+1,
                                                          SudokuBoard::getColumnLocation(toCell)+1,
                                                          toDigit);
    }
  }
#endif

  return prunedGraph;
}

void AicGraphBuilder::build_singleton_nodes(std::vector<AicNode> &nodes) {
  for (Cell cell = 0; cell < 81; ++cell) {
    for (Digit d = 1; d <= 9; ++d) {
      if (board.isSolved(cell) || !board.hasCandidate(cell, d)) {
        continue;
      }

      AicNode n = get_node_id(d, cell);
      nodes.push_back(n);
    }
  }
}

void AicGraphBuilder::add_group_if_new(std::vector<AicNode> &nodes, Digit digit, const CellSet &cells) {
  if (cells.size() < 2) {
    return;
  }

  AicNode candidate = get_node_id(digit, cells);

  // TODO: questo controllo effettivamente serve? Da vedere
  /*for (const auto &n : nodes) {
    if (same_node(n, candidate)) {
      return;
    }
  }*/

  nodes.push_back(candidate);
}

void AicGraphBuilder::build_grouped_nodes(std::vector<AicNode> &nodes) {
  // grouped da intersezione row∩box, col∩box, box∩row, box∩col
  for (Digit d = 1; d <= 9; ++d) {
    // row ∩ box
    /*for (const Unit &r : SudokuBoard::getRows()) {
      CellSet pos = board.getPositionsOfDigit(r, d);
      for (const Unit &b : SudokuBoard::getBoxes()) {
        CellSet g = pos & b;
        add_group_if_new(nodes, d, g);
      }
    }

    // col ∩ box
    for (const Unit &c : SudokuBoard::getColumns()) {
      CellSet pos = board.getPositionsOfDigit(c, d);
      for (const Unit &b : SudokuBoard::getBoxes()) {
        CellSet g = pos & b;
        add_group_if_new(nodes, d, g);
      }
    }*/

    // mi sa che questi due for qua sopra non servono a niente, quello qui sotto basta

    // box ∩ row / box ∩ col
    for (const Unit &b : SudokuBoard::getBoxes()) {
      CellSet pos = board.getPositionsOfDigit(b, d);
      for (const Unit &r : SudokuBoard::getRows()) {
        CellSet g = pos & r;
        add_group_if_new(nodes, d, g);
      }
      for (const Unit &c : SudokuBoard::getColumns()) {
        CellSet g = pos & c;
        add_group_if_new(nodes, d, g);
      }
    }
  }
}

AicNode AicGraphBuilder::get_node_id(Digit digit, Cell cell) const {
  return get_node_id(DigitSet({digit}), CellSet({cell}));
}

AicNode AicGraphBuilder::get_node_id(Digit digit, const CellSet &cells) const {
  return get_node_id(DigitSet({digit}), cells);
}

AicNode AicGraphBuilder::get_node_id(const DigitSet &digits, Cell cell) const {
  return get_node_id(digits, CellSet({cell}));
}

AicNode AicGraphBuilder::get_node_id(const DigitSet &digits, const CellSet &cells) const {
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

void AicGraphBuilder::add_strong_edge(std::map<AicNode, std::vector<AicNode>> &adj, AicNode a, AicNode b) {
  if (a == b) {
    return;
  }

  auto add_one_way = [&](AicNode x, AicNode y) {
    auto &v = adj[x];
    if (std::find(v.begin(), v.end(), y) == v.end()) {
      v.push_back(y);
    }
  };

  add_one_way(a, b);
  add_one_way(b, a);
}

void AicGraphBuilder::build_strong_links(const std::vector<AicNode> &nodes,
                                         std::map<AicNode, std::vector<AicNode>> &strong_links) {
  // Bilocations
  for (Digit d = 1; d <= 9; ++d) {
    build_strong_links_in_units(nodes, strong_links, SudokuBoard::getRows(), d);
    build_strong_links_in_units(nodes, strong_links, SudokuBoard::getColumns(), d);
    build_strong_links_in_units(nodes, strong_links, SudokuBoard::getBoxes(), d);
  }

  // Bivalues
  for (Cell cell = 0; cell < 81; ++cell) {
    build_strong_links_in_cells(nodes, strong_links, cell);
  }

  // TODO: ho il dubbio che queste funzioni trovino già i nodi di tipo bilocation fra singleton
  // Grouped cells (including empty rectangle intersection)
  /*for (Digit d = 1; d <= 9; ++d) {
    // singletons and groups in a row
    build_grouped_strong_row_box(nodes, strong_links, d);
    // singletons and groups in a column
    build_grouped_strong_col_box(nodes, strong_links, d);
    // minirows in a box
    build_grouped_strong_box_row(nodes, strong_links, d);
    // minicolumns in a box
    build_grouped_strong_box_col(nodes, strong_links, d);
    // empty rectangle intersection
    build_grouped_strong_eri(nodes, strong_links, d);
  }*/
}

void AicGraphBuilder::build_strong_links_in_units(const std::vector<AicNode> &nodes,
                                                  std::map<AicNode, std::vector<AicNode>> &strong_links,
                                                  const std::vector<Unit> &units,
                                                  Digit d) {
  for (const Unit &unit : units) {
    CellSet pos = board.getPositionsOfDigit(unit, d);
    if (pos.size() != 2) {
      continue;
    }

    std::vector<int> v = pos.to_vector();
    CellSet c1 = CellSet({v[0]});
    CellSet c2 = CellSet({v[1]});

    AicNode n1 = get_node_id(d, c1);
    AicNode n2 = get_node_id(d, c2);
    if (n1 && n2) add_strong_edge(strong_links, n1, n2);
  }
}

void AicGraphBuilder::build_strong_links_in_cells(const std::vector<AicNode> &nodes,
                                                  std::map<AicNode, std::vector<AicNode>> &strong_links,
                                                  Cell cell) {
  DigitSet candidates = board.getCandidates(cell);
  if (candidates.size() != 2) {
    return;
  }

  std::vector<int> v = candidates.to_vector();
  Digit d1 = v[0];
  Digit d2 = v[1];

  AicNode n1 = get_node_id(d1, cell);
  AicNode n2 = get_node_id(d2, cell);
  if (n1 && n2) add_strong_edge(strong_links, n1, n2);
}

void AicGraphBuilder::build_grouped_strong_row_box(const std::vector<AicNode> &nodes,
                                                   std::map<AicNode, std::vector<AicNode>> &strong_links,
                                                   Digit d) {
  for (const Unit &r : SudokuBoard::getRows()) {
    CellSet pos = board.getPositionsOfDigit(r, d);
    if (pos.size() < 2) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &b : SudokuBoard::getBoxes()) {
      CellSet g = pos & b;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNode a = get_node_id(d, parts[0]);
      AicNode b = get_node_id(d, parts[1]);
      if (a && b) add_strong_edge(strong_links, a, b);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_col_box(const std::vector<AicNode> &nodes,
                                                   std::map<AicNode, std::vector<AicNode>> &strong_links,
                                                   Digit d) {
  for (const Unit &c : SudokuBoard::getColumns()) {
    CellSet pos = board.getPositionsOfDigit(c, d);
    if (pos.size() < 2) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &b : SudokuBoard::getBoxes()) {
      CellSet g = pos & b;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNode a = get_node_id(d, parts[0]);
      AicNode b = get_node_id(d, parts[1]);
      if (a && b) add_strong_edge(strong_links, a, b);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_box_row(const std::vector<AicNode> &nodes,
                                                   std::map<AicNode, std::vector<AicNode>> &strong_links,
                                                   Digit d) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, d);
    if (pos.size() < 2) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &r : SudokuBoard::getRows()) {
      CellSet g = pos & r;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNode a = get_node_id(d, parts[0]);
      AicNode bb = get_node_id(d, parts[1]);
      if (a && bb) add_strong_edge(strong_links, a, bb);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_box_col(const std::vector<AicNode> &nodes,
                                                   std::map<AicNode, std::vector<AicNode>> &strong_links,
                                                   Digit d) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, d);
    if (pos.size() < 2) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &c : SudokuBoard::getColumns()) {
      CellSet g = pos & c;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNode a = get_node_id(d, parts[0]);
      AicNode bb = get_node_id(d, parts[1]);
      if (a && bb) add_strong_edge(strong_links, a, bb);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_eri(const std::vector<AicNode> &nodes,
                                               std::map<AicNode, std::vector<AicNode>> &strong_links,
                                               Digit d) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, d);
    if (pos.size() < 2) {
      continue;
    }

    std::vector<CellSet> row_parts;
    for (const Unit &r : SudokuBoard::getRows()) {
      CellSet g = pos & r;
      if (g.size() > 1) row_parts.push_back(g);
    }

    std::vector<CellSet> col_parts;
    for (const Unit &c : SudokuBoard::getColumns()) {
      CellSet g = pos & c;
      if (g.size() > 1) col_parts.push_back(g);
    }

    if (row_parts.size() == 1 && col_parts.size() == 1) {
      AicNode r = get_node_id(d, row_parts[0]);
      AicNode c = get_node_id(d, col_parts[0]);
      if (r && c) add_strong_edge(strong_links, r, c);
    }
  }
}

/* ---------------------------------------------------------------------- */

AicSearcher::AicSearcher(const SudokuBoard &board)
  : board(board) {

}

const AicConfig &AicSearcher::setConfigAndReturn(ReasonId reason) {
  switch(reason) {
    case ReasonId::RemotePair:
      config = {
        .useWeakLinks = false,
        .multiDigit = false,
        .useGroupedCells = false,
        .useStrongBivalues = false,
        .useStrongBilocations = true,
        .useWeakInCell = false,
        .useWeakInUnit = true,
        .useRemotePairs = true,
      };
      break;
    case ReasonId::SingleDigitPattern:
      config = {
        .useWeakLinks = true,
        .multiDigit = false,
        .useGroupedCells = false,
        .useStrongBivalues = false,
        .useStrongBilocations = true,
        .useWeakInCell = false,
        .useWeakInUnit = true,
        .useRemotePairs = false,
        .max_depth = 3,
      };
      break;
    case ReasonId::SimpleColoring:
      config = {
        .useWeakLinks = false,
        .multiDigit = false,
        .useGroupedCells = false,
        .useStrongBivalues = false,
        .useStrongBilocations = true,
        .useWeakInCell = false,
        .useWeakInUnit = false,
        .useRemotePairs = false,
      };
      break;
    case ReasonId::_3DMedusa:
      config = {
        .useWeakLinks = false,
        .multiDigit = true,
        .useGroupedCells = false,
        .useStrongBivalues = true,
        .useStrongBilocations = true,
        .useWeakInCell = false,
        .useWeakInUnit = false,
        .useRemotePairs = false,
      };
      break;
    case ReasonId::XChain:
      config = {
        .useWeakLinks = true,
        .multiDigit = false,
        .useGroupedCells = false,
        .useStrongBivalues = false,
        .useStrongBilocations = true,
        .useWeakInCell = false,
        .useWeakInUnit = true,
        .useRemotePairs = false,
        .max_depth = 9,
      };
      break;
    case ReasonId::XYChain:
      config = {
        .useWeakLinks = true,
        .multiDigit = true,
        .useGroupedCells = false,
        .useStrongBivalues = true,
        .useStrongBilocations = false,
        .useWeakInCell = false,
        .useWeakInUnit = true,
        .useRemotePairs = false,
        .max_depth = 9,
      };
      break;
    case ReasonId::AIC:
      config = {
        .useWeakLinks = true,
        .multiDigit = true,
        .useGroupedCells = false,
        .useStrongBivalues = true,
        .useStrongBilocations = true,
        .useWeakInCell = true,
        .useWeakInUnit = true,
        .useRemotePairs = false,
        .max_depth = 9,
      };
      break;
    default:
      break;
  }

  this->reason = reason;

  return config;
}

std::optional<Event> AicSearcher::run_search(AicGraph &graph) {
  visited.clear();

  if (config.useWeakLinks) {
    // generic AIC search
    std::optional<Event> maybeEvent;
    maybeEvent = aic_search_from(graph);
    if (maybeEvent) {
      Event &event = *maybeEvent;
      if (event.reason == ReasonId::SingleDigitPattern) {
        // identify the specific type of single digit pattern
        auto &sources = event.getSources();

        Cell a = *sources[0].cells.begin();
        Cell b = *sources[1].cells.begin();
        Cell c = *sources[2].cells.begin();
        Cell d = *sources[3].cells.begin();

        Location aRow = board.getRowLocation(a);
        Location bRow = board.getRowLocation(b);
        Location cRow = board.getRowLocation(c);
        Location dRow = board.getRowLocation(d);

        Location aCol = board.getColumnLocation(a);
        Location bCol = board.getColumnLocation(b);
        Location cCol = board.getColumnLocation(c);
        Location dCol = board.getColumnLocation(d);

        Location aBox = board.getBoxLocation(a);
        Location bBox = board.getBoxLocation(b);
        Location cBox = board.getBoxLocation(c);
        Location dBox = board.getBoxLocation(d);

        if ((aRow == bRow && cRow == dRow) ||
            (aCol == bCol && cCol == dCol)) {
          event.reason = ReasonId::Skyscraper;
        } else if ((aRow == bRow && cCol == dCol) ||
                   (aCol == bCol && cRow == dRow)) {
          event.reason = ReasonId::TwoStringKite;
        } else if ((aRow == bRow && bCol == cCol) ||
                   (aCol == bCol && bRow == cRow) ||
                   (bRow == cRow && cCol == dCol) ||
                   (bCol == cCol && cRow == dRow)) {
          event.reason = ReasonId::Crane;
        }
      }
      return maybeEvent;
    }
  } else {
    // color-based search, only strong links
    for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
      AicNode start = it->first;

      std::optional<Event> maybeEvent;
      maybeEvent = coloring_search_from(start, graph);

      if (maybeEvent) {
        return maybeEvent;
      }
    }
  }

  return {};
}

std::optional<Event> AicSearcher::aic_search_from(AicGraph &graph) {
  struct QueueItem {
    AicNode start;
    AicNode node;
    EdgeType next_type;
    int depth;
    int state_index;
  };

  std::vector<AicSearchState> states;
  std::vector<AicParent> parents;
  std::deque<QueueItem> q;

  auto push_state = [&](AicNode start, AicNode node, EdgeType next_type, int depth, int prev_idx, AicNode prev_node, EdgeType edge_used) {
    AicSearchState st{node, .next_type = next_type};
    states.push_back(st);
    parents.push_back({prev_idx, prev_node, edge_used});
    int idx = static_cast<int>(states.size()) - 1;
    q.push_back({start, node, next_type, depth, idx});
    return idx;
  };

  // Partiamo dal nodo iniziale e imponiamo che il primo arco sia strong.
  for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
    AicNode start = it->first;
    push_state(start, start, EdgeType::STRONG, 0, -1, start, EdgeType::STRONG);
  }

  while (!q.empty()) {
    QueueItem cur = q.front();
    q.pop_front();

    if (cur.depth >= config.max_depth) {
      continue;
    }

#ifdef DEBUG
    bool isGrouped;
    CellSet cellSet;
    DigitSet digitSet;
    deserialize_unitcode(cur.node, cellSet, digitSet, isGrouped);
    if (!isGrouped) {
      Cell cell = *cellSet.begin();
      Digit digit = *digitSet.begin();
      console_log("CURRENT STATE: r%dc%d (%d) - %s LINK - Length %d", SudokuBoard::getRowLocation(cell)+1,
                                                                      SudokuBoard::getColumnLocation(cell)+1,
                                                                      digit,
                                                                      cur.next_type == EdgeType::STRONG ? "STRONG" : "WEAK",
                                                                      cur.depth);
    }
#endif
    if (cur.next_type == EdgeType::STRONG) {
      for (AicNode nb : graph.strong_links[cur.node]) {
        if (path_contains_node(cur.state_index, nb, states, parents)) {
          continue;
        }

        int next_idx = push_state(cur.start, nb, EdgeType::WEAK, cur.depth + 1,
                                  cur.state_index, cur.node, EdgeType::STRONG);

        // Look for eliminations according to AIC rules
        std::optional<Event> event = execute_aic_rules(cur.start, nb, next_idx, states, parents);
        if (event) {
          return event;
        }
      }
    } else {
      for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
        AicNode nb = it->first;
        if (nb == cur.node) {
          continue;
        }
        if (path_contains_node(cur.state_index, nb, states, parents)) {
          continue;
        }
        if (!are_weakly_linked(cur.node, nb)) {
          continue;
        }

        push_state(cur.start, nb, EdgeType::STRONG, cur.depth + 1,
                   cur.state_index, cur.node, EdgeType::WEAK);
      }
    }
  }

  return {};
}

std::optional<Event> AicSearcher::coloring_search_from(AicNode start, AicGraph &graph) {
  struct QueueItem {
    AicNode node;
    ColorType next_color;
    int depth;
    int state_index;
  };

  std::vector<AicSearchState> states;
  std::deque<QueueItem> q;

  auto push_state = [&](AicNode node, ColorType next_color, int depth, int prev_idx, AicNode prev_node, ColorType color_used) {
    AicSearchState st{node, .next_color = next_color};
    states.push_back(st);
    int idx = static_cast<int>(states.size()) - 1;
    q.push_back({node, next_color, depth, idx});
    return idx;
  };

  // Partiamo dal nodo iniziale e imponiamo il primo colore.
  push_state(start, ColorType::SECOND, 0, -1, start, ColorType::FIRST);

  while (!q.empty()) {
    QueueItem cur = q.front();
    q.pop_front();

    if (visited.find(cur.node) != visited.end()) {
      continue;
    }
    visited.insert(cur.node);

#ifdef DEBUG
    bool isGrouped;
    CellSet cellSet;
    DigitSet digitSet;
    deserialize_unitcode(cur.node, cellSet, digitSet, isGrouped);

    Cell cell = *cellSet.begin();
    Digit digit = *digitSet.begin();

    if (!isGrouped) {
      console_log("CURRENT STATE: r%dc%d (%d) - %s COLOR - Length %d", SudokuBoard::getRowLocation(cell)+1,
                                                                       SudokuBoard::getColumnLocation(cell)+1,
                                                                       digit,
                                                                       cur.next_color == ColorType::FIRST ? "FIRST" : "SECOND",
                                                                       cur.depth);
    }
#endif
    for (AicNode nb : graph.strong_links[cur.node]) {
      ColorType color_used = cur.next_color;
      ColorType next_color = color_used == ColorType::FIRST ? ColorType::SECOND : ColorType::FIRST;

      int next_idx = push_state(nb, next_color, cur.depth + 1,
                                cur.state_index, cur.node, color_used);
    }
  }

  // Look for contradictions among colors
  std::optional<Event> event = execute_coloring_rules(start, states);

  return event;
}

bool AicSearcher::path_contains_node(int state_idx,
                                     AicNode node,
                                     const std::vector<AicSearchState> &states,
                                     const std::vector<AicParent> &parents) const {
  int idx = state_idx;
  while (idx >= 0) {
    if (states[idx].node == node) {
      return true;
    }
    idx = parents[idx].prev_state_index;
  }
  return false;
}

AicPath AicSearcher::reconstruct_path(int end_state_idx,
                                      const std::vector<AicSearchState> &states,
                                      const std::vector<AicParent> &parents) const {
  AicPath p;
  std::vector<AicNode> rev_nodes;
  std::vector<EdgeType> rev_edges;

  int idx = end_state_idx;
  while (idx >= 0) {
    rev_nodes.push_back(states[idx].node);
    if (parents[idx].prev_state_index >= 0) {
      rev_edges.push_back(parents[idx].edge_used);
    }
    idx = parents[idx].prev_state_index;
  }

  p.nodes.assign(rev_nodes.rbegin(), rev_nodes.rend());
  p.edges.assign(rev_edges.rbegin(), rev_edges.rend());
  return p;
}

std::optional<Event> AicSearcher::execute_aic_rules(
  AicNode start,
  AicNode end,
  int end_state_idx,
  const std::vector<AicSearchState> &states,
  const std::vector<AicParent> &parents) const
{
  CellSet startCellSet;
  DigitSet startDigitSet;
  bool startIsGrouped;
  deserialize_unitcode(start, startCellSet, startDigitSet, startIsGrouped);

  CellSet endCellSet;
  DigitSet endDigitSet;
  bool endIsGrouped;
  deserialize_unitcode(end, endCellSet, endDigitSet, endIsGrouped);

  Digit start_digit = *startDigitSet.begin();
  Digit end_digit = *endDigitSet.begin();

  // Per adesso non facciamo chain con gruppi.
  if (startIsGrouped || endIsGrouped) {
    return {};
  }

  Cell start_cell = *startCellSet.begin();
  Cell end_cell = *endCellSet.begin();

  // AIC Type 1
  if (start_digit == end_digit && !board.sees(start_cell, end_cell)) {
    CellSet peers = board.getPeers({start_cell, end_cell});
    AicPath path = reconstruct_path(end_state_idx, states, parents);

    Event event(EventType::RemoveCandidate, reason == ReasonId::AIC ? ReasonId::AICType1 : reason);
    for (int i = 0; i < path.nodes.size(); ++i) {
      AicNode node = path.nodes[i];
      CellSet cellSet;
      DigitSet digitSet;
      bool isGrouped;
      deserialize_unitcode(node, cellSet, digitSet, isGrouped);
      event.addSource(cellSet, digitSet);
    }

    for (Cell idx : peers) {
      if (board.hasCandidate(idx, start_digit)) {
        event.addOperation(idx, start_digit);
      }
    }

    if (event.getNumberOfOperations() > 0) {
      return event;
    }
  }

  // AIC Type 2
  if (start_digit != end_digit && board.sees(start_cell, end_cell)) {
    AicPath path = reconstruct_path(end_state_idx, states, parents);

    if (board.hasCandidate(start_cell, end_digit) || board.hasCandidate(end_cell, start_digit)) {
      Event event(EventType::RemoveCandidate, reason == ReasonId::AIC ? ReasonId::AICType2 : reason);
      for (int i = 0; i < path.nodes.size(); ++i) {
        AicNode node = path.nodes[i];
        CellSet cellSet;
        DigitSet digitSet;
        bool isGrouped;
        deserialize_unitcode(node, cellSet, digitSet, isGrouped);
        event.addSource(cellSet, digitSet);
      }
      if (board.hasCandidate(start_cell, end_digit)) {
        event.addOperation(start_cell, end_digit);
      }
      if (board.hasCandidate(end_cell, start_digit)) {
        event.addOperation(end_cell, start_digit);
      }
      return event;
    }
  }

  // AIC Type 3 (Ring)
  if (are_weakly_linked(start, end)) {
    AicPath path = reconstruct_path(end_state_idx, states, parents);

    Event event(EventType::RemoveCandidate, reason == ReasonId::XChain ? ReasonId::XCycle :
                                            reason == ReasonId::XYChain ? ReasonId::XYCycle :
                                            reason == ReasonId::AIC ? ReasonId::AICType3 :
                                            reason);
    for (int i = 0; i < path.nodes.size(); ++i) {
      AicNode node = path.nodes[i];
      CellSet cellSet;
      DigitSet digitSet;
      bool isGrouped;
      deserialize_unitcode(node, cellSet, digitSet, isGrouped);
      event.addSource(cellSet, digitSet);
    }
    {
      // add again the first node since this is a ring
      AicNode node = path.nodes[0];
      CellSet cellSet;
      DigitSet digitSet;
      bool isGrouped;
      deserialize_unitcode(node, cellSet, digitSet, isGrouped);
      event.addSource(cellSet, digitSet);
    }

    for (int i = 0; i < path.nodes.size()-1; ++i) {
      AicNode node = path.nodes[i];
      CellSet cellSet;
      DigitSet digitSet;
      bool isGrouped;
      deserialize_unitcode(node, cellSet, digitSet, isGrouped);
      AicNode nextNode = path.nodes[i+1];
      CellSet nextCellSet;
      DigitSet nextDigitSet;
      bool nextIsGrouped;
      deserialize_unitcode(nextNode, nextCellSet, nextDigitSet, nextIsGrouped);

      if (cellSet != nextCellSet) {
        // different cell, same digit
        CellSet peers = board.getPeers(cellSet | nextCellSet);
        Digit digit = *digitSet.begin();
        for (Cell idx : peers) {
          if (board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
      }
      if (cellSet == nextCellSet) {
        // same cell, different digit
        Cell cell = *cellSet.begin();
        Digit digit = *digitSet.begin();
        Digit nextDigit = *nextDigitSet.begin();
        DigitSet toRemove = board.getCandidates(cell) - digitSet - nextDigitSet;
        if (!toRemove.empty()) {
          event.addOperation(cell, toRemove);
        }
      }
    }

    if (event.getNumberOfOperations() > 0) {
      return event;
    }
  }

  return {};
}

std::optional<Event> AicSearcher::execute_coloring_rules(
  AicNode start,
  const std::vector<AicSearchState> &states) const
{
  struct ColorNode {
    Cell cell;
    Digit digit;
    AicPath reason;
  };
  std::vector<ColorNode> firstColorNodes;
  std::vector<ColorNode> secondColorNodes;

  // divide nodes in two lists by color
  for (auto &state : states) {
    bool isGrouped;
    CellSet cellSet;
    DigitSet digitSet;
    deserialize_unitcode(state.node, cellSet, digitSet, isGrouped);

    ColorNode node{
      .cell = static_cast<Cell>(*cellSet.begin()),
      .digit = static_cast<Digit>(*digitSet.begin())
    };

    // the state contains the next color, so the current color is the other one
    if (state.next_color == ColorType::SECOND) {
      firstColorNodes.push_back(node);
    }
    if (state.next_color == ColorType::FIRST) {
      secondColorNodes.push_back(node);
    }
  }

  // Special case for Remote Pair
  if (reason == ReasonId::RemotePair) {
    DigitSet remotePair;
    Event event(EventType::RemoveCandidate, reason);
    for (int i = 0; i < firstColorNodes.size(); ++i) {
      for (int j = 0; j < secondColorNodes.size(); ++j) {
        ColorNode &a = firstColorNodes[i];
        ColorNode &b = secondColorNodes[j];
        
        // be careful: graph nodes have only one digit, use SudokuBoard to get the digits of the pair
        remotePair = board.getCandidates(a.cell);
        Digit x = remotePair.to_vector()[0];
        Digit y = remotePair.to_vector()[1];
        CellSet toRemove = board.getPeersContaining({a.cell, b.cell}, x) | board.getPeersContaining({a.cell, b.cell}, y);
        if (!toRemove.empty()) {
          for (Cell idx : toRemove) {
            event.addOperation(idx, remotePair);
          }
        }
      }
    }
    if (event.getNumberOfOperations() > 0) {
      // add sources first || second and return event
      for (int i = 0; i < firstColorNodes.size(); ++i) {
        ColorNode node = firstColorNodes[i];
        event.addSource(node.cell, remotePair);
      }
      event.addDelimiter();
      for (int i = 0; i < secondColorNodes.size(); ++i) {
        ColorNode node = secondColorNodes[i];
        event.addSource(node.cell, remotePair);
      }
      return event;
    }
    return {};
  }

  // Color Trap test
  auto scanColor = [&](const std::vector<ColorNode> &nodes, const std::vector<ColorNode> &other) -> std::optional<Event>
  {
    bool found = false;
    ReasonId detailedReason;
    Cell emptiedCellIdx = -1;
    for (int i = 0; i < nodes.size(); ++i) {
      for (int j = i+1; j < nodes.size(); ++j) {
        const ColorNode &a = nodes[i];
        const ColorNode &b = nodes[j];
        if (a.digit != b.digit && a.cell == b.cell) {
          // 3D Medusa Rule 1 : Twice in a Cell
          found = true;
          detailedReason = ReasonId::_3DMedusaColorTrap;
          goto end_loop;
        }
        if (a.digit == b.digit && board.sees(a.cell, b.cell)) {
          // 3D Medusa Rule 2 : Twice in a Unit (Color Trap)
          // Simple Coloring : Color Trap
          found = true;
          if (reason == ReasonId::SimpleColoring) {
            detailedReason = ReasonId::SimpleColoringColorTrap;
          } else {
            detailedReason = ReasonId::_3DMedusaColorTrap;
          }
          goto end_loop;
        }
      }
    }
    for (int i = 0; i < nodes.size(); ++i) {
      for (int j = i+1; j < nodes.size(); ++j) {
        const ColorNode &a = nodes[i];
        const ColorNode &b = nodes[j];
        if (a.digit != b.digit && a.cell != b.cell) {
          DigitSet abDigits = DigitSet({a.digit, b.digit});
          CellSet abCells = CellSet({a.cell, b.cell});

          CellSet peers = board.getPeers(abCells);
          CellSet bivalues = board.getBivalues();
          for (Cell idx : peers & bivalues) {
            if (board.getCandidates(idx) == abDigits) {
              // 3D Medusa Rule 6 : Cell emptied by colour
              found = true;
              detailedReason = ReasonId::_3DMedusaEmptiedCell;
              emptiedCellIdx = idx;
              goto end_loop;
            }
          }
          for (int k = j+1; k < nodes.size(); ++k) {
            const ColorNode &c = nodes[k];
            if (!abDigits.contains(c.digit) && !abCells.contains(c.cell)) {
              CellSet peers = board.getPeers({a.cell, b.cell, c.cell});
              for (Cell idx : peers) {
                if (board.getCandidates(idx) == DigitSet({a.digit, b.digit, c.digit})) {
                  // 3D Medusa Rule 6 : Cell emptied by colour
                  found = true;
                  detailedReason = ReasonId::_3DMedusaEmptiedCell;
                  emptiedCellIdx = idx;
                  goto end_loop;
                }
              }
            }
          }
        }
      }
    }

end_loop:
    if (found) {
      // 'other' is the solution
      // source is: other || nodes || emptied cell (if applicable)
      // operation is: set value to 'other'
      Event event(EventType::SetValue, detailedReason);
      for (int i = 0; i < other.size(); ++i) {
        ColorNode node = other[i];
        event.addSource(node.cell, node.digit);
        event.addOperation(node.cell, node.digit);
      }
      event.addDelimiter();
      for (int i = 0; i < nodes.size(); ++i) {
        ColorNode node = nodes[i];
        event.addSource(node.cell, node.digit);
      }
      if (emptiedCellIdx != -1) {
        event.addDelimiter();
        event.addSource(emptiedCellIdx, board.getCandidates(emptiedCellIdx));
      }
      return event;
    }

    return {};
  };

  std::optional<Event> first = scanColor(firstColorNodes, secondColorNodes);
  if (first) return *first;

  std::optional<Event> second = scanColor(secondColorNodes, firstColorNodes);
  if (second) return *second;

  // Color Wrap test
  Event event(EventType::RemoveCandidate, reason);
  for (int i = 0; i < firstColorNodes.size(); ++i) {
    for (int j = 0; j < secondColorNodes.size(); ++j) {
      ColorNode &a = firstColorNodes[i];
      ColorNode &b = secondColorNodes[j];
      if (a.digit != b.digit && a.cell == b.cell) {
        // 3D Medusa Rule 3 : Two colours in a Cell
        DigitSet toRemove = board.getCandidates(a.cell) - DigitSet({a.digit, b.digit});
        if (!toRemove.empty()) {
          event.addOperation(a.cell, toRemove);
        }
        event.reason = ReasonId::_3DMedusaColorWrap;
      }
      if (a.digit == b.digit) {
        // 3D Medusa Rule 4 : Two colours elsewhere (Color Wrap)
        // Simple Coloring : Color Wrap
        CellSet toRemove = board.getPeersContaining({a.cell, b.cell}, a.digit);
        if (!toRemove.empty()) {
          for (Cell idx : toRemove) {
            event.addOperation(idx, a.digit);
          }
        }
        if (reason == ReasonId::SimpleColoring) {
          event.reason = ReasonId::SimpleColoringColorWrap;
        } else {
          event.reason = ReasonId::_3DMedusaColorWrap;
        }
      }
      if (a.digit != b.digit && board.sees(a.cell, b.cell)) {
        // 3D Medusa Rule 5 : Two colours Unit + Cell
        if (board.hasCandidate(a.cell, b.digit)) {
          event.addOperation(a.cell, b.digit);
        }
        if (board.hasCandidate(b.cell, a.digit)) {
          event.addOperation(b.cell, a.digit);
        }
        event.reason = ReasonId::_3DMedusaColorWrap;
      }
    }
  }
  if (event.getNumberOfOperations() > 0) {
    // add sources first || second and return event
    for (int i = 0; i < firstColorNodes.size(); ++i) {
      ColorNode node = firstColorNodes[i];
      event.addSource(node.cell, node.digit);
    }
    event.addDelimiter();
    for (int i = 0; i < secondColorNodes.size(); ++i) {
      ColorNode node = secondColorNodes[i];
      event.addSource(node.cell, node.digit);
    }
    return event;
  }

  return {};
}

bool AicSearcher::are_weakly_linked(AicNode a, AicNode b) const {
  bool AisGrouped;
  CellSet cellSetA;
  DigitSet digitSetA;
  deserialize_unitcode(a, cellSetA, digitSetA, AisGrouped);

  bool BisGrouped;
  CellSet cellSetB;
  DigitSet digitSetB;
  deserialize_unitcode(b, cellSetB, digitSetB, BisGrouped);

  Digit digitA = *digitSetA.begin();
  Digit digitB = *digitSetB.begin();

  if ((AisGrouped || BisGrouped) && !config.useGroupedCells) {
    return false;
  }

  // candidates inside a cell
  if (!AisGrouped && !BisGrouped && cellSetA == cellSetB && digitA != digitB) {
    if (config.multiDigit && config.useWeakInCell) {
      return true;
    }
  }

  // cells (or group of) that can see each other
  if (digitA == digitB && board.sees(cellSetA, cellSetB)) {
    if (config.useWeakInUnit) {
      return true;
    }
  }

  return false;
}
