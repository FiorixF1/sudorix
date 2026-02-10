
# =========================
# Sudorix - Unified Makefile
# =========================

# ---- Project layout
SRC_DIR         ?= src
INC_DIR         ?= inc
TEST_DIR        ?= test

# Where to put build outputs
BUILD_DIR       ?= build
OBJ_DIR         ?= $(BUILD_DIR)/obj
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
CXXFLAGS        ?= -O3 -std=c++17 -I$(INC_DIR)
LDFLAGS         ?=
EMCCFLAGS       ?= -O3 -std=c++17 -I$(INC_DIR)

# ---- Sources (all .cpp in src)
SRCS            := $(wildcard $(SRC_DIR)/*.cpp)
OBJS            := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Test main
TEST_MAIN_CPP   ?= $(TEST_DIR)/sudorix_solver_test_main.cpp
TEST_BIN        := $(BIN_DIR)/sudorix_test

# Emscripten exports (keep aligned with C API)
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
	@echo "  make serve       -> serve WEB_DIR via http.server (requires wasm)"
	@echo "  make clean       -> remove build artifacts"
	@echo ""
	@echo "Vars:"
	@echo "  SRC_DIR=src INC_DIR=inc TEST_DIR=tests WEB_DIR=web"
	@echo "  PUZZLES=path/to/file.txt MODE=full|step"
	@echo ""
	@echo "Detected sources: $(SRCS)"

# -----------------
# Directory helpers
# -----------------
$(BUILD_DIR) $(OBJ_DIR) $(BIN_DIR) $(WEB_DIR):
	@mkdir -p $@

# ------------
# Native build
# ------------
native: $(OBJS)

# Compile each src/*.cpp -> build/obj/*.o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) -c $< $(CXXFLAGS) -o $@
	@echo "Built: $@"

# ---------------
# Test executable
# ---------------
test: $(TEST_BIN)

$(TEST_BIN): $(TEST_MAIN_CPP) $(OBJS) | $(BIN_DIR)
	$(CXX) $(TEST_MAIN_CPP) $(OBJS) $(LDFLAGS) -std=c++17 -O2 -I$(INC_DIR) -o $@
	@echo "Built: $@"

run: test
	@echo "Running tests: $(TEST_BIN) $(PUZZLES) --mode=$(MODE)"
	$(TEST_BIN) $(PUZZLES) --mode=$(MODE)

# -----------
# WASM build
# -----------
wasm: $(WASM_JS) $(WASM_WASM)

# NOTE: for WASM we compile/link all src/*.cpp in one emcc invocation.
$(WASM_JS) $(WASM_WASM): $(SRCS) | $(WEB_DIR)
	@command -v $(EMCC) >/dev/null 2>&1 || (echo "ERROR: emcc not found. Activate emsdk first (source emsdk_env.sh)." && exit 1)
	$(EMCC) $(SRCS) \
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

# ----------------
# Serve web assets
# ----------------
serve: wasm
	@echo "Serving $(WEB_DIR) on http://localhost:8000"
	cp $(SRC_DIR)/sudorix.html $(WEB_DIR)
	cp $(SRC_DIR)/sudorix.css $(WEB_DIR)
	cp $(SRC_DIR)/sudorix.js $(WEB_DIR)
	cd $(WEB_DIR) && $(PYTHON) -m http.server 8000

# -------
# Cleanup
# -------
clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

distclean: clean
	@rm -rf $(WEB_DIR)
