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
  FullHouse = 1,
  NakedSingle = 2,
  HiddenSingle = 3,
  PointingPair = 4,
  PointingTriple = 5,
  LockedCandidates = 6,
  BoxLineReduction = 7
};

// one operation = set a value or remove a candidate
struct Operation {
  Index idx;
  Digit digit;
};

class Event
{
public:
  Event();
  Event(EventType type, ReasonId reason);

  EventType type;
  ReasonId reason;

  const std::vector<Operation> &getOperations();
  size_t getNumberOfOperations();
  void addOperation(Index idx, Digit digit);

private:
  // an event is a set of multiple operations
  std::vector<Operation> ops;
};

#endif // EVENT_H
