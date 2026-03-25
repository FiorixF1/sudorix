#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include "BitmaskSet.hpp"
#include "debug.hpp"

using Digit = uint8_t;                // a digit in range 1..9
using DigitSet = BitmaskSet<9, 1>;    // a group of digits, typically candidates in a cell

using Cell = int8_t;                  // an index 0..80 for cells in the grid, can be -1 for unknown
using CellSet = BitmaskSet<81, 0>;    // a group of cells 0..80 in the grid
using Unit = CellSet;                 // friendly name for a group of nine cells in the same row, column or box

using Location = int8_t;              // an index 0..8 for a unit (or inside a unit) in the grid, can be -1 for unknown
using LocationSet = BitmaskSet<8, 0>; // a group of units 0..8 in the grid or a group of cells 0..8 in a unit

constexpr DigitSet ALL_DIGITS(0x1FFU);
constexpr LocationSet ALL_LOCATIONS(0x1FFU);

#endif // UTILS_H
