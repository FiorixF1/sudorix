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

std::array<std::vector<Source>, 4> &Event::getSources() {
  return this->sources;
}

size_t Event::getNumberOfSources() const {
  return this->sources[0].size() + this->sources[1].size() + this->sources[2].size() ;
}

void Event::addSource(Cell idx, Digit digit, int group) {
  sources[group].push_back({CellSet({idx}), DigitSet({digit})});
}

void Event::addSource(Cell idx, DigitSet mask, int group) {
  sources[group].push_back({CellSet({idx}), mask});
}

void Event::addSource(CellSet cells, Digit digit, int group) {
  sources[group].push_back({cells, DigitSet({digit})});
}

void Event::addSource(CellSet cells, DigitSet mask, int group) {
  sources[group].push_back({cells, mask});
}

