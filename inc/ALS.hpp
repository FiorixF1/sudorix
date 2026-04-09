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

struct AlsGraph {
  std::map<AlsNodeID, AlsNode> nodes;
  std::map<AlsNodeID, std::vector<AlsEdge>> links;
  // qui consideriamo solo link fra ALS tramite RCC
};

constexpr int DEFAULT_MAX_DEPTH = 4;
constexpr int DEFAULT_MAX_ALS_SIZE = 5;

struct AlsConfig {
  int max_depth = DEFAULT_MAX_DEPTH;         // number of RCC edges
  int max_als_size = DEFAULT_MAX_ALS_SIZE;   // max cells inside an ALS
  bool allow_box = true;
  bool allow_row = true;
  bool allow_col = true;
  bool require_distinct_rcc = true;
};

/* ---------------------------------------------------------------------- */

class AlsGraphBuilder {
public:
  explicit AlsGraphBuilder(const SudokuBoard &board);

  AlsGraph build();

  AlsGraph prune(const AlsGraph &graph);

private:
  const SudokuBoard &board;

  void build_nodes(std::map<AlsNodeID, AlsNode> &nodes);

  void build_nodes_in_unit(std::map<AlsNodeID, AlsNode> &nodes, const Unit &unit);

  void add_node_if_new(std::map<AlsNodeID, AlsNode> &nodes, const CellSet &cells, const DigitSet &digits) const;

  AlsNodeID get_node_id(Digit digit, Cell cell) const;

  AlsNodeID get_node_id(Digit digit, const CellSet &cells) const;

  AlsNodeID get_node_id(const DigitSet &digits, Cell cell) const;

  AlsNodeID get_node_id(const DigitSet &digits, const CellSet &cells) const;

  void build_links(std::map<AlsNodeID, AlsNode> &nodes,
                   std::map<AlsNodeID, std::vector<AlsEdge>> &links);

  bool is_rcc(AlsNode &a, AlsNode &b, Digit digit) const;

  void add_edge(std::map<AlsNodeID, std::vector<AlsEdge>> &adj, AlsNodeID a, AlsNodeID b, Digit rcc);
};

/* ---------------------------------------------------------------------- */

struct AlsSearchState {
  AlsNodeID node = 0;
  Digit last_rcc = 0;
};

struct AlsParent {
  int prev_state_index = -1;
  AlsNodeID prev_node = 0;
  Digit rcc = 0;
};

class AlsSearcher {
public:
  AlsSearcher(const SudokuBoard &board);

  const AlsConfig &setConfigAndReturn(ReasonId reason);

  std::optional<Event> runSearch(AlsGraph &graph);

private:
  const SudokuBoard &board;
  ReasonId reason;
  AlsConfig config;
  std::set<AlsNodeID> visited;

  std::optional<Event> als_search_from(AlsNodeID start, AlsGraph &graph);

  bool path_contains_node(AlsNodeID start,
                          int state_idx,
                          AlsNodeID node,
                          const std::vector<AlsSearchState> &states,
                          const std::vector<AlsParent> &parents) const;

  AlsPath reconstruct_path(AlsGraph &graph,
                           int end_state_idx,
                           const std::vector<AlsSearchState> &states,
                           const std::vector<AlsParent> &parents) const;

  std::optional<Event> execute_als_rules(
    AlsGraph &graph,
    AlsNodeID start,
    AlsNodeID end,
    int end_state_idx,
    const std::vector<AlsSearchState> &states,
    const std::vector<AlsParent> &parents) const;

  DigitSet get_rcc_set(const AlsPath &path) const;

  CellSet get_common_non_rcc_digits(const AlsPath &path,
                                    const DigitSet &startDigits,
                                    const DigitSet &endDigits) const;

  std::optional<Event> build_circular_elimination_event(AlsPath &path,
                                                        ReasonId detailedReason) const;
  std::optional<Event> build_endpoint_elimination_event(AlsPath &path,
                                                        Digit z,
                                                        ReasonId detailedReason) const;
};

#endif // ALS_HPP
