var business_logic = (() => {
  /* =========================================================
   * Constants / Palette
   * ========================================================= */
  const ALL_CANDIDATES_MASK = (1 << 9) - 1;

  const PALETTE = [
    "#00D1FF", /* cyan */
    "#FF2BD6", /* magenta */
    "#7CFF00", /* neon green */
    "#FFD400", /* yellow */
    "#FF4D4D", /* red */
    "#7A5CFF", /* purple */
    "#FF8A00", /* orange */
    "#00FF9A", /* mint */
    "#2B6BFF"  /* blue */
  ];

  /* =========================================================
   * WASM solver bridge (C++/Emscripten)
   *
   * Expects Emscripten output:
   *   - solver_wasm.js
   *   - solver_wasm.wasm
   * built with EXPORT_NAME=createSudorixSolver and exported functions sudorix_solver_step + sudorix_queue_clear.
   * ========================================================= */
  let wasmModule = null;
  let wasmSolveFull = null;      // cwrap'd function
  let wasmSolveInit = null;      // cwrap'd function
  let wasmSolveNextStep = null;  // cwrap'd function
  let wasmSolveHint = null;      // cwrap'd function
  let wasmBufValues = 0;         // malloc'ed pointers in WASM heap
  let wasmBufCands  = 0;
  let wasmBufOut    = 0;

  const WASM_REASON = {
    0: "Solver",
    1: "Full house",
    2: "Naked single",
    3: "Hidden single",
    4: "Pointing Pair",
    5: "Pointing Triple",
    6: "Locked candidates"
  };

  function initWasmSolver() {
    // createSudorixSolver is defined by solver_wasm.js (Emscripten output)
    if (typeof createSudorixSolver !== "function") {
      appendLog("WASM: solver_wasm.js ne ŝargita. Solvilo ne disponebla.");
      return;
    }

    wasmReady = createSudorixSolver({
      locateFile: (path) => path  // keep .wasm next to .js
    }).then((Module) => {
      wasmModule = Module;
      wasmSolveFull = wasmModule.cwrap("sudorix_solver_full", "number", ["number", "number"]);
      wasmSolveInit = wasmModule.cwrap("sudorix_solver_init_board", "number", ["number"]);
      wasmSolveNextStep = wasmModule.cwrap("sudorix_solver_next_step", "number", ["number"]);
      wasmSolveHint = wasmModule.cwrap("sudorix_solver_hint", "number", ["number", "number", "number"]);

      wasmBufValues = wasmModule._malloc(81);          // uint8_t[81]
      wasmBufCands  = wasmModule._malloc(81 * 2);      // uint16_t[81]
      wasmBufInStr  = wasmModule._malloc(82);          // char[81] + '\0'
      wasmBufOut    = wasmModule._malloc(5 * 4);       // uint32_t[5]

      appendLog("WASM: solvilo preta.");
      return Module;
    }).catch((e) => {
      appendLog("WASM: malsukcesis ŝargi solvilon: " + (e && e.message ? e.message : String(e)));
      wasmModule = null;
      wasmSolveFull = null;
      wasmSolveInit = null;
      wasmSolveNextStep = null;
      wasmSolveHint = null;
    });
  }

  function wasmRunFullSolve(in81) {
    if (!wasmModule || !wasmSolveFull) {
      return null;
    }

    const inLen = wasmModule.lengthBytesUTF8(in81) + 1;
    const inPtr = wasmModule._malloc(inLen);
    const outPtr = wasmModule._malloc(82); // 81 chars + '\0'

    try {
      wasmModule.stringToUTF8(in81, inPtr, inLen);
      wasmModule.HEAPU8.fill(0, outPtr, outPtr + 82);

      const rc = wasmSolveFull(inPtr, outPtr);
      if (rc === 0) {
        return null;
      }

      return wasmModule.UTF8ToString(outPtr, 81);
    } finally {
      wasmModule._free(inPtr);
      wasmModule._free(outPtr);
    }
  }

  function wasmInitBoard(boardRef) {
    if (!wasmModule || !wasmSolveInit) {
      return null;
    }

    const s = boardRef.exportToString();
    const enc = new TextEncoder();
    const bytes = enc.encode(String(s || ""));
    // Copy at most 81 chars; terminate.
    const max = Math.min(bytes.length, 81);
    for (let i = 0; i < max; i++) {
      wasmModule.HEAPU8[wasmBufInStr + i] = bytes[i];
    }
    for (let i = max; i < 81; i++) {
      wasmModule.HEAPU8[wasmBufInStr + i] = 46; // '.'
    }
    wasmModule.HEAPU8[wasmBufInStr + 81] = 0;

    const ok = wasmSolveInit(wasmBufInStr);
    if (!ok) {
      // Boh
    }
    return null;
  }

  function wasmComputeNextStep() {
    if (!wasmModule || !wasmSolveNextStep) {
      return null;
    }

    // Call C++: out[0]=type, out[1]=idx, out[2]=digit, out[3]=reasonId, out[4]=fromPrev
    const ok = wasmSolveNextStep(wasmBufOut);
    if (!ok) {
      return null;
    }

    const out = wasmModule.HEAPU32.subarray(wasmBufOut >> 2, (wasmBufOut >> 2) + 5);
    const type = out[0] >>> 0;
    const idx = out[1] >>> 0;
    const digit = out[2] >>> 0;
    const reasonId = out[3] >>> 0;
    const fromPrev = (out[4] >>> 0) !== 0;

    if (type === 1) {
      return { type: "setValue", idx, digit, reason: WASM_REASON[reasonId] || "Solver", fromPrev };
    }
    if (type === 2) {
      return { type: "removeCandidate", idx, digit, reason: WASM_REASON[reasonId] || "Solver", fromPrev };
    }
    return null;
  }

  function wasmComputeHint(boardRef) {
    if (!wasmModule || !wasmSolveHint) {
      return null;
    }

    // Snapshot board -> TypedArrays
    const values = new Uint8Array(81);
    const cands  = new Uint16Array(81);
    for (let i = 0; i < 81; i++) {
      values[i] = boardRef.getValue(i) & 0xFF;
      cands[i]  = boardRef.getCandidateMask(i) & 0xFFFF;
    }

    // Copy into WASM heap
    wasmModule.HEAPU8.set(values, wasmBufValues);
    wasmModule.HEAPU16.set(cands, wasmBufCands >> 1);

    // Call C++: out[0]=type, out[1]=idx, out[2]=digit, out[3]=reasonId, out[4]=fromPrev
    const ok = wasmSolveHint(wasmBufValues, wasmBufCands, wasmBufOut);
    if (!ok) {
      return null;
    }

    const out = wasmModule.HEAPU32.subarray(wasmBufOut >> 2, (wasmBufOut >> 2) + 5);
    const type = out[0] >>> 0;
    const idx = out[1] >>> 0;
    const digit = out[2] >>> 0;
    const reasonId = out[3] >>> 0;
    const fromPrev = (out[4] >>> 0) !== 0;

    if (type === 1) {
      return { type: "setValue", idx, digit, reason: WASM_REASON[reasonId] || "Solver", fromPrev };
    }
    if (type === 2) {
      return { type: "removeCandidate", idx, digit, reason: WASM_REASON[reasonId] || "Solver", fromPrev };
    }
    return null;
  }

  /* =========================================================
   * Small utils (no board knowledge)
   * ========================================================= */
  const rowOf = (idx) => Math.floor(idx / 9);
  const colOf = (idx) => idx % 9;

  function idxToRC(idx) {
    return { r: rowOf(idx) + 1, c: colOf(idx) + 1 };
  }

  function RCToIdx(r, c) {
    return r*9 + c;
  }

  function parseHexColor(hex) {
    const s = hex.replace("#", "").trim();
    if (s.length !== 6) {
      return { r: 0, g: 0, b: 0 };
    }
    const r = parseInt(s.slice(0, 2), 16);
    const g = parseInt(s.slice(2, 4), 16);
    const b = parseInt(s.slice(4, 6), 16);
    return { r, g, b };
  }

  function relativeLuminance(hex) {
    const { r, g, b } = parseHexColor(hex);
    const srgb = [r, g, b].map((v) => v / 255);
    const lin = srgb.map((c) => (c <= 0.04045) ? (c / 12.92) : Math.pow((c + 0.055) / 1.055, 2.4));
    return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2];
  }

  function bestTextColorForBg(bgHex) {
    const L = relativeLuminance(bgHex);
    /* heuristic: if background is bright -> black text, else white text */
    return (L > 0.36) ? "#0b0f14" : "#eaf2ff";
  }

  function countBits9(mask) {
    // m fits in 9 bits; JS bit ops are 32-bit
    let m = mask & 0x1FF;
    let c = 0;
    while (m) {
      m &= (m - 1);
      c++;
    }
    // return number of active bits
    return c;
  }

  function singleBitIndex(mask) {
    // return 0..8 for least significant set bit
    const m = mask & 0x1FF;
    const lb = m & -m;
    return Math.log2(lb) | 0;
  }

  function digitToBit(digit) {
    return 1 << (digit - 1);
  }

  function assertDigit(digit) {
    return Number.isInteger(digit) && digit >= 1 && digit <= 9;
  }

  function getDigitFromKeyEvent(e) {
    if (e.key >= "1" && e.key <= "9") {
      return parseInt(e.key, 10);
    }

    if (typeof e.code === "string" && e.code.startsWith("Numpad")) {
      const tail = e.code.slice("Numpad".length);
      if (tail >= "1" && tail <= "9") {
        return parseInt(tail, 10);
      }
    }

    const map = {
      NumpadEnd: 1,
      NumpadArrowDown: 2,
      NumpadPageDown: 3,
      NumpadArrowLeft: 4,
      NumpadClear: 5,
      NumpadArrowRight: 6,
      NumpadHome: 7,
      NumpadArrowUp: 8,
      NumpadPageUp: 9
    };

    if (map[e.code]) {
      return map[e.code];
    }

    return null;
  }

  /* =========================================================
   * Precomputed units / peers
   * ========================================================= */
  const UNITS = {
    rows: Array.from({ length: 9 }, (_, r) => Array.from({ length: 9 }, (_, c) => r * 9 + c)),
    cols: Array.from({ length: 9 }, (_, c) => Array.from({ length: 9 }, (_, r) => r * 9 + c)),
    boxs: Array.from({ length: 9 }, (_, b) => {
      const br = Math.floor(b / 3) * 3;
      const bc = (b % 3) * 3;
      const out = [];
      for (let dr = 0; dr < 3; dr++) {
        for (let dc = 0; dc < 3; dc++) {
          out.push((br + dr) * 9 + (bc + dc));
        }
      }
      return out;
    })
  };

  const PEERS = Array.from({ length: 81 }, (_, idx) => {
    const s = new Set();
    const r = rowOf(idx);
    const c = colOf(idx);
    const b = Math.floor(r / 3) * 3 + Math.floor(c / 3);

    for (const j of UNITS.rows[r]) { if (j !== idx) { s.add(j); } }
    for (const j of UNITS.cols[c]) { if (j !== idx) { s.add(j); } }
    for (const j of UNITS.boxs[b]) { if (j !== idx) { s.add(j); } }

    return Array.from(s);
  });

  /* =========================================================
   * OOP: SudokuCell (private state)
   * ========================================================= */
  class SudokuCell {
    #value;
    #candidateMask;
    #given;

    #cellColorIndex;
    #candidateColorIndex; /* length 9 array */

    constructor() {
      this.#value = 0;          // value 0..9
      this.#candidateMask = 0;  // candidate mask - start without candidates
      this.#given = false;      // imported as fixed clue

      /* cell background color (palette index or -1 for none) */
      this.#cellColorIndex = -1;
      /* per-candidate background colors (palette index or -1) */
      this.#candidateColorIndex = Array.from({ length: 9 }, () => -1);
    }

    /* ---- value / given ---- */
    getValue() {
      return this.#value;
    }

    isSolved() {
      return this.#value !== 0;
    }

    isGiven() {
      return this.#given;
    }

    setGiven(isGiven) {
      this.#given = !!isGiven;
    }

    setValue(digit) {
      this.#value = digit;
      if (digit === 0) {
        /* keep candidates as-is; solver / recalc can fill later */
        return;
      }
      this.#candidateMask = digitToBit(digit);
    }

    clearValue() {
      this.#value = 0;
      /* do not force candidates here */
    }

    /* ---- candidates ---- */
    getCandidateMask() {
      return this.#candidateMask & 0x1FF;
    }

    setCandidateMask(mask) {
      this.#candidateMask = (mask & 0x1FF);
    }

    hasCandidate(digit) {
      return !!(this.#candidateMask & digitToBit(digit));
    }

    enableCandidate(digit) {
      this.#candidateMask |= digitToBit(digit);
    }

    disableCandidate(digit) {
      this.#candidateMask &= ~digitToBit(digit);

      /* If the candidate is removed, remove its candidate-color too */
      this.#candidateColorIndex[digit - 1] = -1;
    }

    toggleCandidate(digit) {
      const bit = digitToBit(digit);
      const wasOn = !!(this.#candidateMask & bit);

      this.#candidateMask ^= bit;

      /* If candidate removed, remove its color too */
      if (wasOn && !(this.#candidateMask & bit)) {
        this.#candidateColorIndex[digit - 1] = -1;
      }

      return !wasOn;
    }

    countCandidates() {
      return countBits9(this.#candidateMask);
    }

    /* ---- coloring ---- */
    getCellColorIndex() {
      return this.#cellColorIndex;
    }

    toggleCellColorIndex(colorIndex) {
      if (this.#cellColorIndex === colorIndex) {
        this.#cellColorIndex = -1;
        return false;
      }
      this.#cellColorIndex = colorIndex;
      return true;
    }

    clearCellColor() {
      this.#cellColorIndex = -1;
    }

    getCandidateColorIndex(digit) {
      return this.#candidateColorIndex[digit - 1];
    }

    toggleCandidateColorIndex(digit, colorIndex) {
      const idx = digit - 1;
      if (this.#candidateColorIndex[idx] === colorIndex) {
        this.#candidateColorIndex[idx] = -1;
        return false;
      }
      this.#candidateColorIndex[idx] = colorIndex;
      return true;
    }

    clearCandidateColor(digit) {
      this.#candidateColorIndex[digit - 1] = -1;
    }

    clearAllColors() {
      this.#cellColorIndex = -1;
      for (let i = 0; i < 9; i++) {
        this.#candidateColorIndex[i] = -1;
      }
    }
  }

  /* =========================================================
   * OOP: SudokuBoard
   * ========================================================= */
  class SudokuBoard {
    #cells;
    #filledCount;

    constructor() {
      this.#cells = Array.from({ length: 81 }, () => new SudokuCell());
      this.#filledCount = 0;
    }

    /* ---- meta ---- */
    getCellCount() {
      return 81;
    }

    getFilledCount() {
      return this.#filledCount;
    }

    isComplete() {
      return this.#filledCount === 81;
    }

    /* ---- low-level access ---- */
    #cellAt(idx) {
      return this.#cells[idx];
    }

    /* ---- query API ---- */
    getValue(idx) {
      return this.#cellAt(idx).getValue();
    }

    isSolved(idx) {
      return this.#cellAt(idx).isSolved();
    }

    isGiven(idx) {
      return this.#cellAt(idx).isGiven();
    }

    getCandidateMask(idx) {
      return this.#cellAt(idx).getCandidateMask();
    }

    hasCandidate(idx, digit) {
      return this.#cellAt(idx).hasCandidate(digit);
    }

    countCandidates(idx) {
      return this.#cellAt(idx).countCandidates();
    }

    getCellColorIndex(idx) {
      return this.#cellAt(idx).getCellColorIndex();
    }

    getCandidateColorIndex(idx, digit) {
      return this.#cellAt(idx).getCandidateColorIndex(digit);
    }

    /* ---- mutation API ---- */
    resetAll() {
      for (let i = 0; i < 81; i++) {
        const cell = this.#cellAt(i);
        cell.setGiven(false);
        cell.setValue(0);
        cell.setCandidateMask(0);
        cell.clearAllColors();
      }
      this.#filledCount = 0;
    }

    importFromString(text) {
      // parse: digits 1..9 are values; 0 or '.' are empty; ignore others
      const tokens = [];
      for (const ch of text) {
        if (ch === "." || ch === "0") {
          tokens.push(0);
        } else if (ch >= "1" && ch <= "9") {
          tokens.push(parseInt(ch, 10));
        } else {
          continue;
        }

        if (tokens.length === 81) {
          break;
        }
      }

      /* Sudoku incompleto se non ho 81 simboli riconosciuti (0-9 o '.') */
      if (tokens.length < 81) {
        return { ok: false, error: `Neplena Sudoku: trovitaj ${tokens.length} validaj simboloj, bezonataj 81.` };
      }

      /* overwrite everything */
      this.#filledCount = 0;

      for (let i = 0; i < 81; i++) {
        const d = tokens[i];
        const cell = this.#cellAt(i);

        cell.clearAllColors();

        if (d === 0) {
          cell.setGiven(false);
          cell.setValue(0);
          cell.setCandidateMask(0);
          continue;
        }

        cell.setGiven(true);
        cell.setValue(d);
        cell.setCandidateMask(digitToBit(d));
        this.#filledCount++;
      }

      return { ok: true };
    }

    exportToString() {
      let text = "";
      for (let i = 0; i < 81; i++) {
        const cell = this.#cellAt(i);
        text += cell.isSolved() ? cell.getValue() : ".";
      }
      return text;
    }

    setManualValue(idx, digit) {
      const cell = this.#cellAt(idx);
      if (cell.isGiven()) {
        return { ok: false, reason: "given" };
      }

      const prev = cell.getValue();

      /* toggle-to-clear if same value */
      if (digit !== 0 && prev === digit) {
        cell.clearValue();
        if (prev !== 0) {
          this.#filledCount--;
        }
        return { ok: true, changed: true, action: "clear" };
      }

      if (digit === 0) {
        if (prev !== 0) {
          cell.clearValue();
          this.#filledCount--;
          return { ok: true, changed: true, action: "clear" };
        }
        return { ok: true, changed: false, action: "noop" };
      }

      cell.setValue(digit);
      cell.setCandidateMask(digitToBit(digit));

      if (prev === 0) {
        this.#filledCount++;
      }

      return { ok: true, changed: true, action: "set" };
    }

    toggleManualCandidate(idx, digit) {
      const cell = this.#cellAt(idx);
      if (cell.isGiven() || cell.isSolved()) {
        return { ok: false, reason: "notEditable" };
      }

      const nowOn = cell.toggleCandidate(digit);
      return { ok: true, changed: true, nowOn };
    }

    removeCandidate(idx, digit) {
      const cell = this.#cellAt(idx);
      if (cell.isGiven() || cell.isSolved()) {
        return { ok: false, changed: false };
      }
      if (!cell.hasCandidate(digit)) {
        return { ok: true, changed: false };
      }
      cell.disableCandidate(digit);
      return { ok: true, changed: true };
    }

    /* ---- candidates management ---- */
    recalcAllCandidatesFromValues() {
      // Recompute candidates from values only (basic elimination)
      for (let i = 0; i < 81; i++) {
        const cell = this.#cellAt(i);

        if (cell.isSolved()) {
          cell.setCandidateMask(digitToBit(cell.getValue()));
          continue;
        }

        let mask = ALL_CANDIDATES_MASK;
        for (const p of PEERS[i]) {
          const pv = this.getValue(p);
          if (pv) {
            mask &= ~digitToBit(pv);
          }
        }
        cell.setCandidateMask(mask); // may become 0 if contradiction; that is OK
      }
    }

    // Auto-clear "soft" update.
    // Removes the placed digit from candidates in peers ONLY.
    // Does NOT re-add any candidate bits that the user manually removed.
    autoClearPeersAfterPlacement(idx, digit) {
      const bit = digitToBit(digit);
      // Remove digit from all peers' candidate masks (only if the peer is not filled).
      for (const p of PEERS[idx]) {
        const cell = this.#cellAt(p);
        if (cell.isSolved()) {
          continue;
        }
        if (cell.getCandidateMask() & bit) {
          /* do not touch background here */
          cell.disableCandidate(digit);
        }
      }
    }

    /* ---- coloring ---- */
    toggleCellColor(idx, colorIndex) {
      const cell = this.#cellAt(idx);
      const enabled = cell.toggleCellColorIndex(colorIndex);
      return { ok: true, enabled };
    }

    toggleCandidateColor(idx, digit, colorIndex) {
      const cell = this.#cellAt(idx);
      const enabled = cell.toggleCandidateColorIndex(digit, colorIndex);
      return { ok: true, enabled };
    }

    clearCandidateColor(idx, digit) {
      this.#cellAt(idx).clearCandidateColor(digit);
    }

    /* ---- check ---- */
    checkSolvedGrid() {
      // 1) Must be complete
      for (let i = 0; i < 81; i++) {
        if (!this.getValue(i)) {
          return { ok: false, msg: "Ne finita: estas malplenaj ĉeloj. Ne eblas taksi la kompletan solvon." };
        }
      }

      // Helper: check duplicates in a unit
      const seen = new Array(10);

      const checkUnit = (indices, label) => {
        seen.fill(false);
        for (const idx of indices) {
          const v = this.getValue(idx);
          if (v < 1 || v > 9) {
            return { ok: false, msg: `Eraro: nevalida valoro en ${label}.` };
          }
          if (seen[v]) {
            const { r, c } = idxToRC(idx);
            return { ok: false, msg: `Eraro: duobligo de la numero ${v} en ${label} (ekz. r${r}c${c}).` };
          }
          seen[v] = true;
        }
        return { ok: true, msg: "" };
      };

      // 2) Check rows/cols/boxes
      for (let r = 0; r < 9; r++) {
        const res = checkUnit(UNITS.rows[r], `vico ${r + 1}`);
        if (!res.ok) { return res; }
      }

      for (let c = 0; c < 9; c++) {
        const res = checkUnit(UNITS.cols[c], `kolumno ${c + 1}`);
        if (!res.ok) { return res; }
      }

      for (let b = 0; b < 9; b++) {
        const res = checkUnit(UNITS.boxs[b], `bloko ${b + 1}`);
        if (!res.ok) { return res; }
      }

      return { ok: true, msg: "Ĝusta solvo: neniu duobligo trovita kaj krado kompleta." };
    }
  }

  /* =========================================================
   * DOM bindings
   * ========================================================= */
  const $ = (id) => document.getElementById(id);

  const gridEl = $("sudokuGrid");
  const logEl = $("log");
  const importEl = $("importText");

  const optPrefillEl = $("optPrefill");
  const optAutoClearEl = $("optAutoClear");
  const optHighlightEl = $("optHighlight");

  const digitPadEl = $("digitPad");
  const colorPadEl = $("colorPad");

  const timerTextEl = $("timerText");
  const btnPauseEl = $("btnPause");

  const modalOverlayCheck = $("modalOverlayCheck");
  const modalMsgCheck = $("modalMsgCheck");
  const btnModalOkCheck = $("btnModalOkCheck");

  const modalOverlayPause = $("modalOverlayPause");
  const btnModalOkPause = $("btnModalOkPause");

  /* =========================================================
   * App state (no direct cell access)
   * ========================================================= */
  const board = new SudokuBoard();

  let selectedIdx = -1;
  let mode = "value";  // "value" | "cand" | "color"

  let activeDigit = 0; // 0 means none (except keyboard)
  let activeColorIndex = 0;

  /* Highlight digit selected by clicking solved cells (when optHighlight enabled) */
  let highlightDigit = 0;

  /* solver state */
  let roundNumber = 0;
  let solveTimer = null;
  let flashIdx = -1;
  let flashTimeout = null;

  /* timer state */
  let timerStart = 0;
  let timerSeconds = 0;
  let timerInterval = null;
  let resumeTimerAfterPause = false;

  /* =========================================================
   * Logging / Modals
   * ========================================================= */
  function appendLog(line) {
    const ts = new Date().toISOString().slice(11, 19);
    logEl.value += `[${ts}] ${line}\n`;
    logEl.scrollTop = logEl.scrollHeight;
  }

  function openCheckModal(msg) {
    modalMsgCheck.textContent = msg;
    modalOverlayCheck.classList.add("open");
    btnModalOkCheck.focus();
  }

  function closeCheckModal() {
    modalOverlayCheck.classList.remove("open");
  }

  function openPauseModal() {
    modalOverlayPause.classList.add("open");
    btnModalOkPause.focus();
  }

  function closePauseModal() {
    modalOverlayPause.classList.remove("open");
  }

  function anyModalOpen() {
    return modalOverlayCheck.classList.contains("open") || modalOverlayPause.classList.contains("open");
  }

  btnModalOkCheck.addEventListener("click", () => closeCheckModal());
  btnModalOkPause.addEventListener("click", () => {
    closePauseModal();
    if (resumeTimerAfterPause) {
      startTimer();
    }
    resumeTimerAfterPause = false;
  });

  document.addEventListener("keydown", (e) => {
    if (!anyModalOpen()) {
      return;
    }

    /* In pausa: non permetto escape fuori dal flusso, solo Ok (Enter) */
    if (modalOverlayPause.classList.contains("open")) {
      if (e.key === "Enter") {
        e.preventDefault();
        btnModalOkPause.click();
      }
      return;
    }

    /* In check modal: Enter/Escape chiude */
    if (modalOverlayCheck.classList.contains("open")) {
      if (e.key === "Escape" || e.key === "Enter") {
        e.preventDefault();
        closeCheckModal();
      }
    }
  });

  /* =========================================================
   * Timer logic
   * ========================================================= */
  function formatMMSS(totalSeconds) {
    const mm = Math.floor(totalSeconds / 60);
    const ss = totalSeconds % 60;
    return `${String(mm).padStart(2, "0")}:${String(ss).padStart(2, "0")}`;
  }

  function renderTimer() {
    timerTextEl.textContent = formatMMSS(timerSeconds);
  }

  function timerIsRunning() {
    return !!timerInterval;
  }

  function updatePauseButtonState() {
    btnPauseEl.classList.toggle("disabled", !timerIsRunning());
  }

  function startTimer() {
    if (timerInterval) {
      updatePauseButtonState();
      return;
    }
    timerStart = Date.now();
    timerInterval = setInterval(() => {
      var delta = Date.now() - timerStart;
      timerSeconds = Math.floor(delta / 1000);
      renderTimer();
    }, 1000);
    updatePauseButtonState();
  }

  function stopTimer() {
    if (!timerInterval) {
      updatePauseButtonState();
      return;
    }
    clearInterval(timerInterval);
    timerInterval = null;
    updatePauseButtonState();
  }

  function resetTimer(alsoStop = true) {
    if (alsoStop) {
      stopTimer();
    }
    timerStart = 0;
    timerSeconds = 0;
    renderTimer();
    updatePauseButtonState();
  }

  btnPauseEl.addEventListener("click", () => {
    if (!timerIsRunning()) {
      return; /* disabled */
    }
    /* Pause always stops timer and resumes when modal closes */
    resumeTimerAfterPause = true;
    stopTimer();
    openPauseModal();
    appendLog("Tempomezurilo: paŭzo.");
  });

  /* =========================================================
   * Mode handling (UI-level)
   * ========================================================= */
  function setMode(newMode) {
    mode = newMode;

    $("modeValue").classList.toggle("toggled", newMode === "value");
    $("modeCand").classList.toggle("toggled", newMode === "cand");
    $("modeColor").classList.toggle("toggled", newMode === "color");

    $("modeValue").classList.toggle("secondary", newMode !== "value");
    $("modeCand").classList.toggle("secondary", newMode !== "cand");
    $("modeColor").classList.toggle("secondary", newMode !== "color");

    digitPadEl.classList.toggle("hidden", newMode === "color");
    colorPadEl.classList.toggle("hidden", newMode !== "color");

    /* candidates clickable only in Colorazione */
    gridEl.classList.toggle("candClickable", newMode === "color");

    /* If entering color mode, do not change highlight digit; just don't apply highlight on clicks */
  }

  function isModeToggleKey(e) {
    /* '.' deve switchare SOLO fra Valore e Candidato. In Evidenzia/Colorazione: no-op */
    if (!(mode === "value" || mode === "cand")) {
      return false;
    }
    /* Requirement: use "." on numeric keypad to toggle modes. */
    if (e.code === "NumpadDecimal") {
      return true;
    }
    /* Some layouts report the numpad decimal as "." in key */
    if (e.location === KeyboardEvent.DOM_KEY_LOCATION_NUMPAD && e.key === ".") {
      return true;
    }
    return false;
  }

  /* =========================================================
   * Highlight behavior (still checkbox-driven)
   * ========================================================= */
  function canApplyHighlight() {
    /* only apply when in Value/Cand */
    return optHighlightEl.checked && (mode === "value" || mode === "cand");
  }

  function cellMatchesHighlight(idx) {
    if (!canApplyHighlight()) {
      return false;
    }
    if (!highlightDigit) {
      return false;
    }

    if (board.getValue(idx) === highlightDigit) {
      return true;
    }

    if (!board.isSolved(idx) && board.hasCandidate(idx, highlightDigit)) {
      return true;
    }

    return false;
  }

  function toggleHighlightFromSolvedCell(digit) {
    if (!canApplyHighlight()) {
      return;
    }

    if (highlightDigit === digit) {
      highlightDigit = 0;
      appendLog(`Emfazo: OFF (digit ${digit}).`);
    } else {
      highlightDigit = digit;
      appendLog(`Emfazo: ON (digit ${digit}).`);
    }
  }

  optHighlightEl.addEventListener("change", () => {
    if (!optHighlightEl.checked) {
      highlightDigit = 0;
      appendLog("Emfazo: malŝaltita.");
    } else {
      appendLog("Emfazo: ŝaltita.");
    }
    renderAll();
  });

  /* =========================================================
   * Rendering (no direct cell internals)
   * ========================================================= */
  function applyCellBaseBackground(el, idx) {
    const colorIndex = board.getCellColorIndex(idx);
    if (colorIndex >= 0) {
      el.style.background = PALETTE[colorIndex];
    } else {
      el.style.background = "";
    }
  }

  function addHighlightLayersIfNeeded(el, idx) {
    if (!cellMatchesHighlight(idx)) {
      return;
    }

    const hl = document.createElement("div");
    hl.className = "hlLayer";
    el.appendChild(hl);

    const br = document.createElement("div");
    br.className = "hlBorder";
    el.appendChild(br);
  }

  function renderCell(idx) {
    const el = gridEl.children[idx];
    const v = board.getValue(idx);

    el.classList.toggle("selected", idx === selectedIdx);
    el.classList.toggle("given", board.isGiven(idx));

    // Keep flash class if present (do not wipe it by replacing element)
    const hadFlash = el.classList.contains("flashSet") || el.classList.contains("flashRemove");

    applyCellBaseBackground(el, idx);
    // Clear existing content
    el.innerHTML = "";

    if (v) {
      // Needed to highlight correctly solved cells
      addHighlightLayersIfNeeded(el, idx);

      const dv = document.createElement("div");
      dv.className = "value";
      dv.textContent = String(v);

      /* Text color must remain readable on custom backgrounds */
      const cellColorIndex = board.getCellColorIndex(idx);
      if (cellColorIndex >= 0) {
        dv.style.color = bestTextColorForBg(PALETTE[cellColorIndex]);
      } else {
        /* if highlighted and given, still fine as default */
        dv.style.color = "";
      }

      el.appendChild(dv);

      if (hadFlash) {
        el.classList.add("flash");
      }
      return;
    }

    // candidates
    const cands = document.createElement("div");
    cands.className = "cands";

    for (let d = 1; d <= 9; d++) {
      const on = board.hasCandidate(idx, d);

      const span = document.createElement("div");
      span.className = "cand " + (on ? "on" : "off");
      span.textContent = String(d);

      /* Base color logic: candidate-specific, else inherit readability from cell bg if any */
      const candColorIndex = board.getCandidateColorIndex(idx, d);
      const cellColorIndex = board.getCellColorIndex(idx);

      if (candColorIndex >= 0) {
        /* candidato colorato singolarmente */
        const bg = PALETTE[candColorIndex];
        span.style.background = bg;
        span.style.borderColor = "rgba(230, 238, 252, 0.18)";
        span.style.color = bestTextColorForBg(bg);
        span.style.opacity = "1";
      } else {
        /* candidato non colorato: se la cella ha un colore, adatta il font per leggibilità */
        span.style.background = "";
        span.style.borderColor = "";
        span.style.opacity = "";

        if (cellColorIndex >= 0) {
          span.style.color = bestTextColorForBg(PALETTE[cellColorIndex]);
        } else {
          span.style.color = "";
        }
      }

      /* Highlight candidates (render-only): only when highlight enabled + digit selected */
      if (canApplyHighlight() && highlightDigit !== 0 && d === highlightDigit && on) {
        span.style.background = "var(--hlCandBg)";
        span.style.borderColor = "var(--hlBorder)";
        span.style.color = "#0b0f14";
        span.style.opacity = "1";
        span.style.fontWeight = "800";
      }

      /* Candidate click behavior:
         - Kolorigado: toggleCandidateColor
         - Otherwise: no-op (no mouse candidate edits, no highlight from candidates) */
      span.addEventListener("click", (e) => {
        e.stopPropagation();
        selectCell(idx);

        if (mode === "color") {
          /* Kolorigado: if candidate absent, treat as transparent => toggle cell color */
          if (!board.hasCandidate(idx, d)) {
            const res = board.toggleCellColor(idx, activeColorIndex);
            appendLog(`Kolorigado: ĉelo r${rowOf(idx) + 1}c${colOf(idx) + 1} -> ${res.enabled ? (activeColorIndex + 1) : "OFF"}`);
            renderCell(idx);
            return;
          }

          const res = board.toggleCandidateColor(idx, d, activeColorIndex);
          appendLog(`Kolorigado: kandidato ${d} r${rowOf(idx) + 1}c${colOf(idx) + 1} -> ${res.enabled ? (activeColorIndex + 1) : "OFF"}`);
          renderCell(idx);
          return;
        }
      });

      cands.appendChild(span);
    }

    el.appendChild(cands);

    if (hadFlash) {
      el.classList.add("flash");
    }
  }

  function renderAll() {
    for (let i = 0; i < 81; i++) {
      renderCell(i);
    }
  }

  function buildGridUI() {
    gridEl.innerHTML = "";
    for (let i = 0; i < 81; i++) {
      const cell = document.createElement("div");
      cell.className = "cell";
      cell.dataset.idx = String(i);

      const r = rowOf(i);
      const c = colOf(i);

      /* Thick separators on 3x3 boundaries */
      if (c % 3 === 0) { cell.classList.add("thickL"); }
      if (r % 3 === 0) { cell.classList.add("thickT"); }
      if (c === 8) { cell.classList.add("thickR"); }
      if (r === 8) { cell.classList.add("thickB"); }

      cell.addEventListener("click", () => handleCellClick(i));
      gridEl.appendChild(cell);
    }
    renderAll();
  }

  function selectCell(idx) {
    // unselect old cell (if present)
    if (selectedIdx >= 0) {
      var el = gridEl.children[selectedIdx];
      var v = board.getValue(selectedIdx);

      el.classList.toggle("selected", idx === selectedIdx);
      el.classList.toggle("given", board.isGiven(selectedIdx));

      applyCellBaseBackground(el, selectedIdx);
    }

    selectedIdx = idx;

    // select new cell
    var el = gridEl.children[idx];
    var v = board.getValue(idx);

    el.classList.toggle("selected", idx === selectedIdx);
    el.classList.toggle("given", board.isGiven(idx));

    applyCellBaseBackground(el, idx);

    // do not rerender the whole grid just for this!
  }

  function handleCellClick(idx) {
    selectCell(idx);

    if (mode === "color") {
      const res = board.toggleCellColor(idx, activeColorIndex);
      appendLog(`Kolorigado: ĉelo r${rowOf(idx) + 1}c${colOf(idx) + 1} -> ${res.enabled ? (activeColorIndex + 1) : "OFF"}`);
      renderCell(idx);
      return;
    }

    /* With highlight checkbox enabled: only solved cell clicks set highlight digit */
    if (canApplyHighlight()) {
      const v = board.getValue(idx);
      if (v >= 1 && v <= 9) {
        toggleHighlightFromSolvedCell(v);
        renderAll();
        return;
      }
    }
  }

  function moveSelection(dr, dc) {
    if (selectedIdx < 0) {
      return;
    }
    const r = rowOf(selectedIdx);
    const c = colOf(selectedIdx);
    const nr = Math.max(0, Math.min(8, r + dr));
    const nc = Math.max(0, Math.min(8, c + dc));
    const ni = nr * 9 + nc;
    if (ni === selectedIdx) {
      return;
    }
    selectCell(ni);
  }

  /* =========================================================
   * Candidate / value operations via board API
   * ========================================================= */
  function setManualValueAtSelection(digit) {
    if (selectedIdx < 0) {
      return;
    }

    const res = board.setManualValue(selectedIdx, digit);
    if (!res.ok && res.reason === "given") {
      return;
    }

    const { r, c } = idxToRC(selectedIdx);
    if (digit === 0 || res.action === "clear") {
      appendLog(`Mane: malplenigita r${r}c${c}`);
    } else {
      appendLog(`Mane: agordita r${r}c${c} = ${digit} (ne validigita)`);
    }

    renderCell(selectedIdx);

    if (optAutoClearEl.checked && digit !== 0 && res.action === "set") {
      board.autoClearPeersAfterPlacement(selectedIdx, digit);
      /* render peers quickly */
      for (const p of PEERS[selectedIdx]) {
        renderCell(p);
      }
    }

    triggerAutoCheckIfComplete();
  }

  function toggleManualCandidateAtSelection(digit) {
    if (selectedIdx < 0) {
      return;
    }
    const res = board.toggleManualCandidate(selectedIdx, digit);
    if (!res.ok) {
      return;
    }

    const { r, c } = idxToRC(selectedIdx);
    appendLog(`Mane: ŝalti kandidaton ${digit} su r${r}c${c} -> ${res.nowOn ? "ON" : "OFF"}`);

    renderCell(selectedIdx);
  }

  function recalcCandidates() {
    board.recalcAllCandidatesFromValues();
    renderAll();
  }

  /* =========================================================
   * Completion / auto-check
   * ========================================================= */
  function triggerAutoCheckIfComplete() {
    if (!board.isComplete()) {
      return;
    }

    /* When complete: auto-check exactly now */
    const res = board.checkSolvedGrid();
    openCheckModal(res.msg);

    if (res.ok) {
      stopTimer(); /* stop but do not reset */
      appendLog("Tempomezurilo: HALT (ĝusta solvo).");
    } else {
      /* keep running */
      appendLog("Kontrolo: malsukcesis (tempomezurilo daŭras).");
    }
  }

  /* =========================================================
   * Solver: event queue architecture (techniques enqueue events)
   * ========================================================= */
  function flashCell(idx, type) {
    if (idx < 0) {
      return;
    }

    // clear previous flash
    if (flashTimeout) {
      clearTimeout(flashTimeout);
    }

    if (flashIdx >= 0) {
      const prevEl = gridEl.children[flashIdx];
      if (prevEl) {
        prevEl.classList.remove("flashSet");
        prevEl.classList.remove("flashRemove");
      }
    }

    flashIdx = idx;
    const el = gridEl.children[idx];
    if (el) {
      if (type === "setValue") {
        el.classList.add("flashSet");
      }
      if (type === "removeCandidate") {
        el.classList.add("flashRemove");
      }
    }

    flashTimeout = setTimeout(() => {
      const el2 = gridEl.children[idx];
      if (el2) {
        el2.classList.remove("flashSet");
        el2.classList.remove("flashRemove");
      }
      flashIdx = -1;
      flashTimeout = null;
    }, 240);
  }

  function stopSolving() {
    if (solveTimer) {
      clearInterval(solveTimer);
      solveTimer = null;
      appendLog("Solvilo: halti.");
    }
    if (flashTimeout) {
      clearTimeout(flashTimeout);
      flashTimeout = null;
    }
    if (flashIdx >= 0) {
      const el = gridEl.children[flashIdx];
      if (el) {
        el.classList.remove("flash");
      }
      flashIdx = -1;
    }
  }

  function applyEvent(ev) {
    if (ev.type === "setValue") {
      const idx = ev.idx;
      const digit = ev.digit;

      if (!assertDigit(digit)) {
        return false;
      }

      const wasSolved = board.isSolved(idx);
      const res = board.setManualValue(idx, digit); /* solver uses same setter but not user log */
      if (!res.ok) {
        return false;
      }
      if (wasSolved && board.getValue(idx) === digit) {
        return false;
      }

      const { r, c } = idxToRC(idx);
      appendLog(`Round ${roundNumber} - ${ev.reason || "Solver"}: r${r}c${c} = ${digit}`);

      flashCell(idx, ev.type);
      renderCell(idx);

      /* update candidates */
      board.autoClearPeersAfterPlacement(idx, digit);
      for (const p of PEERS[idx]) {
        renderCell(p);
      }

      triggerAutoCheckIfComplete();
      return true;
    }

    if (ev.type === "removeCandidate") {
      const idx = ev.idx;
      const digit = ev.digit;

      if (!assertDigit(digit)) {
        return false;
      }

      const res = board.removeCandidate(idx, digit);
      if (!res.ok || !res.changed) {
        return false;
      }

      const { r, c } = idxToRC(idx);
      appendLog(`Round ${roundNumber} - ${ev.reason || "Solver"}: remove cand ${digit} from r${r}c${c}`);

      renderCell(idx);
      return true;
    }

    return false;
  }

  function ensureWasmReadyOrNotify() {
    if (wasmModule && wasmSolveFull && wasmSolveNextStep && wasmSolveInit && wasmSolveHint) {
      return true;
    }

    openCheckModal(
      "La solvilo WASM ne estas disponebla.\n\n" +
      "Kontrolu ke vi lanĉas la paĝon per HTTP (ne per file://) kaj ke solver_wasm.js/.wasm estas ĉeestaj."
    );
    appendLog("Solvilo: WASM ne disponebla.");
    return false;
  }

  function solverTick() {
    if (!ensureWasmReadyOrNotify()) {
      return false;
    }

    const ev = wasmComputeNextStep();
    if (!ev) {
      return false;
    }

    if (!ev.fromPrev) {
      roundNumber++;
    }

    const did = applyEvent(ev);
    if (did) {
      renderAll();
      flashCell(ev.idx, ev.type);
    }
    return did;
  }

  function startSolving() {
    stopSolving();

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    appendLog("Solvilo: starto (WASM, interna event-vico).");

    board.recalcAllCandidatesFromValues();
    renderAll();
    wasmInitBoard(board);

    roundNumber = 0;

    solveTimer = setInterval(() => {
      const keepGoing = solverTick();
      if (!keepGoing) {
        appendLog("Solvilo: halti (neniu plia evento).");
        stopSolving();
      }
    }, 250);
  }

  function solveWasmFull() {
    stopSolving();

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    const in81 = board.exportToString();
    const out81 = wasmRunFullSolve(in81);

    if (!out81 || out81.length < 81) {
      openCheckModal("WASM plen-solve malsukcesis (neniu rezulto).");
      appendLog("WASM plen-solve: malsukceso.");
      return;
    }

    importSudoku(out81);
    appendLog("WASM plen-solve: finita.");
  }

  function solveOneStep() {
    stopSolving();

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    // don't recalculate candidates, otherwise you could end up in an infinite loop

    const ev = wasmComputeHint(board);
    if (!ev) {
      appendLog("Paŝo: neniu evento.");
      return;
    }

    if (!ev.fromPrev) {
      roundNumber++;
    }

    applyEvent(ev);
    renderAll();
    flashCell(ev.idx, ev.type);
  }

  /* =========================================================
   * Pads (digits / colors)
   * ========================================================= */
  function buildDigitPad3x3() {
    digitPadEl.innerHTML = "";
    const order = [1, 2, 3, 4, 5, 6, 7, 8, 9];

    for (const d of order) {
      const b = document.createElement("button");
      b.className = "digitBtnBig";
      b.textContent = String(d);
      b.addEventListener("click", () => {
        activeDigit = d;

        if (mode === "value") {
          setManualValueAtSelection(d);
        } else if (mode === "cand") {
          toggleManualCandidateAtSelection(d);
        }
      });

      digitPadEl.appendChild(b);
    }
  }

  function buildColorPad3x3() {
    colorPadEl.innerHTML = "";
    for (let i = 0; i < PALETTE.length; i++) {
      const b = document.createElement("button");
      b.className = "colorBtn";
      b.style.setProperty("--swatch", PALETTE[i]);
      b.title = `Colore ${i + 1}`;
      b.addEventListener("click", () => {
        activeColorIndex = i;
        refreshColorSelectionUI();
      });
      colorPadEl.appendChild(b);
    }
    refreshColorSelectionUI();
  }

  function refreshColorSelectionUI() {
    const btns = colorPadEl.querySelectorAll("button");
    for (let i = 0; i < btns.length; i++) {
      btns[i].classList.toggle("selected", i === activeColorIndex);
    }
  }

  /* =========================================================
   * Import / Reset (via board API)
   * ========================================================= */
  function resetGrid() {
    stopSolving();

    board.resetAll();

    selectedIdx = -1;
    activeDigit = 0;
    activeColorIndex = 0;
    highlightDigit = 0;

    setMode("value");
    refreshColorSelectionUI();

    appendLog("Reagordo: krado purigita.");

    /* Timer: reset and STOP */
    resetTimer(true);

    renderAll();
  }

  function importSudoku(text) {
    stopSolving();

    const res = board.importFromString(text);
    if (!res.ok) {
      openCheckModal(`Enporta eraro: ${res.error}`);
      appendLog(`Enporto: malsukcesis (${res.error})`);
      return;
    }

    highlightDigit = 0;

    if (optPrefillEl.checked) {
      board.recalcAllCandidatesFromValues();
      appendLog("Enporto: Sudoku ŝargita. Antaŭplenigo aktiva -> kandidatoj kalkulitaj.");
    } else {
      appendLog("Enporto: Sudoku ŝargita. Kandidatoj ne kalkulitaj (premu 'Rekalkuli kandidatojn' se vi volas).");
    }

    renderAll();

    /* Timer: reset and START */
    resetTimer(true);
    startTimer();
    appendLog("Tempomezurilo: reagordo + starto (enporto).");

    /* If import is already complete (rare), auto-check immediately */
    triggerAutoCheckIfComplete();
  }

  /* =========================================================
   * Keyboard handling
   * ========================================================= */
  function handleKey(e) {
    if (anyModalOpen()) {
      return;
    }

    // Accept number keys and backspace/delete
    const t = e.target;
    const isTextInput = t && (
      t.tagName === "TEXTAREA" ||
      (t.tagName === "INPUT" && (t.type === "text" || t.type === "search" || t.type === "password" || t.type === "email" || t.type === "number")) ||
      t.isContentEditable
    );
    if (isTextInput) {
      return;
    }

    /* Arrow-key navigation */
    if (isModeToggleKey(e)) {
      setMode(mode === "value" ? "cand" : "value");
      appendLog(`UI: ŝanĝi reĝimon -> ${mode === "value" ? "Valoro" : "Kandidato"}`);
      e.preventDefault();
      return;
    }

    if (e.key === "ArrowUp") { moveSelection(-1, 0); e.preventDefault(); return; }
    if (e.key === "ArrowDown") { moveSelection(1, 0); e.preventDefault(); return; }
    if (e.key === "ArrowLeft") { moveSelection(0, -1); e.preventDefault(); return; }
    if (e.key === "ArrowRight") { moveSelection(0, 1); e.preventDefault(); return; }

    const d = getDigitFromKeyEvent(e);
    if (d) {
      activeDigit = d;
      if (mode === "value") {
        setManualValueAtSelection(d);
      } else if (mode === "cand") {
        toggleManualCandidateAtSelection(d);
      }
      e.preventDefault();
      return;
    }

    if (e.key === "0" || e.key === "Backspace" || e.key === "Delete") {
      if (selectedIdx >= 0 && mode === "value") {
        setManualValueAtSelection(0);
        e.preventDefault();
      }
    }
  }

  /* =========================================================
   * UI wiring
   * ========================================================= */
  $("btnImport").addEventListener("click", () => importSudoku(importEl.value));
  $("btnClear").addEventListener("click", () => resetGrid());
  $("btnRecalc").addEventListener("click", () => {
    recalcCandidates();
    appendLog("Kandidatoj: rekalkulo finita.");
  });

  $("btnCheck").addEventListener("click", () => {
    const res = board.checkSolvedGrid();
    openCheckModal(res.msg);
    /* Manual check: do not auto-stop timer here; only stop on auto-check success at completion */
  });

  $("btnSolve").addEventListener("click", () => startSolving());
  $("btnSolveWasmFull").addEventListener("click", () => solveWasmFull());
  $("btnStop").addEventListener("click", () => stopSolving());

  $("btnClearLog").addEventListener("click", () => { logEl.value = ""; });

  $("btnDemoStep").addEventListener("click", () => {
    solveOneStep();
  });

  // Wire up right mode buttons
  $("modeValue").addEventListener("click", () => setMode("value"));
  $("modeCand").addEventListener("click", () => setMode("cand"));
  $("modeColor").addEventListener("click", () => setMode("color"));

  document.addEventListener("keydown", handleKey);

  /* =========================================================
   * Init
   * ========================================================= */
  function init() {
    buildDigitPad3x3();
    buildColorPad3x3();
    buildGridUI();

    initWasmSolver();

    setMode("value");

    renderTimer();
    updatePauseButtonState();

    appendLog("Preta. Algluu Sudokuon kaj premu 'Enporti Sudokuon'.");
  }

  init();
});
