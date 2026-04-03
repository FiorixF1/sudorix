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
using AlsNode = uint32_t;

// Deserialized node from ID
struct FullAlsNode {
  CellSet cellSet;
  DigitSet digitSet;
  bool isGrouped;
};

struct AlsEdge {
  AlsNode to = 0;
  Digit rcc = 0;

  bool operator==(const AlsEdge &other) const {
    return to == other.to && rcc == other.rcc;
  }
};

struct AlsPath {
  std::vector<FullAlsNode> nodes;
  std::vector<AlsEdge> edges;  // edges[i] connects nodes[i] -> nodes[i+1] through edges[i].rcc
};

struct AlsGraph {
  std::vector<AlsNode> nodes;
  std::map<AlsNode, std::vector<AlsEdge>> links;
  // qui consideriamo solo link fra ALS tramite RCC
};

constexpr int DEFAULT_MAX_DEPTH = 6;
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

  void build_nodes(std::vector<AlsNode> &nodes);

  void build_nodes_in_unit(std::vector<AlsNode> &nodes, const Unit &unit);

  void add_node_if_new(std::vector<AlsNode> &nodes, const CellSet &cells, const DigitSet &digits) const;

  AlsNode get_node_id(Digit digit, Cell cell) const;

  AlsNode get_node_id(Digit digit, const CellSet &cells) const;

  AlsNode get_node_id(const DigitSet &digits, Cell cell) const;

  AlsNode get_node_id(const DigitSet &digits, const CellSet &cells) const;

  void build_links(const std::vector<AlsNode> &nodes,
                   std::map<AlsNode, std::vector<AlsEdge>> &links);

  bool is_rcc(AlsNode a, AlsNode b, Digit digit) const;

  void add_edge(std::map<AlsNode, std::vector<AlsEdge>> &adj, AlsNode a, AlsNode b, Digit rcc);
};

/* ---------------------------------------------------------------------- */

struct AlsSearchState {
  AlsNode node = 0;
  Digit last_rcc = 0;
};

struct AlsParent {
  int prev_state_index = -1;
  AlsNode prev_node = 0;
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
  std::set<AlsNode> visited;

  std::optional<Event> als_search_from(AlsGraph &graph);

  bool path_contains_node(int state_idx,
                          AlsNode node,
                          const std::vector<AlsSearchState> &states,
                          const std::vector<AlsParent> &parents) const;

  AlsPath reconstruct_path(int end_state_idx,
                           const std::vector<AlsSearchState> &states,
                           const std::vector<AlsParent> &parents) const;

  std::optional<Event> execute_als_rules(
    AlsNode start,
    AlsNode end,
    int end_state_idx,
    const std::vector<AlsSearchState> &states,
    const std::vector<AlsParent> &parents) const;

  DigitSet get_rcc_set(const AlsPath &path) const;

  CellSet get_common_non_rcc_digits(const AlsPath &path,
                                    const DigitSet &startDigits,
                                    const DigitSet &endDigits) const;

  std::optional<Event> build_circular_elimination_event(const AlsPath &path,
                                                        ReasonId detailedReason) const;
  std::optional<Event> build_endpoint_elimination_event(const AlsPath &path,
                                                        Digit z,
                                                        ReasonId detailedReason) const;
};

#endif // ALS_HPP
