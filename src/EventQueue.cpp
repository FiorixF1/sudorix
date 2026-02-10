#include "EventQueue.hpp"
#include <stdexcept>

// =========================================================
// Event queue (implementation avoids duplicates)
// =========================================================

EventQueue::EventQueue() = default;

// push solo se l'elemento non è già presente
// evita eventi duplicati nella stessa iterazione
bool EventQueue::push(const Event &x) {
  if (s.find(x.getEventId()) != s.end()) {
    return false;
  }

  q.push(x);
  s.insert(x.getEventId());
  return true;
}

// rimozione coerente queue + set
void EventQueue::pop() {
  if (q.empty()) {
    throw std::logic_error("EventQueue::pop() on empty queue");
  }

  const Event &front = q.front();
  s.erase(front.getEventId());
  q.pop();
}

const Event &EventQueue::front() const {
  if (q.empty()) {
    throw std::logic_error("EventQueue::front() on empty queue");
  }

  return q.front();
}

bool EventQueue::contains(const Event &x) const {
  return s.find(x.getEventId()) != s.end();
}

std::size_t EventQueue::size() const noexcept {
  return q.size();
}

bool EventQueue::empty() const noexcept {
  return q.empty();
}

void EventQueue::enqueueSetValue(SudokuBoard &board, int idx, uint8_t digit, ReasonId reason) {
  if (digit == 0) {
    return;
  }
  if (board.isSolved(idx)) {
    return;
  }

  Event ev;
  ev.type = EventType::SetValue;
  ev.idx = (uint32_t)idx;
  ev.digit = (uint32_t)digit;
  ev.reason = reason;
  this->push(ev);
}

void EventQueue::enqueueRemoveCandidate(SudokuBoard &board, int idx, uint8_t digit, ReasonId reason) {
  if (digit == 0) {
    return;
  }
  if (board.isSolved(idx)) {
    return;
  }
  if (!board.hasCandidate(idx, digit)) {
    return;
  }

  Event ev;
  ev.type = EventType::RemoveCandidate;
  ev.idx = (uint32_t)idx;
  ev.digit = (uint32_t)digit;
  ev.reason = reason;
  this->push(ev);
}
