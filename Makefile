
# =========================
# Sudorix - Unified Makefile
# =========================

# ---- Project layout
SRC_DIR         ?= src
INC_DIR         ?= inc
TEST_DIR        ?= test
SOLVER_CPP      ?= $(SRC_DIR)/solver.cpp
SOLVER_HPP      ?= $(INC_DIR)/solver.hpp
TEST_MAIN_CPP   ?= $(TEST_DIR)/sudorix_solver_test_main.cpp

# Where to put build outputs
BUILD_DIR       ?= build
BIN_DIR         ?= bin
WEB_DIR         ?= web

# Test data file (one puzzle per line, 81 chars, 0-9 or '.')
PUZZLES         ?= puzzles.txt
MODE            ?= full   # full|step (step is stub in current test main)

# Tools
CXX             ?= g++
EMCC            ?= emcc
PYTHON          ?= python3

# Flags
CXXFLAGS        ?= -O3 -std=c++17
EMCCFLAGS       ?= -O3 -std=c++17

# Emscripten exports (from your build_wasm.sh)
EMCC_EXPORTED_FUNCTIONS := "['_malloc','_free','_sudorix_solver_full','_sudorix_solver_init_board','_sudorix_solver_next_step','_sudorix_solver_hint']"
EMCC_EXPORTED_RUNTIME   := "['cwrap','HEAPU8','HEAPU16','HEAPU32','lengthBytesUTF8','stringToUTF8','UTF8ToString']"

# Targets
SOLVER_OBJ      := $(BUILD_DIR)/solver.o
TEST_BIN        := $(BIN_DIR)/sudorix_test
WASM_JS         := $(WEB_DIR)/solver_wasm.js
WASM_WASM       := $(WEB_DIR)/solver_wasm.wasm

.PHONY: all wasm native test run clean distclean serve help

all: wasm native test

help:
	@echo "Targets:"
	@echo "  make wasm        -> build WASM (solver_wasm.js + solver_wasm.wasm)"
	@echo "  make native      -> build native object (solver.o)"
	@echo "  make test        -> build test binary"
	@echo "  make run         -> run tests (PUZZLES=..., MODE=full|step)"
	@echo "  make serve       -> serve WEB_DIR via http.server"
	@echo "  make clean       -> remove build artifacts"
	@echo ""
	@echo "Vars:"
	@echo "  PUZZLES=path/to/file.txt   MODE=full|step"
	@echo "  SOLVER_CPP=... TEST_MAIN_CPP=... WEB_DIR=... BUILD_DIR=... BIN_DIR=..."

# -----------------
# Directory helpers
# -----------------
$(BUILD_DIR) $(BIN_DIR) $(WEB_DIR):
	@mkdir -p $@

# ------------
# Native build
# ------------
native: $(SOLVER_OBJ)

$(SOLVER_OBJ): $(SOLVER_CPP) $(SOLVER_HPP) | $(BUILD_DIR)
	$(CXX) -c $(SOLVER_CPP) -I$(INC_DIR) $(CXXFLAGS) -o $@
	@echo "Built: $@"

# -----------
# WASM build
# -----------
wasm: $(WASM_JS) $(WASM_WASM)

$(WASM_JS) $(WASM_WASM): $(SOLVER_CPP) $(SOLVER_HPP) | $(WEB_DIR)
	@command -v $(EMCC) >/dev/null 2>&1 || (echo "ERROR: emcc not found. Activate emsdk first (source emsdk_env.sh)." && exit 1)
	$(EMCC) $(SOLVER_CPP) \
	  -I$(INC_DIR) \
	  $(EMCCFLAGS) \
	  -sWASM=1 \
	  -sENVIRONMENT=web \
	  -sMODULARIZE=1 \
	  -sEXPORT_NAME="createSudorixSolver" \
	  -sALLOW_MEMORY_GROWTH=1 \
	  -sEXPORTED_FUNCTIONS=$(EMCC_EXPORTED_FUNCTIONS) \
	  -sEXPORTED_RUNTIME_METHODS=$(EMCC_EXPORTED_RUNTIME) \
	  -o $(WASM_JS)
	@echo "Built: $(WASM_JS) + $(WASM_WASM)"

# ---------------
# Test executable
# ---------------
test: $(TEST_BIN)

$(TEST_BIN): $(TEST_MAIN_CPP) $(SOLVER_OBJ) $(SOLVER_HPP) | $(BIN_DIR)
	$(CXX) $(TEST_MAIN_CPP) $(SOLVER_OBJ) -I$(INC_DIR) -std=c++17 -O2 -o $@
	@echo "Built: $@"

# ---------
# Run tests
# ---------
run: test
	@echo "Running tests: $(TEST_BIN) $(PUZZLES) --mode=$(MODE)"
	$(TEST_BIN) $(PUZZLES) --mode=$(MODE)

# ----------------
# Serve web assets
# ----------------
serve: wasm
	@echo "Serving $(WEB_DIR) on http://localhost:8000"
	cp $(SRC_DIR)/Sudorix.html $(WEB_DIR)
	cd $(WEB_DIR) && $(PYTHON) -m http.server 8000

# -------
# Cleanup
# -------
clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

distclean: clean
	@rm -rf $(WEB_DIR)
