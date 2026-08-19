# Sudorix Solver API

## Superrigardo

Sudorix estas Sudoku-solvilo desegnita por liveri **klarigeblajn solvopaŝojn**.
La solvilo povas esti uzata per C API aŭ per WebAssembly.

Ĉiu solvopaŝo liveras:

- la **operaciojn** (ŝanĝoj aplikataj al la krado)
- la **fontojn** (logika klarigo pro kiu la paŝo estas valida)

La reta interfaco interpretas ĉi tiujn strukturojn por montri kolorigojn kaj klarigojn.

---

# Eniga formato de Sudokuo

La sudokua krado estas reprezentata kiel ĉeno de **81 signoj**.

Permesitaj signoj:

- `1..9` → jam solvita ĉelo
- `0` aŭ `.` → malplena ĉelo

Ekzemplo:

530070000600195000098000060800060003400803001700020006060000280000419005000080079

---

# Solver API

La interna interfaco entenas unuopan funkcion, kiu ricevas ĉenigitan JSON-objekton kaj respondas per alia JSON-objekto:

```
const char *sudorix_solver_api(const char *requestJson)
```

Se la solvilo estas memstare kompilata kaj ligita al ekstera programo, pli altnivela interfaco estas disponebla:

```
json sudorix_solver_api(const json &request)
```

Ĉiu eniga objekto havas tiun ĉi strukturon:

```
{
    "command": "la_komando"
    // enigaj datumoj
}
```

kaj la solvilo respondas per:

```
{
    "status": "ok"
    // eligaj datumoj
}
```

Se iu ajn eraro okazas, la respondo estos:

```
{
    "status": "error",
    "code": "priskriba kodo",
    "error": "priskriba mesaĝo"
}
```

Sekvas listo de eblaj komandoj kun iliaj parametroj kaj ekzemploj de enigo kaj eligo.  

---

## countSolutions

Ricevas Sudokuon kiel ĉenon kaj liveras la nombron da eblaj solvoj.

### Ekzempla enigo

```
{
  "command": "countSolutions",
  "puzzle": "...8.6...2...1..74..97...1...6...2.13.....6...2........3...5.....2....8.81...2953"
}
```

### Ekzempla eligo

```
{
  "status": "ok",
  "solutions": 1
}
```

---

## fullSolve

Ricevas Sudokuon kiel ĉenon kaj solvas ĝin en unu paŝo.

### Ekzempla enigo

```
{
  "command": "fullSolve",
  "puzzle": "...8.6.2926..1..74..972..16..6...2.13..2..6...2..6.7..937.85.62652....87814672953"
}
```

### Ekzempla eligo

```
{
  "status": "ok",
  "solution": "000806029260010074009720016006000201300200600020060700937085062652000087814672953"
}
```

---

## initBoard

Ricevas Sudokuon kiel ĉenon kaj ŝargas ĝin en la interna stato de la solvilo.

### Ekzempla enigo

```
{
  "command": "initBoard",
  "puzzle": "...8.6.2926..1..74..972..16..6...2.13..2..6...2.......93..85.62652....8.81...2953"
}
```

### Ekzempla eligo

```
{
  "status": "ok"
}
```

---

## nextStep

Liveras unu solvopaŝon de la ŝargita Sudokuo kaj ĝisdatigas la internan staton.

### Ekzempla enigo

```
{
  "command": "nextStep"
}
```

### Ekzempla eligo

```
{
  "status": "ok",
  "step": {
    "reason": "Unique Rectangle",
    "detailedReason": "Unique Rectangle (Type 1)",
    "type": "removeCandidate",
    "operations": [
      {
        "cell": 66,
        "digits": [1, 4]
      }
    ],
    "sources": [
      {
        "name": "UR",
        "list": [
          {
            "cells": [57, 60],
            "digits": [1, 4],
            "eureka": "r7c47"
          },
          {
            "cells": [66, 69],
            "digits": [1, 4],
            "eureka": "r8c47"
          }
        ]
      }
    ]
  }
}
```

---

## exportBoard

Eksportas la staton de la ŝargita Sudokuo kun solvitaj ĉeloj kaj kandidatoj.

### Ekzempla enigo

```
{
  "command": "exportBoard"
}
```

### Ekzempla eligo

```
{
  "status": "ok",
  "board": {
    "values": "173846529268519374549723816796458231381297645425361798937185462652934187814672953",
    "candidates": [
                    [1],
                    [7],
                    [3],
                    ...
                  ]
  }
}
```

---

## hint

Ricevas Sudokuon kiel tabelojn enhavantajn kaj la jam solvitajn ĉelojn kaj la kandidatojn por ĉiu ĉelo, kaj liveras unu solvopaŝon **sen ĝisdatigo de la interna stato**.

### Ekzempla enigo

```
{
  "command": "hint",
  "board": {
    "values": "...8.6...2...1..74..97...1...6...2.13.....6...2........3...5.....2....8.81...2953",
    "candidates": [
                    [1, 4, 5, 7],
                    [4, 5, 7],
                    [1, 3, 4, 5, 7],
                    [8],
                    [2, 3, 4, 5, 9],
                    ...
                  ]
  }
}
```

### Ekzempla eligo

```
{
  "status": "ok",
  "step": {
    "reason": "Pointing Set",
    "detailedReason": "Pointing Triple",
    "type": "removeCandidate",
    "operations": [
      {
        "cell": 51,
        "digits": [3]
      }
    ],
    "sources": [
      {
        "name": "basic",
        "list": [
          {
            "cells": [6, 15, 24],
            "digits": [3],
            "eureka": "r123c7"
          }
        ]
      }
    ]
  }
}
```

---

## allPossibleSteps

Ricevas Sudokuon kiel tabelojn enhavantajn kaj la jam solvitajn ĉelojn kaj la kandidatojn por ĉiu ĉelo, kune kun aplikenda tekniko kaj liveras liston de ĉiuj eblaj operacioj de tiu tekniko sur la donita skemo.

### Ekzempla enigo

```
{
  "command": "allPossibleSteps",
  "technique": "XY-Chain",
  "board": {
    "values": "...8.6...2...1..74..97...1...6...2.13.....6...2........3...5.....2....8.81...2953",
    "candidates": [
                    [1, 4, 5, 7],
                    [4, 5, 7],
                    [1, 3, 4, 5, 7],
                    [8],
                    [2, 3, 4, 5, 9],
                    ...
                  ]
  }
}
```

### Ekzempla eligo

```
{
  "status": "ok",
  "instances": 12,
  "steps": [
    {
      "reason": "XY-Chain",
      "detailedReason": "XY-Ring",
      "type": "removeCandidate",
      "operations": [
        {
          "cell": 0,
          "digits": [5]
        },
        {
          "cell": 4,
          "digits": [5]
        },
        {
          "cell": 4,
          "digits": [3]
        }
      ],
      "sources": [ ... ]
    },
    {
      "reason": "XY-Chain",
      "detailedReason": "XY-Ring",
      "type": "removeCandidate",
      "operations": [
        {
          "cell": 4,
          "digits": [3]
        },
        {
          "cell": 4,
          "digits": [5]
        },
        {
          "cell": 0,
          "digits": [5]
        }
      ],
      "sources": [ ... ]
    },
    ...
  ]
}
```

---

## setEnabledTechniques

Ricevas liston de ŝaltendaj teknikoj por la solvado.

### Ekzempla enigo

```
{
  "command": "setEnabledTechniques",
  "techniques": [
    "Full House",
    "Hidden Single",
    "Naked Single",
    "X-Wing"
  ]
}
```

### Ekzempla eligo

```
{
  "status": "ok"
}
```

---

## getTechniques

Liveras liston de disponeblaj teknikoj.

### Ekzempla enigo

```
{
  "command": "getTechniques"
}
```

### Ekzempla eligo

```
{
  "status": "ok",
  "techniques": [
    {
      "name": "Full House",
      "category": "Single"
    },
    {
      "name": "Hidden Single",
      "category": "Single"
    },
    {
      "name": "Pointing Set",
      "category": "Intersection"
    },
    {
      "name": "X_Wing",
      "category": "Fish"
    }
  ]
}
```

---

# Rimarko

La solvilo liveras nur **strukturitajn datumojn**.

La reta interfaco interpretas la fontojn por produkti legeblajn klarigojn kaj kolorigojn en la krado.
