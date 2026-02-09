#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "solver.hpp"

static inline bool isDigitChar(char c) {
  return c >= '0' && c <= '9';
}

static inline bool isValidSudokuChar(char c) {
  return (c == '.') || isDigitChar(c);
}

static std::string trim(const std::string &s) {
  size_t a = 0;
  while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) {
    a++;
  }
  size_t b = s.size();
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) {
    b--;
  }
  return s.substr(a, b - a);
}

static std::string normalize81(const std::string &line, std::string *err) {
  std::string s = trim(line);

  // Allow comments and blank lines
  if (s.empty() || s[0] == '#') {
    return "";
  }

  // Remove spaces in-between if the file uses spaced formatting.
  std::string compact;
  compact.reserve(s.size());
  for (char c : s) {
    if (c == ' ' || c == '\t') {
      continue;
    }
    compact.push_back(c);
  }

  if (compact.size() != 81) {
    if (err) {
      std::ostringstream oss;
      oss << "Expected 81 chars, got " << compact.size();
      *err = oss.str();
    }
    return "";
  }

  for (char &c : compact) {
    if (!isValidSudokuChar(c)) {
      if (err) {
        *err = "Invalid character (allowed: 0-9 or .)";
      }
      return "";
    }
    if (c == '.') {
      c = '0';
    }
  }

  return compact;
}

static inline uint16_t bitForDigit(int d) {
  return static_cast<uint16_t>(1u << (d - 1));
}

static bool checkUnitMask(const std::vector<int> &idxs, const std::string &out81, std::string *why) {
  uint16_t seen = 0;
  for (int idx : idxs) {
    char c = out81[(size_t)idx];
    if (c < '1' || c > '9') {
      if (why) {
        std::ostringstream oss;
        oss << "Non-digit in solution at idx=" << idx << " ('" << c << "')";
        *why = oss.str();
      }
      return false;
    }
    int d = c - '0';
    uint16_t b = bitForDigit(d);
    if ((seen & b) != 0) {
      if (why) {
        std::ostringstream oss;
        oss << "Duplicate digit " << d << " in unit";
        *why = oss.str();
      }
      return false;
    }
    seen = static_cast<uint16_t>(seen | b);
  }
  if (seen != 0x01FFu) {
    if (why) {
      *why = "Unit does not contain all digits 1..9";
    }
    return false;
  }
  return true;
}

static bool validateSolution(const std::string &in81, const std::string &out81, std::string *why) {
  if (out81.size() != 81) {
    if (why) {
      *why = "Output length != 81";
    }
    return false;
  }

  // Check givens are preserved
  for (int i = 0; i < 81; i++) {
    char in = in81[(size_t)i];
    char out = out81[(size_t)i];
    if (in >= '1' && in <= '9') {
      if (out != in) {
        if (why) {
          std::ostringstream oss;
          oss << "Given mismatch at idx=" << i << " (in=" << in << ", out=" << out << ")";
          *why = oss.str();
        }
        return false;
      }
    }
  }

  // Build unit indices once
  static std::vector<std::vector<int>> rows;
  static std::vector<std::vector<int>> cols;
  static std::vector<std::vector<int>> boxes;

  if (rows.empty()) {
    rows.resize(9);
    cols.resize(9);
    boxes.resize(9);
    for (int r = 0; r < 9; r++) {
      for (int c = 0; c < 9; c++) {
        int idx = r * 9 + c;
        rows[(size_t)r].push_back(idx);
        cols[(size_t)c].push_back(idx);
        int b = (r / 3) * 3 + (c / 3);
        boxes[(size_t)b].push_back(idx);
      }
    }
  }

  // Check all rows/cols/boxes contain 1..9 exactly once.
  for (int u = 0; u < 9; u++) {
    std::string w;
    if (!checkUnitMask(rows[(size_t)u], out81, &w)) {
      if (why) {
        std::ostringstream oss;
        oss << "Row " << u << " invalid: " << w;
        *why = oss.str();
      }
      return false;
    }
    if (!checkUnitMask(cols[(size_t)u], out81, &w)) {
      if (why) {
        std::ostringstream oss;
        oss << "Col " << u << " invalid: " << w;
        *why = oss.str();
      }
      return false;
    }
    if (!checkUnitMask(boxes[(size_t)u], out81, &w)) {
      if (why) {
        std::ostringstream oss;
        oss << "Box " << u << " invalid: " << w;
        *why = oss.str();
      }
      return false;
    }
  }

  return true;
}

static int runFullSolveOne(const std::string &in81, std::string *out81, std::string *why) {
  char outBuf[82];
  std::memset(outBuf, 0, sizeof(outBuf));

  int rc = sudorix_solver_full(in81.c_str(), outBuf);

  // Ensure null termination for printing even if solver returns non-terminated out.
  outBuf[81] = '\0';

  *out81 = std::string(outBuf, 81);

  if (rc == 0) {
    if (why) {
      *why = "sudorix_solver_full returned 0 (failure)";
    }
    return 0;
  }

  std::string w;
  if (!validateSolution(in81, *out81, &w)) {
    if (why) {
      *why = w;
    }
    return 0;
  }

  return 1;
}

// Future: step-based runner stub.
// For now it just calls full solve, but the structure is here to extend.
static int runStepSolveOne(const std::string &in81, std::string *out81, std::string *why) {
  (void)why;
  // TODO: implement when you expose an "export" or allow reading internal board after next_step loop.
  // A possible approach:
  //   sudorix_solver_init_board(in81.c_str());
  //   loop:
  //     sudorix_solver_next_step(outEvent);
  //     if outEvent.type==0 break;
  //   sudorix_solver_export(out81) OR pass out81 as an output parameter.
  //
  // For now, fall back to full.
  return runFullSolveOne(in81, out81, why);
}

static void usage(const char *argv0) {
  std::cerr
      << "Usage: " << argv0 << " <sudoku_file.txt> [--mode=full|step]\n"
      << "  Each non-empty, non-comment line must contain 81 chars: digits 0-9 or '.' for empty.\n";
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }

  std::string path = argv[1];
  std::string mode = "full";
  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a.rfind("--mode=", 0) == 0) {
      mode = a.substr(std::strlen("--mode="));
    }
  }

  if (mode != "full" && mode != "step") {
    std::cerr << "Unknown mode: " << mode << "\n";
    usage(argv[0]);
    return 2;
  }

  std::ifstream fin(path);
  if (!fin) {
    std::cerr << "Failed to open file: " << path << "\n";
    return 2;
  }

  size_t total = 0;
  size_t passed = 0;
  size_t failed = 0;

  std::string line;
  size_t lineNo = 0;

  while (std::getline(fin, line)) {
    lineNo++;

    std::string err;
    std::string in81 = normalize81(line, &err);
    if (in81.empty()) {
      // Either blank/comment, or invalid. Distinguish:
      std::string t = trim(line);
      if (!t.empty() && t[0] != '#') {
        total++;
        failed++;
        std::cout << "[#" << total << " line " << lineNo << "] "
                  << "INPUT: " << t << "\n"
                  << "OUTPUT: " << "(n/a)\n"
                  << "RESULT: FAILED (" << err << ")\n\n";
      }
      continue;
    }

    total++;

    std::string out81;
    std::string why;

    int ok = 0;
    if (mode == "full") {
      ok = runFullSolveOne(in81, &out81, &why);
    } else {
      ok = runStepSolveOne(in81, &out81, &why);
    }

    if (ok) {
      passed++;
      std::cout << "[#" << total << " line " << lineNo << "] " << "\n"
                << "INPUT:  " << in81 << "\n"
                << "OUTPUT: " << out81 << "\n"
                << "RESULT: PASSED\n\n";
    } else {
      failed++;
      std::cout << "[#" << total << " line " << lineNo << "] " << "\n"
                << "INPUT:  " << in81 << "\n"
                << "OUTPUT: " << out81 << "\n"
                << "RESULT: FAILED (" << why << ")\n\n";
    }
  }

  std::cout << "SUMMARY: total=" << total << " passed=" << passed << " failed=" << failed << "\n";

  return 0;
}
