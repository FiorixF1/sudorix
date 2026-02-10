#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

// =========================================================
// Precomputed indices (rows / cols / boxes)
// =========================================================

static constexpr int ROW_CELLS[9][9] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17 },
    { 18, 19, 20, 21, 22, 23, 24, 25, 26 },
    { 27, 28, 29, 30, 31, 32, 33, 34, 35 },
    { 36, 37, 38, 39, 40, 41, 42, 43, 44 },
    { 45, 46, 47, 48, 49, 50, 51, 52, 53 },
    { 54, 55, 56, 57, 58, 59, 60, 61, 62 },
    { 63, 64, 65, 66, 67, 68, 69, 70, 71 },
    { 72, 73, 74, 75, 76, 77, 78, 79, 80 }
  };

static constexpr int COL_CELLS[9][9] = {
    { 0, 9, 18, 27, 36, 45, 54, 63, 72 },
    { 1, 10, 19, 28, 37, 46, 55, 64, 73 },
    { 2, 11, 20, 29, 38, 47, 56, 65, 74 },
    { 3, 12, 21, 30, 39, 48, 57, 66, 75 },
    { 4, 13, 22, 31, 40, 49, 58, 67, 76 },
    { 5, 14, 23, 32, 41, 50, 59, 68, 77 },
    { 6, 15, 24, 33, 42, 51, 60, 69, 78 },
    { 7, 16, 25, 34, 43, 52, 61, 70, 79 },
    { 8, 17, 26, 35, 44, 53, 62, 71, 80 }
  };

static constexpr int BOX_CELLS[9][9] = {
    { 0, 1, 2, 9, 10, 11, 18, 19, 20 },
    { 3, 4, 5, 12, 13, 14, 21, 22, 23 },
    { 6, 7, 8, 15, 16, 17, 24, 25, 26 },
    { 27, 28, 29, 36, 37, 38, 45, 46, 47 },
    { 30, 31, 32, 39, 40, 41, 48, 49, 50 },
    { 33, 34, 35, 42, 43, 44, 51, 52, 53 },
    { 54, 55, 56, 63, 64, 65, 72, 73, 74 },
    { 57, 58, 59, 66, 67, 68, 75, 76, 77 },
    { 60, 61, 62, 69, 70, 71, 78, 79, 80 }
  };

inline uint8_t idxRow(int idx) {
  return (uint8_t)(idx / 9);
}

inline uint8_t idxCol(int idx) {
  return (uint8_t)(idx % 9);
}

inline uint8_t idxBox(int idx) {
  const uint8_t r = idxRow(idx);
  const uint8_t c = idxCol(idx);
  return (uint8_t)((r / 3) * 3 + (c / 3));
}

// =========================================================
// Helpers (bitmasks)
// =========================================================

inline uint16_t digitToBit(uint8_t d) {
  // d: 1..9 -> bit (d-1)
  return (uint16_t)(1u << (d - 1u));
}

inline uint8_t countBits9(uint16_t mask) {
  mask &= 0x1FFu;
  // builtin popcount if available
#if defined(__GNUC__) || defined(__clang__)
  return (uint8_t)__builtin_popcount((unsigned)mask);
#else
  uint8_t c = 0;
  while (mask) {
    c += (mask & 1u);
    mask >>= 1u;
  }
  return c;
#endif
}

inline uint8_t bitToDigitSingle(uint16_t mask) {
  // assumes exactly one bit set (1..9)
#if defined(__GNUC__) || defined(__clang__)
  return (uint8_t)(__builtin_ctz((unsigned)mask) + 1u);
#else
  for (uint8_t d = 1; d <= 9; d++) {
    if (mask & digitToBit(d)) {
      return d;
    }
  }
  return 0;
#endif
}

#endif // UTILS_H
