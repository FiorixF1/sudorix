(function () {
  const REGISTRY = Object.create(null);

  function getCtx() {
    return (typeof window !== "undefined" && window.SudorixFormatterContext) ? window.SudorixFormatterContext : null;
  }

  const bugFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      switch (name) {
        case "BUG": return 1;
        case "peers": return 2;
      }
      return 1;
    }
  };
  REGISTRY["BUG"] = bugFormatter;

  const fishFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      let baseSets = [];
      let fins = [];
      let digit = 0;
      for (let group of sources) {
        if (group.name == "base") {
          for (let list of group.list) {
            baseSets.push(list.eureka);
            digit = list.digits[0];  // assume there is only one digit
          }
        }
        if (group.name == "fin") {
          for (let list of group.list) {
            fins.push(list.eureka);
          }
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
    },
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      switch (name) {
        case "base": return 1;
        case "fin": return 2;
      }
      return 1;
    }
  };
  REGISTRY["Fish"] = fishFormatter;

  const rectangleFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      let digitSet = new Set();
      let rectangleSet = [];
      for (let group of sources) {
        if (group.name == "UR") {
          for (let list of group.list) {
            digitSet = digitSet.union(new Set(list.digits));
            rectangleSet.push(list.eureka);
          }
        }
      }
      let digits = digitSet.values().toArray();

      parts.push(`<div>{${ctx.escapeHtml(digits.join(","))}} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(rectangleSet.join(","))}</span> => </div>`);

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      switch (name) {
        case "UR": return 1;
        case "guardian": return 2;
      }
      return 1;
    }
  };
  REGISTRY["UR"] = rectangleFormatter;

  const wingFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      let digitSet = new Set();
      let wingSet = [];
      for (let group of sources) {
        if (group.name == "Wing") {
          for (let list of group.list) {
            digitSet = digitSet.union(new Set(list.digits));
            wingSet.push(list.eureka);
          }
        }
      }
      let digits = digitSet.values().toArray();

      parts.push(`<div>{${ctx.escapeHtml(digits.join(","))}} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(wingSet)}</span> => </div>`);

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      switch (name) {
        case "Wing": return 1;
        case "Z": return 2;
      }
      return 1;
    }
  };
  REGISTRY["Wing"] = wingFormatter;

  const fireworksFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      for (let group of sources) {
        let digitSet = new Set();
        let fireworksSet = [];
        if (group.name.startsWith("firework")) {
          for (let list of group.list) {
            digitSet = digitSet.union(new Set(list.digits));
            fireworksSet.push(list.eureka);
          }
        }
        let digits = digitSet.values().toArray();

        parts.push(`<div>{${ctx.escapeHtml(digits.join(","))}} en <span class="logCellRef logSourceCategory1">${ctx.escapeHtml(fireworksSet)}</span> => </div>`);
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    }
  };
  REGISTRY["Fireworks"] = fireworksFormatter;

  const colorFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      if (name == "color_A") {
        // green for first color
        return 1;
      }
      if (name == "empty") {
        // yellow for emptied cell
        return 4;
      }
      if (name == "color_B") {
        if (ev.detailedReason.includes("Wrap") || ev.detailedReason.includes("Emptied")) {
          // red for second color when eliminated
          return 13;
        } else {
          // blue for second color
          return 2;
        }
      }
    }
  };
  REGISTRY["Coloring"] = colorFormatter;

  const chainFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      for (let group of sources) {
        let digitCounter = new Set();
        let nodes = [];
        let digits = [];

        for (let list of group.list) {
          digitCounter = digitCounter.union(new Set(list.digits));
          nodes.push(list.eureka);
          digits.push(list.digits[0]);
        }

        // stringify chain (Eureka notation)
        if (digitCounter.size == 1) {
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
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      // alternate color between each node
      return (groupIndex % 2) + 1;
    }
  };
  REGISTRY["AIC"] = chainFormatter;

  const forcingFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];
      const detailedReason = ev.detailedReason;

      for (let idx in sources) {
        let group = sources[idx];
        let digitCounter = new Set();
        let nodes = [];
        let digits = [];
        for (let list of group.list) {
          digitCounter = digitCounter.union(new Set(list.digits));
          nodes.push(list.eureka);
          digits.push(list.digits[0]);
        }

        // stringify chain (Eureka notation) - use only multi digit formatting
        let chainString = "";
        let WANT_STRONG = false;  // FC generally starts from a weak link
        if (detailedReason == "Digit Forcing Chain" && idx == 1) {
          // unless you are reading the second chain of a Digit Forcing Chain
          // TODO: trovare modo per rimuovere dipendenza da indice: esempio aggiungere un campo start_from = weak
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
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      if (ev.detailedReason == "Digit Forcing Chain" && groupIndex == 0) {
        // in the very first digit of a Digit Forcing Chain
        // use yellow since that value is both true and false
        return 4;
      } else if (ev.detailedReason == "Digit Forcing Chain" && sourceIndex == 1) {
        // in the second chain of a Digit Forcing Chain
        // start from blue and alternate with green
        // TODO: trovare modo per rimuovere dipendenza da indice: esempio aggiungere un campo start_from = weak
        return (groupIndex + 1) % 2 + 1;
      } else {
        // default
        // start from green and alternate with blue
        return (groupIndex % 2) + 1;
      }


    }
  };
  REGISTRY["FC"] = forcingFormatter;

  const alsFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      let alsCellsList = [];
      let alsDigitsList = [];
      let rccs = [];
      let zs = [];
      for (let group of sources) {
        if (group.name == "ALS") {
          for (let list of group.list) {
            alsCellsList.push(list.eureka);
            alsDigitsList.push(list.digits);
          }
        }
        if (group.name == "RCC") {
          for (let list of group.list) {
            rccs.push(list.digits);
          }
        }
        if (group.name == "Z") {
          for (let list of group.list) {
            let digit = list.digits[0];
            if (zs.indexOf(digit) == -1) {
              zs.push(digit);
            }
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
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      switch (name) {
        // use a different color for each ALS
        case "ALS": return (groupIndex % 9) + 4;
        // purple for RCCs
        case "RCC": return 3;
        // blue for Z
        case "Z": return 2;
      }
      return 1;
    }
  };
  REGISTRY["ALS"] = alsFormatter;

  const sdcFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      parts.push(`<div>`);
      for (let idx in sources) {
        let group = sources[idx];
        let sdc = group.list[0];
        let name = group.name;
        parts.push(`{${sdc.digits}} en <span class="logSourceCategory${name == "line" ? 2 : idx == "box" ? 3 : 6}">${sdc.eureka}</span>`);
        if (idx == sources.length-1) {
          parts.push(` => </div>`);
        } else {
          parts.push(`</br>`);
        }
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      switch (name) {
        case "line": return 2;  // blue
        case "box": return 3;   // purple
        case "extra": return 6; // orange
      }
      return 1;
    }
  };
  REGISTRY["SDC"] = sdcFormatter;

  const blossomFormatter = {
    formatLog(ev, ctx) {
      const sources = ev.sources;
      const parts = [];

      parts.push(`<div>`);
      for (let group of sources) {
        if (group.name == "stem") {
          let blossom = group.list[0];
          parts.push(`Tigo en <span class="logCellRef logSourceCategory1">${blossom.eureka}</span> kun {${blossom.digits}}</br>`);
        }
      }

      for (let group of sources) {
        if (group.name == "petal") {
          for (let idx in group.list) {
            let petal = group.list[idx];
            parts.push(`{${petal.digits}} en <span class="logSourceCategory${(idx % 9) + 3}">${petal.eureka}</span>`);
            // last set
            if (idx == group.list.length-1) {
              parts.push(` => </div>`);
            } else {
              parts.push(`</br>`);
            }
          }
        }
      }

      defaultOperationsFormatter(ctx, ev, parts);

      return { title: ev.detailedReason, bodyHtml: parts.join("") };
    },
    getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex) {
      switch (name) {
        case "stem": return 2;
        case "petal": return (groupIndex % 9) + 3;
      }
      return 1;
    }
  };
  REGISTRY["Blossom"] = blossomFormatter;

  window.SudorixFormatterRegistry = REGISTRY;

  window.formatEventLogByTechnique = function (ev) {
    const ctx = getCtx();
    const fmt = REGISTRY[ctx.getTechniqueCategory(ev.reason)];
    if (!ctx || !fmt || typeof fmt.formatLog !== "function") {
      return defaultFormatter(ev, ctx);
    }
    return fmt.formatLog(ev, ctx);
  };

  window.getSourceCategoryByTechnique = function (ev, name, sourceIndex, groupIndex, cellIndex) {
    const ctx = getCtx();
    const fmt = REGISTRY[ctx.getTechniqueCategory(ev.reason)];
    if (!ctx || !fmt || typeof fmt.getSourceCategory !== "function") {
      return 0;
    }
    return fmt.getSourceCategory(ev, name, sourceIndex, groupIndex, cellIndex);
  };

  window.defaultFormatter = function (ev, ctx) {
    const sources = ev.sources;
    const parts = [];

    defaultSourcesFormatter(ctx, ev, parts, sources);
    defaultOperationsFormatter(ctx, ev, parts);

    return { title: ev.detailedReason || "Solver", bodyHtml: parts.join("") };
  }

  // works well for basic techniques (singles, naked/hidden sets and intersections)
  window.defaultSourcesFormatter = function (ctx, ev, parts, sources) {
    if (sources.length > 0) {
      for (let sourceIndex in sources) {
        const source = sources[sourceIndex];
        const name = source.name;
        const list = source.list;

        for (let groupIndex in list) {
          const group = list[groupIndex];
          const cells = group.cells;
          const digits = group.digits;
          const category = getSourceCategoryByTechnique(ev, name, +sourceIndex, +groupIndex, 0);

          if (cells && cells.length > 0) {
            const ref = group.eureka;
            if (digits.length > 1) {
              parts.push(`<div>{${ctx.escapeHtml(digits.join(","))}} en <span class="logCellRef logSourceCategory${category}">${ctx.escapeHtml(ref)}</span> => </div>`);
            } else {
              parts.push(`<div>${ctx.escapeHtml(digits.join(","))} en <span class="logCellRef logSourceCategory${category}">${ctx.escapeHtml(ref)}</span> => </div>`);
            }
          } else if (digits.length > 0) {
            parts.push(`<div><span class="logCellRef logSourceCategory${category}">{${ctx.escapeHtml(digits.join(","))}}</span></div>`);
          }
        }
      }
    }
  }

  // not meant to be customized, actually
  window.defaultOperationsFormatter = function (ctx, ev, parts) {
    if (!ev.operations || ev.operations.length === 0) {
      parts.push(`<div>Neniu operacio.</div>`);
      return;
    }

    if (ev.type === "setValue") {
      for (const op of ev.operations) {
        const d = op.digits[0];
        const ref = ctx.idxToRef(op.cell);
        if (!d) {
          const digs = op.digits;
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpSet">=</span> <span class="logOpSet">${ctx.escapeHtml(digs.join(","))}</span></div>`);
        } else {
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpSet">=</span> <span class="logOpSet">${d}</span></div>`);
        }
      }
    } else if (ev.type === "removeCandidate") {
      for (const op of ev.operations) {
        const ref = ctx.idxToRef(op.cell);
        const digs = op.digits;
        if (digs.length > 1) {
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpRemove">&lt;&gt;</span> <span class="logOpRemove">{${ctx.escapeHtml(digs.join(","))}}</span></div>`);
        } else {
          parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> <span class="logOpRemove">&lt;&gt;</span> <span class="logOpRemove">${digs[0]}</span></div>`);
        }
      }
    } else {
      for (const op of ev.operations) {
        const ref = ctx.idxToRef(op.idx);
        const digs = op.digits;
        parts.push(`<div><span class="logCellRef">${ctx.escapeHtml(ref)}</span> ${ctx.escapeHtml(digs.join(","))}</div>`);
      }
    }
  }
})();
