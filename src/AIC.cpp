#include "AIC.hpp"
#include "encoder.hpp"

AicGraphBuilder::AicGraphBuilder(const SudokuBoard &board) : board(board) { }

AicGraph AicGraphBuilder::build() {
  AicGraph g;

  build_singleton_nodes(g.nodes);
  build_grouped_nodes(g.nodes);

  build_strong_links(g.nodes, g.strong_links);

  return g;
}

AicGraph AicGraphBuilder::prune(AicGraph &graph, const AicConfig &config) {
  AicGraph prunedGraph;

  // build edges according to config
  for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
    AicNode &source = graph.nodes[it->first];

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

    auto &edges = it->second;
    for (AicNodeID edge : edges) {
      AicNode &target = graph.nodes[edge];
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
      add_strong_edge(prunedGraph.strong_links, source.id, target.id);
    }
  }

  // build nodes from filtered edges
  for (auto it = prunedGraph.strong_links.begin(); it != prunedGraph.strong_links.end(); ++it) {
    prunedGraph.nodes[it->first] = graph.nodes[it->first];
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
      AicNode &to = graph.nodes[edge];

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

void AicGraphBuilder::build_singleton_nodes(std::map<AicNodeID, AicNode> &nodes) {
  for (Cell cell = 0; cell < 81; ++cell) {
    for (Digit d = 1; d <= 9; ++d) {
      if (board.isSolved(cell) || !board.hasCandidate(cell, d)) {
        continue;
      }

      AicNodeID id = get_node_id(d, cell);
      if (!id) return;
      AicNode node{
        .id = id,
        .cellSet = {cell},
        .digitSet = {d},
        .isGrouped = false
      };
      nodes[id] = node;
    }
  }
}

void AicGraphBuilder::add_group_if_new(std::map<AicNodeID, AicNode> &nodes, Digit digit, const CellSet &cells) {
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

void AicGraphBuilder::build_grouped_nodes(std::map<AicNodeID, AicNode> &nodes) {
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

AicNodeID AicGraphBuilder::get_node_id(Digit digit, Cell cell) const {
  return get_node_id(DigitSet({digit}), CellSet({cell}));
}

AicNodeID AicGraphBuilder::get_node_id(Digit digit, const CellSet &cells) const {
  return get_node_id(DigitSet({digit}), cells);
}

AicNodeID AicGraphBuilder::get_node_id(const DigitSet &digits, Cell cell) const {
  return get_node_id(digits, CellSet({cell}));
}

AicNodeID AicGraphBuilder::get_node_id(const DigitSet &digits, const CellSet &cells) const {
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

void AicGraphBuilder::add_strong_edge(std::map<AicNodeID, std::vector<AicNodeID>> &adj, AicNodeID a, AicNodeID b) {
  if (a == b) {
    return;
  }

  auto add_one_way = [&](AicNodeID x, AicNodeID y) {
    auto &v = adj[x];
    if (std::find(v.begin(), v.end(), y) == v.end()) {
      v.push_back(y);
    }
  };

  add_one_way(a, b);
  add_one_way(b, a);
}

void AicGraphBuilder::build_strong_links(const std::map<AicNodeID, AicNode> &nodes,
                                         std::map<AicNodeID, std::vector<AicNodeID>> &strong_links) {
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

  // Grouped cells (including empty rectangle intersection)
  for (Digit d = 1; d <= 9; ++d) {
    // singletons and groups in a row
    build_grouped_strong_row_box(nodes, strong_links, d);
    // singletons and groups in a column
    build_grouped_strong_col_box(nodes, strong_links, d);
    // minirows in a box
    //build_grouped_strong_box_row(nodes, strong_links, d);
    // minicolumns in a box
    //build_grouped_strong_box_col(nodes, strong_links, d);
    // empty rectangle intersection
    build_grouped_strong_eri(nodes, strong_links, d);
  }
}

void AicGraphBuilder::build_strong_links_in_units(const std::map<AicNodeID, AicNode> &nodes,
                                                  std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                                  const std::vector<Unit> &units,
                                                  Digit d) {
  for (const Unit &unit : units) {
    CellSet pos = board.getPositionsOfDigit(unit, d);
    if (pos.size() != 2) {
      continue;
    }

    std::vector<int> v = pos.to_vector();
    Cell c1 = v[0];
    Cell c2 = v[1];

    AicNodeID n1 = get_node_id(d, c1);
    AicNodeID n2 = get_node_id(d, c2);
    if (n1 && n2) add_strong_edge(strong_links, n1, n2);
  }
}

void AicGraphBuilder::build_strong_links_in_cells(const std::map<AicNodeID, AicNode> &nodes,
                                                  std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                                  Cell cell) {
  DigitSet candidates = board.getCandidates(cell);
  if (candidates.size() != 2) {
    return;
  }

  std::vector<int> v = candidates.to_vector();
  Digit d1 = v[0];
  Digit d2 = v[1];

  AicNodeID n1 = get_node_id(d1, cell);
  AicNodeID n2 = get_node_id(d2, cell);
  if (n1 && n2) add_strong_edge(strong_links, n1, n2);
}

void AicGraphBuilder::build_grouped_strong_row_box(const std::map<AicNodeID, AicNode> &nodes,
                                                   std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                                   Digit d) {
  for (const Unit &r : SudokuBoard::getRows()) {
    CellSet pos = board.getPositionsOfDigit(r, d);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &b : SudokuBoard::getBoxes()) {
      CellSet g = pos & b;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(d, parts[0]);
      AicNodeID b = get_node_id(d, parts[1]);
      if (a && b) add_strong_edge(strong_links, a, b);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_col_box(const std::map<AicNodeID, AicNode> &nodes,
                                                   std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                                   Digit d) {
  for (const Unit &c : SudokuBoard::getColumns()) {
    CellSet pos = board.getPositionsOfDigit(c, d);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &b : SudokuBoard::getBoxes()) {
      CellSet g = pos & b;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(d, parts[0]);
      AicNodeID b = get_node_id(d, parts[1]);
      if (a && b) add_strong_edge(strong_links, a, b);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_box_row(const std::map<AicNodeID, AicNode> &nodes,
                                                   std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                                   Digit d) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, d);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &r : SudokuBoard::getRows()) {
      CellSet g = pos & r;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(d, parts[0]);
      AicNodeID b = get_node_id(d, parts[1]);
      if (a && b) add_strong_edge(strong_links, a, b);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_box_col(const std::map<AicNodeID, AicNode> &nodes,
                                                   std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                                   Digit d) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, d);
    if (pos.size() < 3) {
      continue;
    }

    std::vector<CellSet> parts;
    for (const Unit &c : SudokuBoard::getColumns()) {
      CellSet g = pos & c;
      if (!g.empty()) parts.push_back(g);
    }

    if (parts.size() == 2) {
      AicNodeID a = get_node_id(d, parts[0]);
      AicNodeID b = get_node_id(d, parts[1]);
      if (a && b) add_strong_edge(strong_links, a, b);
    }
  }
}

void AicGraphBuilder::build_grouped_strong_eri(const std::map<AicNodeID, AicNode> &nodes,
                                               std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                               Digit d) {
  for (const Unit &b : SudokuBoard::getBoxes()) {
    CellSet pos = board.getPositionsOfDigit(b, d);
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
        AicNodeID r = get_node_id(d, row_part);
        AicNodeID c = get_node_id(d, col_part);
        if (r && c) add_strong_edge(strong_links, r, c);
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
        .max_depth = 11,
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
        .max_depth = 11,
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
    default:
      break;
  }

  this->reason = reason;

  return config;
}

std::optional<Event> AicSearcher::runSearch(AicGraph &graph) {
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
        if (!valid) return {};
      }
      return maybeEvent;
    }
  } else {
    // color-based search, only strong links
    for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
      AicNodeID start = it->first;

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
  std::deque<AicSearchNode *> q;

  // Partiamo dal nodo iniziale e imponiamo che il primo arco sia strong.
  for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
    AicNodeID start = it->first;
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
    AicNode &current = graph.nodes[cur->node];
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
      for (AicNodeID nb : graph.strong_links[cur->node]) {
        if (path_contains_node(cur->start, cur, nb)) {
          continue;
        }

        AicSearchNode *child = make_node(cur->start,
                                         nb,
                                         EdgeType::WEAK,
                                         cur->depth + 1,
                                         cur);

        // Look for eliminations according to AIC rules
        std::optional<Event> event = execute_aic_rules(graph, cur->start, nb, child);
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
    } else {
      for (auto it = graph.strong_links.begin(); it != graph.strong_links.end(); ++it) {
        AicNodeID nb = it->first;
        if (nb == cur->node) {
          continue;
        }
        if (path_contains_node(cur->start, cur, nb)) {
          continue;
        }
        if (!are_weakly_linked(graph.nodes[cur->node], graph.nodes[nb])) {
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

  return {};
}

std::optional<Event> AicSearcher::coloring_search_from(AicNodeID start, AicGraph &graph) {
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
    AicNode &current = graph.nodes[cur->node];
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
    for (AicNodeID nb : graph.strong_links[cur->node]) {
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
  std::optional<Event> event = execute_coloring_rules(graph, start, states);

  return event;
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

AicPath AicSearcher::reconstruct_path(AicGraph &graph, AicSearchNode *end) const {
  AicPath p;
  std::vector<AicNode> rev_nodes;
  std::vector<EdgeType> rev_edges;

  AicSearchNode *cur = end;
  while (cur) {
    rev_nodes.push_back(graph.nodes[cur->node]);

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
  AicGraph &graph,
  AicNodeID start,
  AicNodeID end,
  AicSearchNode *end_state) const
{
  AicNode &Start = graph.nodes[start];
  AicNode &End = graph.nodes[end];

  Digit start_digit = *Start.digitSet.begin();
  Digit end_digit = *End.digitSet.begin();

  // AIC Type 2 requires one end to be a singleton
  Cell start_cell = *Start.cellSet.begin();
  Cell end_cell = *End.cellSet.begin();

  // AIC Type 1
  if (start_digit == end_digit && !are_weakly_linked(Start, End)) {
    CellSet peers = board.getPeers(Start.cellSet | End.cellSet);
    AicPath path = reconstruct_path(graph, end_state);

    Event event(EventType::RemoveCandidate, reason == ReasonId::AIC ? ReasonId::AICType1 :
                                            reason == ReasonId::GroupedAIC ? ReasonId::GroupedAICType1 :
                                            reason);
    for (int i = 0; i < path.nodes.size(); ++i) {
      AicNode &node = path.nodes[i];
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
    AicPath path = reconstruct_path(graph, end_state);

    if ((!Start.isGrouped && board.hasCandidate(start_cell, end_digit)) ||
        (!End.isGrouped && board.hasCandidate(end_cell, start_digit))) {
      Event event(EventType::RemoveCandidate, reason == ReasonId::AIC ? ReasonId::AICType2 :
                                              reason == ReasonId::GroupedAIC ? ReasonId::GroupedAICType2 :
                                              reason);
      for (int i = 0; i < path.nodes.size(); ++i) {
        AicNode &node = path.nodes[i];
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
  if (are_weakly_linked(Start, End)) {
    AicPath path = reconstruct_path(graph, end_state);

    Event event(EventType::RemoveCandidate, reason == ReasonId::XChain ? ReasonId::XRing :
                                            reason == ReasonId::XYChain ? ReasonId::XYRing :
                                            reason == ReasonId::AIC ? ReasonId::AICType3 :
                                            reason == ReasonId::GroupedAIC ? ReasonId::GroupedAICType3 :
                                            reason == ReasonId::GroupedXChain ? ReasonId::GroupedXRing :
                                            reason);
    for (int i = 0; i < path.nodes.size(); ++i) {
      AicNode &node = path.nodes[i];
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

std::optional<Event> AicSearcher::execute_coloring_rules(
  AicGraph &graph,
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
    AicNode &tmp = graph.nodes[state.node];

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

bool AicSearcher::are_weakly_linked(AicNode &a, AicNode &b) const {
  Digit aDigit = *a.digitSet.begin();
  Digit bDigit = *b.digitSet.begin();

  if ((a.isGrouped || b.isGrouped) && !config.useGroupedCells) {
    return false;
  }

  // candidates inside a cell
  if (!a.isGrouped && !b.isGrouped && a.cellSet == b.cellSet && aDigit != bDigit) {
    if (config.multiDigit && config.useWeakInCell) {
      return true;
    }
  }

  // cells (or group of) that can see each other
  if (aDigit == bDigit && board.sees(a.cellSet, b.cellSet)) {
    if (config.useWeakInUnit) {
      return true;
    }
  }

  return false;
}
