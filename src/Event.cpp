#include "Event.hpp"

// =========================================================
// Events
// =========================================================

Event::Event() { }

Event::Event(EventType type, ReasonId reason) : type(type), reason(reason) { }

const std::vector<Operation> &Event::getOperations() {
  return this->ops;
};

size_t Event::getNumberOfOperations() {
  return this->ops.size();
}

void Event::addOperation(uint8_t idx, uint8_t digit) {
  ops.push_back({idx, digit});
}
