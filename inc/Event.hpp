#ifndef EVENT_H
#define EVENT_H

#include <cstdint>
#include <vector>
#include "utils.hpp"

enum class EventType : uint8_t {
  None = 0,
  SetValue = 1,
  RemoveCandidate = 2
};

enum class ReasonId : uint8_t {
  Solver = 0,
  FullHouse,
  NakedSingle,
  NakedPair,
  NakedTriple,
  NakedQuad,
  HiddenSingle,
  HiddenPair,
  HiddenTriple,
  HiddenQuad,
  PointingPair,
  PointingTriple,
  ClaimingPair,
  ClaimingTriple,
  XWing,
  Swordfish,
  Jellyfish,
  XYWing,
  XYZWing,
  BUGPlusOne,
  Skyscraper
};

// one operation = set value(s) or remove candidate(s)
struct Operation {
  Cell idx;
  DigitSet mask;
};

// one source = a (cell, mask) pair explaining why the rule triggers
struct Source {
  Cell idx;
  DigitSet mask;
};

class Event
{
public:
  Event();
  Event(EventType type, ReasonId reason);

  EventType type;
  ReasonId reason;

  std::vector<Operation> &getOperations();
  size_t getNumberOfOperations() const;
  void addOperation(Cell idx, Digit digit);
  void addOperation(Cell idx, DigitSet mask);

  std::vector<Source> &getSources();
  size_t getNumberOfSources() const;
  void addSource(Cell idx, Digit digit);
  void addSource(Cell idx, DigitSet mask);

private:
  friend class EventQueue;
  // an event is a set of multiple operations
  std::vector<Operation> ops;
  std::vector<Source> sources;
};

#endif // EVENT_H
