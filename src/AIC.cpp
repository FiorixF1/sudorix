#include "AIC.hpp"

AicGraphBuilder::AicGraphBuilder(const SudokuBoard &board) : board(board) { }

AicGraph AicGraphBuilder::build() {
  AicGraph g;

  build_singleton_nodes(g.nodes);
  //build_grouped_nodes(g.nodes);

  build_strong_links(g.nodes, g.strong_links);

#ifdef DEBUG
  console_log("AIC GRAPH");
  for (auto &it : g.strong_links) {
    AicNode fromID = it.first;
    auto &edges = it.second;

    CellSet fromSet;
    DigitSet fromDigitSet;
    bool isGrouped;
    deserialize_unitcode(fromID, fromSet, fromDigitSet, isGrouped);
    if (isGrouped) continue;  // debug only singletons

    for (auto &toID : edges) {
      CellSet toSet;
      DigitSet toDigitSet;
      bool isGrouped;
      deserialize_unitcode(toID, toSet, toDigitSet, isGrouped);
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

  return g;
}

AicGraph AicGraphBuilder::prune(const AicGraph &graph, const AicConfig &config) {
  AicGraph prunedGraph;

  // build edges
  for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
    AicNode source = it->first;

    bool isGrouped;
    CellSet cellSet;
    DigitSet digitSet;
    deserialize_unitcode(source, cellSet, digitSet, isGrouped);
    if (isGrouped && !config.useGroupedCells) {
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
      if (targetDigitSet != digitSet && (!config.multiDigit || !config.useStrongBivalues)) {
        continue;
      }
      if (targetCellSet != cellSet && !config.useStrongBilocations) {
        continue;
      }
      add_strong_edge(prunedGraph.strong_links, source, target);
    }
  }

  // build nodes from filtered edges
  for (auto it = prunedGraph.strong_links.begin(); it != prunedGraph.strong_links.end(); ++it) {
    prunedGraph.nodes.push_back(it->first);
  }

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

AicSearcher::AicSearcher(const SudokuBoard &board,
                         AicGraph &graph,
                         const AicConfig &config)
  : board(board), graph(graph), config(config) {

}

std::vector<AicElimination> AicSearcher::find_aic_eliminations() const {
  std::vector<AicElimination> out;

  for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
    AicNode start = it->first;

    auto found = search_from(start);
    if (!found.empty()) {
      // found a chain that causes eliminations
      out.insert(out.end(), found.begin(), found.end());
      break;
    }
  }

  return out;
}

std::vector<AicElimination> AicSearcher::search_from(AicNode start) const {
  std::vector<AicElimination> found;

  struct QueueItem {
    AicNode node;
    EdgeType next_type;
    int depth;
    int state_index;
  };

  std::vector<AicSearchState> states;
  std::vector<AicParent> parents;
  std::deque<QueueItem> q;

  auto push_state = [&](AicNode node, EdgeType next_type, int depth, int prev_idx, AicNode prev_node, EdgeType edge_used) {
    AicSearchState st{node, next_type};
    states.push_back(st);
    parents.push_back({prev_idx, prev_node, edge_used});
    int idx = static_cast<int>(states.size()) - 1;
    q.push_back({node, next_type, depth, idx});
    return idx;
  };

  // Partiamo dal nodo iniziale e imponiamo che il primo arco sia strong.
  push_state(start, EdgeType::STRONG, 0, -1, start, EdgeType::STRONG);

  while (!q.empty()) {
    QueueItem cur = q.front();
    q.pop_front();

    if (cur.depth >= config.max_depth) {
      continue;
    }

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
    if (cur.next_type == EdgeType::STRONG) {
      for (AicNode nb : graph.strong_links[cur.node]) {
        // config check (TODO: sicuro che serve? c'è già nel pruning)
        bool targetIsGrouped;
        CellSet targetCellSet;
        DigitSet targetDigitSet;
        deserialize_unitcode(nb, targetCellSet, targetDigitSet, targetIsGrouped);
        if (targetIsGrouped && !config.useGroupedCells) {
          continue;
        }
        if (targetDigitSet != digitSet && (!config.multiDigit || !config.useStrongBivalues)) {
          continue;
        }
        if (targetCellSet != cellSet && !config.useStrongBilocations) {
          continue;
        }
        // ---
        if (path_contains_node(cur.state_index, nb, states, parents)) {
          continue;
        }

        int next_idx = push_state(nb, EdgeType::WEAK, cur.depth + 1,
                                  cur.state_index, cur.node, EdgeType::STRONG);

        // Eliminazioni: chain che inizia e finisce con strong
        // quindi quando arrivi qui hai appena raggiunto uno strong.
        auto elim = common_peer_eliminations(start, nb, next_idx, states, parents);
        if (!elim.empty()) {
          // Eliminazioni type 1
          found.insert(found.end(), elim.begin(), elim.end());
          return found;
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

        push_state(nb, EdgeType::STRONG, cur.depth + 1,
                   cur.state_index, cur.node, EdgeType::WEAK);
      }
    }
  }

  return found;
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

std::vector<AicElimination> AicSearcher::common_peer_eliminations(
  AicNode start,
  AicNode end,
  int end_state_idx,
  const std::vector<AicSearchState> &states,
  const std::vector<AicParent> &parents) const
{
  std::vector<AicElimination> out;

  CellSet cellSetA;
  DigitSet digitSetA;
  bool AisGrouped;
  deserialize_unitcode(start, cellSetA, digitSetA, AisGrouped);

  CellSet cellSetB;
  DigitSet digitSetB;
  bool BisGrouped;
  deserialize_unitcode(end, cellSetB, digitSetB, BisGrouped);

  Digit digitA = *digitSetA.begin();
  Digit digitB = *digitSetB.begin();

  // Per adesso non facciamo chain con gruppi.
  if (AisGrouped || BisGrouped) {
    return out;
  }

  Cell cell_a = *cellSetA.begin();
  Cell cell_b = *cellSetB.begin();
  if (cell_a < 0 || cell_b < 0 || cell_a == cell_b) {
    return out;
  }

  // Eliminazione AIC Type 1
  if (digitA == digitB) {
    CellSet common = board.getPeers({cell_a, cell_b});
    AicPath reason = reconstruct_path(end_state_idx, states, parents);

    for (Cell cell : common) {
      if (!board.hasCandidate(cell, digitA)) {
        continue;
      }

      AicElimination e;
      e.cell = cell;
      e.digit = digitA;
      e.reason = reason;
      out.push_back(e);
    }

    return out;
  }

  // Eliminazione AIC Type 2
  if (board.sees(cell_a, cell_b)) {
    if (board.hasCandidate(cell_a, digitB)) {
      AicPath reason = reconstruct_path(end_state_idx, states, parents);
      AicElimination e;
      e.cell = cell_a;
      e.digit = digitB;
      e.reason = reason;
      out.push_back(e);
    }

    if (board.hasCandidate(cell_b, digitA)) {
      AicPath reason = reconstruct_path(end_state_idx, states, parents);
      AicElimination e;
      e.cell = cell_b;
      e.digit = digitA;
      e.reason = reason;
      out.push_back(e);
    }

    return out;
  }

  return out;
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

  // TODO. casistica useWeakLinks disattivata (per Coloring e Medusa)

  return false;
}
