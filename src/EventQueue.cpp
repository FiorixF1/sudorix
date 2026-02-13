#include "EventQueue.hpp"
#include <stdexcept>

// =========================================================
// Event queue
// =========================================================

EventQueue::EventQueue() = default;

void EventQueue::enqueue(SudokuBoard &board, Event &event) {
  // avoid adding empty events (it can happen as a result of the anti-duplication filter)
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

std::size_t EventQueue::size() const {
  return q.size();
}

bool EventQueue::empty() const {
  return q.empty();
}
