#include "API.hpp"

// =========================================================
// JSON Objects
// =========================================================

json api_ok() {
  return {
    {"status", "ok"}
  };
}

json api_error(ApiError code, const std::string &message) {
  return {
    {"status", "error"},
    {"code", code},
    {"error", message}
  };
}
