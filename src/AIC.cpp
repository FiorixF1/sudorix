#include "AIC.hpp"
#include "encoder.hpp"
#include "EventQueue.hpp"

AicGraphBuilder::AicGraphBuilder(const SudokuBoard &board) : board(board) { }

AicGraph AicGraphBuilder::build() {
  AicGraph g;

  build_singleton_nodes(g.nodes);
  build_grouped_nodes(g.nodes);

  build_links(g.nodes, g.strong_links, g.weak_links);

  return g;
}

AicGraph AicGraphBuilder::prune(AicGraph &graph, const AicConfig &config) {
  AicGraph prunedGraph;

  // build strong edges according to config
  for (auto &it : graph.strong_links) {
    AicNode &source = graph.nodes[it.first];

    if (source.isGrouped && !config.useGroupedCells) {
      continue;
    }
    if (source.isGrouped && !config.useWeakLinks) {
      // groups are not allowed in coloring techniques
      continue;
    }
    // be careful: graph nodes have only one digit, use SudokuBoard to get the digits of the pair
    DigitSet remotePair = board.getCandidates(*source.cellSet.begin());
    if (config.useRemotePairs && (source.isGrouped || remotePair.size() != 2)) {
      continue;
    }

    auto &edges = it.second;
    for (const AicEdge &edge : edges) {
      AicNode &target = graph.nodes[edge.to];
      if (target.isGrouped && !config.useGroupedCells) {
        continue;
      }
      if (target.isGrouped && !config.useWeakLinks) {
        // groups are not allowed in coloring techniques
        continue;
      }
      if (target.digitSet != source.digitSet && (!config.multiDigit || !config.useStrongBivalues)) {
        continue;
      }
      if (target.cellSet != source.cellSet && !config.useStrongBilocations) {
        continue;
      }
      // be careful: graph nodes have only one digit, use SudokuBoard to get the digits of the pair
      // however ensure that the single digit between two nodes is the same
      DigitSet targetRemotePair = board.getCandidates(*target.cellSet.begin());
      if (config.useRemotePairs && (target.isGrouped || targetRemotePair != remotePair || target.digitSet != source.digitSet)) {
        continue;
      }
      add_edge(prunedGraph.strong_links, source.id, target.id, EdgeType::STRONG);
    }
  }

  // build weak edges according to config
  if (config.useWeakLinks) {
    for (auto &it : graph.weak_links) {
      AicNode &source = graph.nodes[it.first];

      if (source.isGrouped && !config.useGroupedCells) {
        continue;
      }

      auto &edges = it.second;
      for (const AicEdge &edge : edges) {
        AicNode &target = graph.nodes[edge.to];
        if (target.isGrouped && !config.useGroupedCells) {
          continue;
        }
        if (target.digitSet != source.digitSet && (!config.multiDigit || !config.useWeakInCell)) {
          continue;
        }
        if (target.cellSet != source.cellSet && !config.useWeakInUnit) {
          continue;
        }
        add_edge(prunedGraph.weak_links, source.id, target.id, EdgeType::WEAK);
      }
    }
  }

  // build nodes from filtered edges
  // TODO: maybe you can iterate only over weak links since every strong link is also weak
  for (auto &it : prunedGraph.strong_links) {
    prunedGraph.nodes[it.first] = graph.nodes[it.first];
  }
  for (auto &it : prunedGraph.weak_links) {
    prunedGraph.nodes[it.first] = graph.nodes[it.first];
  }

#if 0
  console_log("AIC GRAPH");
  for (auto &it : prunedGraph.strong_links) {
    AicNode &from = graph.nodes[it.first];

    Cell fromCell = *from.cellSet.begin();
    Digit fromDigit = *from.digitSet.begin();
    if (from.isGrouped) continue;  // debug only singletons

    auto &edges = it.second;
    for (auto &edge : edges) {
      AicNode &to = graph.nodes[edge.to];

      Cell toCell = *to.cellSet.begin();
      Digit toDigit = *to.digitSet.begin();
      if (to.isGrouped) continue;  // debug only singletons

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

void AicGraphBuilder::build_singleton_nodes(AicGraphNodes &nodes) {
  for (Cell cell = 0; cell < 81; ++cell) {
    for (Digit digit = 1; digit <= 9; ++digit) {
      if (board.isSolved(cell) || !board.hasCandidate(cell, digit)) {
        continue;
      }

      AicNodeID id = get_node_id(digit, cell);
      if (!id) return;
      AicNode node{
        .id = id,
        .cellSet = {cell},
        .digitSet = {digit},
        .isGrouped = false
      };
      nodes[id] = node;
    }
  }
}

void AicGraphBuilder::add_group_if_new(AicGraphNodes &nodes, Digit digit, const CellSet &cells) {
  if (cells.size() < 2) {
    return;
  }

  AicNodeID id = get_node_id(digit, cells);
  if (!id) return;
  AicNode candidate{
    .id = id,
    .cellSet = cells,
    .digitSet = {digit},
    .isGrouped = true
  };
  nodes[id] = candidate;
}

void AicGraphBuilder::build_grouped_nodes(AicGraphNodes &nodes) {
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

AicNodeID AicGraphBuilder::get_node_id(Digit digit, Cell cell) {
  return get_node_id(DigitSet({digit}), CellSet({cell}));
}

AicNodeID AicGraphBuilder::get_node_id(Digit digit, const CellSet &cells) {
  return get_node_id(DigitSet({digit}), cells);
}

AicNodeID AicGraphBuilder::get_node_id(const DigitSet &digits, Cell cell) {
  return get_node_id(digits, CellSet({cell}));
}

AicNodeID AicGraphBuilder::get_node_id(const DigitSet &digits, const CellSet &cells) {
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

void AicGraphBuilder::add_edge(AicGraphEdges &adj, AicNodeID a, AicNodeID b, EdgeType type) {
  if (a == b) {
    return;
  }

  auto add_one_way = [&](AicNodeID x, AicNodeID y) {
    auto &v = adj[x];
    AicEdge edge{y, type};
    if (std::find(v.begin(), v.end(), edge) == v.end()) {
      v.push_back(edge);
    }
  };

  add_one_way(a, b);
  add_one_way(b, a);
}

void AicGraphBuilder::build_links(AicGraphNodes &nodes,
                                  AicGraphEdges &strong_links,
                                  AicGraphEdges &weak_links) {
  // Bilocations (strong) and positions (weak)
  for (Digit digit = 1; digit <= 9; ++digit) {
    build_links_in_units(nodes, strong_links, weak_links, SudokuBoard::getRows(), digit);
    build_links_in_units(nodes, strong_links, weak_links, SudokuBoard::getColumns(), digit);
    build_links_in_units(nodes, strong_links, weak_links, SudokuBoard::getBoxes(), digit);
  }

  // Bivalues (strong) and candidates (weak)
  for (Cell cell = 0; cell < 81; ++cell) {
    build_links_in_cells(nodes, strong_links, weak_links, cell);
  }

  // Grouped cells (including empty rectangle intersection)
  for (Digit digit = 1; digit <= 9; ++digit) {
    // singletons and groups in a row
    build_grouped_row_box(nodes, strong_links, weak_links, digit);
    // singletons and groups in a column
    build_grouped_col_box(nodes, strong_links, weak_links, digit);
    // minirows in a box
    //build_grouped_box_row(nodes, strong_links, weak_links, digit);
    // minicolumns in a box
    //build_grouped_box_col(nodes, strong_links, weak_links, digit);
    // empty rectangle intersection
    build_grouped_eri(nodes, strong_links, weak_links, digit);
  }
}

void AicGraphBuilder::build_links_in_units(const AicGraphNodes &nodes,
                                           AicGraphEdges &strong_links,
                                           AicGraphEdges &weak_links,
                                           const std::vector<Unit> &units,
                                           Digit digit) {
  for (const Unit &unit : units) {
    CellSet pos = board.getPositionsOfDigit(unit, digit);
    bool is_strong_link = pos.size() == 2;

    std::vector<int> v = pos.to_vector();
    for (int i = 0; i < v.size(); ++i) {
      for (int j = i+1; j < v.size(); ++j) {
        Cell c1 = v[i];
        Cell c2 = v[j];

        AicNodeID n1 = get_node_id(digit, c1);
        AicNodeID n2 = get_node_id(digit, c2);
        if (n1 && n2) {
          if (is_strong_link) add_edge(strong_links, n1, n2, EdgeType::STRONG);
          add_edge(weak_links, n1, n2, EdgeType::WEAK);
        }
      }
    }
  }
}

void AicGraphBuilder::build_links_in_cells(const AicGraphNodes &nodes,
                                           AicGraphEdges &strong_links,
                                           AicGraphEdges &weak_links,
                                           Cell cell) {
  DigitSet candidates = board.getCandidates(cell);
  bool is_strong_link = candidates.size() == 2;

  std::vector<int> v = candidates.to_vector();
  for (int i = 0; i < v.size(); ++i) {
    for (int j = i+1; j < v.size(); ++j) {
      Digit d1 = v[i];
      Digit d2 = v[j];

      AicNodeID n1 = get_node_id(d1, cell);
      AicNodeID n2 = get_node_id(d2, cell);
      if (n1 && n2) {
        if (is_strong_link) add_edge(strong_links, n1, n2, EdgeType::STRONG);
        add_edge(weak_links, n1, n2, EdgeType::WEAK);
      }
    }
  }
}

void AicGraphBuilder::build_grouped_row_box(const AicGraphNodes &nodes,
                                            AicGraphEdges &strong_links,
                                            AicGraphEdges &weak_links,
                                            Digit digit) {
  for (const Unit &r : SudokuBoard::getRows()) {
    CellSet pos = board.getPositionsOfDigit(r, digit);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &b : SudokuBoard::getBoxes()) {
      CellSet g = pos & b;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(digit, parts[0]);
      AicNodeID b = get_node_id(digit, parts[1]);
      if (a && b) {
        add_edge(strong_links, a, b, EdgeType::STRONG);
        add_edge(weak_links, a, b, EdgeType::WEAK);
      }
    }
  }
}

void AicGraphBuilder::build_grouped_col_box(const AicGraphNodes &nodes,
                                            AicGraphEdges &strong_links,
                                            AicGraphEdges &weak_links,
                                            Digit digit) {
  for (const Unit &c : SudokuBoard::getColumns()) {
    CellSet pos = board.getPositionsOfDigit(c, digit);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &b : SudokuBoard::getBoxes()) {
      CellSet g = pos & b;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(digit, parts[0]);
      AicNodeID b = get_node_id(digit, parts[1]);
      if (a && b) {
        add_edge(strong_links, a, b, EdgeType::STRONG);
        add_edge(weak_links, a, b, EdgeType::WEAK);
      }
    }
  }
}

void AicGraphBuilder::build_grouped_box_row(const AicGraphNodes &nodes,
                                            AicGraphEdges &strong_links,
                                            AicGraphEdges &weak_links,
                                            Digit digit) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, digit);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &r : SudokuBoard::getRows()) {
      CellSet g = pos & r;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(digit, parts[0]);
      AicNodeID b = get_node_id(digit, parts[1]);
      if (a && b) {
        add_edge(strong_links, a, b, EdgeType::STRONG);
        add_edge(weak_links, a, b, EdgeType::WEAK);
      }
    }
  }
}

void AicGraphBuilder::build_grouped_box_col(const AicGraphNodes &nodes,
                                            AicGraphEdges &strong_links,
                                            AicGraphEdges &weak_links,
                                            Digit digit) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, digit);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &c : SudokuBoard::getColumns()) {
      CellSet g = pos & c;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(digit, parts[0]);
      AicNodeID b = get_node_id(digit, parts[1]);
      if (a && b) {
        add_edge(strong_links, a, b, EdgeType::STRONG);
        add_edge(weak_links, a, b, EdgeType::WEAK);
      }
    }
  }
}

void AicGraphBuilder::build_grouped_eri(AicGraphNodes &nodes,
                                        AicGraphEdges &strong_links,
                                        AicGraphEdges &weak_links,
                                        Digit digit) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, digit);
    if (pos.size() < 3) {
      continue;
    }

    const Unit *eri_row = nullptr;
    const Unit *eri_col = nullptr;
    for (const Unit &r : SudokuBoard::getRows()) {
      for (const Unit &c : SudokuBoard::getColumns()) {
        CellSet eri = (r | c) & b;
        if (eri.is_superset_of(pos)) {
          eri_row = &r;
          eri_col = &c;
          break;
        }
      }
    }

    if (eri_row && eri_col) {
      CellSet row_part = *eri_row & pos;
      CellSet col_part = *eri_col & pos;

      if (row_part.size() > 1 || col_part.size() > 1) {
        CellSet intersection = row_part & col_part;
        if (intersection.empty()) {
          // the cell in the intersection does not contain the candidate
          AicNodeID r = get_node_id(digit, row_part);
          AicNodeID c = get_node_id(digit, col_part);
          if (r && c) {
            add_edge(strong_links, r, c, EdgeType::STRONG);
            add_edge(weak_links, r, c, EdgeType::WEAK);
          }
        } else {
          // the intersecting cell contains the candidate: split the ERI in two parts
          // a new node could be necessary in some cases
          {
            add_group_if_new(nodes, digit, row_part - intersection);
            AicNodeID r = get_node_id(digit, row_part - intersection);
            AicNodeID c = get_node_id(digit, col_part);
            if (r && c) {
              add_edge(strong_links, r, c, EdgeType::STRONG);
              add_edge(weak_links, r, c, EdgeType::WEAK);
            }
          }
          {
            add_group_if_new(nodes, digit, col_part - intersection);
            AicNodeID r = get_node_id(digit, row_part);
            AicNodeID c = get_node_id(digit, col_part - intersection);
            if (r && c) {
              add_edge(strong_links, r, c, EdgeType::STRONG);
              add_edge(weak_links, r, c, EdgeType::WEAK);
            }
          }
        }
      }
    }
  }
}

/* ---------------------------------------------------------------------- */

static void retain(AicSearchNode *n) {
  if (n) {
    ++n->refcount;
  }
}

static void release(AicSearchNode *n) {
  while (n) {
    --n->refcount;
    if (n->refcount > 0) {
      return;
    }

    AicSearchNode *parent = n->parent;
    delete n;
    n = parent;
  }
}

AicSearchNode *make_node(AicNodeID start,
                         AicNodeID node,
                         EdgeType next_type,
                         int depth,
                         AicSearchNode *parent) {
  AicSearchNode *s = new AicSearchNode{
    .start = start,
    .node = node,
    .next_type = next_type,
    .depth = depth,
    .parent = parent,
    .refcount = 1
  };

  if (parent) {
    retain(parent);
  }

  return s;
}

AicSearchNode *make_node(AicNodeID start,
                         AicNodeID node,
                         ColorType next_color,
                         int depth,
                         AicSearchNode *parent) {
  AicSearchNode *s = new AicSearchNode{
    .start = start,
    .node = node,
    .next_color = next_color,
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

AicSearcher::AicSearcher(const SudokuBoard &board, EventQueue &eventQueue)
  : board(board), eventQueue(eventQueue) { }

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
    case ReasonId::EmptyRectangle:
      config = {
        .useWeakLinks = true,
        .multiDigit = false,
        .useGroupedCells = true,
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
        .max_depth = 19,
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
        .max_depth = 19,
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
        .max_depth = 11,
      };
      break;
    case ReasonId::GroupedXChain:
      config = {
        .useWeakLinks = true,
        .multiDigit = false,
        .useGroupedCells = true,
        .useStrongBivalues = false,
        .useStrongBilocations = true,
        .useWeakInCell = false,
        .useWeakInUnit = true,
        .useRemotePairs = false,
        .max_depth = 11,
      };
      break;
    case ReasonId::GroupedAIC:
      config = {
        .useWeakLinks = true,
        .multiDigit = true,
        .useGroupedCells = true,
        .useStrongBivalues = true,
        .useStrongBilocations = true,
        .useWeakInCell = true,
        .useWeakInUnit = true,
        .useRemotePairs = false,
        .max_depth = 11,
      };
      break;
    case ReasonId::ForcingChain:
      config = {
        .useWeakLinks = true,
        .multiDigit = true,
        .useGroupedCells = false,
        .useStrongBivalues = true,
        .useStrongBilocations = true,
        .useWeakInCell = true,
        .useWeakInUnit = true,
        .useRemotePairs = false,
        .max_depth = 25,
      };
      break;
    default:
      break;
  }

  this->reason = reason;

  return config;
}

bool AicSearcher::runSearch(AicGraph &graph) {
  this->graph = &graph;

  visited.clear();

  if (reason == ReasonId::ForcingChain) {
    // forcing chain search
    bool maybeEvent = forcing_chain_search();

    if (maybeEvent) {
      return true;
    }
  } else if (config.useWeakLinks) {
    // generic AIC search
    bool maybeEvent = aic_search();

    if (maybeEvent) {
      return true;
    }
  } else {
    // color-based search, only strong links
    for (auto &it : graph.strong_links) {
      AicNodeID start = it.first;

      bool maybeEvent = coloring_search_from(start);

      if (maybeEvent) {
        return true;
      }
    }
  }

  return false;
}

bool AicSearcher::aic_search() {
  std::deque<AicSearchNode *> q;

  // Partiamo dal nodo iniziale e imponiamo che il primo arco sia strong.
  for (auto &it : graph->strong_links) {
    AicNodeID start = it.first;
    q.push_back(make_node(start, start, EdgeType::STRONG, 0, nullptr));
  }

  while (!q.empty()) {
    AicSearchNode *cur = q.front();
    q.pop_front();

    if (cur->depth >= config.max_depth) {
      release(cur);
      continue;
    }

#if 0
    AicNode &current = graph->nodes[cur->node];
    if (!current.isGrouped) {
      Cell cell = *current.cellSet.begin();
      Digit digit = *current.digitSet.begin();
      console_log("CURRENT STATE: r%dc%d (%d) - %s LINK - Length %d", SudokuBoard::getRowLocation(cell)+1,
                                                                      SudokuBoard::getColumnLocation(cell)+1,
                                                                      digit,
                                                                      cur->next_type == EdgeType::STRONG ? "STRONG" : "WEAK",
                                                                      cur->depth);
    }
#endif
    if (cur->next_type == EdgeType::STRONG) {
      for (const AicEdge &edge : graph->strong_links[cur->node]) {
        AicNodeID nb = edge.to;
        if (path_contains_node(cur->start, cur, nb)) {
          continue;
        }

        AicSearchNode *child = make_node(cur->start,
                                         nb,
                                         EdgeType::WEAK,
                                         cur->depth + 1,
                                         cur);

        // Look for eliminations according to AIC rules
        std::optional<Event> event = execute_aic_rules(cur->start, nb, child);
        if (event) {
          // for single-digit pattern, empty rectangle and named wings
          bool valid = analyze_event(*event);
          // if event found
          if (valid) {
            if (eventQueue.enqueue(board, *event)) {
              release(child);

              while (!q.empty()) {
                release(q.front());
                q.pop_front();
              }

              release(cur);
              return true;
            }
          }
        }

        q.push_back(child);
      }
    } else {
      for (const AicEdge &edge : graph->weak_links[cur->node]) {
        AicNodeID nb = edge.to;
        if (path_contains_node(cur->start, cur, nb)) {
          continue;
        }

        AicSearchNode *child = make_node(cur->start,
                                         nb,
                                         EdgeType::STRONG,
                                         cur->depth + 1,
                                         cur);
        q.push_back(child);
      }
    }

    release(cur);
  }

  return false;
}

bool AicSearcher::coloring_search_from(AicNodeID start) {
  std::vector<ColorSearchState> states;
  std::deque<AicSearchNode *> q;

  // Partiamo dal nodo iniziale e imponiamo il primo colore.
  q.push_back(make_node(start, start, ColorType::SECOND, 0, nullptr));

  while (!q.empty()) {
    AicSearchNode *cur = q.front();
    q.pop_front();

    if (visited.find(cur->node) != visited.end()) {
      release(cur);
      continue;
    }
    visited.insert(cur->node);

#if 0
    AicNode &current = graph->nodes[cur->node];
    if (!current.isGrouped) {
      Cell cell = *current.cellSet.begin();
      Digit digit = *current.digitSet.begin();
      console_log("CURRENT STATE: r%dc%d (%d) - %s COLOR - Length %d", SudokuBoard::getRowLocation(cell)+1,
                                                                       SudokuBoard::getColumnLocation(cell)+1,
                                                                       digit,
                                                                       cur->next_color == ColorType::FIRST ? "FIRST" : "SECOND",
                                                                       cur->depth);
    }
#endif
    for (const AicEdge &edge : graph->strong_links[cur->node]) {
      AicNodeID nb = edge.to;
      ColorType color_used = cur->next_color;
      ColorType next_color = color_used == ColorType::FIRST ? ColorType::SECOND : ColorType::FIRST;

      ColorSearchState st{.node = nb, .next_color = next_color};
      states.push_back(st);

      AicSearchNode *child = make_node(cur->start,
                                       nb,
                                       next_color,
                                       cur->depth + 1,
                                       cur);
      q.push_back(child);
    }
    release(cur);
  }

  // Look for contradictions among colors
  return execute_coloring_rules(start, states);
}

bool AicSearcher::forcing_chain_search() {
  return execute_fc_rules();
}

bool AicSearcher::execute_fc_rules() const {
  return find_contradiction() || find_common_consequences();
}

// utility functions for forcing chains
namespace {

  static constexpr ForcingID make_forcing_id(Cell cell, Digit digit, bool on) {
    return static_cast<ForcingID>(((static_cast<int>(cell) * 10 + static_cast<int>(digit)) * 2) + (on ? 1 : 0));
  }

  static constexpr Cell forcing_cell(ForcingID id) {
    return static_cast<Cell>((id / 2) / 10);
  }

  static constexpr Digit forcing_digit(ForcingID id) {
    return static_cast<Digit>((id / 2) % 10);
  }

  static constexpr bool forcing_on(ForcingID id) {
    return (id & 1U) != 0;
  }

  static constexpr ForcingID opposite_forcing_id(ForcingID id) {
    return static_cast<ForcingID>(id ^ 1U);
  }

  static bool valid_forcing_id(ForcingID id) {
    return id < FORCING_STATE_COUNT && forcing_digit(id) >= 1 && forcing_digit(id) <= 9;
  }

  static std::optional<ForcingID> forcing_id_from_aic_node(const AicNode &node, bool on) {
    if (node.isGrouped || node.cellSet.size() != 1 || node.digitSet.size() != 1) {
      return std::nullopt;
    }

    Cell cell = *node.cellSet.begin();
    Digit digit = *node.digitSet.begin();
    return make_forcing_id(cell, digit, on);
  }

  static AicNodeID forcing_id_to_aic_node_id(ForcingID id) {
    if (!valid_forcing_id(id)) {
      return 0;
    }
    return AicGraphBuilder::get_node_id(forcing_digit(id), forcing_cell(id));
  }

} // namespace

bool ForcingSearchResult::contains(ForcingID id) const {
  return valid_forcing_id(id) && entries[id].visited;
}

std::optional<ForcingPath> ForcingSearchResult::reconstructPath(ForcingID target) const {
  if (!contains(target)) {
    return std::nullopt;
  }

  ForcingPath path;
  ForcingID cur = target;

  while (cur != FORCING_INVALID_ID) {
    path.nodes.push_back(cur);
    if (cur == root) {
      break;
    }
    cur = entries[cur].parent;
  }

  if (path.nodes.empty() || path.nodes.back() != root) {
    return std::nullopt;
  }

  std::reverse(path.nodes.begin(), path.nodes.end());
  return path;
}

bool AicSearcher::find_contradiction() const {
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
  for (auto it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
    const AicNode &node = it->second;
    auto maybeRoot = forcing_id_from_aic_node(node, true);
    if (!maybeRoot) {
      continue;
    }

    // store the initial implication: the candidate is ON
    ForcingID rootOn = *maybeRoot;
    Cell assumptionCell = forcing_cell(rootOn);
    Digit assumptionDigit = forcing_digit(rootOn);

    // find the reachable consequences from the assumption
    const ForcingSearchResult reach = reachable_from(rootOn);

    // look for consequences that are contradictory
    for (auto ot = graph->nodes.begin(); ot != graph->nodes.end(); ++ot) {
      const AicNode &outNode = ot->second;
      auto maybeOn = forcing_id_from_aic_node(outNode, true);
      if (!maybeOn) {
        continue;
      }

      ForcingID onConsequence = *maybeOn;
      ForcingID offConsequence = opposite_forcing_id(onConsequence);

      Cell cell = forcing_cell(onConsequence);
      Digit digit = forcing_digit(onConsequence);
      if (cell == assumptionCell && digit == assumptionDigit) {
        continue;
      }

      // a consequence is both true and false -> the assumption is false
      if (reach.contains(onConsequence) && reach.contains(offConsequence)) {
        auto pathA = reach.reconstructPath(onConsequence);
        auto pathB = reach.reconstructPath(offConsequence);
        if (!pathA || !pathB) {
          continue;
        }

        // Nishio Forcing Chain spotted: source is the two chains starting from the assumption
        Event event(EventType::RemoveCandidate, ReasonId::ForcingChain, ReasonId::NishioForcingChain);
        add_path_sources(event, *pathA);
        event.addDelimiter();
        add_path_sources(event, *pathB);
        event.addOperation(assumptionCell, assumptionDigit);
        if (eventQueue.enqueue(board, event)) return true;
      }
    }
  }

  return false;
}

bool AicSearcher::find_common_consequences() const {
  auto emit_common_consequence = [&](const std::vector<ForcingSearchResult> &searches,
                                     ForcingID consequence,
                                     ReasonId detailedReason) -> bool {
    if (searches.empty()) {
      return false;
    }

    // corner case: there is no path for the desired consequence
    std::vector<ForcingPath> paths;
    paths.reserve(searches.size());
    for (const ForcingSearchResult &search : searches) {
      auto path = search.reconstructPath(consequence);
      if (!path) {
        return false;
      }
      paths.push_back(*path);
    }

    Event event(forcing_on(consequence) ? EventType::SetValue : EventType::RemoveCandidate,
                ReasonId::ForcingChain,
                detailedReason);

    for (size_t i = 0; i < paths.size(); ++i) {
      if (i > 0) {
        event.addDelimiter();
      }
      add_path_sources(event, paths[i]);
    }

    event.addOperation(forcing_cell(consequence), forcing_digit(consequence));
    return eventQueue.enqueue(board, event);
  };

  auto common_consequence_from_searches = [&](const std::vector<ForcingSearchResult> &searches,
                                              const std::vector<ForcingID> &assumptions,
                                              ReasonId detailedReason) -> bool {
    if (searches.empty()) {
      return false;
    }

    for (ForcingID candidate : searches.front().reachable) {
      if (!valid_forcing_id(candidate)) {
        continue;
      }

      // corner case: a consequence is an initial assumption
      bool isAssumption = false;
      for (ForcingID assumption : assumptions) {
        if ((assumption / 2) == (candidate / 2)) {
          isAssumption = true;
          break;
        }
      }
      if (isAssumption) {
        continue;
      }

      // corner case: the consequence is not reachable by every assumption
      bool common = true;
      for (size_t i = 1; i < searches.size(); ++i) {
        if (!searches[i].contains(candidate)) {
          common = false;
          break;
        }
      }
      if (!common) {
        continue;
      }

      if (emit_common_consequence(searches, candidate, detailedReason)) {
        return true;
      }
    }

    return false;
  };

  // Digit Forcing Chain
  // Supported cases:
  // - One candidate is ON either the assumption is true or false. It must be the solution.
  // - One candidate is OFF either the assumption is true or false. It can be removed.
  // Not yet supported:
  // - Two candidates in a cell are both ON, all other numbers can be removed.
  // - Two candidates on the same unit are both ON, all other numbers can be removed on that unit.
  for (auto it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
    const AicNode &node = it->second;
    auto maybeRootOn = forcing_id_from_aic_node(node, true);
    if (!maybeRootOn) {
      continue;
    }

    // store the initial implications: the candidate can be ON or OFF
    ForcingID rootOn = *maybeRootOn;
    ForcingID rootOff = opposite_forcing_id(rootOn);

    // find the reachable consequences from each assumption
    std::vector<ForcingSearchResult> searches;
    searches.push_back(reachable_from(rootOn));
    searches.push_back(reachable_from(rootOff));

    // look for consequences that are common to all assumptions
    if (common_consequence_from_searches(searches, {rootOn, rootOff}, ReasonId::DigitForcingChain)) {
      return true;
    }
  }

  auto multiForcingChain = [&](int size) -> bool {
    // Cell Forcing Chain
    // Supported cases:
    // - One candidate is ON for each possible solution of the starting cell. It must be the solution.
    // - One candidate is OFF for each possible solution of the starting cell. It can be removed.
    // Not yet supported:
    // - Two candidates in a cell are both ON, all other numbers can be removed.
    // - All candidates that can see all ends of the chain can be removed.
    for (Cell assumptionCell = 0; assumptionCell < 81; ++assumptionCell) {
      if (board.isSolved(assumptionCell)) {
        continue;
      }

      std::vector<int> assumptionDigits = board.getCandidates(assumptionCell).to_vector();
      if (static_cast<int>(assumptionDigits.size()) != size) {
        continue;
      }

      std::vector<ForcingID> assumptions;
      std::vector<ForcingSearchResult> searches;
      for (int rawDigit : assumptionDigits) {
        Digit digit = static_cast<Digit>(rawDigit);
        AicNodeID nodeId = AicGraphBuilder::get_node_id(digit, assumptionCell);
        if (graph->nodes.find(nodeId) == graph->nodes.end()) {
          assumptions.clear();
          searches.clear();
          break;
        }
        // store the initial implications: each candidate in the cell is true
        ForcingID root = make_forcing_id(assumptionCell, digit, true);
        assumptions.push_back(root);
        // find the reachable consequences from each assumption
        searches.push_back(reachable_from(root));
      }

      // look for consequences that are common to all assumptions
      if (!searches.empty() && common_consequence_from_searches(searches, assumptions, ReasonId::CellForcingChain)) {
        return true;
      }
    }

    // Unit Forcing Chain
    // Supported cases:
    // - One candidate is ON for each possible position of the starting unit. It must be the solution.
    // - One candidate is OFF for each possible position of the starting unit. It can be removed.
    // Not yet supported:
    // - Two candidates in a cell are both ON, all other numbers can be removed.
    // - All candidates that can see all ends of the chain can be removed.
    auto unitForcingChain = [&](const Unit &unit) -> bool {
      for (Digit assumptionDigit = 1; assumptionDigit <= 9; ++assumptionDigit) {
        std::vector<int> assumptionCells = board.getPositionsOfDigit(unit, assumptionDigit).to_vector();
        if (static_cast<int>(assumptionCells.size()) != size) {
          continue;
        }

        std::vector<ForcingID> assumptions;
        std::vector<ForcingSearchResult> searches;
        for (int rawCell : assumptionCells) {
          Cell cell = static_cast<Cell>(rawCell);
          AicNodeID nodeId = AicGraphBuilder::get_node_id(assumptionDigit, cell);
          if (graph->nodes.find(nodeId) == graph->nodes.end()) {
            assumptions.clear();
            searches.clear();
            break;
          }
          // store the initial implications: each candidate in the unit is true
          ForcingID root = make_forcing_id(cell, assumptionDigit, true);
          assumptions.push_back(root);
          // find the reachable consequences from each assumption
          searches.push_back(reachable_from(root));
        }

        // look for consequences that are common to all assumptions
        if (!searches.empty() && common_consequence_from_searches(searches, assumptions, ReasonId::UnitForcingChain)) {
          return true;
        }
      }
      return false;
    };

    for (const Unit &row : SudokuBoard::getRows()) {
      if (unitForcingChain(row)) return true;
    }
    for (const Unit &column : SudokuBoard::getColumns()) {
      if (unitForcingChain(column)) return true;
    }
    for (const Unit &box : SudokuBoard::getBoxes()) {
      if (unitForcingChain(box)) return true;
    }

    return false;
  };

  for (int size = 2; size <= 4; ++size) {
    if (multiForcingChain(size)) return true;
  }

  return false;
}

/*
 * A true -> B true   ----- Start from A with WEAK, end in B with STRONG
 * A true -> B false  ----- Start from A with WEAK, end in B with WEAK
 * A false -> B true  ----- Start from A with STRONG, end in B with STRONG
 * A false -> B false ----- Start from A with STRONG, end in B with WEAK
 */
ForcingSearchResult AicSearcher::reachable_from(ForcingID root) const {
  ForcingSearchResult result;
  result.root = root;

  if (!valid_forcing_id(root)) {
    return result;
  }

  std::deque<ForcingID> q;
  result.entries[root].visited = true;
  result.entries[root].parent = FORCING_INVALID_ID;
  result.entries[root].depth = 0;
  result.reachable.push_back(root);
  q.push_back(root);

  while (!q.empty()) {
    ForcingID cur = q.front();
    q.pop_front();

    const ForcingSearchEntry &curEntry = result.entries[cur];
    if (curEntry.depth >= config.max_depth) {
      continue;
    }

    AicNodeID curNodeId = forcing_id_to_aic_node_id(cur);
    if (curNodeId == 0 || graph->nodes.find(curNodeId) == graph->nodes.end()) {
      continue;
    }

    const bool curOn = forcing_on(cur);
    const AicGraphEdges &adj = curOn ? graph->weak_links : graph->strong_links;
    auto edgeIt = adj.find(curNodeId);
    if (edgeIt == adj.end()) {
      continue;
    }

    const bool nextOn = !curOn;
    for (const AicEdge &edge : edgeIt->second) {
      auto targetIt = graph->nodes.find(edge.to);
      if (targetIt == graph->nodes.end()) {
        continue;
      }

      auto maybeNext = forcing_id_from_aic_node(targetIt->second, nextOn);
      if (!maybeNext) {
        continue;
      }

      ForcingID next = *maybeNext;
      if (result.entries[next].visited) {
        continue;
      }

      result.entries[next].visited = true;
      result.entries[next].parent = cur;
      result.entries[next].depth = static_cast<uint16_t>(curEntry.depth + 1);
      result.reachable.push_back(next);
      q.push_back(next);
    }
  }

  return result;
}

void AicSearcher::add_path_sources(Event &event, const ForcingPath &path) const {
  for (ForcingID id : path.nodes) {
    if (!valid_forcing_id(id)) {
      continue;
    }

    AicNodeID nodeId = forcing_id_to_aic_node_id(id);
    auto it = graph->nodes.find(nodeId);
    if (it != graph->nodes.end()) {
      event.addSource(it->second.cellSet, it->second.digitSet);
    } else {
      event.addSource(CellSet({forcing_cell(id)}), DigitSet({forcing_digit(id)}));
    }
  }
}

bool AicSearcher::analyze_event(Event &event) {
  if (event.reason == ReasonId::SingleDigitPattern) {
    // identify the specific type of single digit pattern
    auto &sources = event.getSources();

    if (sources.size() != 4) {
      return false;
    }

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
      event.detailedReason = ReasonId::Skyscraper;
      return true;
    } else if ((aRow == bRow && cCol == dCol) ||
               (aCol == bCol && cRow == dRow)) {
      event.detailedReason = ReasonId::TwoStringKite;
      return true;
    } else if ((aRow == bRow && bCol == cCol) ||
               (aCol == bCol && bRow == cRow) ||
               (bRow == cRow && cCol == dCol) ||
               (bCol == cCol && cRow == dRow)) {
      event.detailedReason = ReasonId::Crane;
      return true;
    } else {
      return false;
    }
  } else if (event.reason == ReasonId::EmptyRectangle) {
    // identify if this is an empty rectangle pattern
    bool valid = false;
    auto &sources = event.getSources();
    if (sources.size() == 4) {
      CellSet a = sources[0].cells;
      CellSet b = sources[1].cells;
      CellSet c = sources[2].cells;
      CellSet d = sources[3].cells;
      if (a.size() >= 1 && b.size() >= 1 && c.size() == 1 && d.size() == 1) {
        for (const Unit &box : SudokuBoard::getBoxes()) {
          if ((a | b).is_subset_of(box)) {
            valid = true;
          }
        }
      } else if (a.size() == 1 && b.size() == 1 && c.size() >= 1 && d.size() >= 1) {
        for (const Unit &box : SudokuBoard::getBoxes()) {
          if ((c | d).is_subset_of(box)) {
            valid = true;
          }
        }
      }
    }
    return valid;
  } else if (reason == ReasonId::AIC || reason == ReasonId::GroupedAIC) {
    // identify named wing
    std::string valueLocationString;   // LVL, VVL...
    std::string digitsString;          // abba...
    std::string cellSectorString;      // CSSC...
    char digitToChar[10] = {0};
    char charCounter = 'a';
    int digitCounter = 0;
    bool WANT_STRONG = true;
    bool IS_GROUPED = false;

    auto &sources = event.getSources();
    for (int i = 0; i < sources.size()-1; ++i) {
      const Source &current = sources[i];
      const Source &next = sources[i+1];
      if (current.cells == next.cells) {
        // same cells, different digit
        if (WANT_STRONG) {
          valueLocationString += 'V';
        }
        cellSectorString += 'C';
      } else {
        // same digit, different cells
        if (WANT_STRONG) {
          valueLocationString += 'L';
        }
        cellSectorString += 'S';
      }
      if (current.cells.size() > 1 || next.cells.size() > 1) {
        IS_GROUPED = true;
      }

      Digit currentDigit = *current.mask.begin();
      if (digitToChar[currentDigit] == 0) {
        digitToChar[currentDigit] = charCounter++;
        ++digitCounter;
      }
      digitsString += digitToChar[currentDigit];

      WANT_STRONG = !WANT_STRONG;
    }
    // last digit
    Digit lastDigit = *(*sources.rbegin()).mask.begin();
    if (digitToChar[lastDigit] == 0) {
      digitToChar[lastDigit] = charCounter++;
      ++digitCounter;
    }
    digitsString += digitToChar[lastDigit];

    std::string finalString = valueLocationString + '-' + std::to_string(digitCounter) + '-' + cellSectorString;
    std::reverse(valueLocationString.begin(), valueLocationString.end());
    std::reverse(cellSectorString.begin(), cellSectorString.end());
    std::string revFinalString = valueLocationString + '-' + std::to_string(digitCounter) + '-' + cellSectorString;

    if (IS_GROUPED) {
      finalString += '+';
      revFinalString += '+';
    }

    const std::map<std::string, ReasonId> WING_TABLE = {
      // wings
      {"VVV-3-CSCSC", ReasonId::XYWing},
      {"VLV-2-CSSSC", ReasonId::WWing},
      {"LVL-2-SSCSS", ReasonId::SWing},
      {"VLL-2-CSSCS", ReasonId::M2Wing},
      {"VLL-3-CSSCS", ReasonId::M3Wing},
      {"LLL-1-SSSSS", ReasonId::L1Wing},
      {"LLL-2-SCSSS", ReasonId::L2Wing},
      {"LLL-2-SCSCS", ReasonId::L2Wing},
      {"LLL-3-SCSCS", ReasonId::L3Wing},
      {"VLL-2-CSSSS", ReasonId::H1Wing},
      {"VVL-2-CSCSS", ReasonId::H2Wing},
      {"VVL-3-CSCSS", ReasonId::H3Wing},
      {"LLLL-4-SCSCSCS", ReasonId::StrongWing},
      {"LLLL-2-SCSSSCS", ReasonId::iWWing},
      {"VLVL-2-CSSSCCS", ReasonId::DualWWing},
      {"LLLL-3-SCSCSCS", ReasonId::iXYWing},
      {"LLLL-2-SSSCSSS", ReasonId::iSWing},
      {"LLVL-2-SCSSCSS", ReasonId::iM2Wing},
      {"LLVL-3-SCSSCSS", ReasonId::iM3Wing},
      {"LLLL-1-SSSSSSS", ReasonId::iL1Wing},
      {"LVLL-2-SSCSSSS", ReasonId::iL2Wing},
      {"LVVL-2-SSCSCSS", ReasonId::iL2Wing},
      {"LVVL-3-SSCSCSS", ReasonId::iL3Wing},
      {"LLLL-2-SCSSSSS", ReasonId::iH1Wing},
      {"LLLL-2-SCSCSSS", ReasonId::iH2Wing},
      {"LLLL-3-SCSCSSS", ReasonId::iH3Wing},
      // rings
      {"VLVL-2-CSSSCSSS", ReasonId::WRing},
      {"LVLV-2-SSCSSSCS", ReasonId::WRing},
      {"VLL-2-CSSCSS", ReasonId::M2Ring},
      {"LLV-2-SCSSCS", ReasonId::M2Ring},
      {"LVL-2-SSCSSC", ReasonId::M2Ring},
      {"LLL-1-SSSSSS", ReasonId::L1Ring},
      {"LLL-2-SCSCSS", ReasonId::L2Ring},
      {"LLL-2-SCSSSC", ReasonId::L2Ring},
      {"LLL-2-SCSCSS", ReasonId::L2Ring},
      {"LVV-2-SSCSCS", ReasonId::H2Ring},
      {"VLV-2-CSSSCS", ReasonId::H2Ring},
      {"LLLL-4-SCSCSCSC", ReasonId::StrongRing},
      {"VVVV-4-CSCSCSCS", ReasonId::StrongRing},
      {"LLLL-2-SCSSSCSS", ReasonId::iWRing},
      {"LLLL-3-SCSCSCS", ReasonId::iXYRing},
      {"LLLL-2-SSSCSSSC", ReasonId::iSRing},
      {"LLVL-2-SCSSCSSS", ReasonId::iM2Ring},
      {"LLVL-3-SCSSCSSC", ReasonId::iM3Ring},
      {"LLLL-1-SSSSSSSS", ReasonId::iL1Ring},
      {"LVLL-2-SSCSSSSC", ReasonId::iL2Ring},
      {"LVVL-2-SSCSCSSS", ReasonId::iL2Ring},
      {"LVVL-3-SSCSCSSC", ReasonId::iL3Ring},
      {"LLLL-2-SCSSSSSC", ReasonId::iH1Ring},
      {"LLLL-2-SCSCSSSS", ReasonId::iH2Ring},
      {"LLLL-3-SCSCSSSC", ReasonId::iH3Ring},
      // grouped wings
      {"VVV-3-CSCSC+", ReasonId::GroupedXYWing},
      {"VLV-2-CSSSC+", ReasonId::GroupedWWing},
      {"LVL-2-SSCSS+", ReasonId::GroupedSWing},
      {"VLL-2-CSSCS+", ReasonId::GroupedM2Wing},
      {"VLL-3-CSSCS+", ReasonId::GroupedM3Wing},
      {"LLL-1-SSSSS+", ReasonId::GroupedL1Wing},
      {"LLL-2-SCSSS+", ReasonId::GroupedL2Wing},
      {"LLL-2-SCSCS+", ReasonId::GroupedL2Wing},
      {"LLL-3-SCSCS+", ReasonId::GroupedL3Wing},
      {"VLL-2-CSSSS+", ReasonId::GroupedH1Wing},
      {"VVL-2-CSCSS+", ReasonId::GroupedH2Wing},
      {"VVL-3-CSCSS+", ReasonId::GroupedH3Wing},
      {"LLLL-4-SCSCSCS+", ReasonId::GroupedStrongWing},
      {"LLLL-2-SCSSSCS+", ReasonId::GroupediWWing},
      {"VLVL-2-CSSSCCS+", ReasonId::GroupedDualWWing},
      {"LLLL-3-SCSCSCS+", ReasonId::GroupediXYWing},
      {"LLLL-2-SSSCSSS+", ReasonId::GroupediSWing},
      {"LLVL-2-SCSSCSS+", ReasonId::GroupediM2Wing},
      {"LLVL-3-SCSSCSS+", ReasonId::GroupediM3Wing},
      {"LLLL-1-SSSSSSS+", ReasonId::GroupediL1Wing},
      {"LVLL-2-SSCSSSS+", ReasonId::GroupediL2Wing},
      {"LVVL-2-SSCSCSS+", ReasonId::GroupediL2Wing},
      {"LVVL-3-SSCSCSS+", ReasonId::GroupediL3Wing},
      {"LLLL-2-SCSSSSS+", ReasonId::GroupediH1Wing},
      {"LLLL-2-SCSCSSS+", ReasonId::GroupediH2Wing},
      {"LLLL-3-SCSCSSS+", ReasonId::GroupediH3Wing},
      // grouped rings
      {"VLVL-2-CSSSCSSS+", ReasonId::GroupedWRing},
      {"LVLV-2-SSCSSSCS+", ReasonId::GroupedWRing},
      {"VLL-2-CSSCSS+", ReasonId::GroupedM2Ring},
      {"LLV-2-SCSSCS+", ReasonId::GroupedM2Ring},
      {"LVL-2-SSCSSC+", ReasonId::GroupedM2Ring},
      {"LLL-1-SSSSSS+", ReasonId::GroupedL1Ring},
      {"LLL-2-SCSCSS+", ReasonId::GroupedL2Ring},
      {"LLL-2-SCSSSC+", ReasonId::GroupedL2Ring},
      {"LLL-2-SCSCSS+", ReasonId::GroupedL2Ring},
      {"LVV-2-SSCSCS+", ReasonId::GroupedH2Ring},
      {"VLV-2-CSSSCS+", ReasonId::GroupedH2Ring},
      {"LLLL-4-SCSCSCSC+", ReasonId::GroupedStrongRing},
      {"VVVV-4-CSCSCSCS+", ReasonId::GroupedStrongRing},
      {"LLLL-2-SCSSSCSS+", ReasonId::GroupediWRing},
      {"LLLL-3-SCSCSCS+", ReasonId::GroupediXYRing},
      {"LLLL-2-SSSCSSSC+", ReasonId::GroupediSRing},
      {"LLVL-2-SCSSCSSS+", ReasonId::GroupediM2Ring},
      {"LLVL-3-SCSSCSSC+", ReasonId::GroupediM3Ring},
      {"LLLL-1-SSSSSSSS+", ReasonId::GroupediL1Ring},
      {"LVLL-2-SSCSSSSC+", ReasonId::GroupediL2Ring},
      {"LVVL-2-SSCSCSSS+", ReasonId::GroupediL2Ring},
      {"LVVL-3-SSCSCSSC+", ReasonId::GroupediL3Ring},
      {"LLLL-2-SCSSSSSC+", ReasonId::GroupediH1Ring},
      {"LLLL-2-SCSCSSSS+", ReasonId::GroupediH2Ring},
      {"LLLL-3-SCSCSSSC+", ReasonId::GroupediH3Ring},
    };

    if (WING_TABLE.find(finalString) != WING_TABLE.end()) {
      event.detailedReason = WING_TABLE.at(finalString);
    } else if (WING_TABLE.find(revFinalString) != WING_TABLE.end()) {
      event.detailedReason = WING_TABLE.at(revFinalString);
    }
  }

  // any other type of event is valid
  return true;
}

bool AicSearcher::path_contains_node(AicNodeID start, AicSearchNode *cur, AicNodeID node) const {
  while (cur) {
    if (cur->node == node) {
      return true;
    }
    cur = cur->parent;
  }
  return false;
}

AicPath AicSearcher::reconstruct_path(AicSearchNode *end) const {
  AicPath p;
  std::vector<AicNode> rev_nodes;
  std::vector<EdgeType> rev_edges;

  AicSearchNode *cur = end;
  while (cur) {
    rev_nodes.push_back(graph->nodes[cur->node]);

    if (cur->parent) {
      rev_edges.push_back(cur->next_type);
    }

    cur = cur->parent;
  }

  p.nodes.assign(rev_nodes.rbegin(), rev_nodes.rend());
  p.edges.assign(rev_edges.rbegin(), rev_edges.rend());
  return p;
}

std::optional<Event> AicSearcher::execute_aic_rules(
  AicNodeID start,
  AicNodeID end,
  AicSearchNode *end_state) const
{
  AicNode &Start = graph->nodes[start];
  AicNode &End = graph->nodes[end];

  Digit start_digit = *Start.digitSet.begin();
  Digit end_digit = *End.digitSet.begin();

  // AIC Type 2 requires one end to be a singleton
  Cell start_cell = *Start.cellSet.begin();
  Cell end_cell = *End.cellSet.begin();

  // ensure that the chain contains groups if needed
  if (reason == ReasonId::GroupedXChain || reason == ReasonId::GroupedAIC) {
    AicPath path = reconstruct_path(end_state);
    bool chain_has_groups = false;
    for (const AicNode &node : path.nodes) {
      chain_has_groups |= node.isGrouped;
    }
    if (!chain_has_groups) {
      return {};
    }
  }

  // AIC Type 1
  if (start_digit == end_digit && !are_weakly_linked(start, end)) {
    CellSet peers = board.getPeers(Start.cellSet | End.cellSet);
    AicPath path = reconstruct_path(end_state);

    Event event(EventType::RemoveCandidate, reason,
                                            reason == ReasonId::AIC ? ReasonId::AICType1 :
                                            reason == ReasonId::GroupedAIC ? ReasonId::GroupedAICType1 :
                                            reason);
    for (const AicNode &node : path.nodes) {
      event.addSource(node.cellSet, node.digitSet);
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
  if (start_digit != end_digit && board.sees(Start.cellSet, End.cellSet)) {
    AicPath path = reconstruct_path(end_state);

    if ((!Start.isGrouped && board.hasCandidate(start_cell, end_digit)) ||
        (!End.isGrouped && board.hasCandidate(end_cell, start_digit))) {
      Event event(EventType::RemoveCandidate, reason,
                                              reason == ReasonId::AIC ? ReasonId::AICType2 :
                                              reason == ReasonId::GroupedAIC ? ReasonId::GroupedAICType2 :
                                              reason);
      for (const AicNode &node : path.nodes) {
        event.addSource(node.cellSet, node.digitSet);
      }
      if (!Start.isGrouped && board.hasCandidate(start_cell, end_digit)) {
        event.addOperation(start_cell, end_digit);
      }
      if (!End.isGrouped && board.hasCandidate(end_cell, start_digit)) {
        event.addOperation(end_cell, start_digit);
      }
      return event;
    }
  }

  // AIC Type 3 (Ring)
  if (are_weakly_linked(start, end)) {
    AicPath path = reconstruct_path(end_state);

    Event event(EventType::RemoveCandidate, reason,
                                            reason == ReasonId::XChain ? ReasonId::XRing :
                                            reason == ReasonId::XYChain ? ReasonId::XYRing :
                                            reason == ReasonId::AIC ? ReasonId::AICType3 :
                                            reason == ReasonId::GroupedAIC ? ReasonId::GroupedAICType3 :
                                            reason == ReasonId::GroupedXChain ? ReasonId::GroupedXRing :
                                            reason);
    for (const AicNode &node : path.nodes) {
      event.addSource(node.cellSet, node.digitSet);
    }
    {
      // add again the first node since this is a ring
      AicNode &node = path.nodes[0];
      event.addSource(node.cellSet, node.digitSet);
    }

    for (int i = 0; i < path.nodes.size(); ++i) {
      AicNode &node = path.nodes[i];
      AicNode &nextNode = path.nodes[(i+1) % path.nodes.size()]; // wrap around when reaching the last node

      if (node.cellSet != nextNode.cellSet) {
        // different cell, same digit
        CellSet peers = board.getPeers(node.cellSet | nextNode.cellSet); 
        Digit digit = *node.digitSet.begin();
        for (Cell idx : peers) {
          if (board.hasCandidate(idx, digit)) {
            event.addOperation(idx, digit);
          }
        }
      }
      if (node.cellSet == nextNode.cellSet) {
        // same cell, different digit
        Cell cell = *node.cellSet.begin();
        Digit digit = *node.digitSet.begin();
        Digit nextDigit = *nextNode.digitSet.begin();
        DigitSet toRemove = board.getCandidates(cell) - node.digitSet - nextNode.digitSet;
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

bool AicSearcher::execute_coloring_rules(
  AicNodeID start,
  const std::vector<ColorSearchState> &states) const
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
    AicNode &tmp = graph->nodes[state.node];

    ColorNode node{
      .cell = static_cast<Cell>(*tmp.cellSet.begin()),
      .digit = static_cast<Digit>(*tmp.digitSet.begin())
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
      if (eventQueue.enqueue(board, event)) return true;
    }
    return false;
  }

  // Color Wrap test
  auto scanColor = [&](const std::vector<ColorNode> &nodes, const std::vector<ColorNode> &other) -> bool
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
          detailedReason = ReasonId::_3DMedusaColorWrap;
          goto end_loop;
        }
        if (a.digit == b.digit && board.sees(a.cell, b.cell)) {
          // 3D Medusa Rule 2 : Twice in a Unit (Color Wrap)
          // Simple Coloring : Color Wrap
          found = true;
          if (reason == ReasonId::SimpleColoring) {
            detailedReason = ReasonId::SimpleColoringColorWrap;
          } else {
            detailedReason = ReasonId::_3DMedusaColorWrap;
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
      Event event(EventType::SetValue, reason, detailedReason);
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
      if (eventQueue.enqueue(board, event)) return true;
    }

    return false;
  };

  bool first = scanColor(firstColorNodes, secondColorNodes);
  if (first) return true;

  bool second = scanColor(secondColorNodes, firstColorNodes);
  if (second) return true;

  // Color Trap test
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
        event.detailedReason = ReasonId::_3DMedusaColorTrap;
      }
      if (a.digit == b.digit) {
        // 3D Medusa Rule 4 : Two colours elsewhere (Color Trap)
        // Simple Coloring : Color Trap
        CellSet toRemove = board.getPeersContaining({a.cell, b.cell}, a.digit);
        if (!toRemove.empty()) {
          for (Cell idx : toRemove) {
            event.addOperation(idx, a.digit);
          }
        }
        if (reason == ReasonId::SimpleColoring) {
          event.detailedReason = ReasonId::SimpleColoringColorTrap;
        } else {
          event.detailedReason = ReasonId::_3DMedusaColorTrap;
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
        event.detailedReason = ReasonId::_3DMedusaColorTrap;
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
    if (eventQueue.enqueue(board, event)) return true;
  }

  return false;
}

bool AicSearcher::are_weakly_linked(AicNodeID a, AicNodeID b) const {
  for (const AicEdge &edge : graph->weak_links[a]) {
    if (edge.to == b) {
      return true;
    }
  }

  return false;
}
