
#include "SudokuBoard.hpp"
#include "encoder.hpp"

static uint32_t encode_unit_cells(uint32_t unitId, uint32_t mask9);

static bool cellset_common_row(const std::vector<int> &cells, int &rowOut, uint32_t &mask9Out);

static bool cellset_common_col(const std::vector<int> &cells, int &colOut, uint32_t &mask9Out);

static bool cellset_common_box(const std::vector<int> &cells, int &boxOut, uint32_t &mask9Out);

std::vector<uint32_t> serialize_cellset_to_unitcodes(const CellSet &cells) {
  std::vector<uint32_t> out;
  const std::vector<int> v = cells.to_vector();
  if (v.empty()) {
    // special case: encode the empty set as a string of zeroes
    out.push_back(encode_unit_cells(0, 0));
    return out;
  }

  int r = -1, c = -1, b = -1;
  uint32_t mask9 = 0;

  if (cellset_common_row(v, r, mask9)) {
    out.push_back(encode_unit_cells((uint32_t)r, mask9));
    return out;
  }
  if (cellset_common_col(v, c, mask9)) {
    out.push_back(encode_unit_cells((uint32_t)(9 + c), mask9));
    return out;
  }
  if (cellset_common_box(v, b, mask9)) {
    out.push_back(encode_unit_cells((uint32_t)(18 + b), mask9));
    return out;
  }

  // No single common unit: split by box (always possible).
  uint32_t boxMasks[9] = {0};
  for (int idx : v) {
    int bb = SudokuBoard::getBoxLocation(idx);
    int rr = SudokuBoard::getRowLocation(idx) % 3;
    int cc = SudokuBoard::getColumnLocation(idx) % 3;
    int pos = rr * 3 + cc;
    boxMasks[bb] |= (1u << (uint32_t)pos);
  }
  for (int bb = 0; bb < 9; bb++) {
    if (boxMasks[bb]) {
      out.push_back(encode_unit_cells((uint32_t)(18 + bb), boxMasks[bb]));
    }
  }
  return out;
}

void deserialize_unitcode(uint32_t code, CellSet &outCellSet, DigitSet &outDigitSet, bool &outIsGrouped) {
  uint32_t unitId = (code & 0x1F) >> 0;
  uint32_t mask9 = (code >> 5) & 0x1FF;
  uint32_t digits = (code >> 16) & 0x1FF;

  CellSet cellSet;
  DigitSet digitSet;

  // empty set
  if (mask9 == 0) {
    outCellSet = cellSet;
    outDigitSet = digitSet;
    outIsGrouped = false;
    return;
  }

  if (unitId <= 8) {
    // rows
    Location r = unitId;
    for (Location c = 0; c < 9; c++) {
      if (mask9 & (1 << c)) {
        cellSet.insert(r * 9 + c);
      }
    }
  } else if (unitId <= 17) {
    // columns
    Location c = unitId - 9;
    for (Location r = 0; r < 9; r++) {
      if (mask9 & (1 << r)) {
        cellSet.insert(r * 9 + c);
      }
    }
  } else if (unitId <= 26) {
    // boxes
    Location b = unitId - 18;
    Location br = (b / 3) * 3;
    Location bc = (b % 3) * 3;
    for (Location p = 0; p < 9; p++) {
      if (mask9 & (1 << p)) {
        Location r = br + (p / 3);
        Location c = bc + (p % 3);
        cellSet.insert(r * 9 + c);
      }
    }
  }

  // digits
  digitSet = DigitSet(digits);

  // output
  outCellSet = cellSet;
  outDigitSet = digitSet;
  outIsGrouped = (cellSet.size() > 1);
}

uint32_t encode_unit_cells(uint32_t unitId, uint32_t mask9) {
  return ((mask9 & 0x1FFu) << 5) | (unitId & 0x1Fu);
}

bool cellset_common_row(const std::vector<int> &cells, int &rowOut, uint32_t &mask9Out) {
  if (cells.empty()) return false;
  int r = SudokuBoard::getRowLocation(cells[0]);
  uint32_t m = 0;
  for (int idx : cells) {
    if (SudokuBoard::getRowLocation(idx) != r) return false;
    m |= (1u << (uint32_t)SudokuBoard::getColumnLocation(idx));
  }
  rowOut = r;
  mask9Out = m;
  return true;
}

bool cellset_common_col(const std::vector<int> &cells, int &colOut, uint32_t &mask9Out) {
  if (cells.empty()) return false;
  int c = SudokuBoard::getColumnLocation(cells[0]);
  uint32_t m = 0;
  for (int idx : cells) {
    if (SudokuBoard::getColumnLocation(idx) != c) return false;
    m |= (1u << (uint32_t)SudokuBoard::getRowLocation(idx));
  }
  colOut = c;
  mask9Out = m;
  return true;
}

bool cellset_common_box(const std::vector<int> &cells, int &boxOut, uint32_t &mask9Out) {
  if (cells.empty()) return false;
  int b = SudokuBoard::getBoxLocation(cells[0]);
  uint32_t m = 0;
  for (int idx : cells) {
    if (SudokuBoard::getBoxLocation(idx) != b) return false;
    int r = SudokuBoard::getRowLocation(idx) % 3;
    int c = SudokuBoard::getColumnLocation(idx) % 3;
    int pos = r * 3 + c;
    m |= (1u << (uint32_t)pos);
  }
  boxOut = b;
  mask9Out = m;
  return true;
}
