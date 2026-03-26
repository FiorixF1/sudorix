#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Event.hpp"
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

static constexpr size_t kReasonCount = static_cast<size_t>(ReasonId::DeathBlossom) + 1;
static std::vector<uint64_t> g_reasonCounts(kReasonCount, 0);

static const char *reasonIdToString(ReasonId reason) {
  switch (reason) {
    case ReasonId::Solver: return "Solver";
    case ReasonId::FullHouse: return "Full House";
    case ReasonId::NakedSingle: return "Naked Single";
    case ReasonId::NakedPair: return "Naked Pair";
    case ReasonId::NakedTriple: return "Naked Triple";
    case ReasonId::NakedQuad: return "Naked Quad";
    case ReasonId::HiddenSingle: return "Hidden Single";
    case ReasonId::HiddenPair: return "Hidden Pair";
    case ReasonId::HiddenTriple: return "Hidden Triple";
    case ReasonId::HiddenQuad: return "Hidden Quad";
    case ReasonId::PointingPair: return "Pointing Pair";
    case ReasonId::PointingTriple: return "Pointing Triple";
    case ReasonId::ClaimingPair: return "Claiming Pair";
    case ReasonId::ClaimingTriple: return "Claiming Triple";
    case ReasonId::XWing: return "X-Wing";
    case ReasonId::Swordfish: return "Swordfish";
    case ReasonId::Jellyfish: return "Jellyfish";
    case ReasonId::FinnedXWing: return "Finned X-Wing";
    case ReasonId::FinnedSwordfish: return "Finned Swordfish";
    case ReasonId::FinnedJellyfish: return "Finned Jellyfish";
    case ReasonId::SashimiXWing: return "Sashimi X-Wing";
    case ReasonId::SashimiSwordfish: return "Sashimi Swordfish";
    case ReasonId::SashimiJellyfish: return "Sashimi Jellyfish";
    case ReasonId::FrankenXWing: return "Franken X-Wing";
    case ReasonId::FrankenSwordfish: return "Franken Swordfish";
    case ReasonId::FrankenJellyfish: return "Franken Jellyfish";
    case ReasonId::FinnedFrankenXWing: return "Finned Franken X-Wing";
    case ReasonId::FinnedFrankenSwordfish: return "Finned Franken Swordfish";
    case ReasonId::FinnedFrankenJellyfish: return "Finned Franken Jellyfish";
    case ReasonId::MutantXWing: return "Mutant X-Wing";
    case ReasonId::MutantSwordfish: return "Mutant Swordfish";
    case ReasonId::MutantJellyfish: return "Mutant Jellyfish";
    case ReasonId::FinnedMutantXWing: return "Finned Mutant X-Wing";
    case ReasonId::FinnedMutantSwordfish: return "Finned Mutant Swordfish";
    case ReasonId::FinnedMutantJellyfish: return "Finned Mutant Jellyfish";
    case ReasonId::SiameseFish: return "Siamese Fish";
    case ReasonId::KrakenFish: return "Kraken Fish";
    case ReasonId::SingleDigitPattern: return "Single Digit Pattern";
    case ReasonId::Skyscraper: return "Skyscraper";
    case ReasonId::TwoStringKite: return "Two-String Kite";
    case ReasonId::Crane: return "Crane";
    case ReasonId::EmptyRectangle: return "Empty Rectangle";
    case ReasonId::UniqueRectangle: return "Unique Rectangle";
    case ReasonId::UniqueRectangleType1: return "Unique Rectangle (Type 1)";
    case ReasonId::UniqueRectangleType2: return "Unique Rectangle (Type 2)";
    case ReasonId::UniqueRectangleType3: return "Unique Rectangle (Type 3)";
    case ReasonId::UniqueRectangleType4: return "Unique Rectangle (Type 4)";
    case ReasonId::UniqueRectangleType5: return "Unique Rectangle (Type 5)";
    case ReasonId::UniqueRectangleType6: return "Unique Rectangle (Type 6)";
    case ReasonId::HiddenRectangle: return "Hidden Rectangle";
    case ReasonId::AvoidableRectangle: return "Avoidable Rectangle";
    case ReasonId::BUGPlusOne: return "BUG+1";
    case ReasonId::XYWing: return "XY-Wing";
    case ReasonId::XYZWing: return "XYZ-Wing";
    case ReasonId::WXYZWing: return "WXYZ-Wing";
    case ReasonId::ChuteRemotePair: return "Chute Remote Pair";
    case ReasonId::WWing: return "W-Wing";
    case ReasonId::SimpleColoring: return "Simple Coloring";
    case ReasonId::SimpleColoringColorTrap: return "Simple Coloring (Color Trap)";
    case ReasonId::SimpleColoringColorWrap: return "Simple Coloring (Color Wrap)";
    case ReasonId::_3DMedusa: return "3D Medusa";
    case ReasonId::_3DMedusaColorTrap: return "3D Medusa (Color Trap)";
    case ReasonId::_3DMedusaColorWrap: return "3D Medusa (Color Wrap)";
    case ReasonId::_3DMedusaEmptiedCell: return "3D Medusa (Emptied Cell)";
    case ReasonId::RemotePair: return "Remote Pair";
    case ReasonId::XChain: return "X-Chain";
    case ReasonId::XRing: return "X-Ring";
    case ReasonId::XYChain: return "XY-Chain";
    case ReasonId::XYRing: return "XY-Ring";
    case ReasonId::AIC: return "AIC";
    case ReasonId::AICType1: return "AIC (Type 1)";
    case ReasonId::AICType2: return "AIC (Type 2)";
    case ReasonId::AICType3: return "AIC (Type 3)";
    case ReasonId::GroupedAIC: return "Grouped AIC";
    case ReasonId::GroupedAICType1: return "Grouped AIC (Type 1)";
    case ReasonId::GroupedAICType2: return "Grouped AIC (Type 2)";
    case ReasonId::GroupedAICType3: return "Grouped AIC (Type 3)";
    case ReasonId::ALSXZ: return "ALS-XZ";
    case ReasonId::ALSXY: return "ALS-XY";
    case ReasonId::ALSChain: return "ALS Chain";
    case ReasonId::SueDeCoq: return "Sue-De-Coq";
    case ReasonId::DeathBlossom: return "Death Blossom";
  }
  return "Unknown Reason";
}

static void recordReasonId(uint32_t reasonIdRaw) {
  if (reasonIdRaw < g_reasonCounts.size()) {
    g_reasonCounts[reasonIdRaw]++;
  }
}

static void printTechniqueUsageSummary() {
  bool printedAny = false;
  std::cout << "TECHNIQUE USAGE:\n";
  for (size_t i = 0; i < g_reasonCounts.size(); i++) {
    printedAny = true;
    std::cout << "  " << std::left << std::setw(28)
              << reasonIdToString(static_cast<ReasonId>(i))
              << " : " << g_reasonCounts[i] << "\n";
  }
  if (!printedAny) {
    std::cout << "  (no techniques recorded)\n";
  }
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

static int runStepSolveOne(const std::string &in81, std::string *out81, std::string *why) {
  uint8_t outValuesBuf[81];
  std::memset(outValuesBuf, 0, sizeof(outValuesBuf));

  uint16_t outCandidatesBuf[81];
  std::memset(outCandidatesBuf, 0, sizeof(outCandidatesBuf));

  int rc = sudorix_solver_init_board(in81.c_str());
  if (rc == 0) {
    if (why) {
      *why = "sudorix_solver_init_board returned 0 (failure)";
    }
    return 0;
  }

  int guard = 0;
  const int guardMax = 200000;

  while (guard++ < guardMax) {
    uint32_t out[1024];
    std::memset(out, 0, sizeof(out));

    rc = sudorix_solver_next_step(out, 1024);
    if (rc == 0) {
      break;
    }

    if (out[0] != 0) {
      recordReasonId(out[1]);
    }
  }

  if (guard >= guardMax) {
    if (why) {
      *why = "Step solve guard limit reached";
    }
    return 0;
  }

  rc = sudorix_solver_export_board(outValuesBuf, outCandidatesBuf);
  if (rc == 0) {
    if (why) {
      *why = "sudorix_solver_export_board returned 0 (failure)";
    }
    return 0;
  }

  out81->clear();
  out81->reserve(81);
  for (int i = 0; i < 81; i++) {
    const uint8_t value = outValuesBuf[i];
    out81->push_back(value ? static_cast<char>('0' + value) : '.');
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

static void usage(const char *argv0) {
  std::cerr
      << "Usage: " << argv0 << " <sudoku_file.txt> [--mode=full|step] [--threads=N]\n"
      << "  Each non-empty, non-comment line must contain 81 chars: digits 0-9 or '.' for empty.\n";
}

struct TestCase {
  size_t caseNo;
  size_t lineNo;
  std::string input81;
};

struct TestResult {
  bool passed = false;
  size_t caseNo = 0;
  size_t lineNo = 0;
  std::string input;
  std::string output;
  std::string why;
};

static std::string formatResult(const TestResult &r) {
  std::ostringstream oss;
  oss << "[#" << r.caseNo << " line " << r.lineNo << "] \n"
      << "INPUT:  " << r.input << "\n"
      << "OUTPUT: " << r.output << "\n"
      << "RESULT: " << (r.passed ? "PASSED" : "FAILED");
  if (!r.passed && !r.why.empty()) {
    oss << " (" << r.why << ")";
  }
  oss << "\n\n";
  return oss.str();
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }

  std::string path = argv[1];
  std::string mode = "full";
  unsigned int threads = std::thread::hardware_concurrency();
  if (threads == 0) {
    threads = 1;
  }

  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a.rfind("--mode=", 0) == 0) {
      mode = a.substr(std::strlen("--mode="));
    } else if (a.rfind("--threads=", 0) == 0) {
      threads = static_cast<unsigned int>(std::strtoul(a.substr(std::strlen("--threads=")).c_str(), nullptr, 10));
      if (threads == 0) {
        threads = 1;
      }
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

  std::vector<TestCase> cases;
  std::vector<TestResult> invalidResults;

  std::string line;
  size_t lineNo = 0;
  size_t caseNo = 0;

  while (std::getline(fin, line)) {
    lineNo++;

    std::string err;
    std::string in81 = normalize81(line, &err);
    if (in81.empty()) {
      // Either blank/comment, or invalid. Distinguish:
      std::string t = trim(line);
      if (!t.empty() && t[0] != '#') {
        caseNo++;
        TestResult r;
        r.passed = false;
        r.caseNo = caseNo;
        r.lineNo = lineNo;
        r.input = t;
        r.output = "(n/a)";
        r.why = err;
        invalidResults.push_back(std::move(r));
      }
      continue;
    }

    caseNo++;
    TestCase tc;
    tc.caseNo = caseNo;
    tc.lineNo = lineNo;
    tc.input81 = std::move(in81);
    cases.push_back(std::move(tc));
  }

  if (mode == "step") {
    if (threads != 1) {
      std::cerr << "Step mode is not thread-safe, forcing --threads=1\n";
    }
    threads = 1;
  } else {
    if (!cases.empty() && threads > cases.size()) {
      threads = static_cast<unsigned int>(cases.size());
    }
    if (threads == 0) {
      threads = 1;
    }
  }

  std::cerr << "Loaded " << caseNo << " test lines, running " << cases.size()
            << " valid puzzles on " << threads << " thread(s)...\n";

  bool singleThreadMode = (threads == 1);

  std::vector<TestResult> results(cases.size());
  std::atomic<size_t> nextIndex{0};

  auto worker = [&]() {
    while (true) {
      size_t i = nextIndex.fetch_add(1);
      if (i >= cases.size()) {
        break;
      }

      const TestCase &tc = cases[i];
      TestResult r;
      r.caseNo = tc.caseNo;
      r.lineNo = tc.lineNo;
      r.input = tc.input81;

      std::string out81;
      std::string why;
      int ok = 0;

      if (mode == "full") {
        ok = runFullSolveOne(tc.input81, &out81, &why);
      } else {
        ok = runStepSolveOne(tc.input81, &out81, &why);
      }

      r.passed = (ok != 0);
      r.output = out81;
      r.why = why;
      if (singleThreadMode) {
        std::cout << formatResult(r);
      }
      results[i] = std::move(r);
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(threads);
  for (unsigned int t = 0; t < threads; t++) {
    pool.emplace_back(worker);
  }
  for (auto &th : pool) {
    th.join();
  }

  size_t total = 0;
  size_t passed = 0;
  size_t failed = 0;

  for (const auto &r : invalidResults) {
    total++;
    failed++;
    if (!singleThreadMode) std::cout << formatResult(r);
  }

  for (const auto &r : results) {
    total++;
    if (r.passed) {
      passed++;
    } else {
      failed++;
    }
    if (!singleThreadMode) std::cout << formatResult(r);
  }

  std::cout << "SUMMARY: total=" << total << " passed=" << passed << " failed=" << failed << "\n";
  if (singleThreadMode) printTechniqueUsageSummary();
  return (failed == 0) ? 0 : 1;
}
