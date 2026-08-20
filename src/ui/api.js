(function () {
  let wasmModule = null;
  let wasmSolverAPI = null;

  window.initWasmSolver = function () {
    // createSudorixSolver is defined by solver_wasm.js (Emscripten output)
    if (typeof createSudorixSolver !== "function") {
      return Promise.reject(
        new Error("createSudorixSolver ne disponebla")
      );
    }

    wasmReady = createSudorixSolver({
      locateFile: (path) => path  // keep .wasm next to .js
    }).then((Module) => {
      wasmModule = Module;
      wasmSolverAPI = wasmModule.cwrap("sudorix_solver_api", "string", ["string"]);
      return Module;
    }).catch((e) => {
      wasmModule = null;
      wasmSolverAPI = null;
      throw e;
    });

    return wasmReady;
  };

  window.isWasmReady = function () {
    return wasmModule && wasmSolverAPI;
  };

  // General API
  function solverApi(request) {
    if (!wasmModule || !wasmSolverAPI) {
      throw new Error("Funkcio sudorix_solver_api ne disponeblas en tiu ĉi WASM build.");
    }

    // JS object to string
    const requestStr = JSON.stringify(request);

    // Call WASM engine
    const responseStr = wasmSolverAPI(requestStr);

    // Convert the result to JS object
    const response = JSON.parse(responseStr);

    // Error handling
    if (response.status !== "ok") {
      throw new Error(response.error || "Unknown solver error", {cause: response.code || "Unknown code"});
    }

    return response;
  }

  // API wrapper
  {
    const API = Object.create(null);

    API.countSolutions = function (puzzle) {
      return solverApi({
        command: "countSolutions",
        puzzle: puzzle
      });
    };

    API.fullSolve = function (puzzle) {
      return solverApi({
        command: "fullSolve",
        puzzle: puzzle
      });
    };

    API.initBoard = function (puzzle) {
      return solverApi({
        command: "initBoard",
        puzzle: puzzle
      });
    };

    API.nextStep = function () {
      return solverApi({
        command: "nextStep"
      });
    };

    API.hint = function (values, candidates) {
      return solverApi({
        command: "hint",
        board: {
          values: values,
          candidates: candidates
        }
      });
    };
  
    API.allPossibleStepsForTechnique = function (technique, values, candidates) {
      return solverApi({
        command: "allPossibleSteps",
        technique: technique,
        board: {
          values: values,
          candidates: candidates
        }
      });
    };

    API.setEnabledTechniques = function (techniques) {
      return solverApi({
        command: "setEnabledTechniques",
        techniques: techniques
      });
    };

    API.getTechniques = function () {
      return solverApi({
        command: "getTechniques"
      });
    };

    window.API = API;
  }
})();