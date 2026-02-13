#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <queue>
#include "Event.hpp"
#include "SudokuBoard.hpp"

class EventQueue
{
public:
  EventQueue();

  void enqueue(SudokuBoard &board, Event &event);

  bool dequeue(Event &ev);

  bool peek(Event &) const;

  std::size_t size() const;

  bool empty() const;

private:
  std::queue<Event> q;
};

#endif // EVENT_QUEUE_H
