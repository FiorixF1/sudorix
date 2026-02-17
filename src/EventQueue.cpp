#include "EventQueue.hpp"

// =========================================================
// Event queue
// =========================================================

EventQueue::EventQueue() = default;

void EventQueue::enqueue(SudokuBoard &board, Event &event) {
  // avoid adding empty events otherwise the solver loop would stop
  if (event.getNumberOfOperations() > 0) {
    q.push(event);
  }
}

bool EventQueue::dequeue(Event &ev) {
  if (q.empty()) {
    return false;
  }

  ev = q.front();
  q.pop();
  return true;
}

bool EventQueue::peek(Event &ev) const {
  if (q.empty()) {
    return false;
  }

  ev = q.front();
  return true;
}

size_t EventQueue::size() const {
  return q.size();
}

bool EventQueue::empty() const {
  return q.empty();
}
