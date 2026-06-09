#ifndef FORCING_HPP
#define FORCING_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "Event.hpp"
#include "SudokuBoard.hpp"
#include "EventQueue.hpp"
#include "types.hpp"

struct ForcingLiteral {
  Cell cell = -1;
  Digit digit = 0;
  bool on = false; // true = candidate/value forced ON, false = candidate forced OFF

  bool valid() const { return cell >= 0 && cell < 81 && digit >= 1 && digit <= 9; }
};

struct ForcingConfig {
  int maxChainDepth = 25;
};

struct ForcingPath {
  std::vector<ForcingLiteral> nodes;
};

class ForcingChainBuilder {
public:
  explicit ForcingChainBuilder(const SudokuBoard &baseBoard, EventQueue &eventQueue, const ForcingConfig &config = ForcingConfig{});

  void build();
  bool find();

private:
  struct Edge {
    int to = -1;
  };

  const SudokuBoard &base;
  EventQueue &eventQueue;
  ForcingConfig config;
  std::vector<std::vector<Edge>> graph;

  static int literalIndex(Cell cell, Digit digit, bool on);
  static int literalIndex(ForcingLiteral literal);
  static ForcingLiteral literalFromIndex(int index);
  static ForcingLiteral complement(ForcingLiteral literal);

  bool candidateExists(Cell cell, Digit digit) const;
  bool operationStillUseful(ForcingLiteral literal) const;
  void addImplication(ForcingLiteral from, ForcingLiteral to);

  void addCellImplications();
  void addPeerImplications();
  void addUnitStrongLinkImplications();

  std::optional<ForcingPath> findPath(ForcingLiteral from, ForcingLiteral to) const;
  std::vector<int> reachableFrom(ForcingLiteral from) const;

  bool findContradiction();
  bool findCommonConsequences();

  static void addPathSources(Event &event, const ForcingPath &path);
};

#endif // FORCING_HPP
