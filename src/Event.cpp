#include "Event.hpp"

// =========================================================
// Events
// =========================================================

bool Event::operator<(const Event &other) const {
  return this->getEventId() < other.getEventId();
}

uint32_t Event::getEventId() const {
  // make the event comparable by mapping it to a unique integer
  return (static_cast<uint32_t>(this->type) & 0xFFu) |
        ((static_cast<uint32_t>(this->idx) & 0xFFu) << 8) |
        ((static_cast<uint32_t>(this->digit) & 0xFFu) << 16) ;
}
