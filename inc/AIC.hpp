#ifndef AIC_HPP
#define AIC_HPP

#include <set>
#include <map>
#include <vector>
#include <deque>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <array>
#include "SudokuCell.hpp"

class SudokuBoard;
class EventQueue;

// Unique ID for nodes.
// The ID, both for singletons and groups, is built as follows:
// - Encode the cell/group as if it where the source of a technique
//   -> for singletons it will produce only one code
//   -> for grouped cells it will work because they are always part of the same unit
// - Translate the digit (or group of) as a 9-bit mask
// - Shift the digit mask left by 16 bits
// - Concatenate the code and the digit mask with a bitwise OR
// Result:
// 0000000z zzzzzzzz 00yyyyyy yyyxxxxx
//        ^digits      ^cells    ^unit
// This way the ID can be used also to represent the node itself
// because all its data can be univocally extracted.
//
// Note: a node could potentially have more than one representation, but if the ID is built
// with serialize_cellset_to_unitcodes it is guaranteed to be unique.
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

// Edge structure
struct AicEdge {
  AicNodeID to;
  EdgeType type;

  bool operator==(const AicEdge &other) const {
    return to == other.to && type == other.type;
  }
};

struct AicPath {
  std::vector<AicNode> nodes;
  std::vector<EdgeType> edges;  // edges[i] connects nodes[i] -> nodes[i+1]
};

using AicGraphNodes = std::map<AicNodeID, AicNode>;
using AicGraphEdges = std::map<AicNodeID, std::vector<AicEdge>>;

struct AicGraph {
  AicGraphNodes nodes;
  AicGraphEdges strong_links;
  AicGraphEdges weak_links;
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
  int max_depth; // max number of edges, makes sense if odd in AIC
  std::string pattern; // used to identify named chains
};

// Compact state ID for a singleton candidate plus polarity in a forcing chain:
//   id = ((cell * 10 + digit) * 2) + polarity
// where polarity is 1 for ON and 0 for OFF.
// Digit 0 is intentionally unused, so the fixed table has 81*10*2 entries.
using ForcingID = uint16_t;
static constexpr ForcingID FORCING_INVALID_ID = 0xFFFF;
static constexpr size_t FORCING_STATE_COUNT = 81 * 10 * 2;

struct ForcingPath {
  std::vector<ForcingID> nodes;
};

struct ForcingSearchEntry {
  bool visited = false;
  ForcingID parent = FORCING_INVALID_ID;
  uint16_t depth = 0;
};

struct ForcingSearchResult {
  ForcingID root = FORCING_INVALID_ID;
  std::array<ForcingSearchEntry, FORCING_STATE_COUNT> entries{};
  std::vector<ForcingID> reachable;

  bool contains(ForcingID id) const;
  std::optional<ForcingPath> reconstructPath(ForcingID target) const;
};

/* ---------------------------------------------------------------------- */

class AicGraphBuilder {
public:
  explicit AicGraphBuilder(const SudokuBoard &board);

  AicGraph build();

  AicGraph prune(AicGraph &graph, const AicConfig &config);

  static AicNodeID get_node_id(Digit digit, Cell cell);
  static AicNodeID get_node_id(Digit digit, const CellSet &cells);
  static AicNodeID get_node_id(const DigitSet &digits, Cell cell);
  static AicNodeID get_node_id(const DigitSet &digits, const CellSet &cells);

private:
  const SudokuBoard &board;

  void build_singleton_nodes(AicGraphNodes &nodes);

  void build_grouped_nodes(AicGraphNodes &nodes);

  void add_group_if_new(AicGraphNodes &nodes, Digit digit, const CellSet &cells);

  void add_edge(AicGraphEdges &adj, AicNodeID a, AicNodeID b, EdgeType type);

  void build_links(AicGraphNodes &nodes,
                   AicGraphEdges &strong_links,
                   AicGraphEdges &weak_links);

  void build_links_in_units(const AicGraphNodes &nodes,
                            AicGraphEdges &strong_links,
                            AicGraphEdges &weak_links,
                            const std::vector<Unit> &units,
                            Digit digit);

  void build_links_in_cells(const AicGraphNodes &nodes,
                            AicGraphEdges &strong_links,
                            AicGraphEdges &weak_links,
                            Cell cell);

  void build_grouped_row_box(const AicGraphNodes &nodes,
                             AicGraphEdges &strong_links,
                             AicGraphEdges &weak_links,
                             Digit digit);

  void build_grouped_col_box(const AicGraphNodes &nodes,
                             AicGraphEdges &strong_links,
                             AicGraphEdges &weak_links,
                             Digit digit);

  void build_grouped_box_row(const AicGraphNodes &nodes,
                             AicGraphEdges &strong_links,
                             AicGraphEdges &weak_links,
                             Digit digit);

  void build_grouped_box_col(const AicGraphNodes &nodes,
                             AicGraphEdges &strong_links,
                             AicGraphEdges &weak_links,
                             Digit digit);

  void build_grouped_eri(AicGraphNodes &nodes,
                         AicGraphEdges &strong_links,
                         AicGraphEdges &weak_links,
                         Digit digit);
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
  AicSearcher(const SudokuBoard &board, EventQueue &eventQueue);

  const AicConfig &setConfigAndReturn(ReasonId reason);

  // true if an event has been produced
  bool runSearch(AicGraph &graph);

private:
  const SudokuBoard &board;
  EventQueue &eventQueue;
  AicGraph *graph;
  ReasonId reason;
  AicConfig config;
  std::set<AicNodeID> visited;

  bool aic_search();
  bool coloring_search_from(AicNodeID start);
  bool forcing_chain_search();

  bool analyze_event(Event &event);

  bool path_contains_node(AicNodeID start, AicSearchNode *cur, AicNodeID node) const;

  AicPath reconstruct_path(AicSearchNode *end) const;

  std::optional<Event> execute_aic_rules(
    AicNodeID start,
    AicNodeID end,
    AicSearchNode *end_state) const;
  bool execute_coloring_rules(
    AicNodeID start,
    const std::vector<ColorSearchState> &states) const;
  bool execute_fc_rules() const;

  bool are_weakly_linked(AicNodeID a, AicNodeID b) const;

  bool find_contradiction() const;
  bool find_common_consequences() const;
  ForcingSearchResult reachable_from(ForcingID root) const;
  void add_path_sources(Event &event, const ForcingPath &path, int group) const;
};

#endif // AIC_HPP
