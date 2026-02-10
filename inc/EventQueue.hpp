#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <queue>
#include <set>
#include "Event.hpp"
#include "SudokuBoard.hpp"

class EventQueue
{
public:
  EventQueue();

  bool push(const Event &x);

  void pop();

  const Event &front() const;

  bool contains(const Event &x) const;

  std::size_t size() const noexcept;

  bool empty() const noexcept;

  void enqueueSetValue(SudokuBoard &board, int idx, uint8_t digit, ReasonId reason);

  void enqueueRemoveCandidate(SudokuBoard &board, int idx, uint8_t digit, ReasonId reason);

private:
  std::queue<Event> q;
  std::set<uint32_t> s;
};

#endif // EVENT_QUEUE_H
