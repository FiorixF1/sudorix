#ifndef EVENT_H
#define EVENT_H

#include <cstdint>

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
  LockedCandidates = 6
};

class Event
{
public:
  EventType type;
  uint8_t   idx;
  uint8_t   digit;
  ReasonId  reason;

  bool operator<(const Event &other) const;

private:
  friend class EventQueue;
  uint32_t getEventId() const;
};

#endif // EVENT_H
