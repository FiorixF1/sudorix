var business_logic = (() => {
  /* =========================================================
   * Constants / Palette
   * ========================================================= */
  const ALL_CANDIDATES_LIST = [1, 2, 3, 4, 5, 6, 7, 8, 9];

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

  const FORCING_PALETTE = [
    "#7AA2FF", /* theme color */
    "#FF2BD6", /* magenta */
    "#7CFF00", /* neon green */
    "#FFD400", /* yellow */
  ];

  /* =========================================================
   * Technique utilities
   * ========================================================= */
  const TECHNIQUE_STORAGE_KEY = "sudorix.enabledTechniques.v1";

  function getEnabledTechniques() {
    // as array
    return enabledTechniques.values().toArray()
  }

  function getTechniqueCategory(tech) {
    return techniqueToCategory[tech];
  }

  function loadStoredTechnique() {
    try {
      const raw = window.localStorage.getItem(TECHNIQUE_STORAGE_KEY);
      if (!raw) {
        return [];
      }
      const parsed = JSON.parse(raw);
      if (!Array.isArray(parsed)) {
        return [];
      }
      return parsed;
    } catch (_) {
      return [];
    }
  }

  function saveStoredTechnique(techs) {
    try {
      window.localStorage.setItem(TECHNIQUE_STORAGE_KEY, JSON.stringify(Array.from(techs)));
    } catch (_) {
      /* localStorage unavailable: keep the in-memory setting only */
    }
  }

  /* =========================================================
   * Utils
   * ========================================================= */
  const rowOf = (idx) => Math.floor(idx / 9);
  const colOf = (idx) => idx % 9;
  const boxOf = (idx) => Math.floor(rowOf(idx)/3)*3 + Math.floor(colOf(idx)/3);

  function idxToRCB(idx) {
    return { r: rowOf(idx) + 1,
             c: colOf(idx) + 1,
             b: boxOf(idx) + 1 };
  }

  function idxToRef(idx) {
    const { r, c, b } = idxToRCB(idx);
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

  function digitToBit(digit) {
    return 1 << (digit - 1);
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

  function categoryClassName(category) {
    return `flashCategory${category}`;
  }

  function removeSourceCategoryClasses(el) {
    if (!el) {
      return;
    }
    for (let i = 1; i <= 12; i++) {
      el.classList.remove(`flashCategory${i}`);
    }
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

  function addCandidateFlashMask(idx, kind, digits, sourceCategory) {
    if (kind === "source") {
      const category = sourceCategory || 1;
      for (const d of digits) {
        candFlashSourceCategory[idx][d - 1] = category;
      }
    } else if (kind === "set") {
      for (const d of digits) {
        candFlashSetMask[idx] |= digitToBit(d);
      }
    } else if (kind === "remove") {
      for (const d of digits) {
        candFlashRemoveMask[idx] |= digitToBit(d);
      }
    }
  }

  function setCellSourceFlash(idx, category) {
    cellFlashSourceCategory[idx] = category || 1;
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

    const sources = ev.sources;

    // Sources
    for (let sourceIndex in sources) {
      const source = sources[sourceIndex];
      const name = source.name;
      const list = source.list;

      for (let groupIndex in list) {
        const group = list[groupIndex];
        const cells = group.cells;
        const digits = group.digits;

        if (cells && cells.length > 0) {
          for (let cellIndex in cells) {
            const idx = cells[cellIndex];
            const category = getSourceCategoryByTechnique(ev, name, +sourceIndex, +groupIndex, +cellIndex);
            setCellSourceFlash(idx, category);
            addCandidateFlashMask(idx, "source", digits, category);
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
    if (ev.operations) {
      for (const op of ev.operations) {
        const idx = op.cell;

        if (ev.type === "setValue") {
          addCandidateFlashMask(idx, "set", op.digits);

          const d = op.digits[0]; // TODO: valutare se mettere solo digit lato C++
          if (d) {
            const el = getCandidateElement(idx, d);
            if (el) {
              applyCandidateFlashClasses(el, idx, d);
            }
          }
        } else if (ev.type === "removeCandidate") {
          addCandidateFlashMask(idx, "remove", op.digits);

          const digits = op.digits;
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

    if (getTechniqueCategory(ev.reason) == "AIC") {
      const sources = ev.sources;

      // Draw chain from sources
      for (let group of sources) {
        let i = 0;
        let WANT_STRONG = true;
        let list = group.list;
        while (i < list.length) {
          const s = list[i];
          const t = list[i+1];
          if (s && s.cells && s.cells.length > 0 &&
              t && t.cells && t.cells.length > 0) {
            addCandidateLink(s.cells[0],
                             s.digits[0],
                             t.cells[0],
                             t.digits[0],
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
      }
    } else if (getTechniqueCategory(ev.reason) == "ALS") {
      const sources = ev.sources;

      // Draw links between RCCs from sources
      for (let group of sources) {
        if (group.name == "RCC") {
          let i = 0;
          let list = group.list;
          while (i < list.length) {
            const s = list[i];
            const t = list[i+1];
            if (s && s.cells && s.cells.length > 0 &&
                t && t.cells && t.cells.length > 0) {
              addCandidateLink(s.cells[0],
                               s.digits[0],
                               t.cells[0],
                               t.digits[0],
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
    } else if (getTechniqueCategory(ev.reason) == "FC") {
      const sources = ev.sources;

      // Draw chain from sources
      for (let idx in sources) {
        let group = sources[idx];
        let i = 0;
        let WANT_STRONG = false;  // FC generally starts from a weak link
        if (ev.detailedReason == "Digit Forcing Chain" && idx == 1) {
          // unless you are reading the second chain of a Digit Forcing Chain
          // TODO: trovare modo per rimuovere dipendenza da indice: esempio aggiungere un campo start_from = weak
          WANT_STRONG = true;
        }
        let list = group.list;
        while (i < list.length) {
          const s = list[i];
          const t = list[i+1];
          if (s && s.cells && s.cells.length > 0 &&
              t && t.cells && t.cells.length > 0) {
            addCandidateLink(s.cells[0],
                             s.digits[0],
                             t.cells[0],
                             t.digits[0],
                             {
                               dashed: !WANT_STRONG,
                               bold: WANT_STRONG,
                               color: FORCING_PALETTE[idx],
                             }
            );
            WANT_STRONG = !WANT_STRONG;
          }
          ++i;
        }
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
      const dx = b.x - a.x;
      const dy = b.y - a.y;
      const len = Math.hypot(dx, dy) || 1;
      const nx = -dy / len;
      const ny = dx / len;
      const bend = Number.isFinite(link.bend) ? link.bend : 0;
      const cx = (a.x + b.x) / 2 + nx * bend;
      const cy = (a.y + b.y) / 2 + ny * bend;

      const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
      path.setAttribute("d", `M ${a.x.toFixed(2)} ${a.y.toFixed(2)} Q ${cx.toFixed(2)} ${cy.toFixed(2)} ${b.x.toFixed(2)} ${b.y.toFixed(2)}`);
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
      color: options.color || null,
      bend: Number.isFinite(options.bend) ? options.bend : ((Math.random() < 0.5 ? -1 : 1) * (10 + Math.random() * 18))
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
    #candidates;
    #given;

    #cellColorIndex;
    #candidateColorIndex; /* length 9 array */

    constructor() {
      this.#value = 0;               // value 0..9
      this.#candidates = new Set();  // candidates - start without candidates
      this.#given = false;           // imported as fixed clue

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
      this.#candidates = new Set([digit]);
    }

    clearValue() {
      this.#value = 0;
      /* do not force candidates here */
    }

    /* ---- candidates ---- */
    getCandidates() {
      return this.#candidates;
    }

    setCandidates(candidates) {
      if (typeof candidates === "number") {
        if (candidates === 0) {
          this.#candidates = new Set();
        } else {
          this.#candidates = new Set([candidates]);
        }
      } else {
        this.#candidates = new Set(candidates);
      }
    }

    hasCandidate(digit) {
      return this.#candidates.has(digit);
    }

    enableCandidate(digit) {
      this.#candidates.add(digit);
    }

    disableCandidate(digit) {
      this.#candidates.delete(digit);

      /* If the candidate is removed, remove its candidate-color too */
      this.#candidateColorIndex[digit - 1] = -1;
    }

    toggleCandidate(digit) {
      const wasOn = this.#candidates.has(digit);

      if (wasOn) {
        /* If candidate removed, remove its color too */
        this.#candidates.delete(digit);
        this.#candidateColorIndex[digit - 1] = -1;
      } else {
        this.#candidates.add(digit);
      }

      return !wasOn;
    }

    countCandidates() {
      return this.#candidates.size;
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

    getCandidates(idx) {
      return this.#cellAt(idx).getCandidates();
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
        cell.setCandidates(0);
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
          cell.setCandidates(0);
          continue;
        }

        cell.setGiven(true);
        cell.setValue(d);
        cell.setCandidates(d);
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

    exportCandidates() {
      let total_candidates = [];
      for (let i = 0; i < 81; i++) {
        const cell = this.#cellAt(i);
        const candidates = cell.getCandidates().values().toArray();
        total_candidates.push(candidates);
      }
      return total_candidates;
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
          candMask: cell.getCandidates().values().toArray(),
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
        cell.setCandidates(s.candMask);

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
      cell.setCandidates(digit);

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
          cell.setCandidates(cell.getValue());
          continue;
        }

        let candidates = new Set(ALL_CANDIDATES_LIST);
        for (const p of PEERS[i]) {
          const pv = this.getValue(p);
          if (pv) {
            candidates.delete(pv);
          }
        }
        cell.setCandidates(candidates); // may become empty if contradiction; that is OK
      }
    }

    // Auto-clear "soft" update.
    // Removes the placed digit from candidates in peers ONLY.
    // Does NOT re-add any candidate bits that the user manually removed.
    autoClearPeersAfterPlacement(idx, digit) {
      // Remove digit from all peers' candidate masks (only if the peer is not filled).
      for (const p of PEERS[idx]) {
        const cell = this.#cellAt(p);
        if (cell.isSolved()) {
          continue;
        }
        if (cell.hasCandidate(digit)) {
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
    findContradictoryValueCells() {
      const bad = new Set();
      const scanUnit = (indices) => {
        const byDigit = Array.from({ length: 10 }, () => []);
        for (const idx of indices) {
          const v = this.getValue(idx);
          if (v >= 1 && v <= 9) {
            byDigit[v].push(idx);
          }
        }
        for (let d = 1; d <= 9; d++) {
          if (byDigit[d].length > 1) {
            for (const idx of byDigit[d]) {
              bad.add(idx);
            }
          }
        }
      };

      for (let r = 0; r < 9; r++) { scanUnit(UNITS.rows[r]); }
      for (let c = 0; c < 9; c++) { scanUnit(UNITS.cols[c]); }
      for (let b = 0; b < 9; b++) { scanUnit(UNITS.boxs[b]); }
      return bad;
    }

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
            const { r, c, b } = idxToRCB(idx);
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
  const techniqueListEl = $("techniqueList");
  const techniqueSummaryEl = $("techniqueSummary");

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
  let availableTechniques = new Set();  // all techniques existing in the solver
  let enabledTechniques = new Set(loadStoredTechnique());  // only techniques checked by the user
  let techniqueToCategory = {};

  /* Highlight digit selected by clicking solved cells (when optHighlight enabled) */
  let highlightDigit = 0;
  let contradictionIdxs = new Set();

  function updateContradictionHighlights() {
    contradictionIdxs = board.findContradictoryValueCells();
  }

  /* solver state */
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

  function updateTechniqueSummary() {
    if (!techniqueSummaryEl) {
      return;
    }
    const count = getEnabledTechniques().length;
    techniqueSummaryEl.textContent = count + " teknikoj aktivaj";
  }

  function updateStepButtonLabel() {
    const btn = $("btnStep");
    if (btn) {
      btn.textContent = pendingStepEvent ? "Apliki" : "Ruli 1 paŝon";
      btn.classList.toggle("ok", !!pendingStepEvent);
      btn.classList.toggle("secondary", !pendingStepEvent);
    }
  }

  function updateSolveButtonState() {
    const btn = $("btnSolve");
    if (!btn) {
      return;
    }

    const running = !!solveTimer;
    btn.textContent = running ? "Halti" : "Komenci solvadon";
    btn.classList.toggle("danger", running);
    btn.classList.toggle("ok", !running);
    btn.classList.remove("secondary");
    btn.title = running ? "Haltigi la nunan solvadon" : "Komenci aŭtomatan solvadon";
  }

  function updateAllPossibleStepsButtonState() {
    const btn = $("btnAllPossibleSteps");
    if (!btn) {
      return;
    }

    btn.disabled = !!allPossibleScanRunning;
    btn.classList.toggle("disabled", !!allPossibleScanRunning);
  }

  function hasAnyCandidateInUnsolvedCell() {
    for (let i = 0; i < 81; i++) {
      if (!board.isSolved(i) && board.getCandidates(i).size !== 0) {
        return true;
      }
    }
    return false;
  }

  function ensureCandidatesBeforeStep() {
    if (hasAnyCandidateInUnsolvedCell()) {
      return false;
    }

    board.recalcAllCandidatesFromValues();
    renderAll();
    return true;
  }

  function clearPendingStepPreview() {
    pendingStepEvent = null;
    clearAllEventHighlights();
    clearCandidateLinks();
    updateStepButtonLabel();
  }

  function setTechniqueSelection(techs) {
    enabledTechniques = new Set(techs);

    if (techniqueListEl) {
      const inputs = techniqueListEl.querySelectorAll("input[type=checkbox][data-technique]");
      for (const input of inputs) {
        const tech = input.dataset.technique;
        input.checked = enabledTechniques.has(tech);
      }
    }

    updateTechniqueSummary();
    saveStoredTechnique(enabledTechniques);
    clearPendingStepPreview();
  }

  function buildTechniquePanel() {
    if (!techniqueListEl) {
      return;
    }

    techniqueListEl.innerHTML = "";
    try {
      const response = API.getTechniques();
      const manifest = response["techniques"];
      for (const entry of manifest) {
        const name = entry.name;
        const category = entry.category;

        // avoid duplicates
        if (!availableTechniques.has(name)) {
          availableTechniques.add(name);
          techniqueToCategory[name] = category;

          const label = document.createElement("label");
          label.className = "techniqueItem";

          const input = document.createElement("input");
          input.type = "checkbox";
          input.checked = enabledTechniques.has(name);
          input.dataset.technique = name;
          input.addEventListener("change", () => {
            if (input.checked) {
              enabledTechniques.add(name);
            } else {
              enabledTechniques.delete(name);
            }
            updateTechniqueSummary();
            saveStoredTechnique(enabledTechniques);
            clearPendingStepPreview();
          });

          const text = document.createElement("span");
          text.className = "techniqueItemLabel";
          text.textContent = name;

          label.appendChild(input);
          label.appendChild(text);
          techniqueListEl.appendChild(label);
        }
      }

      updateTechniqueSummary();
    } catch (e) {
      openErrorModal(e);
      return;
    }
  }

  /* chain overlay state */
  let chainLinks = [];

  /* =========================================================
   * Rich Log (clickable) + Snapshot Preview
   * ========================================================= */
  const eventLogEntries = [];
  let previewActive = false;
  let previewSavedLiveState = null;
  let previewActiveIndex = -1;
  let allPossibleScanRunning = false;
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
    const ids = ["btnStep", "btnSolve", "btnFullSolve", "btnAllPossibleSteps"];
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
    return formatEventLogByTechnique(ev);
  }

  // Functions visible to formatter.js
  if (typeof window !== "undefined") {
    window.SudorixFormatterContext = {
      escapeHtml,
      idxToRef,
      addCandidateLink,
      clearCandidateLinks,
      renderChainLinks,
      getTechniqueCategory,
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
  function openErrorModal(e) {
    openCheckModal("ERROR: " + e.cause + " - " + e.message);
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
    digitPadEl.classList.toggle("digitPadValueMode", newMode === "value");
    digitPadEl.classList.toggle("digitPadCandidateMode", newMode === "cand");
    colorPadEl.classList.toggle("hidden", newMode !== "color");
    const colorActionsEl = $("colorActions");
    if (colorActionsEl) {
      colorActionsEl.classList.toggle("hidden", newMode !== "color");
    }

    for (const btn of digitPadEl.querySelectorAll(".digitBtnBig")) {
      const d = btn.dataset.digit || btn.textContent.trim();
      const main = btn.querySelector(".digitBtnMain");
      const sub = btn.querySelector(".digitBtnSub");
      if (main) { main.textContent = d; }
      if (sub) { sub.textContent = newMode === "cand" ? "Kandidato" : "Valoro"; }
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
    el.classList.toggle("contradiction", contradictionIdxs.has(idx));
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
      el.classList.toggle("contradiction", contradictionIdxs.has(selectedIdx));

      applyCellBaseBackground(el, selectedIdx);
    }

    selectedIdx = idx;

    // select new cell
    var el = gridEl.children[idx];
    var v = board.getValue(idx);

    el.classList.toggle("selected", idx === selectedIdx);
    el.classList.toggle("given", board.isGiven(idx));
    el.classList.toggle("contradiction", contradictionIdxs.has(idx));
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
    updateSolveButtonState();
    updateAllPossibleStepsButtonState();
  }

  /* =========================================================
   * Solver
   * ========================================================= */
  function applyEvent(ev) {
    if (!ev || !ev.operations || ev.operations.length === 0) {
      return false;
    }

    if (ev.type === "setValue") {
      let any = false;

      for (const op of ev.operations) {
        const idx = op.cell;
        const digit = op.digits[0];

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

      for (const op of ev.operations) {
        const idx = op.cell;
        const digits = op.digits;

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
    if (isWasmReady()) {
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
    let ev;

    if (!ensureWasmReadyOrNotify()) {
      callbackDone(false);
      return;
    }

    try {
      const response = API.nextStep();
      ev = response["step"];
    } catch (e) {
      if (e.cause == "NO_STEP") {
        callbackDone(false);
      } else {
        openErrorModal(e);
      }
      return;
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
      // no need to redraw sources and links

      setTimeout(() => {
        clearAllEventHighlights();
        clearCandidateLinks();
        callbackDone(did);
      }, 200);
    }, 200);
  }

  function startSolving() {
    stopSolving();

    clearLog();

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    board.recalcAllCandidatesFromValues();
    renderAll();

    try {
      const techniques = getEnabledTechniques();
      const response = API.setEnabledTechniques(techniques);
    } catch (e) {
      openErrorModal(e);
      return;
    }

    try {
      API.initBoard(board.exportToString());
    } catch (e) {
      openErrorModal(e);
      return;
    }

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
    updateSolveButtonState();
    loop();
  }

  function toggleSolving() {
    if (solveTimer) {
      stopSolving();
      return;
    }

    startSolving();
  }

  function runFullSolve() {
    stopSolving();

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    try {
      const techniques = getEnabledTechniques();
      const response = API.setEnabledTechniques(techniques);
    } catch (e) {
      openErrorModal(e);
      return;
    }

    try {
      const response = API.fullSolve(board.exportToString());
      const solution = response.solution;
      importSudoku(solution);
      appendInfo("WASM plen-solve: finita.");
    } catch (e) {
      if (e.cause == "NO_STEP") {
        openCheckModal("WASM plen-solve malsukcesis (neniu rezulto).");
        appendInfo("WASM plen-solve: malsukceso.");
      } else {
        openErrorModal(e);
      }
    }
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
      try {
        const techniques = getEnabledTechniques();
        const response = API.setEnabledTechniques(techniques);
      } catch (e) {
        openErrorModal(e);
        return;
      }

      // If the user imported only values and no candidates are present in unsolved cells,
      // prepare candidates automatically. If at least one unsolved cell already has
      // candidates, keep the board exactly as the user configured it.
      const candidatesWereGenerated = ensureCandidatesBeforeStep();
      if (candidatesWereGenerated) {
        // ...
      }

      let ev;
      try {
        const values = board.exportToString();
        const candidates = board.exportCandidates();
        const response = API.hint(values, candidates);
        ev = response["step"];
      } catch (e) {
        if (e.cause == "NO_STEP") {
          appendInfo("Neniu plia evento.");
          updateStepButtonLabel();
        } else {
          openErrorModal(e);
        }
        return;
      }

      pendingStepEvent = ev;
      updateStepButtonLabel();

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
    updateStepButtonLabel();

    saveUndoSnapshot();
    const did = applyEvent(ev);
    renderAll();
    // no need to redraw sources and links

    setTimeout(() => {
      clearAllEventHighlights();
      clearCandidateLinks();
    }, 200);

    if (!did) {
      appendInfo("Evento ne aplikebla.");
    }
  }

  function runAllPossibleSteps() {
    stopSolving();
    clearPendingStepPreview();
    clearLog();

    if (allPossibleScanRunning) {
      return;
    }

    if (!ensureWasmReadyOrNotify()) {
      return;
    }

    const selectedTechniques = getEnabledTechniques();
    if (selectedTechniques.length === 0) {
      openCheckModal("Neniu tekniko estas aktiva.");
      return;
    }

    ensureCandidatesBeforeStep();
    renderAll();
    clearAllEventHighlights();
    clearCandidateLinks();

    allPossibleScanRunning = true;
    const btn = $("btnAllPossibleSteps");
    const oldText = btn ? btn.textContent : "";
    let totalEvents = 0;
    let index = 0;

    updateAllPossibleStepsButtonState();
    appendInfo("Serĉo de ĉiuj eblaj paŝoj komenciĝis.");

    const finish = () => {
      allPossibleScanRunning = false;
      if (btn) {
        btn.textContent = oldText || "Ĉiuj eblaj paŝoj";
      }
      updateAllPossibleStepsButtonState();

      let j = (totalEvents > 1) ? "j" : "";
      appendInfo(`Ĉiuj eblaj paŝoj: ${totalEvents} evento${j} trovita${j}.`);
    };

    const scanNextTechnique = () => {
      if (index >= selectedTechniques.length) {
        finish();
        return;
      }

      const technique = selectedTechniques[index++];
      if (btn) {
        btn.textContent = `Serĉas ${index}/${selectedTechniques.length}`;
      }

      // Yield before each expensive WASM call so the UI can repaint the progress text.
      setTimeout(() => {
        let events;
        try {
          const values = board.exportToString();
          const candidates = board.exportCandidates();
          const response = API.allPossibleStepsForTechnique(technique, values, candidates);
          events = response["steps"];
        } catch (e) {
          if (e.cause == "NO_STEP") {
            scanNextTechnique();
          } else {
            openErrorModal(e);
          }
          return;
        }

        if (events.length > 0) {
          let j = (events.length > 1) ? "j" : "";
          appendInfo(`${technique}: ${events.length} ebla${j} paŝo${j}.`);
          for (const ev of events) {
            logEventOnce(ev);
          }
          totalEvents += events.length;
        }

        scanNextTechnique();
      }, 0);
    };

    scanNextTechnique();
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
      b.dataset.digit = String(d);
      b.innerHTML = `<span class="digitBtnMain">${d}</span>`;
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
      b.title = `Koloro ${i + 1}`;
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
    contradictionIdxs = new Set();
    clearPendingStepPreview();

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

    updateContradictionHighlights();
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
    try {
      const response = API.countSolutions(board.exportToString());
      const solutions = response.solutions;
      if (solutions === -1) {
        openCheckModal("Eraro: malvalida Sudoku-ĉeno (kodiga eraro).");
      } else if (solutions === 0) {
        openCheckModal("Rezulto: neniu solvo (0).");
      } else if (solutions === 1) {
        openCheckModal("Rezulto: unu sola solvo (unika).");
      } else {
        openCheckModal("Rezulto: pluraj solvoj (" + solutions + ").");
      }
    } catch (e) {
      openErrorModal(e);
    }
  });

  $("btnSolve").addEventListener("click", () => toggleSolving());
  $("btnFullSolve").addEventListener("click", () => runFullSolve());

  $("btnClearLog").addEventListener("click", () => clearLog());

  $("btnTechAll").addEventListener("click", () => setTechniqueSelection(availableTechniques));
  $("btnTechNone").addEventListener("click", () => setTechniqueSelection([]));
  $("btnTechDefaults").addEventListener("click", () => setTechniqueSelection(availableTechniques));

  $("btnStep").addEventListener("click", () => solveOneStep());
  $("btnAllPossibleSteps").addEventListener("click", () => runAllPossibleSteps());

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
  async function init() {
    buildDigitPad3x3();
    buildColorPad3x3();
    buildGridUI();

    try {
      setSolverStatus(false, "WASM solvilo ne preta.");
      await initWasmSolver();
      setSolverStatus(true, "WASM solvilo preta.");
      buildTechniquePanel();
    } catch (e) {
      setSolverStatus(false, "WASM malsukcesis: " + (e && e.message ? e.message : String(e)));
    }

    setMode("value");
    updateStepButtonLabel();

    appendInfo("La solvilo skribos ĉi tie la paŝojn.");
    window.addEventListener("resize", () => renderChainLinks());
    renderTimer();
    updatePauseButtonState();
    updateSolveButtonState();
  }

  init();
});
