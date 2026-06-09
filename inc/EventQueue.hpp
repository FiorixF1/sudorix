#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <queue>
#include "Event.hpp"
#include "SudokuBoard.hpp"

class EventQueue
{
public:
  EventQueue();

  bool enqueue(const SudokuBoard &board, Event &event);

  bool dequeue(Event &event);

  bool peek(Event &event) const;

  size_t size() const;

  bool empty() const;

private:
  std::queue<Event> q;
};

#endif // EVENT_QUEUE_H
