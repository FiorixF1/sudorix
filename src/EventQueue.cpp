#include "EventQueue.hpp"

// =========================================================
// Event queue
// =========================================================

EventQueue::EventQueue() = default;

void EventQueue::enqueue(SudokuBoard &board, Event &event) {
  // filter empty events and operations that would stop the solver loop
  if (event.getNumberOfOperations() > 0) {
    event.ops.erase(
      std::remove_if(event.ops.begin(), event.ops.end(),
          [&](Operation op) {
            return (board.isSolved(op.idx) || !board.hasCandidate(op.idx, op.digit));
          }),
      event.ops.end()
    );
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
