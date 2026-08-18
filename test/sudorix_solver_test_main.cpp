#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <chrono>
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

static std::string normalize81(const std::string &line, std::string &err) {
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
    std::ostringstream oss;
    oss << "Expected 81 chars, got " << compact.size();
    err = oss.str();
    return "";
  }

  for (char &c : compact) {
    if (!isValidSudokuChar(c)) {
      err = "Invalid character (allowed: 0-9 or .)";
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

// TODO: rimuovere riferimento all'ultimo valore dell'enum
static constexpr size_t kReasonCount = static_cast<size_t>(ReasonId::QuadrupleFireworks) + 1;
static std::vector<uint64_t> g_reasonCounts(kReasonCount, 0);

static void recordReasonId(uint32_t reasonIdRaw, uint32_t detailedReasonIdRaw) {
  if (reasonIdRaw < g_reasonCounts.size()) {
    g_reasonCounts[reasonIdRaw]++;
  }
  if (detailedReasonIdRaw != reasonIdRaw && detailedReasonIdRaw < g_reasonCounts.size()) {
    g_reasonCounts[detailedReasonIdRaw]++;
  }
}

static void printTechniqueUsageSummary() {
  bool printedAny = false;
  std::cout << "TECHNIQUE USAGE:\n";
  for (size_t i = 0; i < g_reasonCounts.size(); i++) {
    printedAny = true;
    std::cout << "  " << std::left << std::setw(32)
              << json(static_cast<ReasonId>(i)).dump()
              << " : " << g_reasonCounts[i] << "\n";
  }
  if (!printedAny) {
    std::cout << "  (no techniques recorded)\n";
  }
}

static bool checkUnitMask(const std::vector<int> &idxs, const std::string &out81, std::string &why) {
  uint16_t seen = 0;
  for (int idx : idxs) {
    char c = out81[(size_t)idx];
    if (c < '1' || c > '9') {
      std::ostringstream oss;
      oss << "Non-digit in solution at idx=" << idx << " ('" << c << "')";
      why = oss.str();
      return false;
    }
    int d = c - '0';
    uint16_t b = bitForDigit(d);
    if ((seen & b) != 0) {
      std::ostringstream oss;
      oss << "Duplicate digit " << d << " in unit";
      why = oss.str();
      return false;
    }
    seen = static_cast<uint16_t>(seen | b);
  }
  if (seen != 0x01FFu) {
    why = "Unit does not contain all digits 1..9";
    return false;
  }
  return true;
}

static bool validateSolution(const std::string &in81, const std::string &out81, std::string &why) {
  if (out81.size() != 81) {
    why = "Output length != 81";
    return false;
  }

  // Check givens are preserved
  for (int i = 0; i < 81; i++) {
    char in = in81[(size_t)i];
    char out = out81[(size_t)i];
    if (in >= '1' && in <= '9') {
      if (out != in) {
        std::ostringstream oss;
        oss << "Given mismatch at idx=" << i << " (in=" << in << ", out=" << out << ")";
        why = oss.str();
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
    if (!checkUnitMask(rows[(size_t)u], out81, w)) {
      std::ostringstream oss;
      oss << "Row " << u << " invalid: " << w;
      why = oss.str();
      return false;
    }
    if (!checkUnitMask(cols[(size_t)u], out81, w)) {
      std::ostringstream oss;
      oss << "Col " << u << " invalid: " << w;
      why = oss.str();
      return false;
    }
    if (!checkUnitMask(boxes[(size_t)u], out81, w)) {
      std::ostringstream oss;
      oss << "Box " << u << " invalid: " << w;
      why = oss.str();
      return false;
    }
  }

  return true;
}

static int runFullSolveOne(const std::string &in81, std::string &out81, std::string &why) {
  json request;
  request["command"] = "fullSolve";
  request["puzzle"] = in81;
  for (int i = 0; i < g_reasonCounts.size(); ++i) request["techniques"].push_back(json(static_cast<ReasonId>(i)));
  json response = sudorix_solver_api(request);

  if (response["status"].get<std::string>() == "error") {
    why = response["error"].get<std::string>();
    return 0;
  }

  out81 = response["solution"].get<std::string>();

  return validateSolution(in81, out81, why);
}

static int runStepSolveOne(const std::string &in81, std::string &out81, std::string &why) {
  json request;
  request["command"] = "initBoard";
  request["puzzle"] = in81;
  json response = sudorix_solver_api(request);

  if (response["status"].get<std::string>() == "error") {
    why = response["error"].get<std::string>();
    return 0;
  }

  int guard = 0;
  const int guardMax = 200000;

  while (guard++ < guardMax) {
    uint32_t out[1024];
    std::memset(out, 0, sizeof(out));
  
    json request;
    request["command"] = "nextStep";
    json response = sudorix_solver_api(request);
    if (response["status"].get<std::string>() == "error") {
      break;
    }

    if (response.contains("step")) {
      ReasonId reason = response["step"]["reason"];
      ReasonId detailedReason = response["step"]["detailedReason"];
      recordReasonId(static_cast<uint32_t>(reason), static_cast<uint32_t>(detailedReason));
    }
  }

  if (guard >= guardMax) {
    why = "Step solve guard limit reached";
    return 0;
  }

  request["command"] = "exportBoard";
  response = sudorix_solver_api(request);
  if (response["status"].get<std::string>() == "error") {
    why = response["error"].get<std::string>();
    return 0;
  }

  std::string values = response["board"]["values"].get<std::string>();
  out81 = values;

  return validateSolution(in81, out81, why);
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
  uint64_t elapsedUs = 0;
  std::string input;
  std::string output;
  std::string why;
};

static std::string formatResult(const TestResult &r) {
  std::ostringstream oss;
  oss << "[#" << r.caseNo << " line " << r.lineNo << "] \n"
      << "INPUT:  " << r.input << "\n"
      << "OUTPUT: " << r.output << "\n"
      << "TIME:   " << r.elapsedUs/1000.0 << " ms\n"
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
    std::string in81 = normalize81(line, err);
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
  std::vector<uint64_t> solveTimesUs(cases.size(), 0);
  std::atomic<size_t> nextIndex{0};

  // set allowed techniques - default all
  json request;
  request["command"] = "setEnabledTechniques";
  request["techniques"] = json::array();
  for (int i = 0; i < g_reasonCounts.size(); ++i) request["techniques"].push_back(json(static_cast<ReasonId>(i)));
  sudorix_solver_api(request);

  auto worker = [&]() {
    while (true) {
      size_t i = nextIndex.fetch_add(1);
      if (i >= cases.size()) {
        break;
      }

      if (i % 1000 == 0) {
        std::cerr << "Reached puzzle " << i << "\n";
      }

      const TestCase &tc = cases[i];
      TestResult r;
      r.caseNo = tc.caseNo;
      r.lineNo = tc.lineNo;
      r.input = tc.input81;

      std::string out81;
      std::string why;
      int ok = 0;

      auto t0 = std::chrono::steady_clock::now();
      if (mode == "full") {
        ok = runFullSolveOne(tc.input81, out81, why);
      } else {
        ok = runStepSolveOne(tc.input81, out81, why);
      }
      auto t1 = std::chrono::steady_clock::now();

      uint64_t elapsedUs = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
      solveTimesUs[i] = elapsedUs;

      r.passed = (ok != 0);
      r.elapsedUs = elapsedUs;
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

  std::cout << "SUMMARY: total=" << total << " passed=" << passed << " failed=" << failed << "\n\n";

  solveTimesUs;
  uint64_t minTimeUs = 999999999;
  uint64_t maxTimeUs = 0;
  uint64_t sumTimeUs = 0;
  for (uint64_t t : solveTimesUs) {
    minTimeUs = std::min(minTimeUs, t);
    maxTimeUs = std::max(maxTimeUs, t);
    sumTimeUs += t;
  }
  uint64_t avgTimeUs = passed != 0 ? static_cast<double>(sumTimeUs) / passed : 0;

  std::cout << "PERFORMANCE SUMMARY:\n";
  std::cout << "Minimum time: " << minTimeUs/1000.0 << " ms\n";
  std::cout << "Average time: " << avgTimeUs/1000.0 << " ms\n";
  std::cout << "Maximum time: " << maxTimeUs/1000.0 << " ms\n\n";

  if (singleThreadMode) printTechniqueUsageSummary();

  return 0;
}
