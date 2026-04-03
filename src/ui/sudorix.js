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
   * ========================================================= */
  let wasmModule = null;
  let wasmSolveFull = null;      // cwrap'd function
  let wasmSolveInit = null;      // cwrap'd function
  let wasmSolveNextStep = null;  // cwrap'd function
  let wasmSolveHint = null;      // cwrap'd function
  let wasmCountSolutions = null; // cwrap'd function
  let wasmBufValues = 0;         // malloc'ed pointers in WASM heap
  let wasmBufCands  = 0;
  let wasmBufInStr  = 0;
  let wasmBufOut    = 0;
  const WASM_OUT_WORDS = 1024;

  // must follow the same order in Event.hpp
  const WASM_REASON = [
    "Solver",
    "Full House",
    "Naked Single",
    "Naked Pair",
    "Naked Triple",
    "Naked Quad",
    "Hidden Single",
    "Hidden Pair",
    "Hidden Triple",
    "Hidden Quad",
    "Pointing Pair",
    "Pointing Triple",
    "Claiming Pair",
    "Claiming Triple",
    "X-Wing",
    "Swordfish",
    "Jellyfish",
    "Finned X-Wing",
    "Finned Swordfish",
    "Finned Jellyfish",
    "Sashimi X-Wing",
    "Sashimi Swordfish",
    "Sashimi Jellyfish",
    "Franken X-Wing",
    "Franken Swordfish",
    "Franken Jellyfish",
    "Finned Franken X-Wing",
    "Finned Franken Swordfish",
    "Finned Franken Jellyfish",
    "Mutant X-Wing",
    "Mutant Swordfish",
    "Mutant Jellyfish",
    "Finned Mutant X-Wing",
    "Finned Mutant Swordfish",
    "Finned Mutant Jellyfish",
    "Siamese Fish",
    "Kraken Fish",
    "Single Digit Pattern",
    "Skyscraper",
    "Two-String Kite",
    "Crane",
    "Empty Rectangle",
    "Unique Rectangle",
    "Unique Rectangle (Type 1)",
    "Unique Rectangle (Type 2)",
    "Unique Rectangle (Type 3)",
    "Unique Rectangle (Type 4)",
    "Unique Rectangle (Type 5)",
    "Unique Rectangle (Type 6)",
    "Hidden Rectangle",
    "Avoidable Rectangle",
    "BUG+1",
    "XY-Wing",
    "XYZ-Wing",
    "WXYZ-Wing",
    "Chute Remote Pair",
    "W-Wing",
    "Simple Coloring",
    "Simple Coloring (Color Trap)",
    "Simple Coloring (Color Wrap)",
    "3D Medusa",
    "3D Medusa (Color Trap)",
    "3D Medusa (Color Wrap)",
    "3D Medusa (Emptied Cell)",
    "Remote Pair",
    "X-Chain",
    "X-Ring",
    "XY-Chain",
    "XY-Ring",
    "Alternating Inference Chain",
    "Alternating Inference Chain (Type 1)",
    "Alternating Inference Chain (Type 2)",
    "Alternating Inference Chain (Type 3)",
    "Grouped X-Chain",
    "Grouped X-Ring",
    "Grouped Alternating Inference Chain",
    "Grouped Alternating Inference Chain (Type 1)",
    "Grouped Alternating Inference Chain (Type 2)",
    "Grouped Alternating Inference Chain (Type 3)",
    "Almost Locked Set XZ",
    "Almost Locked Set XZ Singly Linked",
    "Almost Locked Set XZ Doubly Linked",
    "Almost Locked Set XY-Wing",
    "Almost Locked Set XY-Ring",
    "Almost Locked Set Chain",
    "Almost Locked Set Ring",
    "Sue de Coq",
    "Death Blossom",
  ];

  // techniques that require drawing of links
  const CHAINS = [
    "Single Digit Pattern",
    "Skyscraper",
    "Two-String Kite",
    "Crane",
    "Empty Rectangle",
    "X-Chain",
    "X-Ring",
    "XY-Chain",
    "XY-Ring",
    "Alternating Inference Chain",
    "Alternating Inference Chain (Type 1)",
    "Alternating Inference Chain (Type 2)",
    "Alternating Inference Chain (Type 3)",
    "Grouped X-Chain",
    "Grouped X-Ring",
    "Grouped Alternating Inference Chain",
    "Grouped Alternating Inference Chain (Type 1)",
    "Grouped Alternating Inference Chain (Type 2)",
    "Grouped Alternating Inference Chain (Type 3)",
  ];

  function initWasmSolver() {
    // createSudorixSolver is defined by solver_wasm.js (Emscripten output)
    if (typeof createSudorixSolver !== "function") {
      setSolverStatus(false, "solver_wasm.js ne ŝargita. Solvilo ne disponebla.");
      return;
    }

    wasmReady = createSudorixSolver({
      locateFile: (path) => path  // keep .wasm next to .js
    }).then((Module) => {
      wasmModule = Module;
      wasmSolveFull = wasmModule.cwrap("sudorix_solver_full", "number", ["number", "number"]);
      wasmSolveInit = wasmModule.cwrap("sudorix_solver_init_board", "number", ["number"]);
      wasmSolveNextStep = wasmModule.cwrap("sudorix_solver_next_step", "number", ["number", "number"]);
      wasmSolveHint = wasmModule.cwrap("sudorix_solver_hint", "number", ["number", "number", "number", "number"]);
      wasmCountSolutions = wasmModule.cwrap("sudorix_solver_count_solutions", "number", ["number"]);

      wasmBufValues = wasmModule._malloc(81);          // uint8_t[81]
      wasmBufCands  = wasmModule._malloc(81 * 2);      // uint16_t[81]
      wasmBufInStr  = wasmModule._malloc(82);          // char[81] + '\0'
      wasmBufOut    = wasmModule._malloc(WASM_OUT_WORDS * 4); // uint32_t[WASM_OUT_WORDS]

      setSolverStatus(true, "WASM solvilo preta.");
      return Module;
    }).catch((e) => {
      setSolverStatus(false, "WASM malsukcesis: " + (e && e.message ? e.message : String(e)));
      wasmModule = null;
      wasmSolveFull = null;
      wasmSolveInit = null;
      wasmSolveNextStep = null;
      wasmSolveHint = null;
    });
  }

  function wasmCountSolutionsFromString(boardRef) {
    if (!wasmModule || !wasmCountSolutions) {
      return { ok: false, err: "Funkcio sudorix_solver_count_solutions ne disponeblas en ĉi tiu WASM build." };
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

    const n = wasmCountSolutions(wasmBufInStr);
    return { ok: true, n };
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

    // C++ batch ABI - see solver.hpp
    const ok = wasmSolveNextStep(wasmBufOut, WASM_OUT_WORDS);
    if (!ok) {
      return null;
    }

    const out = wasmModule.HEAPU32.subarray(wasmBufOut >> 2, (wasmBufOut >> 2) + WASM_OUT_WORDS);
    const typeN = out[0] >>> 0;
    const reasonId = out[1] >>> 0;
    const fromPrev = (out[2] >>> 0) !== 0;
    const opCount = out[3] >>> 0;
    const srcCount = out[4] >>> 0;

    if (typeN === 0 || opCount === 0) {
      return null;
    }

    const ev = {
      type: (typeN === 1) ? "setValue" : (typeN === 2) ? "removeCandidate" : "none",
      reason: WASM_REASON[reasonId] || "Solver",
      fromPrev: fromPrev,
      ops: [],
      sources: []
    };

    // sources (already decoded)
    for (let i = 0; i < srcCount; i++) {
      const cells = out[5 + 2 * i + 0] >>> 0;
      const mask = out[5 + 2 * i + 1] >>> 0;
      ev.sources.push({ cells: decodeSourceCellCode(cells), mask: mask & 0x1FF });
    }

    // operations
    const opsBase = 5 + 2 * srcCount;
    for (let i = 0; i < opCount; i++) {
      const idx = out[opsBase + 2 * i + 0] >>> 0;
      const mask = out[opsBase + 2 * i + 1] >>> 0;
      ev.ops.push({ idx: idx, mask: mask & 0x1FF });
    }

    return ev;
  }

  function wasmComputeHint(board) {
    if (!wasmModule || !wasmSolveHint) {
      return null;
    }

    // Prepare inputs (snapshot board)
    let inValues = new Uint8Array(81);
    let inCands  = new Uint16Array(81);
    for (let i = 0; i < 81; i++) {
      inValues[i] = board.getValue(i) & 0xFF;
      inCands[i]  = board.getCandidateMask(i) & 0xFFFF;
    }

    wasmModule.HEAPU8.set(inValues, wasmBufValues);
    wasmModule.HEAPU16.set(inCands, wasmBufCands >> 1);

    const ok = wasmSolveHint(wasmBufValues, wasmBufCands, wasmBufOut, WASM_OUT_WORDS);
    if (!ok) {
      return null;
    }

    const out = wasmModule.HEAPU32.subarray(wasmBufOut >> 2, (wasmBufOut >> 2) + WASM_OUT_WORDS);
    const typeN = out[0] >>> 0;
    const reasonId = out[1] >>> 0;
    const fromPrev = (out[2] >>> 0) !== 0;
    const opCount = out[3] >>> 0;
    const srcCount = out[4] >>> 0;

    if (typeN === 0 || opCount === 0) {
      return null;
    }

    const ev = {
      type: (typeN === 1) ? "setValue" : (typeN === 2) ? "removeCandidate" : "none",
      reason: WASM_REASON[reasonId] || "Solver",
      fromPrev: fromPrev,
      ops: [],
      sources: []
    };

    // sources (already decoded)
    for (let i = 0; i < srcCount; i++) {
      const cells = out[5 + 2 * i + 0] >>> 0;
      const mask = out[5 + 2 * i + 1] >>> 0;
      ev.sources.push({ cells: decodeSourceCellCode(cells), mask: mask & 0x1FF });
    }

    // operations
    const opsBase = 5 + 2 * srcCount;
    for (let i = 0; i < opCount; i++) {
      const idx = out[opsBase + 2 * i + 0] >>> 0;
      const mask = out[opsBase + 2 * i + 1] >>> 0;
      ev.ops.push({ idx: idx, mask: mask & 0x1FF });
    }

    return ev;
  }

  /* =========================================================
   * Utils
   * ========================================================= */
  const rowOf = (idx) => Math.floor(idx / 9);
  const colOf = (idx) => idx % 9;

  function idxToRC(idx) {
    return { r: rowOf(idx) + 1, c: colOf(idx) + 1 };
  }

  function RCToIdx(r, c) {
    return r*9 + c;
  }

  function idxToRef(idx) {
    const r = Math.floor(idx / 9) + 1;
    const c = (idx % 9) + 1;
    return `r${r}c${c}`;
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

  function maskToDigits(mask) {
    const out = [];
    const m = (mask >>> 0) & 0x1FF;
    for (let d = 1; d <= 9; d++) {
      if (m & digitToBit(d)) {
        out.push(d);
      }
    }
    return out;
  }

  function maskToSingleDigit(mask) {
    const m = (mask >>> 0) & 0x1FF;
    if (m === 0 || (m & (m - 1)) !== 0) {
      return 0;
    }
    for (let d = 1; d <= 9; d++) {
      if (m === digitToBit(d)) {
        return d;
      }
    }
    return 0;
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

  function getCandidateElement(idx, digit) {
    const cellEl = gridEl.children[idx];
    if (!cellEl) {
      return null;
    }
    const candsEl = cellEl.querySelector(".cands");
    if (!candsEl) {
      return null;
    }
    const list = candsEl.querySelectorAll(".cand");
    return list && list[digit - 1] ? list[digit - 1] : null;
  }

  // Encoding (uint32):
  //   bits[0..4]   : unitId (0..26)
  //   bits[5..13]  : 9-bit mask of cells inside the unit
  // unitId mapping:
  //   0..8   rows
  //   9..17  cols
  //   18..26 boxes
  // Special case when bits[5..13] are all zero: empty cell-set.
  // If the associated source digit-mask is also zero, the source is treated as a delimiter.
  // Output:
  //   { unitId, kind (row|col|box|empty), mask9, idxs }
  function decodeSourceCellCode(code) {
    const unitId = (code & 0x1F) >>> 0;
    const mask9 = (code >>> 5) & 0x1FF;
    const idxs = [];

    if (mask9 == 0) {
      return { unitId, kind: "empty", mask9, idxs };
    }

    if (unitId <= 8) {
      const r = unitId;
      for (let c = 0; c < 9; c++) {
        if (mask9 & (1 << c)) {
          idxs.push(r * 9 + c);
        }
      }
      return { unitId, kind: "row", mask9, idxs };
    }

    if (unitId <= 17) {
      const c = unitId - 9;
      for (let r = 0; r < 9; r++) {
        if (mask9 & (1 << r)) {
          idxs.push(r * 9 + c);
        }
      }
      return { unitId, kind: "col", mask9, idxs };
    }

    if (unitId <= 26) {
      const b = unitId - 18;
      const br = Math.floor(b / 3) * 3;
      const bc = (b % 3) * 3;
      for (let p = 0; p < 9; p++) {
        if (mask9 & (1 << p)) {
          const r = br + Math.floor(p / 3);
          const c = bc + (p % 3);
          idxs.push(r * 9 + c);
        }
      }
      return { unitId, kind: "box", mask9, idxs, box: b };
    }

    return { unitId, kind: "unknown", mask9, idxs };
  }

  function formatEurekaCellCode(source) {
    if (!source || !source.idxs || source.idxs.length === 0) {
      return "";
    }

    if (source.kind === "row") {
      const r = source.unitId + 1;
      const cols = [];
      for (let c = 0; c < 9; c++) {
        if (source.mask9 & (1 << c)) {
          cols.push(String(c + 1));
        }
      }
      return `r${r}c${cols.join("")}`;
    }

    if (source.kind === "col") {
      const c = (source.unitId - 9) + 1;
      const rows = [];
      for (let r = 0; r < 9; r++) {
        if (source.mask9 & (1 << r)) {
          rows.push(String(r + 1));
        }
      }
      return `r${rows.join("")}c${c}`;
    }

    if (source.kind === "box") {
      const b = (source.unitId - 18) + 1;
      const cells = [];
      for (let i = 0; i < 9; i++) {
        if (source.mask9 & (1 << i)) {
          cells.push(String(i + 1));
        }
      }
      return `b${b}p${cells.join("")}`;
    }

    return source.idxs.map(idx => idxToRef(idx)).join(",");
  }

  function sourceIsDelimiter(source) {
    if (!source || !source.cells) {
      return false;
    }
    return source.cells.idxs.length === 0 && ((source.mask >>> 0) & 0x1FF) === 0;
  }

  function splitSourceGroups(sources) {
    const groups = [];
    let current = [];
    for (const src of (sources || [])) {
      if (sourceIsDelimiter(src)) {
        if (current.length > 0) {
          groups.push(current);
          current = [];
        }
        continue;
      }
      current.push(src);
    }
    if (current.length > 0) {
      groups.push(current);
    }
    return groups;
  }

  function normalizeSourceCategory(category) {
    const n = Number(category) | 0;
    if (n < 1) {
      return 1;
    }
    if (n > 13) {
      return ((n - 1) % 13) + 1;
    }
    return n;
  }

  function categoryClassName(category) {
    return `flashCategory${normalizeSourceCategory(category)}`;
  }

  function removeSourceCategoryClasses(el) {
    if (!el) {
      return;
    }
    for (let i = 1; i <= 12; i++) {
      el.classList.remove(`flashCategory${i}`);
    }
  }

  function resolveSourceCategory(ev, source, sourceIndex, groupIndex, sourceIndexInGroup) {
    if (typeof getSourceCategoryByReason === "function") {
      const custom = getSourceCategoryByReason(ev, source, sourceIndex, groupIndex, sourceIndexInGroup);
      if (custom) {
        return normalizeSourceCategory(custom);
      }
    }
    return normalizeSourceCategory(groupIndex + 1);
  }

  /* =========================================================
   * Event highlight persistence (sources + candidates)
   * ========================================================= */
  const candFlashSourceCategory = Array.from({ length: 81 }, () => new Uint8Array(9));
  const candFlashSetMask = new Uint16Array(81);
  const candFlashRemoveMask = new Uint16Array(81);
  const cellFlashSourceCategory = new Uint8Array(81);

  function clearCandidateFlashMasks() {
    for (let i = 0; i < 81; i++) {
      candFlashSourceCategory[i].fill(0);
    }
    candFlashSetMask.fill(0);
    candFlashRemoveMask.fill(0);
    cellFlashSourceCategory.fill(0);
  }

  function addCandidateFlashMask(idx, kind, mask, sourceCategory) {
    const m = (mask & 0x1FF);
    if (!m) {
      return;
    }
    if (kind === "source") {
      const category = normalizeSourceCategory(sourceCategory || 1);
      for (let d = 1; d <= 9; d++) {
        if (m & digitToBit(d)) {
          candFlashSourceCategory[idx][d - 1] = category;
        }
      }
    } else if (kind === "set") {
      candFlashSetMask[idx] |= m;
    } else if (kind === "remove") {
      candFlashRemoveMask[idx] |= m;
    }
  }

  function setCellSourceFlash(idx, category) {
    cellFlashSourceCategory[idx] = normalizeSourceCategory(category || 1);
  }

  function applyCellFlashClasses(cellEl, idx) {
    removeSourceCategoryClasses(cellEl);
    const category = cellFlashSourceCategory[idx] | 0;
    if (category > 0) {
      cellEl.classList.add(categoryClassName(category));
    }
  }

  function applyCandidateFlashClasses(spanEl, idx, digit) {
    const bit = digitToBit(digit);
    removeSourceCategoryClasses(spanEl);
    const category = candFlashSourceCategory[idx][digit - 1] | 0;
    if (category > 0) {
      spanEl.classList.add(categoryClassName(category));
    }
    if (candFlashSetMask[idx] & bit) {
      spanEl.classList.add("flashSetCand");
    }
    if (candFlashRemoveMask[idx] & bit) {
      spanEl.classList.add("flashRemoveCand");
    }
  }

  function clearAllEventHighlights() {
    clearCandidateFlashMasks();
    for (let i = 0; i < 81; i++) {
      const cellEl = gridEl.children[i];
      if (!cellEl) {
        continue;
      }
      cellEl.classList.remove("flashSet");
      cellEl.classList.remove("flashRemove");
      removeSourceCategoryClasses(cellEl);
      const candsEl = cellEl.querySelector(".cands");
      if (!candsEl) {
        continue;
      }
      for (const candEl of candsEl.querySelectorAll(".cand")) {
        candEl.classList.remove("flashSource");
        candEl.classList.remove("flashSetCand");
        candEl.classList.remove("flashRemoveCand");
        removeSourceCategoryClasses(candEl);
      }
    }
  }

  function highlightSourcesAndOps(ev) {
    if (!ev) {
      return;
    }

    const groups = splitSourceGroups(ev.sources || []);
    let sourceIndex = 0;

    // Sources
    for (let groupIndex = 0; groupIndex < groups.length; groupIndex++) {
      const group = groups[groupIndex];
      for (let groupPos = 0; groupPos < group.length; groupPos++) {
        const s = group[groupPos];
        const category = resolveSourceCategory(ev, s, sourceIndex, groupIndex, groupPos);
        sourceIndex++;

        if (s.cells && s.cells.idxs && s.cells.idxs.length > 0) {
          for (const idx of s.cells.idxs) {
            setCellSourceFlash(idx, category);
            addCandidateFlashMask(idx, "source", s.mask, category);
            const digits = maskToDigits(s.mask);
            for (const d of digits) {
              const el = getCandidateElement(idx, d);
              if (el) {
                applyCandidateFlashClasses(el, idx, d);
              }
            }
          }
        }
      }
    }

    for (let idx = 0; idx < 81; idx++) {
      const cellEl = gridEl.children[idx];
      if (cellEl) {
        applyCellFlashClasses(cellEl, idx);
      }
    }

    // Operations: set => green, remove => red
    if (ev.ops) {
      for (const op of ev.ops) {
        const idx = op.idx;

        if (ev.type === "setValue") {
          addCandidateFlashMask(idx, "set", op.mask);

          const d = maskToSingleDigit(op.mask);
          if (d) {
            const el = getCandidateElement(idx, d);
            if (el) {
              applyCandidateFlashClasses(el, idx, d);
            }
          }
        } else if (ev.type === "removeCandidate") {
          addCandidateFlashMask(idx, "remove", op.mask);

          const digits = maskToDigits(op.mask);
          for (const d of digits) {
            const el = getCandidateElement(idx, d);
            if (el) {
              applyCandidateFlashClasses(el, idx, d);
            }
          }
        }

        flashCell(idx, ev.type);
      }
    }
  }

  function drawCandidateLinks(ev) {
    if (!ev) {
      return;
    }

    if (CHAINS.indexOf(ev.reason) != -1) {
      // AIC
      const groups = splitSourceGroups(ev.sources || []);

      // Draw chain from sources
      let i = 0;
      let WANT_STRONG = true;
      while (i < groups[0].length) {
        const s = groups[0][i];
        const t = groups[0][i+1];
        if (s && s.cells && s.cells.idxs && s.cells.idxs.length > 0 &&
            t && t.cells && t.cells.idxs && t.cells.idxs.length > 0) {
          addCandidateLink(s.cells.idxs[0],
                           maskToSingleDigit(s.mask),
                           t.cells.idxs[0],
                           maskToSingleDigit(t.mask),
                           {
                             dashed: !WANT_STRONG,
                             bold: WANT_STRONG,
                             color: null,
                           }
          );
          WANT_STRONG = !WANT_STRONG;
        }
        ++i;
      }
    } else if (ev.reason.indexOf("Almost Locked Set") != -1) {
      // ALS
      const groups = splitSourceGroups(ev.sources || []);

      // Draw links between RCCs from sources
      let i = 0;
      while (i < groups[1].length) {
        const s = groups[1][i];
        const t = groups[1][i+1];
        if (s && s.cells && s.cells.idxs && s.cells.idxs.length > 0 &&
            t && t.cells && t.cells.idxs && t.cells.idxs.length > 0) {
          addCandidateLink(s.cells.idxs[0],
                           maskToSingleDigit(s.mask),
                           t.cells.idxs[0],
                           maskToSingleDigit(t.mask),
                           {
                             dashed: true,
                             bold: false,
                             color: null,
                           }
          );
        }
        i += 2;
      }
    }
  }

  function getChainOverlayEl() {
    return $("chainOverlay");
  }

  function getCandidateCenter(idx, digit) {
    const el = getCandidateElement(idx, digit);
    const overlay = getChainOverlayEl();
    if (!el || !overlay) {
      return null;
    }
    const er = el.getBoundingClientRect();
    const or = overlay.getBoundingClientRect();
    return {
      x: (er.left + er.right) / 2 - or.left,
      y: (er.top + er.bottom) / 2 - or.top
    };
  }

  function renderChainLinks() {
    const overlay = getChainOverlayEl();
    if (!overlay) {
      return;
    }
    overlay.innerHTML = "";

    const gridRect = gridEl.getBoundingClientRect();
    overlay.setAttribute("viewBox", `0 0 ${gridRect.width} ${gridRect.height}`);

    for (const link of chainLinks) {
      const a = getCandidateCenter(link.from.idx, link.from.digit);
      const b = getCandidateCenter(link.to.idx, link.to.digit);
      if (!a || !b) {
        continue;
      }
      const path = document.createElementNS("http://www.w3.org/2000/svg", "line");
      path.setAttribute("x1", String(a.x));
      path.setAttribute("y1", String(a.y));
      path.setAttribute("x2", String(b.x));
      path.setAttribute("y2", String(b.y));
      path.setAttribute("stroke", link.color || "var(--accent)");
      path.setAttribute("class", `chainLink ${link.dashed ? "chainLinkDashed" : ""} ${link.bold ? "chainLinkBold" : "chainLinkNormal"}`.trim());
      overlay.appendChild(path);
    }
  }

  function clearCandidateLinks() {
    chainLinks = [];
    const overlay = getChainOverlayEl();
    if (overlay) {
      overlay.innerHTML = "";
    }
  }

  function addCandidateLink(fromIdx, fromDigit, toIdx, toDigit, options = {}) {
    chainLinks.push({
      from: { idx: fromIdx, digit: fromDigit },
      to: { idx: toIdx, digit: toDigit },
      dashed: !!options.dashed,
      bold: !!options.bold,
      color: options.color || null
    });
    renderChainLinks();
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

    exportState() {
      const state = {
        filledCount: this.#filledCount,
        cells: []
      };
      for (let i = 0; i < 81; i++) {
        const cell = this.#cellAt(i);
        const candColors = [];
        for (let d = 1; d <= 9; d++) {
          candColors.push(cell.getCandidateColorIndex(d));
        }
        state.cells.push({
          value: cell.getValue(),
          given: cell.isGiven(),
          candMask: cell.getCandidateMask(),
          cellColorIndex: cell.getCellColorIndex(),
          candColorIndex: candColors
        });
      }
      return state;
    }

    importState(state) {
      if (!state || !state.cells || state.cells.length !== 81) {
        return { ok: false, error: "Nevalida malfarstato." };
      }

      this.#filledCount = 0;
      for (let i = 0; i < 81; i++) {
        const s = state.cells[i];
        const cell = this.#cellAt(i);

        cell.setGiven(!!s.given);
        cell.setValue(s.value || 0);
        cell.setCandidateMask((s.candMask >>> 0) & 0x1FF);

        cell.clearAllColors();
        if (typeof s.cellColorIndex === "number") {
          cell.toggleCellColorIndex(s.cellColorIndex);
          // toggle sets, but if it's -1 it clears anyway; ensure exact:
          if (s.cellColorIndex === -1) {
            cell.clearCellColor();
          }
        }
        if (Array.isArray(s.candColorIndex) && s.candColorIndex.length === 9) {
          for (let d = 1; d <= 9; d++) {
            const ci = s.candColorIndex[d - 1];
            if (typeof ci === "number" && ci >= 0) {
              cell.toggleCandidateColorIndex(d, ci);
            } else {
              cell.clearCandidateColor(d);
            }
          }
        }

        if (cell.isSolved()) {
          this.#filledCount++;
        }
      }

      return { ok: true };
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

    clearAllColors() {
      for (let i = 0; i < 81; i++) {
        this.#cellAt(i).clearAllColors();
      }
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
  const optFastSolveEl = $("optFastSolve");

  const digitPadEl = $("digitPad");
  const colorPadEl = $("colorPad");

  const timerTextEl = $("timerText");
  const btnPauseEl = $("btnPause");

  const btnUndoEl = $("btnUndo");
  const btnRedoEl = $("btnRedo");

  const modalOverlayCheck = $("modalOverlayCheck");
  const modalMsgCheck = $("modalMsgCheck");
  const btnModalOkCheck = $("btnModalOkCheck");

  const modalOverlayPause = $("modalOverlayPause");
  const btnModalOkPause = $("btnModalOkPause");

  const modalOverlayExport = $("modalOverlayExport");
  const modalExportText = $("modalExportText");
  const btnModalCopyExport = $("btnModalCopyExport");
  const btnModalOkExport = $("btnModalOkExport");

  /* =========================================================
   * App state
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
  let pendingStepEvent = null;
  let undoStack = [];
  let redoStack = [];
  const HISTORY_LIMIT = 100;

  /* timer state */
  let timerStart = 0;
  let timerSeconds = 0;
  let timerSecondsBeforePause = 0;
  let timerInterval = null;
  let resumeTimerAfterPause = false;

  /* chain overlay state */
  let chainLinks = [];

  /* =========================================================
   * Rich Log (clickable) + Snapshot Preview
   * ========================================================= */
  const eventLogEntries = [];
  let previewActive = false;
  let previewSavedLiveState = null;
  let previewActiveIndex = -1;
  let oldUndoDisabled = false;
  let oldRedoDisabled = false;
  let oldUndoContainsDisabled = false;
  let oldRedoContainsDisabled = false;

  function escapeHtml(s) {
    return String(s)
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }

  function clearLog() {
    logEl.innerHTML = "";
    eventLogEntries.length = 0;
    exitLogPreview();
  }

  function setControlsDuringPreview(disabled) {
    const ids = ["btnStep", "btnSolve", "btnStop", "btnSolveWasmFull"];
    for (const id of ids) {
      const b = $(id);
      if (b) {
        b.disabled = !!disabled;
        b.classList.toggle("disabled", !!disabled);
      }
    }
    // different handling for undo/redo
    if (disabled) {
      // save previous state
      oldUndoDisabled = $("btnUndo").disabled;
      oldRedoDisabled = $("btnRedo").disabled;
      oldUndoContainsDisabled = $("btnUndo").classList.contains("disabled");
      oldRedoContainsDisabled = $("btnRedo").classList.contains("disabled");
      $("btnUndo").disabled = !!disabled;
      $("btnUndo").classList.toggle("disabled", !!disabled);
      $("btnRedo").disabled = !!disabled;
      $("btnRedo").classList.toggle("disabled", !!disabled);
    } else {
      // restore previous state
      $("btnUndo").disabled = oldUndoDisabled;
      oldUndoContainsDisabled ? $("btnUndo").classList.add("disabled")
                              : $("btnUndo").classList.remove("disabled");
      $("btnRedo").disabled = oldRedoDisabled;
      oldRedoContainsDisabled ? $("btnRedo").classList.add("disabled")
                              : $("btnRedo").classList.remove("disabled");
    }
  }

  // print event once without applying
  function logEventOnce(ev) {
    if (!ev) {
      return;
    }
    if (ev._logged) {
      return;
    }
    appendEvent(ev);
    ev._logged = true;
  }

  function appendEvent(ev) {
    const object = formatEventLog(ev);
    const title = object.title;
    const bodyHtml = object.bodyHtml;
    saveEventLogEntry({ title, bodyHtml, boardState: board.exportState(), ev });
  }

  function saveEventLogEntry({ title, bodyHtml, boardState, ev }) {
    const index = eventLogEntries.length;
    eventLogEntries.push({ title, bodyHtml, boardState, ev });
    appendLogEntryWithIndex(index, { title, bodyHtml });
  }

  function appendLogEntryWithIndex(index, { title, bodyHtml }) {
    const entry = document.createElement("div");
    entry.className = "logEntry";
    entry.dataset.logIndex = String(index);

    const h = document.createElement("div");
    h.className = "logEntryTitle";
    h.textContent = title;

    const b = document.createElement("div");
    b.className = "logEntryBody";
    b.innerHTML = bodyHtml;

    entry.appendChild(h);
    entry.appendChild(b);

    // Click-to-preview toggle
    entry.addEventListener("click", () => {
      const idx = Number(entry.dataset.logIndex);
      if (!Number.isFinite(idx)) {
        return;
      }
      if (previewActive && previewActiveIndex == idx) {
        exitLogPreview();
        return;
      }
      enterLogPreview(idx);
    });

    logEl.appendChild(entry);
    logEl.scrollTop = logEl.scrollHeight;
  }

  function appendInfo(text) {
    const ts = new Date().toISOString().slice(11, 19);
    const title = "Info";
    const bodyHtml = `<div class="logEntryBody">${escapeHtml(text)}</div>`;
    appendLogEntry({ title, bodyHtml });
  }

  function appendLogEntry({ title, bodyHtml }) {
    const entry = document.createElement("div");
    entry.className = "logEntry";

    const h = document.createElement("div");
    h.className = "logEntryTitle";
    h.textContent = title;

    const b = document.createElement("div");
    b.innerHTML = bodyHtml;

    entry.appendChild(h);
    entry.appendChild(b);
    logEl.appendChild(entry);

    logEl.scrollTop = logEl.scrollHeight;
  }

  function enterLogPreview(index) {
    const entry = eventLogEntries[index];
    if (!entry) {
      return;
    }

    // Close any existing preview
    if (previewActive) {
      exitLogPreview();
    }

    // Save current live state without touching undo/redo stacks
    previewSavedLiveState = board.exportState();
    previewActive = true;
    previewActiveIndex = index;

    setControlsDuringPreview(true);

    // Restore snapshot corresponding to this log entry
    board.importState(entry.boardState);
    renderAll();

    clearAllEventHighlights();
    clearCandidateLinks();
    if (entry.ev) {
      highlightSourcesAndOps(entry.ev);
      drawCandidateLinks(entry.ev);
    }

    // Mark active
    logEl.querySelectorAll(".logEntry.active").forEach(x => x.classList.remove("active"));
    const node = logEl.querySelector(`.logEntry[data-log-index="${index}"]`);
    if (node) {
      node.classList.add("active");
    }
  }

  function exitLogPreview() {
    if (!previewActive) {
      return;
    }

    clearAllEventHighlights();
    clearCandidateLinks();

    if (previewSavedLiveState) {
      board.importState(previewSavedLiveState);
      renderAll();
    }

    previewSavedLiveState = null;
    previewActive = false;
    previewActiveIndex = -1;

    logEl.querySelectorAll(".logEntry.active").forEach(x => x.classList.remove("active"));

    setControlsDuringPreview(false);
  }

  function formatEventLog(ev) {
    // Returns { title, bodyHtml } for appendLogEntry.
    // Different implementation for each technique, found in formatter.js
    return formatEventLogByReason(ev);
  }

  // Functions visible to formatter.js
  if (typeof window !== "undefined") {
    window.SudorixFormatterContext = {
      escapeHtml,
      idxToRef,
      maskToDigits,
      maskToSingleDigit,
      formatEurekaCellCode,
      splitSourceGroups,
      normalizeSourceCategory,
      resolveSourceCategory,
      sourceIsDelimiter,
      addCandidateLink,
      clearCandidateLinks,
      renderChainLinks,
      palette: PALETTE
    };
  }

  function setSolverStatus(ok, text) {
    const dot = $("solverStatusDot");
    const label = $("solverStatusText");
    if (!dot || !label) {
      return;
    }
    dot.classList.toggle("statusDotGreen", !!ok);
    dot.classList.toggle("statusDotRed", !ok);
    label.textContent = text || (ok ? "WASM solvilo preta." : "WASM solvilo ne preta.");
  }

  /* =========================================================
   * Modals
   * ========================================================= */
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

  function openExportModal(text) {
    modalExportText.value = text || "";
    modalOverlayExport.classList.add("open");
    setTimeout(() => {
      modalExportText.focus();
      modalExportText.select();
    }, 0);
  }

  function closeExportModal() {
    modalOverlayExport.classList.remove("open");
  }

  function anyModalOpen() {
    return modalOverlayCheck.classList.contains("open") || modalOverlayPause.classList.contains("open") || modalOverlayExport.classList.contains("open");
  }

  btnModalOkCheck.addEventListener("click", () => closeCheckModal());
  btnModalCopyExport.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(modalExportText.value || "");
    } catch (e) {
      modalExportText.focus();
      modalExportText.select();
    }
  });
  btnModalOkExport.addEventListener("click", () => closeExportModal());
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

    /* In export modal: Enter/Escape chiude */
    if (modalOverlayExport.classList.contains("open")) {
      if (e.key === "Escape" || e.key === "Enter") {
        e.preventDefault();
        closeExportModal();
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
   * Timer
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
      var delta = Date.now() - timerStart + timerSecondsBeforePause*1000;
      timerSeconds = Math.floor(delta / 1000);
      renderTimer();
    }, 1000);
    updatePauseButtonState();
  }

  function stopTimer() {
    timerSecondsBeforePause = timerSeconds;
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
    timerSecondsBeforePause = 0;
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
    const colorActionsEl = $("colorActions");
    if (colorActionsEl) {
      colorActionsEl.classList.toggle("hidden", newMode !== "color");
    }

    /* candidates clickable only in Kolorigado */
    gridEl.classList.toggle("candClickable", newMode === "color");

    /* If entering color mode, do not change highlight digit; just don't apply highlight on clicks */
  }

  function isModeToggleKey(e) {
    /* '.' deve switchare SOLO fra Valoro e Kandidato. In Kolorigado: no-op */
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
   * Highlight behavior (checkbox-driven)
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
    } else {
      highlightDigit = digit;
    }
  }

  optHighlightEl.addEventListener("change", () => {
    if (!optHighlightEl.checked) {
      highlightDigit = 0;
    }
    renderAll();
  });

  /* =========================================================
   * Rendering
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
    applyCellFlashClasses(el, idx);

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
      span.dataset.idx = String(idx);
      span.dataset.digit = String(d);

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

      applyCandidateFlashClasses(span, idx, d);

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
            renderCell(idx);
            return;
          }

          const res = board.toggleCandidateColor(idx, d, activeColorIndex);
          renderCell(idx);
          return;
        }
      });

      cands.appendChild(span);
    }

    el.appendChild(cands);
  }

  function renderAll() {
    for (let i = 0; i < 81; i++) {
      renderCell(i);
    }
    renderChainLinks();
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
    applyCellFlashClasses(el, idx);

    applyCellBaseBackground(el, idx);

    // do not rerender the whole grid just for this!
  }

  function handleCellClick(idx) {
    selectCell(idx);

    if (mode === "color") {
      const res = board.toggleCellColor(idx, activeColorIndex);
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
    saveUndoSnapshot();

    if (selectedIdx < 0) {
      return;
    }

    const res = board.setManualValue(selectedIdx, digit);
    if (!res.ok && res.reason === "given") {
      return;
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
    saveUndoSnapshot();

    if (selectedIdx < 0) {
      return;
    }
    const res = board.toggleManualCandidate(selectedIdx, digit);
    if (!res.ok) {
      return;
    }

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
    }
  }

  /* =========================================================
   * Solver: event queue architecture (techniques enqueue events)
   * ========================================================= */
  function flashCell(idx, type) {
    if (idx < 0) {
      return;
    }

    const el = gridEl.children[idx];
    if (el) {
      if (type === "setValue") {
        el.classList.add("flashSet");
      }
      if (type === "removeCandidate") {
        el.classList.add("flashRemove");
      }
    }
  }

  /* =========================================================
   * History management
   * ========================================================= */
  function updateHistoryButtons() {
    if (undoStack.length > 0) {
      btnUndoEl.classList.remove("disabled");
    } else {
      btnUndoEl.classList.add("disabled");
    }

    if (redoStack.length > 0) {
      btnRedoEl.classList.remove("disabled");
    } else {
      btnRedoEl.classList.add("disabled");
    }
  }

  function clearHistory() {
    undoStack = [];
    redoStack = [];
    pendingStepEvent = null;
    updateHistoryButtons();
  }

  function pushUndoSnapshot() {
    const snap = board.exportState();
    undoStack.push(snap);
    if (undoStack.length > HISTORY_LIMIT) {
      undoStack.shift();
    }
    redoStack = [];
    updateHistoryButtons();
  }

  function saveUndoSnapshot() {
    pushUndoSnapshot();
  }

  function doUndo() {
    if (undoStack.length == 0) {
      return;
    }

    stopSolving();
    clearAllEventHighlights();
    clearCandidateLinks();
    pendingStepEvent = null;

    const current = board.exportState();
    redoStack.push(current);
    if (redoStack.length > HISTORY_LIMIT) {
      redoStack.shift();
    }

    const prev = undoStack.pop();
    const res = board.importState(prev);
    updateHistoryButtons();

    if (!res.ok) {
      openCheckModal(`Malfara eraro: ${res.error}`);
      return;
    }

    renderAll();
  }

  function doRedo() {
    if (redoStack.length == 0) {
      return;
    }

    stopSolving();
    clearAllEventHighlights();
    clearCandidateLinks();
    pendingStepEvent = null;

    const current = board.exportState();
    undoStack.push(current);
    if (undoStack.length > HISTORY_LIMIT) {
      undoStack.shift();
    }

    const next = redoStack.pop();
    const res = board.importState(next);
    updateHistoryButtons();

    if (!res.ok) {
      openCheckModal(`Refara eraro: ${res.error}`);
      return;
    }

    renderAll();
  }

  function stopSolving() {
    if (solveTimer) {
      solveTimer = null;
    }
  }

  /* =========================================================
   * Solver
   * ========================================================= */
  function applyEvent(ev) {
    if (!ev || !ev.ops || ev.ops.length === 0) {
      return false;
    }

    if (ev.type === "setValue") {
      let any = false;

      for (const op of ev.ops) {
        const idx = op.idx;
        const digit = maskToSingleDigit(op.mask);

        if (!assertDigit(digit)) {
          continue;
        }

        const wasSolved = board.isSolved(idx);
        const res = board.setManualValue(idx, digit); /* solver uses same setter but not user log */
        if (!res.ok) {
          continue;
        }
        if (wasSolved && board.getValue(idx) === digit) {
          continue;
        }

        /* update candidates */
        board.autoClearPeersAfterPlacement(idx, digit);

        any = true;
      }

      triggerAutoCheckIfComplete();
      return any;
    }

    if (ev.type === "removeCandidate") {
      let any = false;
      let removedCount = 0;

      for (const op of ev.ops) {
        const idx = op.idx;
        const digits = maskToDigits(op.mask);

        // Iterate bits 0..8
        for (const digit of digits) {
          const res = board.removeCandidate(idx, digit);
          if (res.ok && res.changed) {
            removedCount++;
            any = true;
          }
        }
      }

      return any;
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
    setSolverStatus(false, "WASM ne disponebla.");
    return false;
  }

  function solverTick(callbackDone) {
    if (!ensureWasmReadyOrNotify()) {
      callbackDone(false);
      return;
    }

    const ev = wasmComputeNextStep();
    if (!ev) {
      callbackDone(false);
      return;
    }

    if (!ev.fromPrev) {
      roundNumber++;
    }

    if (optFastSolveEl && optFastSolveEl.checked) {
      saveUndoSnapshot();
      logEventOnce(ev);
      const did = applyEvent(ev);
      renderAll();
      clearAllEventHighlights();
      clearCandidateLinks();
      callbackDone(did);
      return;
    }

    // Phase 1: highlight sources + operations BEFORE applying.
    renderAll();
    clearAllEventHighlights();
    highlightSourcesAndOps(ev);
    clearCandidateLinks();
    drawCandidateLinks(ev);

    setTimeout(() => {
      // Phase 2: apply, then highlight again on the updated grid.
      saveUndoSnapshot();
      logEventOnce(ev);
      const did = applyEvent(ev);
      renderAll();
      clearAllEventHighlights();
      highlightSourcesAndOps(ev);
      clearCandidateLinks();
      drawCandidateLinks(ev);

      setTimeout(() => {
        clearAllEventHighlights();
        clearCandidateLinks();
        callbackDone(did);
      }, 200);
    }, 200);
  }

  function startSolving() {
    stopSolving();

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    board.recalcAllCandidatesFromValues();
    renderAll();
    wasmInitBoard(board);

    roundNumber = 0;

    const loop = () => {
      if (!solveTimer) {
        return;
      }
      solverTick((keepGoing) => {
        if (!keepGoing) {
          if (board.checkSolvedGrid().ok) {
            appendInfo("Sudokuo solvita.");
          } else {
            appendInfo("Halti (neniu plia evento).");
          }
          stopSolving();
          return;
        }
        // schedule next
        if (solveTimer) {
          setTimeout(loop, (optFastSolveEl && optFastSolveEl.checked) ? 0 : 10);
        }
      });
    };

    // solveTimer used as "running flag"
    solveTimer = 1;
    loop();
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
      appendInfo("WASM plen-solve: malsukceso.");
      return;
    }

    importSudoku(out81);
    appendInfo("WASM plen-solve: finita.");
  }

  function solveOneStep() {
    stopSolving();

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    // Two-phase behavior:
    //  - if no pending event: compute and PREVIEW (highlight only, do not apply)
    //  - if pending exists: APPLY it, then clear
    if (!pendingStepEvent) {
      // don't recalculate candidates, otherwise you could end up in an infinite loop
      const ev = wasmComputeHint(board);
      if (!ev) {
        appendInfo("Neniu plia evento.");
        return;
      }

      if (!ev.fromPrev) {
        roundNumber++;
      }

      pendingStepEvent = ev;

      renderAll();
      clearAllEventHighlights();
      highlightSourcesAndOps(ev);
      clearCandidateLinks();
      drawCandidateLinks(ev);
      logEventOnce(ev);
      return;
    }

    // apply pending
    const ev = pendingStepEvent;
    pendingStepEvent = null;

    clearAllEventHighlights();
    clearCandidateLinks();
    saveUndoSnapshot();
    const did = applyEvent(ev);
    renderAll();
    clearAllEventHighlights();
    highlightSourcesAndOps(ev);
    clearCandidateLinks();
    drawCandidateLinks(ev);

    setTimeout(() => {
      clearAllEventHighlights();
      clearCandidateLinks();
    }, 200);

    if (!did) {
      appendInfo("Evento ne aplikebla.");
    }
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

  function exportSudoku() {
    openExportModal(board.exportToString());
  }

  function clearAllManualColors() {
    saveUndoSnapshot();
    board.clearAllColors();
    renderAll();
  }

  /* =========================================================
   * Import / Reset
   * ========================================================= */
  function resetGrid() {
    stopSolving();

    board.resetAll();

    roundNumber = 0;
    selectedIdx = -1;
    activeDigit = 0;
    activeColorIndex = 0;
    highlightDigit = 0;

    setMode("value");
    refreshColorSelectionUI();
    clearAllEventHighlights();
    clearCandidateLinks();
    clearHistory();
    clearLog();

    /* Timer: reset and STOP */
    resetTimer(true);

    renderAll();
  }

  function importSudoku(text) {
    stopSolving();

    resetGrid();

    const res = board.importFromString(text);
    if (!res.ok) {
      openCheckModal(`Enporta eraro: ${res.error}`);
      return;
    }

    highlightDigit = 0;

    if (optPrefillEl.checked) {
      board.recalcAllCandidatesFromValues();
    }

    renderAll();

    /* Timer: reset and START */
    resetTimer(true);
    startTimer();

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

    const isUndo = e.ctrlKey && !e.shiftKey && (e.key === "z" || e.key === "Z");
    const isRedo = (e.ctrlKey && (e.key === "y" || e.key === "Y")) || (e.ctrlKey && e.shiftKey && (e.key === "z" || e.key === "Z"));
    if (isUndo) {
      e.preventDefault();
      doUndo();
      return;
    }
    if (isRedo) {
      e.preventDefault();
      doRedo();
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
  $("btnRecalc").addEventListener("click", () => recalcCandidates());

  $("btnExport").addEventListener("click", () => exportSudoku());
  $("btnCheck").addEventListener("click", () => {
    const res = board.checkSolvedGrid();
    openCheckModal(res.msg);
    /* Manual check: do not auto-stop timer here; only stop on auto-check success at completion */
  });

  $("btnCountSolutions").addEventListener("click", () => {
    // Count solutions on the current grid (values only)
    const r = wasmCountSolutionsFromString(board);
    if (!r.ok) {
      openCheckModal(r.err);
      return;
    }

    if (r.n === -1) {
      openCheckModal("Eraro: malvalida Sudoku-ĉeno (kodiga eraro).");
    } else if (r.n === 0) {
      openCheckModal("Rezulto: neniu solvo (0).");
    } else if (r.n === 1) {
      openCheckModal("Rezulto: unu sola solvo (unika).");
    } else {
      openCheckModal("Rezulto: pluraj solvoj (" + r.n + ").");
    }
  });

  $("btnSolve").addEventListener("click", () => startSolving());
  $("btnSolveWasmFull").addEventListener("click", () => solveWasmFull());
  $("btnStop").addEventListener("click", () => stopSolving());

  $("btnClearLog").addEventListener("click", () => clearLog());

  $("btnStep").addEventListener("click", () => solveOneStep());

  $("btnUndo").addEventListener("click", () => doUndo());
  $("btnRedo").addEventListener("click", () => doRedo());
  $("btnClearColors").addEventListener("click", () => clearAllManualColors());

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

    setSolverStatus(false, "WASM solvilo ne preta.");

    initWasmSolver();

    setMode("value");

    appendInfo("La solvilo skribos ĉi tie la paŝojn.");
    window.addEventListener("resize", () => renderChainLinks());
    renderTimer();
    updatePauseButtonState();
  }

  init();
});
