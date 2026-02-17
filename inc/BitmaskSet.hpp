#ifndef BITMASK_SET_HPP
#define BITMASK_SET_HPP

#include <array>
#include <cstdint>
#include <initializer_list>
#include <ostream>
#include <vector>

#if __has_include(<bit>)
  #include <bit>
#endif

// A compact set of integers in the inclusive range [MinValue, MaxValue],
// implemented as a bitmask (array of uint64_t).
//
// NOTE: value_type is int (range is integral by design).
template <int MaxValue, int MinValue = 0>
class BitmaskSet {
public:
  static_assert(MinValue <= MaxValue, "BitmaskSet: MinValue must be <= MaxValue");

  using value_type = int;

  static constexpr int min_value = MinValue;
  static constexpr int max_value = MaxValue;
  static constexpr int bit_count = (MaxValue - MinValue + 1);

private:
  static constexpr std::size_t kWordBits = 64;
  static constexpr std::size_t kWordCount = (bit_count + kWordBits - 1) / kWordBits;

  using storage_type = std::array<std::uint64_t, kWordCount>;
  storage_type w_{}; // zero-initialized

  static constexpr std::uint64_t last_word_mask() noexcept {
    // mask valid bits in last word (so ~ doesn't "create" extra elements)
    constexpr std::size_t rem = bit_count % kWordBits;
    if constexpr (rem == 0) return ~0ULL;
    else return (1ULL << rem) - 1ULL;
  }

  static constexpr void check_range(int x) {
    if (x < MinValue || x > MaxValue) {
      throw std::out_of_range("BitmaskSet: value out of configured range");
    }
  }

  static constexpr std::size_t bit_index(int x) noexcept {
    return static_cast<std::size_t>(x - MinValue);
  }

  static constexpr std::size_t word_index(std::size_t b) noexcept { return b / kWordBits; }
  static constexpr std::size_t bit_offset(std::size_t b) noexcept { return b % kWordBits; }

  static inline int popcount_u64(std::uint64_t v) noexcept {
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
    return static_cast<int>(std::popcount(v));
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<int>(__builtin_popcountll(v));
#elif defined(_MSC_VER)
    return static_cast<int>(__popcnt64(v));
#else
    // portable fallback
    int c = 0;
    while (v) { v &= (v - 1); ++c; }
    return c;
#endif
  }

  static inline unsigned countr_zero_u64(std::uint64_t v) noexcept {
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
    return static_cast<unsigned>(std::countr_zero(v));
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_ctzll(v));
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, v);
    return static_cast<unsigned>(idx);
#else
    // portable fallback (v != 0 required)
    unsigned c = 0;
    while ((v & 1ULL) == 0ULL) { v >>= 1; ++c; }
    return c;
#endif
  }

  constexpr void sanitize_last_word() noexcept {
    w_[kWordCount - 1] &= last_word_mask();
  }

public:
  // ---- ctors ----
  constexpr BitmaskSet() = default;

  // ---- ctor from list of integers
  BitmaskSet(std::initializer_list<int> init) {
    for (int x : init) insert(x);
  }

  // ---- ctor from integral mask
  explicit constexpr BitmaskSet(uint64_t mask) noexcept {
    clear();
    std::size_t bi = 0;
    while (mask != 0 && bi < static_cast<std::size_t>(bit_count)) {
      if (mask & 1ULL) {
        // set bit bi
        w_[word_index(bi)] |= (1ULL << bit_offset(bi));
      }
      mask >>= 1;
      ++bi;
    }
    sanitize_last_word();
  }

  // ---- basic ops ----
  constexpr bool empty() const noexcept {
    for (auto ww : w_) if (ww) return false;
    return true;
  }

  int size() const noexcept {
    int total = 0;
    for (auto ww : w_) total += popcount_u64(ww);
    return total;
  }

  bool contains(int x) const {
    check_range(x);
    auto b = bit_index(x);
    return (w_[word_index(b)] >> bit_offset(b)) & 1ULL;
  }

  void insert(int x) {
    check_range(x);
    auto b = bit_index(x);
    w_[word_index(b)] |= (1ULL << bit_offset(b));
  }

  void erase(int x) {
    check_range(x);
    auto b = bit_index(x);
    w_[word_index(b)] &= ~(1ULL << bit_offset(b));
  }

  void toggle(int x) {
    check_range(x);
    auto b = bit_index(x);
    w_[word_index(b)] ^= (1ULL << bit_offset(b));
  }

  constexpr void clear() noexcept {
    for (auto &ww : w_) ww = 0;
  }

  // ---- set algebra (return new sets) ----
  constexpr BitmaskSet union_with(const BitmaskSet &other) const noexcept {
    BitmaskSet out;
    for (std::size_t i = 0; i < kWordCount; ++i) out.w_[i] = w_[i] | other.w_[i];
    return out;
  }

  constexpr BitmaskSet intersect_with(const BitmaskSet &other) const noexcept {
    BitmaskSet out;
    for (std::size_t i = 0; i < kWordCount; ++i) out.w_[i] = w_[i] & other.w_[i];
    return out;
  }

  constexpr BitmaskSet difference_with(const BitmaskSet &other) const noexcept {
    BitmaskSet out;
    for (std::size_t i = 0; i < kWordCount; ++i) out.w_[i] = w_[i] & ~other.w_[i];
    out.sanitize_last_word();
    return out;
  }

  constexpr BitmaskSet symmetric_difference_with(const BitmaskSet &other) const noexcept {
    BitmaskSet out;
    for (std::size_t i = 0; i < kWordCount; ++i) out.w_[i] = w_[i] ^ other.w_[i];
    out.sanitize_last_word();
    return out;
  }

  // ---- in-place versions ----
  constexpr BitmaskSet &union_assign(const BitmaskSet &other) noexcept {
    for (std::size_t i = 0; i < kWordCount; ++i) w_[i] |= other.w_[i];
    return *this;
  }

  constexpr BitmaskSet &intersect_assign(const BitmaskSet &other) noexcept {
    for (std::size_t i = 0; i < kWordCount; ++i) w_[i] &= other.w_[i];
    return *this;
  }

  constexpr BitmaskSet &difference_assign(const BitmaskSet &other) noexcept {
    for (std::size_t i = 0; i < kWordCount; ++i) w_[i] &= ~other.w_[i];
    sanitize_last_word();
    return *this;
  }

  constexpr BitmaskSet &symmetric_difference_assign(const BitmaskSet &other) noexcept {
    for (std::size_t i = 0; i < kWordCount; ++i) w_[i] ^= other.w_[i];
    sanitize_last_word();
    return *this;
  }

  // ---- operators ----
  friend constexpr BitmaskSet operator|(BitmaskSet a, const BitmaskSet &b) noexcept { return a.union_with(b); }
  friend constexpr BitmaskSet operator&(BitmaskSet a, const BitmaskSet &b) noexcept { return a.intersect_with(b); }
  friend constexpr BitmaskSet operator-(BitmaskSet a, const BitmaskSet &b) noexcept { return a.difference_with(b); }
  friend constexpr BitmaskSet operator^(BitmaskSet a, const BitmaskSet &b) noexcept { return a.symmetric_difference_with(b); }

  constexpr BitmaskSet &operator|=(const BitmaskSet &other) noexcept { return union_assign(other); }
  constexpr BitmaskSet &operator&=(const BitmaskSet &other) noexcept { return intersect_assign(other); }
  constexpr BitmaskSet &operator-=(const BitmaskSet &other) noexcept { return difference_assign(other); }
  constexpr BitmaskSet &operator^=(const BitmaskSet &other) noexcept { return symmetric_difference_assign(other); }

  // ---- comparison ----
  constexpr bool operator==(const BitmaskSet &other) const noexcept {
    for (std::size_t i = 0; i < kWordCount; ++i) {
      if (w_[i] != other.w_[i]) {
        return false;
      }
    }
    return true;
  }

  constexpr bool operator!=(const BitmaskSet &other) const noexcept {
    return !(*this == other);
  }

  // ---- set relations ----
  constexpr bool is_subset_of(const BitmaskSet &other) const noexcept {
    for (std::size_t i = 0; i < kWordCount; ++i) {
      // tutti i bit di this devono essere contenuti in other
      if ((w_[i] & ~other.w_[i]) != 0)
        return false;
    }
    return true;
  }

  constexpr bool is_superset_of(const BitmaskSet &other) const noexcept {
    return other.is_subset_of(*this);
  }

  // ---- utility ----
  std::vector<int> to_vector() const {
    std::vector<int> out;
    out.reserve(static_cast<std::size_t>(size()));

    for (std::size_t wi = 0; wi < kWordCount; ++wi) {
      std::uint64_t w = w_[wi];
      // mask last word to avoid exposing out-of-range bits
      if (wi == kWordCount - 1) w &= last_word_mask();

      while (w) {
        unsigned tz = countr_zero_u64(w);
        std::size_t b = wi * kWordBits + tz;
        out.push_back(MinValue + static_cast<int>(b));
        w &= (w - 1);
      }
    }

    // already in ascending order due to scan order
    return out;
  }

  template <class Pred>
  BitmaskSet filter(Pred pred) const {
    BitmaskSet out;
    for (int v : to_vector()) {
      if (pred(v)) {
        out.insert(v);
      }
    }
    return out;
  }

  // ---- iterator to support range-based for ----
  class const_iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = int;
    using difference_type = std::ptrdiff_t;
    using pointer = const int*;
    using reference = int;

    const_iterator() = default;

    reference operator*() const { return cur_; }

    const_iterator &operator++() {
      advance();
      return *this;
    }

    bool operator==(const const_iterator &other) const {
      return owner_ == other.owner_ && done_ == other.done_ && (done_ || cur_ == other.cur_);
    }
    bool operator!=(const const_iterator &other) const { return !(*this == other); }

  private:
    friend class BitmaskSet;

    const BitmaskSet *owner_{nullptr};
    std::size_t wi_{0};
    std::uint64_t w_{0};
    int cur_{0};
    bool done_{true};

    const_iterator(const BitmaskSet *o, bool end) : owner_(o) {
      if (end || !o) { done_ = true; return; }
      done_ = false;
      wi_ = 0;
      w_ = o->w_[0];
      if (kWordCount == 1) w_ &= last_word_mask();
      seek_next();
    }

    void seek_next() {
      while (true) {
        if (w_) {
          unsigned tz = countr_zero_u64(w_);
          std::size_t b = wi_ * kWordBits + tz;
          cur_ = MinValue + static_cast<int>(b);
          w_ &= (w_ - 1);
          return;
        }
        ++wi_;
        if (wi_ >= kWordCount) { done_ = true; return; }
        w_ = owner_->w_[wi_];
        if (wi_ == kWordCount - 1) w_ &= last_word_mask();
      }
    }

    void advance() { seek_next(); }
  };

  const_iterator begin() const { return const_iterator(this, false); }
  const_iterator end() const { return const_iterator(this, true); }

private:
  // allow stream operator access if desired
  template <int MX, int MN>
  friend std::ostream &operator<<(std::ostream&, const BitmaskSet<MX, MN>&);
};

// ---- stream operator (sorted) ----
template <int MaxValue, int MinValue>
inline std::ostream &operator<<(std::ostream &os, const BitmaskSet<MaxValue, MinValue> &s) {
  os << "{";
  bool first = true;
  for (int v : s) {
    if (!first) os << ", ";
    first = false;
    os << v;
  }
  os << "}";
  return os;
}

#endif // BITMASK_SET_HPP
