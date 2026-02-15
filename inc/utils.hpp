#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include "Set.hpp"

#ifdef __EMSCRIPTEN__
  // WASM
  #include <emscripten/emscripten.h>
#else
  // native
  #include <cstdio>
  #define EMSCRIPTEN_KEEPALIVE
  #define emscripten_log(x, fmt, ...) printf(fmt, ##__VA_ARGS__);
#endif

#ifdef DEBUG
  #define debug_log(fmt, ...) emscripten_log(EM_LOG_CONSOLE, fmt, ##__VA_ARGS__)
#else
  #define debug_log(fmt, ...)
#endif

using Digit = uint8_t;
using Mask = uint16_t;
using Index = int8_t;
using Unit = Set<Index>;

// =========================================================
// Precomputed indices (rows / cols / boxes)
// =========================================================

static const Unit ROW_UNITS[9] = {
    Set({(Index)0,(Index)1,(Index)2,(Index)3,(Index)4,(Index)5,(Index)6,(Index)7,(Index)8 }),
    Set({(Index)9,(Index)10,(Index)11,(Index)12,(Index)13,(Index)14,(Index)15,(Index)16,(Index)17 }),
    Set({(Index)18,(Index)19,(Index)20,(Index)21,(Index)22,(Index)23,(Index)24,(Index)25,(Index)26 }),
    Set({(Index)27,(Index)28,(Index)29,(Index)30,(Index)31,(Index)32,(Index)33,(Index)34,(Index)35 }),
    Set({(Index)36,(Index)37,(Index)38,(Index)39,(Index)40,(Index)41,(Index)42,(Index)43,(Index)44 }),
    Set({(Index)45,(Index)46,(Index)47,(Index)48,(Index)49,(Index)50,(Index)51,(Index)52,(Index)53 }),
    Set({(Index)54,(Index)55,(Index)56,(Index)57,(Index)58,(Index)59,(Index)60,(Index)61,(Index)62 }),
    Set({(Index)63,(Index)64,(Index)65,(Index)66,(Index)67,(Index)68,(Index)69,(Index)70,(Index)71 }),
    Set({(Index)72,(Index)73,(Index)74,(Index)75,(Index)76,(Index)77,(Index)78,(Index)79,(Index)80 })
  };

static const Unit COL_UNITS[9] = {
    Set({(Index)0,(Index)9,(Index)18,(Index)27,(Index)36,(Index)45,(Index)54,(Index)63,(Index)72 }),
    Set({(Index)1,(Index)10,(Index)19,(Index)28,(Index)37,(Index)46,(Index)55,(Index)64,(Index)73 }),
    Set({(Index)2,(Index)11,(Index)20,(Index)29,(Index)38,(Index)47,(Index)56,(Index)65,(Index)74 }),
    Set({(Index)3,(Index)12,(Index)21,(Index)30,(Index)39,(Index)48,(Index)57,(Index)66,(Index)75 }),
    Set({(Index)4,(Index)13,(Index)22,(Index)31,(Index)40,(Index)49,(Index)58,(Index)67,(Index)76 }),
    Set({(Index)5,(Index)14,(Index)23,(Index)32,(Index)41,(Index)50,(Index)59,(Index)68,(Index)77 }),
    Set({(Index)6,(Index)15,(Index)24,(Index)33,(Index)42,(Index)51,(Index)60,(Index)69,(Index)78 }),
    Set({(Index)7,(Index)16,(Index)25,(Index)34,(Index)43,(Index)52,(Index)61,(Index)70,(Index)79 }),
    Set({(Index)8,(Index)17,(Index)26,(Index)35,(Index)44,(Index)53,(Index)62,(Index)71,(Index)80 })
  };

static const Unit BOX_UNITS[9] = {
    Set({(Index)0,(Index)1,(Index)2,(Index)9,(Index)10,(Index)11,(Index)18,(Index)19,(Index)20 }),
    Set({(Index)3,(Index)4,(Index)5,(Index)12,(Index)13,(Index)14,(Index)21,(Index)22,(Index)23 }),
    Set({(Index)6,(Index)7,(Index)8,(Index)15,(Index)16,(Index)17,(Index)24,(Index)25,(Index)26 }),
    Set({(Index)27,(Index)28,(Index)29,(Index)36,(Index)37,(Index)38,(Index)45,(Index)46,(Index)47 }),
    Set({(Index)30,(Index)31,(Index)32,(Index)39,(Index)40,(Index)41,(Index)48,(Index)49,(Index)50 }),
    Set({(Index)33,(Index)34,(Index)35,(Index)42,(Index)43,(Index)44,(Index)51,(Index)52,(Index)53 }),
    Set({(Index)54,(Index)55,(Index)56,(Index)63,(Index)64,(Index)65,(Index)72,(Index)73,(Index)74 }),
    Set({(Index)57,(Index)58,(Index)59,(Index)66,(Index)67,(Index)68,(Index)75,(Index)76,(Index)77 }),
    Set({(Index)60,(Index)61,(Index)62,(Index)69,(Index)70,(Index)71,(Index)78,(Index)79,(Index)80 })
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

// =========================================================
// Helpers (bitmasks)
// =========================================================

inline Mask digitToBit(Digit d) {
  // d: 1..9 -> bit (d-1)
  return (Mask)(1u << (d - 1u));
}

inline uint8_t countBits9(Mask mask) {
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

inline uint8_t bitToDigitSingle(Mask mask) {
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
