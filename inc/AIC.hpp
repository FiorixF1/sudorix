#ifndef AIC_HPP
#define AIC_HPP

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
//
// Note: a node could potentially have more than one representation, but if it is built with
// serialize_cellset_to_unitcodes it is guaranteed to be unique.
using AicNodeID = uint32_t;

// Actual node structure
struct AicNode {
  AicNodeID id;
  CellSet cellSet;
  DigitSet digitSet;
  bool isGrouped;
};

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
  std::map<AicNodeID, AicNode> nodes;
  std::map<AicNodeID, std::vector<AicNodeID>> strong_links;
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
  bool useRemotePairs;
  int max_depth; // max number of edges, makes sense if odd
  std::string pattern;
};

/* ---------------------------------------------------------------------- */

class AicGraphBuilder {
public:
  explicit AicGraphBuilder(const SudokuBoard &board);

  AicGraph build();

  AicGraph prune(AicGraph &graph, const AicConfig &config);

private:
  const SudokuBoard &board;

  void build_singleton_nodes(std::map<AicNodeID, AicNode> &nodes);

  void add_group_if_new(std::map<AicNodeID, AicNode> &nodes, Digit digit, const CellSet &cells);

  void build_grouped_nodes(std::map<AicNodeID, AicNode> &nodes);

  AicNodeID get_node_id(Digit digit, Cell cell) const;

  AicNodeID get_node_id(Digit digit, const CellSet &cells) const;

  AicNodeID get_node_id(const DigitSet &digits, Cell cell) const;

  AicNodeID get_node_id(const DigitSet &digits, const CellSet &cells) const;

  void add_strong_edge(std::map<AicNodeID, std::vector<AicNodeID>> &adj, AicNodeID a, AicNodeID b);

  void build_strong_links(const std::map<AicNodeID, AicNode> &nodes,
                          std::map<AicNodeID, std::vector<AicNodeID>> &strong_links);

  void build_strong_links_in_units(const std::map<AicNodeID, AicNode> &nodes,
                                   std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                   const std::vector<Unit> &units,
                                   Digit d);

  void build_strong_links_in_cells(const std::map<AicNodeID, AicNode> &nodes,
                                   std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                   Cell cell);

  void build_grouped_strong_row_box(const std::map<AicNodeID, AicNode> &nodes,
                                    std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                    Digit d);

  void build_grouped_strong_col_box(const std::map<AicNodeID, AicNode> &nodes,
                                    std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                    Digit d);

  void build_grouped_strong_box_row(const std::map<AicNodeID, AicNode> &nodes,
                                    std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                    Digit d);

  void build_grouped_strong_box_col(const std::map<AicNodeID, AicNode> &nodes,
                                    std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                    Digit d);

  void build_grouped_strong_eri(const std::map<AicNodeID, AicNode> &nodes,
                                std::map<AicNodeID, std::vector<AicNodeID>> &strong_links,
                                Digit d);
};

/* ---------------------------------------------------------------------- */

struct ColorSearchState {
  AicNodeID node;
  ColorType next_color;
};

struct AicSearchNode {
  AicNodeID start;
  AicNodeID node;
  union {
    EdgeType next_type;
    ColorType next_color;
  };
  int depth;

  AicSearchNode *parent;
  int refcount;
};

class AicSearcher {
public:
  AicSearcher(const SudokuBoard &board);

  const AicConfig &setConfigAndReturn(ReasonId reason);

  std::optional<Event> runSearch(AicGraph &graph);

private:
  const SudokuBoard &board;
  ReasonId reason;
  AicConfig config;
  std::set<AicNodeID> visited;

  std::optional<Event> aic_search_from(AicGraph &graph);
  std::optional<Event> coloring_search_from(AicNodeID start, AicGraph &graph);

  bool path_contains_node(AicNodeID start, AicSearchNode *cur, AicNodeID node) const;

  AicPath reconstruct_path(AicGraph &graph, AicSearchNode *end) const;

  std::optional<Event> execute_aic_rules(
    AicGraph &graph,
    AicNodeID start,
    AicNodeID end,
    AicSearchNode *end_state) const;
  std::optional<Event> execute_coloring_rules(
    AicGraph &graph,
    AicNodeID start,
    const std::vector<ColorSearchState> &states) const;

  bool are_weakly_linked(AicNode &a, AicNode &b) const;
};

#endif // AIC_HPP
