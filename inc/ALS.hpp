#ifndef ALS_HPP
#define ALS_HPP

#include <set>
#include <map>
#include <vector>
#include <deque>
#include <cstdint>
#include <algorithm>
#include <optional>
#include "SudokuCell.hpp"

class SudokuBoard;
class EventQueue;

// Unique ID for nodes.
// Same logic as in AIC.
// 0000000z zzzzzzzz 00yyyyyy yyyxxxxx
//        ^digits      ^cells    ^unit
using AlsNodeID = uint32_t;

// Deserialized node from ID
struct AlsNode {
  AlsNodeID id;
  CellSet cellSet;
  DigitSet digitSet;
  bool isGrouped;
};

struct AlsEdge {
  AlsNodeID to = 0;
  Digit rcc = 0;

  bool operator==(const AlsEdge &other) const {
    return to == other.to && rcc == other.rcc;
  }
};

struct AlsPath {
  std::vector<AlsNode> nodes;
  std::vector<AlsEdge> edges;  // edges[i] connects nodes[i] -> nodes[i+1] through edges[i].rcc
};

struct AlsComparator {
  // std::popcount is available since C++20
  bool operator()(AlsNodeID a, AlsNodeID b) const {
    // give priority to ALSs with fewer digits
    auto pa = std::popcount(a >> 16);
    auto pb = std::popcount(b >> 16);

    if (pa != pb)
      return pa < pb;

    // otherwise use the ID numeric value as tiebreaker
    return a < b;
  }
};

using AlsGraphNodes = std::map<AlsNodeID, AlsNode, AlsComparator>;
using AlsGraphEdges = std::map<AlsNodeID, std::vector<AlsEdge>, AlsComparator>;

struct AlsGraph {
  AlsGraphNodes nodes;
  AlsGraphEdges links;
  // qui consideriamo solo link fra ALS tramite RCC
};

constexpr int DEFAULT_MAX_DEPTH = 4;
constexpr int DEFAULT_MAX_ALS_SIZE = 5;

struct AlsConfig {
  int min_depth = 1;
  int max_depth = DEFAULT_MAX_DEPTH;         // number of RCC edges
  int max_als_size = DEFAULT_MAX_ALS_SIZE;   // max cells inside an ALS
  bool allow_box = true;
  bool allow_row = true;
  bool allow_col = true;
};

/* ---------------------------------------------------------------------- */

class AlsGraphBuilder {
public:
  explicit AlsGraphBuilder(const SudokuBoard &board);

  AlsGraph build();

  AlsGraph prune(const AlsGraph &graph);

private:
  const SudokuBoard &board;

  void build_nodes(AlsGraphNodes &nodes);

  void build_nodes_in_unit(AlsGraphNodes &nodes, const Unit &unit);

  void add_node_if_new(AlsGraphNodes &nodes, const CellSet &cells, const DigitSet &digits) const;

  AlsNodeID get_node_id(Digit digit, Cell cell) const;

  AlsNodeID get_node_id(Digit digit, const CellSet &cells) const;

  AlsNodeID get_node_id(const DigitSet &digits, Cell cell) const;

  AlsNodeID get_node_id(const DigitSet &digits, const CellSet &cells) const;

  void build_links(AlsGraphNodes &nodes,
                   AlsGraphEdges &links);

  bool is_rcc(AlsNode &a, AlsNode &b, Digit digit) const;

  void add_edge(AlsGraphEdges &adj, AlsNodeID a, AlsNodeID b, Digit rcc);
};

/* ---------------------------------------------------------------------- */

struct AlsSearchNode {
  AlsNodeID start;
  AlsNodeID node;
  Digit last_rcc;
  int depth;

  AlsSearchNode *parent;
  int refcount;
};

class AlsSearcher {
public:
  AlsSearcher(const SudokuBoard &board, EventQueue &eventQueue);

  const AlsConfig &setConfigAndReturn(ReasonId reason);

  // true if an event has been produced
  bool runSearch(AlsGraph &graph);

private:
  const SudokuBoard &board;
  EventQueue &eventQueue;
  ReasonId reason;
  AlsConfig config;
  std::set<AlsNodeID> visited;

  bool als_search_from(AlsGraph &graph);

  bool path_contains_node(AlsNodeID start, AlsSearchNode *cur, AlsNodeID node) const;

  AlsPath reconstruct_path(AlsGraph &graph, AlsSearchNode *end) const;

  std::optional<Event> execute_als_rules(
    AlsGraph &graph,
    AlsNodeID start,
    AlsNodeID end,
    AlsSearchNode *end_state) const;

  DigitSet get_rcc_set(const AlsPath &path) const;

  std::optional<Event> build_circular_elimination_event(AlsPath &path,
                                                        ReasonId detailedReason) const;
  std::optional<Event> build_endpoint_elimination_event(AlsPath &path,
                                                        DigitSet zs,
                                                        ReasonId detailedReason) const;
};

#endif // ALS_HPP
