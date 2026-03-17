#ifndef AIC_HPP
#define AIC_HPP

#include <set>
#include <map>
#include <vector>
#include <deque>
#include <cstdint>
#include <algorithm>
#include <optional>
#include "encoder.hpp"

// Unique ID for nodes.
// The ID, both for singletons and groups, is built as follows:
// - Encode the cell/group as if it where the source of a technique
//   -> for singletons it will produce only one code
//   -> for grouped cells it will work because they are always part of the same box
// - Translate the digit (or group of) as a 9-bit mask
// - Shift the digit mask left by 16 bits
// - Concatenate the code and the digit mask with a bitwise OR
// Result:
// 0000000z zzzzzzzz 00yyyyyy yyyxxxxx
//        ^digits      ^cells    ^unit
// This way the ID can be used also to represent the node itself
// because all its data can be univocally extracted.
using AicNode = uint32_t;

enum class ColorType : uint8_t {
  FIRST,
  SECOND
};

enum class EdgeType : uint8_t {
  STRONG,
  WEAK
};

struct AicPath {
  std::vector<AicNode> nodes;
  std::vector<EdgeType> edges;  // edges[i] connects nodes[i] -> nodes[i+1]
};

struct AicGraph {
  std::vector<AicNode> nodes;
  std::map<AicNode, std::vector<AicNode>> strong_links;
  // weak non lo materializziamo tutto: lo calcoliamo on-demand
};

struct AicConfig {
  bool useWeakLinks;
  bool multiDigit;
  bool useGroupedCells;
  bool useStrongBivalues;
  bool useStrongBilocations;
  bool useWeakInCell;
  bool useWeakInUnit;
  int max_depth;
  std::string pattern;
};

/* ---------------------------------------------------------------------- */

class AicGraphBuilder {
public:
  explicit AicGraphBuilder(const SudokuBoard &board);

  AicGraph build();

  AicGraph prune(const AicGraph &graph, const AicConfig &config);

private:
  const SudokuBoard &board;

  void build_singleton_nodes(std::vector<AicNode> &nodes);

  void add_group_if_new(std::vector<AicNode> &nodes, Digit digit, const CellSet &cells);

  void build_grouped_nodes(std::vector<AicNode> &nodes);

  AicNode get_node_id(Digit digit, Cell cell) const;

  AicNode get_node_id(Digit digit, const CellSet &cells) const;

  AicNode get_node_id(const DigitSet &digits, Cell cell) const;

  AicNode get_node_id(const DigitSet &digits, const CellSet &cells) const;

  void add_strong_edge(std::map<AicNode, std::vector<AicNode>> &adj, AicNode a, AicNode b);

  void build_strong_links(const std::vector<AicNode> &nodes,
                          std::map<AicNode, std::vector<AicNode>> &strong_links);

  void build_strong_links_in_units(const std::vector<AicNode> &nodes,
                                   std::map<AicNode, std::vector<AicNode>> &strong_links,
                                   const std::vector<Unit> &units,
                                   Digit d);

  void build_strong_links_in_cells(const std::vector<AicNode> &nodes,
                                   std::map<AicNode, std::vector<AicNode>> &strong_links,
                                   Cell cell);

  void build_grouped_strong_row_box(const std::vector<AicNode> &nodes,
                                    std::map<AicNode, std::vector<AicNode>> &strong_links,
                                    Digit d);

  void build_grouped_strong_col_box(const std::vector<AicNode> &nodes,
                                    std::map<AicNode, std::vector<AicNode>> &strong_links,
                                    Digit d);

  void build_grouped_strong_box_row(const std::vector<AicNode> &nodes,
                                    std::map<AicNode, std::vector<AicNode>> &strong_links,
                                    Digit d);

  void build_grouped_strong_box_col(const std::vector<AicNode> &nodes,
                                    std::map<AicNode, std::vector<AicNode>> &strong_links,
                                    Digit d);

  void build_grouped_strong_eri(const std::vector<AicNode> &nodes,
                                std::map<AicNode, std::vector<AicNode>> &strong_links,
                                Digit d);
};

/* ---------------------------------------------------------------------- */

struct AicSearchState {
  AicNode node;
  union {
    EdgeType next_type;
    ColorType next_color;
  };
};

struct AicParent {
  int prev_state_index = -1;
  AicNode prev_node = 0;
  union {
    EdgeType edge_used;
    ColorType color_used;
  };
};

class AicSearcher {
public:
  AicSearcher(const SudokuBoard &board);

  const AicConfig &setConfigAndReturn(ReasonId reason);

  std::optional<Event> run_search(AicGraph &graph);

private:
  const SudokuBoard &board;
  ReasonId reason;
  AicConfig config;
  std::set<AicNode> visited;

  std::optional<Event> aic_search_from(AicNode start, AicGraph &graph);
  std::optional<Event> coloring_search_from(AicNode start, AicGraph &graph);

  bool path_contains_node(int state_idx,
                          AicNode node,
                          const std::vector<AicSearchState> &states,
                          const std::vector<AicParent> &parents) const;

  AicPath reconstruct_path(int end_state_idx,
                           const std::vector<AicSearchState> &states,
                           const std::vector<AicParent> &parents) const;

  std::optional<Event> execute_aic_rules(
    AicNode start,
    AicNode end,
    int end_state_idx,
    const std::vector<AicSearchState> &states,
    const std::vector<AicParent> &parents) const;
  std::optional<Event> execute_coloring_rules(
    AicNode start,
    const std::vector<AicSearchState> &states) const;

  bool are_weakly_linked(AicNode a, AicNode b) const;
};

#endif // AIC_HPP
