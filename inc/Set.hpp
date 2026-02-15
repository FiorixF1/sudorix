#ifndef SET_H
#define SET_H

#include <unordered_set>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <ostream>

template <class T,
          class Hash = std::hash<T>,
          class Eq   = std::equal_to<T>>
class Set {
public:
  using value_type = T;
  using storage_type = std::unordered_set<T, Hash, Eq>;
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  // --- ctors ---
  Set() = default;

  Set(std::initializer_list<T> init) : s_(init) {}

  template <class It>
  Set(It first, It last) : s_(first, last) {}

  // --- basic ops ---
  bool empty() const noexcept { return s_.empty(); }
  std::size_t size() const noexcept { return s_.size(); }

  bool contains(const T &x) const { return s_.find(x) != s_.end(); }

  bool insert(const T &x) { return s_.insert(x).second; }
  bool erase(const T &x)  { return s_.erase(x) != 0; }
  void clear() noexcept   { s_.clear(); }

  iterator begin() noexcept { return s_.begin(); }
  iterator end() noexcept { return s_.end(); }
  const_iterator begin() const noexcept { return s_.begin(); }
  const_iterator end() const noexcept { return s_.end(); }
  const_iterator cbegin() const noexcept { return s_.cbegin(); }
  const_iterator cend() const noexcept { return s_.cend(); }

  template <typename Pred>
  Set filter(Pred p) const {
    Set out;
    for (const auto &x : s_) {
      if (p(x))
        out.s_.insert(x);
    }
    return out;
  }

  // --- set algebra (return new sets) ---
  Set union_with(const Set &other) const {
    Set out;
    out.s_.reserve(size() + other.size());
    out.s_.insert(s_.begin(), s_.end());
    out.s_.insert(other.s_.begin(), other.s_.end());
    return out;
  }

  Set intersect_with(const Set &other) const {
    // iteriamo sul più piccolo per ridurre lookup
    const Set* a = this;
    const Set* b = &other;
    if (a->size() > b->size()) std::swap(a, b);

    Set out;
    out.s_.reserve(a->size());
    for (const auto &x : a->s_) {
      if (b->contains(x)) out.s_.insert(x);
    }
    return out;
  }

  Set difference_with(const Set &other) const {
    Set out;
    out.s_.reserve(size());
    for (const auto &x : s_) {
      if (!other.contains(x)) out.s_.insert(x);
    }
    return out;
  }

  Set symmetric_difference_with(const Set &other) const {
    // (A \ B) ∪ (B \ A)
    Set out;
    out.s_.reserve(size() + other.size());
    for (const auto &x : s_) {
      if (!other.contains(x)) out.s_.insert(x);
    }
    for (const auto &x : other.s_) {
      if (!contains(x)) out.s_.insert(x);
    }
    return out;
  }

  // --- in-place versions ---
  Set &union_assign(const Set &other) {
    s_.reserve(size() + other.size());
    s_.insert(other.s_.begin(), other.s_.end());
    return *this;
  }

  Set &intersect_assign(const Set &other) {
    // rimuovi tutto ciò che non è in other
    for (auto it = s_.begin(); it != s_.end(); ) {
      if (!other.contains(*it)) it = s_.erase(it);
      else ++it;
    }
    return *this;
  }

  Set &difference_assign(const Set &other) {
    for (const auto &x : other.s_) s_.erase(x);
    return *this;
  }

  Set &symmetric_difference_assign(const Set &other) {
    // toggla: se c'è lo togli, se non c'è lo inserisci
    for (const auto &x : other.s_) {
      auto it = s_.find(x);
      if (it == s_.end()) s_.insert(x);
      else s_.erase(it);
    }
    return *this;
  }

  // --- operators (non in-place) ---
  friend Set operator|(const Set &a, const Set &b) { return a.union_with(b); }
  friend Set operator&(const Set &a, const Set &b) { return a.intersect_with(b); }
  friend Set operator-(const Set &a, const Set &b) { return a.difference_with(b); }
  friend Set operator^(const Set &a, const Set &b) { return a.symmetric_difference_with(b); }

  // --- operators (in-place) ---
  Set &operator|=(const Set &other) { return union_assign(other); }
  Set &operator&=(const Set &other) { return intersect_assign(other); }
  Set &operator-=(const Set &other) { return difference_assign(other); }
  Set &operator^=(const Set &other) { return symmetric_difference_assign(other); }

  // --- util: deterministic output if you want (sorted) ---
  std::vector<T> to_sorted_vector() const {
    std::vector<T> v(s_.begin(), s_.end());
    std::sort(v.begin(), v.end());
    return v;
  }

private:
  storage_type s_;
};

// Debug print (stampa ordinata per leggibilità; richiede operator< su T)
template <class T, class H, class E>
std::ostream &operator<<(std::ostream &os, const Set<T,H,E> &set) {
  auto v = set.to_sorted_vector();
  os << "{";
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) os << ", ";
    os << v[i];
  }
  os << "}";
  return os;
}

#endif // SET_H
