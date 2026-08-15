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

std::map<std::string, std::vector<Source>> &Event::getSources() {
  return this->sources;
}

size_t Event::getNumberOfSources() {
  size_t size = 0;
  for (const auto &[name, sourceList] : sources) {
    size += sourceList.size();
  }
  return size;
}

void Event::addSource(const std::string &name, Cell idx, Digit digit) {
  sources[name].push_back({CellSet({idx}), DigitSet({digit})});
}

void Event::addSource(const std::string &name, Cell idx, DigitSet mask) {
  sources[name].push_back({CellSet({idx}), mask});
}

void Event::addSource(const std::string &name, CellSet cells, Digit digit) {
  sources[name].push_back({cells, DigitSet({digit})});
}

void Event::addSource(const std::string &name, CellSet cells, DigitSet mask) {
  sources[name].push_back({cells, mask});
}

