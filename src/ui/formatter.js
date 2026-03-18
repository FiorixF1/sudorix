(function () {
  const REGISTRY = Object.create(null);

  function getCtx() {
    return (typeof window !== "undefined" && window.SudorixFormatterContext) ? window.SudorixFormatterContext : null;
  }

  const noSourcesFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.reason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      return (groupIndex % 2) + 1;
    }
  };
  REGISTRY["BUG+1"] = noSourcesFormatter;

  const fishFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      const baseSets = [];
      for (let baseSet of groups[0]) {
        baseSets.push(ctx.formatEurekaCellCode(baseSet.cells));
      }
      const digit = ctx.maskToSingleDigit(ev.sources[0].mask);

      parts.push(`<div>${digit} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(baseSets.join(","))}</span> => </div>`);

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.reason, bodyHtml: parts.join("") };
    }
  };
  REGISTRY["X-Wing"] = fishFormatter;
  REGISTRY["Swordfish"] = fishFormatter;
  REGISTRY["Jellyfish"] = fishFormatter;

  const wingFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      let totalMask = 0;
      let wingSet = [];
      for (let source of groups[0]) {
        totalMask |= source.mask;
        wingSet.push(ctx.formatEurekaCellCode(source.cells));
      }
      let digits = ctx.maskToDigits(totalMask);

      parts.push(`<div>{${ctx.escapeHtml(digits.join(","))}} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(wingSet)}</span> => </div>`);

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.reason, bodyHtml: parts.join("") };
    }
  };
  REGISTRY["XY-Wing"] = wingFormatter;
  REGISTRY["XYZ-Wing"] = wingFormatter;

  const colorFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.reason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      if (groupIndex == 0) {
        // green for first color
        return 1;
      } else if (groupIndex == 2) {
        // yellow for emptied cell
        return 3;
      } else {
        if (ev.type === "removeCandidate") {
          // blue for second color
          return 2;
        } else {
          // red for second color if it is eliminated
          return 13;
        }
      }
    }
  };
  REGISTRY["Simple Coloring"] = colorFormatter;
  REGISTRY["Simple Coloring (Color Trap)"] = colorFormatter;
  REGISTRY["Simple Coloring (Color Wrap)"] = colorFormatter;
  REGISTRY["3D Medusa"] = colorFormatter;
  REGISTRY["3D Medusa (Color Trap)"] = colorFormatter;
  REGISTRY["3D Medusa (Color Wrap)"] = colorFormatter;
  REGISTRY["3D Medusa (Emptied Cell)"] = colorFormatter;

  const chainFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      let totalMask = 0;
      let nodes = [];
      for (let node of groups[0]) {
        totalMask |= node.mask;
        nodes.push(ctx.formatEurekaCellCode(node.cells));
      }
      let digits = ctx.maskToDigits(totalMask);

      // stringify chain (current format rxcy=rxcy-rxcy=rxcy-...)
      let chainString = "";
      let WANT_STRONG = true;
      for (let node of nodes) {
        if (chainString != "") {
          if (WANT_STRONG) {
            chainString += "=";
          } else {
            chainString += "-";
          }
          WANT_STRONG = !WANT_STRONG;
        }
        chainString += node;
      }

      if (digits.length == 1) {
        // single digit chain
        parts.push(`<div>${digits.join(",")} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(chainString)}</span> => </div>`);
      } else {
        // multi digit chain
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.reason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      return (sourceIndex % 2) + 1;
    }
  };
  REGISTRY["Skyscraper"] = chainFormatter;
  REGISTRY["X-Chain"] = chainFormatter;
  REGISTRY["XY-Chain"] = chainFormatter;
  REGISTRY["Alternating Inference Chain"] = chainFormatter;
  REGISTRY["Alternating Inference Chain (Type 1)"] = chainFormatter;
  REGISTRY["Alternating Inference Chain (Type 2)"] = chainFormatter;
  REGISTRY["Alternating Inference Chain (Type 3)"] = chainFormatter;
  REGISTRY["Grouped Alternating Inference Chain"] = chainFormatter;
  REGISTRY["Grouped Alternating Inference Chain (Type 1)"] = chainFormatter;
  REGISTRY["Grouped Alternating Inference Chain (Type 2)"] = chainFormatter;
  REGISTRY["Grouped Alternating Inference Chain (Type 3)"] = chainFormatter;

  window.SudorixFormatterRegistry = REGISTRY;

  window.formatEventLogByReason = function (ev) {
    const ctx = getCtx();
    const fmt = REGISTRY[ev.reason];
    if (!ctx || !fmt || typeof fmt.formatLog !== "function") {
      return defaultFormatter(ev, ctx);
    }
    return fmt.formatLog(ev, ctx);
  };

  window.getSourceCategoryByReason = function (ev, source, sourceIndex, groupIndex, sourceIndexInGroup) {
    const fmt = REGISTRY[ev.reason];
    if (!fmt || typeof fmt.getSourceCategory !== "function") {
      return 0;
    }
    return fmt.getSourceCategory(ev, source, sourceIndex, groupIndex, sourceIndexInGroup);
  };

  window.defaultFormatter = function (ev, ctx) {
    const groups = ctx.splitSourceGroups(ev.sources || []);
    const parts = [];

    defaultSourcesFormatter(ctx, ev, parts, groups);
    defaultOperationsFormatter(ctx, ev, parts);

    return { title: ev.reason || "Solver", bodyHtml: parts.join("") };
  }

  // works well for basic techniques (naked/hidden sets and intersections)
  window.defaultSourcesFormatter = function (ctx, ev, parts, groups) {
    if (groups.length > 0) {
      let sourceIndex = 0;
      for (let groupIndex = 0; groupIndex < groups.length; groupIndex++) {
        const group = groups[groupIndex];
        if (groups.length > 1) {
          parts.push(`<div class="logSourceCategory${normalizeSourceCategory(groupIndex + 1)}"><b>Group ${groupIndex + 1}</b></div>`);
        }
        for (let groupPos = 0; groupPos < group.length; groupPos++) {
          const s = group[groupPos];
          const category = ctx.resolveSourceCategory(ev, s, sourceIndex, groupIndex, groupPos);
          sourceIndex++;
          const digs = ctx.maskToDigits(s.mask);
          if (s.cells && s.cells.idxs && s.cells.idxs.length > 0) {
            const ref = ctx.formatEurekaCellCode(s.cells);
            if (digs.length > 1) {
              parts.push(`<div>{${ctx.escapeHtml(digs.join(","))}} en <span class="logCellRef logSourceCategory${category}">${ctx.escapeHtml(ref)}</span> => </div>`);
            } else {
              parts.push(`<div>${ctx.escapeHtml(digs.join(","))} en <span class="logCellRef logSourceCategory${category}">${ctx.escapeHtml(ref)}</span> => </div>`);
            }
          } else if (digs.length > 0) {
            parts.push(`<div><span class="logCellRef logSourceCategory${category}">{${ctx.escapeHtml(digs.join(","))}}</span></div>`);
          }
        }
      }
    }
  }

  // not meant to be customized, actually
  window.defaultOperationsFormatter = function (ctx, ev, parts) {
    if (!ev.ops || ev.ops.length === 0) {
      parts.push(`<div>Neniu operacio.</div>`);
      return;
    }

    if (ev.type === "setValue") {
      for (const op of ev.ops) {
        const d = ctx.maskToSingleDigit(op.mask);
        const ref = ctx.idxToRef(op.idx);
        if (!d) {
          const digs = ctx.maskToDigits(op.mask);
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpSet">=</span> <span class="logOpSet">${ctx.escapeHtml(digs.join(","))}</span></div>`);
        } else {
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpSet">=</span> <span class="logOpSet">${d}</span></div>`);
        }
      }
    } else if (ev.type === "removeCandidate") {
      for (const op of ev.ops) {
        const ref = ctx.idxToRef(op.idx);
        const digs = ctx.maskToDigits(op.mask);
        if (digs.length > 1) {
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpRemove">&lt;&gt;</span> <span class="logOpRemove">{${ctx.escapeHtml(digs.join(","))}}</span></div>`);
        } else {
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpRemove">&lt;&gt;</span> <span class="logOpRemove">${digs[0]}</span></div>`);
        }
      }
    } else {
      for (const op of ev.ops) {
        const ref = ctx.idxToRef(op.idx);
        const digs = ctx.maskToDigits(op.mask);
        parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> ${ctx.escapeHtml(digs.join(","))}</div>`);
      }
    }
  }
})();
