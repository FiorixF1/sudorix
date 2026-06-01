#include "Event.hpp"

// =========================================================
// Events
// =========================================================

Event::Event() { }

Event::Event(EventType type, ReasonId reason)
  : type(type), reason(reason), detailedReason(reason) { }

Event::Event(EventType type, ReasonId reason, ReasonId detailedReason)
  : type(type), reason(reason), detailedReason(detailedReason) { }

std::vector<Operation> &Event::getOperations() {
  return this->ops;
};

size_t Event::getNumberOfOperations() const {
  return this->ops.size();
}

void Event::addOperation(Cell idx, Digit digit) {
  // avoid duplicates
  Operation op = {idx, DigitSet({digit})};
  if (std::find(ops.begin(), ops.end(), op) == ops.end()) {
    ops.push_back(op);
  }
}

void Event::addOperation(Cell idx, DigitSet mask) {
  // avoid duplicates
  Operation op = {idx, mask};
  if (std::find(ops.begin(), ops.end(), op) == ops.end()) {
    ops.push_back(op);
  }
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

void Event::addDelimiter() {
  sources.push_back({CellSet(), DigitSet()});
}
