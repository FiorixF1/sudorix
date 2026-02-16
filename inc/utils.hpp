#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include "BitmaskSet.hpp"

#ifdef __EMSCRIPTEN__
  // WASM
  #include <emscripten/emscripten.h>
#else
  // native
  #include <cstdio>
  #define EMSCRIPTEN_KEEPALIVE
  #define emscripten_log(x,  fmt,  ...) printf(fmt,  ##__VA_ARGS__);
#endif

#ifdef DEBUG
  #define debug_log(fmt,  ...) emscripten_log(EM_LOG_CONSOLE,  fmt,  ##__VA_ARGS__)
#else
  #define debug_log(fmt,  ...)
#endif

using Digit = uint8_t;               // digits 1..9
using DigitSet = BitmaskSet<9, 1>;   // candidates in a cell
using Index = int8_t;                // cells 0..80 or units 0..8, -1 for unknown
using IndexSet = BitmaskSet<81, 0>;  // set of locations

constexpr DigitSet ALL_DIGITS(0x1FFU);

// =========================================================
// Precomputed indices (rows / cols / boxes)
// =========================================================

static const IndexSet ROW_UNITS[9] = {
    IndexSet({ 0,  1,  2,  3,  4,  5,  6,  7,  8 }), 
    IndexSet({ 9, 10, 11, 12, 13, 14, 15, 16, 17 }),
    IndexSet({18, 19, 20, 21, 22, 23, 24, 25, 26 }),
    IndexSet({27, 28, 29, 30, 31, 32, 33, 34, 35 }),
    IndexSet({36, 37, 38, 39, 40, 41, 42, 43, 44 }),
    IndexSet({45, 46, 47, 48, 49, 50, 51, 52, 53 }),
    IndexSet({54, 55, 56, 57, 58, 59, 60, 61, 62 }),
    IndexSet({63, 64, 65, 66, 67, 68, 69, 70, 71 }),
    IndexSet({72, 73, 74, 75, 76, 77, 78, 79, 80 })
  };

static const IndexSet COL_UNITS[9] = {
    IndexSet({ 0,  9, 18, 27, 36, 45, 54, 63, 72 }),
    IndexSet({ 1, 10, 19, 28, 37, 46, 55, 64, 73 }),
    IndexSet({ 2, 11, 20, 29, 38, 47, 56, 65, 74 }),
    IndexSet({ 3, 12, 21, 30, 39, 48, 57, 66, 75 }),
    IndexSet({ 4, 13, 22, 31, 40, 49, 58, 67, 76 }),
    IndexSet({ 5, 14, 23, 32, 41, 50, 59, 68, 77 }),
    IndexSet({ 6, 15, 24, 33, 42, 51, 60, 69, 78 }),
    IndexSet({ 7, 16, 25, 34, 43, 52, 61, 70, 79 }),
    IndexSet({ 8, 17, 26, 35, 44, 53, 62, 71, 80 })
  };

static const IndexSet BOX_UNITS[9] = {
    IndexSet({ 0,  1,  2,  9, 10, 11, 18, 19, 20 }),
    IndexSet({ 3,  4,  5, 12, 13, 14, 21, 22, 23 }),
    IndexSet({ 6,  7,  8, 15, 16, 17, 24, 25, 26 }),
    IndexSet({27, 28, 29, 36, 37, 38, 45, 46, 47 }),
    IndexSet({30, 31, 32, 39, 40, 41, 48, 49, 50 }),
    IndexSet({33, 34, 35, 42, 43, 44, 51, 52, 53 }),
    IndexSet({54, 55, 56, 63, 64, 65, 72, 73, 74 }),
    IndexSet({57, 58, 59, 66, 67, 68, 75, 76, 77 }),
    IndexSet({60, 61, 62, 69, 70, 71, 78, 79, 80 })
  };

inline int idxRow(Index idx) {
  return (int)(idx / 9);
}

inline int idxCol(Index idx) {
  return (int)(idx % 9);
}

inline int idxBox(Index idx) {
  const int r = idxRow(idx);
  const int c = idxCol(idx);
  return (int)((r / 3) * 3 + (c / 3));
}

#endif // UTILS_H
