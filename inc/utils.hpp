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
  #define console_log(fmt,  ...) emscripten_log(EM_LOG_CONSOLE,  fmt,  ##__VA_ARGS__)
#else
  #define console_log(fmt,  ...)
#endif

using Digit = uint8_t;               // a digit in range 1..9
using DigitSet = BitmaskSet<9, 1>;   // a group of digits, typically candidates in a cell

using Index = int8_t;                // an index for cells 0..80, can be -1 for unknown
using IndexSet = BitmaskSet<81, 0>;  // a group of indexes, typically a set of cells
using Unit = IndexSet;               // a group of nine cells in the same row, column or box
using Cell = Index;                  // a cell expressed as an index in range 0..80

constexpr DigitSet ALL_DIGITS(0x1FFU);

#endif // UTILS_H
