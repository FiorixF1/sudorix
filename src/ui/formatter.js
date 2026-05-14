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

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
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

      const fins = [];
      if (groups[1]) {
        for (let fin of groups[1]) {
          fins.push(ctx.formatEurekaCellCode(fin.cells));
        }
      }

      if (fins.length == 0) {
        parts.push(`<div>${digit} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(baseSets.join(","))}</span> => </div>`);
      } else {
        parts.push(`<div>${digit} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(baseSets.join(","))}</span>`);
        parts.push(` kaj naĝilo${fins.length > 1 ? "j" : ""} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(fins.join(","))}</span> => </div>`);
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    }
  };
  REGISTRY["X-Wing"] = fishFormatter;
  REGISTRY["Swordfish"] = fishFormatter;
  REGISTRY["Jellyfish"] = fishFormatter;
  REGISTRY["Finned X-Wing"] = fishFormatter;
  REGISTRY["Finned Swordfish"] = fishFormatter;
  REGISTRY["Finned Jellyfish"] = fishFormatter;
  REGISTRY["Franken X-Wing"] = fishFormatter;
  REGISTRY["Franken Swordfish"] = fishFormatter;
  REGISTRY["Franken Jellyfish"] = fishFormatter;
  REGISTRY["Finned Franken X-Wing"] = fishFormatter;
  REGISTRY["Finned Franken Swordfish"] = fishFormatter;
  REGISTRY["Finned Franken Jellyfish"] = fishFormatter;
  REGISTRY["Mutant X-Wing"] = fishFormatter;
  REGISTRY["Mutant Swordfish"] = fishFormatter;
  REGISTRY["Mutant Jellyfish"] = fishFormatter;
  REGISTRY["Finned Mutant X-Wing"] = fishFormatter;
  REGISTRY["Finned Mutant Swordfish"] = fishFormatter;
  REGISTRY["Finned Mutant Jellyfish"] = fishFormatter;

  const rectangleFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      let totalMask = 0;
      let rectangleSet = [];
      for (let source of groups[0]) {
        totalMask |= source.mask;
        rectangleSet.push(ctx.formatEurekaCellCode(source.cells));
      }
      let digits = ctx.maskToDigits(totalMask);

      parts.push(`<div>{${ctx.escapeHtml(digits.join(","))}} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(rectangleSet.join(","))}</span> => </div>`);

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    }
  };
  REGISTRY["Unique Rectangle"] = rectangleFormatter;
  REGISTRY["Hidden Rectangle"] = rectangleFormatter;

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

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    }
  };
  REGISTRY["XY-Wing"] = wingFormatter;
  REGISTRY["XYZ-Wing"] = wingFormatter;
  REGISTRY["W-Wing"] = wingFormatter;

  const colorFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      if (groupIndex == 0) {
        // green for first color
        return 1;
      } else if (groupIndex == 2) {
        // yellow for emptied cell
        return 4;
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
  REGISTRY["Remote Pair"] = colorFormatter;
  REGISTRY["Simple Coloring"] = colorFormatter;
  REGISTRY["3D Medusa"] = colorFormatter;

  const chainFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      for (let group of groups) {
        let totalMask = 0;
        let nodes = [];
        let digits = [];
        for (let node of group) {
          totalMask |= node.mask;
          nodes.push(ctx.formatEurekaCellCode(node.cells));
          digits.push(ctx.maskToSingleDigit(node.mask));
        }
        let digitCounter = ctx.maskToDigits(totalMask);

        // stringify chain (Eureka notation)
        if (digitCounter.length == 1) {
          // single digit chain
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
          parts.push(`<div>(${digits[0]}): <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(chainString)}</span> => </div>`);
        } else {
          // multi digit chain
          let chainString = "";
          let WANT_STRONG = true;
          let i = 0;
          while (i < nodes.length) {
            let node = nodes[i];
            let digit = digits[i];
            let next_node = nodes[i+1];
            let next_digit = digits[i+1];
            let link_type = WANT_STRONG ? "=" : "-";

            if (!next_digit) {
              // last node
              chainString += "(" + digit + ")" + node;
              WANT_STRONG = !WANT_STRONG;
              ++i;
            } else if (digit == next_digit) {
              // digit is not going to change in next node
              chainString += "(" + digit + ")" + node + link_type;
              WANT_STRONG = !WANT_STRONG;
              ++i;
            } else {
              // a new digit is coming in the next node, include it here
              chainString += "(" + digit + link_type + next_digit + ")" + node;
              WANT_STRONG = !WANT_STRONG;
              ++i;
              chainString += WANT_STRONG ? "=" : "-";
              WANT_STRONG = !WANT_STRONG;
              ++i;
            }
          }
          // remove trailing -/=
          if (chainString.endsWith("=") || chainString.endsWith("-")) {
            chainString = chainString.slice(0, -1);
          }
          parts.push(`<div><span class="logCellRef logSourceCategory1">${ctx.escapeHtml(chainString)}</span> => </div>`);
        }
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      return (sourceIndex % 2) + 1;
    }
  };
  REGISTRY["Single Digit Pattern"] = chainFormatter;
  REGISTRY["Empty Rectangle"] = chainFormatter;
  REGISTRY["X-Chain"] = chainFormatter;
  REGISTRY["XY-Chain"] = chainFormatter;
  REGISTRY["Alternating Inference Chain"] = chainFormatter;
  REGISTRY["Grouped X-Chain"] = chainFormatter;
  REGISTRY["Grouped Alternating Inference Chain"] = chainFormatter;

  const forcingFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];
      const detailedReason = ev.detailedReason;

      for (let idx in groups) {
        let group = groups[idx];
        let totalMask = 0;
        let nodes = [];
        let digits = [];
        for (let node of group) {
          totalMask |= node.mask;
          nodes.push(ctx.formatEurekaCellCode(node.cells));
          digits.push(ctx.maskToSingleDigit(node.mask));
        }
        let digitCounter = ctx.maskToDigits(totalMask);

        // stringify chain (Eureka notation) - use only multi digit formatting
        let chainString = "";
        let WANT_STRONG = false;  // FC generally starts from a weak link
        if (detailedReason == "Digit Forcing Chain" && idx == 1) {
          // unless you are reading the second chain of a Digit Forcing Chain
          WANT_STRONG = true;
        }
        let i = 0;
        while (i < nodes.length) {
          let node = nodes[i];
          let digit = digits[i];
          let next_node = nodes[i+1];
          let next_digit = digits[i+1];
          let link_type = WANT_STRONG ? "=" : "-";

          if (WANT_STRONG) {
            chainString += node + "<>" + digit + " ";
          } else {
            chainString += node + "=" + digit + " ";
          }
          WANT_STRONG = !WANT_STRONG;
          ++i;
        }
        // remove trailing space
        if (chainString.endsWith(" ")) {
            chainString = chainString.slice(0, -1);
          }
        parts.push(`<div><span class="logCellRef logSourceCategory1">${ctx.escapeHtml(chainString)}</span> => </div>`);
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      return (sourceIndex % 2) + 1;
    }
  };
  REGISTRY["Forcing Chain"] = forcingFormatter;
  REGISTRY["Forcing Net"] = forcingFormatter;

  const alsFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      let alsCellsList = [];
      let alsDigitsList = [];
      for (let als of groups[0]) {
        alsCellsList.push(ctx.formatEurekaCellCode(als.cells));
        alsDigitsList.push(ctx.maskToDigits(als.mask));
      }

      let rccs = [];
      for (let rcc of groups[1]) {
        rccs.push(ctx.maskToSingleDigit(rcc.mask));
      }

      let zs = [];
      if (groups[2]) {
        for (let z of groups[2]) {
          let digit = ctx.maskToSingleDigit(z.mask);
          if (zs.indexOf(digit) == -1) {
            zs.push(digit);
          }
        }
      }

      const ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      parts.push(`<div>`);
      for (let i in alsCellsList) {
        parts.push(`${ALPHA[i]} = <span class="logSourceCategory${(i % 9) + 4}">${ctx.escapeHtml(alsCellsList[i])} {${ctx.escapeHtml(alsDigitsList[i])}}</span>${rccs[i*2] ? ' -'+rccs[i*2]+'-' : ''}</br>`);
      }

      if (zs.length == 1) {
        parts.push(`Z = ${zs[0]} => </div>`);
      } else if (zs.length > 1) {
        parts.push(`Z = {${zs.join(",")}} => </div>`);
      } else {
        parts.push(`=> </div>`);
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      if (groupIndex == 0) {
        // use a different color for each ALS
        return (sourceIndex % 9) + 4;
      } else if (groupIndex == 1) {
        // purple for RCCs
        return 3;
      } else {
        // blue for Z
        return 2;
      }
    }
  };
  REGISTRY["Almost Locked Set XZ"] = alsFormatter;
  REGISTRY["Almost Locked Set XY-Wing"] = alsFormatter;
  REGISTRY["Almost Locked Set Chain"] = alsFormatter;

  const sdcFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      let sdcCellsList = [];
      let sdcDigitsList = [];

      parts.push(`<div>`);
      for (let i in groups[0]) {
        let sdc = groups[0][i];
        sdcCellsList.push(ctx.formatEurekaCellCode(sdc.cells));
        sdcDigitsList.push(ctx.maskToDigits(sdc.mask));
        parts.push(`{${ctx.maskToDigits(sdc.mask)}} en <span class="logSourceCategory${i == 0 ? 2 : i == 1 ? 3 : 6}">${ctx.formatEurekaCellCode(sdc.cells)}</span>`);
        if (i == groups[0].length-1) {
          parts.push(` => </div>`);
        } else {
          parts.push(`</br>`);
        }
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      if (groupIndex == 0) {
        if (sourceIndex == 0) {
          // blue
          return 2;
        } else if (sourceIndex == 1) {
          // purple
          return 3;
        } else if (sourceIndex == 2) {
          // orange
          return 6;
        }
      }
    }
  };
  REGISTRY["Sue de Coq"] = sdcFormatter;

  const blossomFormatter = {
    formatLog(ev, ctx) {
      const groups = ctx.splitSourceGroups(ev.sources || []);
      const parts = [];

      let blossomCellsList = [];
      let blossomDigitsList = [];

      parts.push(`<div>`);
      for (let i in groups[0]) {
        let blossom = groups[0][i];
        blossomCellsList.push(ctx.formatEurekaCellCode(blossom.cells));
        blossomDigitsList.push(ctx.maskToDigits(blossom.mask));
        // stem vs petal
        if (i == 0) {
          parts.push(`Tigo en <span class="logSourceCategory${(i % 9) + 2}">${ctx.formatEurekaCellCode(blossom.cells)}</span> kun {${ctx.maskToDigits(blossom.mask)}}`);
        } else {
          parts.push(`{${ctx.maskToDigits(blossom.mask)}} en <span class="logSourceCategory${(i % 9) + 2}">${ctx.formatEurekaCellCode(blossom.cells)}</span>`);
        }
        // last set
        if (i == groups[0].length-1) {
          parts.push(` => </div>`);
        } else {
          parts.push(`</br>`);
        }
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, source, sourceIndex, groupIndex) {
      if (groupIndex == 0) {
        return sourceIndex + 2;
      }
    }
  };
  REGISTRY["Death Blossom"] = blossomFormatter;

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

    return { title: ev.detailedReason || "Solver", bodyHtml: parts.join("") };
  }

  // works well for basic techniques (naked/hidden sets and intersections)
  window.defaultSourcesFormatter = function (ctx, ev, parts, groups) {
    if (groups.length > 0) {
      let sourceIndex = 0;
      for (let groupIndex = 0; groupIndex < groups.length; groupIndex++) {
        const group = groups[groupIndex];
        if (groups.length > 1) {
          parts.push(`<div class="logSourceCategory${ctx.normalizeSourceCategory(groupIndex + 1)}"><b>Group ${groupIndex + 1}</b></div>`);
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
