#ifndef ENCODER_HPP
#define ENCODER_HPP

#include <vector>
#include <deque>
#include <cstdint>
#include "SudokuBoard.hpp"

// ---- Source CellSet serialization (CellSet -> uint32_t) ----
// Encoding (uint32):
//   bits[0..4]   : unitId (0..26)
//   bits[5..13]  : 9-bit mask of cells inside the unit
// unitId mapping:
//   0..8   rows
//   9..17  cols
//   18..26 boxes
// Special case when bits[5..13] are all zero: delimiter of group of sources or digit-only source
std::vector<uint32_t> serialize_cellset_to_unitcodes(const CellSet &cells);

// ---- Source CellSet deserialization (uint32_t -> CellSet) ----
// Extra encoding: bits[16..24] for digit mask
void deserialize_unitcode(uint32_t code, CellSet &outCellSet, DigitSet &outDigitSet, bool &outIsGrouped);

// Convert a CellSet to a string in Eureka notation
// If no common unit is found, split by single cells
std::string cellset_to_eureka(const CellSet &cells);

#endif // ENCODER_HPP
