#include "Event.hpp"

// =========================================================
// Events
// =========================================================

Event::Event() { }

Event::Event(EventType type, ReasonId reason) : type(type), reason(reason) { }

std::vector<Operation> &Event::getOperations() {
  return this->ops;
};

size_t Event::getNumberOfOperations() const {
  return this->ops.size();
}

void Event::addOperation(Cell idx, Digit digit) {
  ops.push_back({idx, DigitSet({digit})});
}

void Event::addOperation(Cell idx, DigitSet mask) {
  ops.push_back({idx, mask});
}

std::vector<Source> &Event::getSources() {
  return this->sources;
}

size_t Event::getNumberOfSources() const {
  return this->sources.size();
}

void Event::addSource(Cell idx, Digit digit) {
  sources.push_back({CellSet({idx}), DigitSet({digit})});
}

void Event::addSource(Cell idx, DigitSet mask) {
  sources.push_back({CellSet({idx}), mask});
}

void Event::addSource(CellSet cells, Digit digit) {
  sources.push_back({cells, DigitSet({digit})});
}

void Event::addSource(CellSet cells, DigitSet mask) {
  sources.push_back({cells, mask});
}
