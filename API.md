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

# Eliga formato por solvopaŝo

Funkcioj kiuj liveras solvopaŝojn plenigas bufron de `uint32_t`.

Strukturo:

* `out[0]` = type
* `out[1]` = reasonId
* `out[2]` = detailedReasonId
* `out[3]` = opCount
* `out[4]` = srcCount

Poste venas `srcCount` paroj:

* Ĉelaro
* Cifermasko

Poste venas `opCount` paroj:

* Indekso
* Cifermasko

Valoroj por `type`:

```
enum class EventType : uint8_t {
    None = 0,
    SetValue = 1,
    RemoveCandidate = 2
};
```

Valoroj por `reasonId` kaj `detailedReasonId` (per aldona komento oni markas valorojn aplikeblajn ankaŭ al `reasonId`):

```
enum class ReasonId : uint8_t {
    Solver = 0,
    // naked subsets
    FullHouse,              // <---
    NakedSingle,            // <---
    NakedPair,              // <---
    NakedTriple,            // <---
    NakedQuad,
    // hidden subsets
    HiddenSingle,           // <---
    HiddenPair,             // <---
    HiddenTriple,           // <---
    HiddenQuad,
    // intersections
    PointingSet,            // <---
    PointingPair,
    PointingTriple,
    BoxLineReduction,       // <---
    ClaimingPair,
    ClaimingTriple,
    // basic fish
    XWing,                  // <---
    Swordfish,              // <---
    Jellyfish,
    // finned and sashimi fish
    FinnedXWing,            // <---
    FinnedSwordfish,        // <---
    FinnedJellyfish,        // <---
    SashimiXWing,
    SashimiSwordfish,
    SashimiJellyfish,
    // advanced fish
    FrankenXWing,
    FrankenSwordfish,
    FrankenJellyfish,
    FinnedFrankenXWing,
    FinnedFrankenSwordfish,
    FinnedFrankenJellyfish,
    MutantXWing,
    MutantSwordfish,
    MutantJellyfish,
    FinnedMutantXWing,
    FinnedMutantSwordfish,
    FinnedMutantJellyfish,
    SiameseFish,
    KrakenFish,
    // single digit patterns
    SingleDigitPattern,     // <---
    Skyscraper,
    TwoStringKite,
    Crane,
    EmptyRectangle,         // <---
    // uniqueness
    UniqueRectangle,        // <---
    UniqueRectangleType1,
    UniqueRectangleType2,
    UniqueRectangleType3,
    UniqueRectangleType4,
    UniqueRectangleType5,
    UniqueRectangleType6,
    HiddenRectangle,        // <---
    AvoidableRectangle,
    BUGPlusOne,             // <---
    // wings
    XYWing,                 // <---
    XYZWing,                // <---
    WXYZWing,
    WWing,                  // <---
    // coloring
    SimpleColoring,         // <---
    SimpleColoringColorTrap,
    SimpleColoringColorWrap,
    _3DMedusa,              // <---
    _3DMedusaColorTrap,
    _3DMedusaColorWrap,
    _3DMedusaEmptiedCell,
    // chains
    RemotePair,             // <---
    XChain,                 // <---
    XRing,
    XYChain,                // <---
    XYRing,
    AIC,                    // <---
    AICType1,
    AICType2,
    AICType3,
    GroupedXChain,          // <---
    GroupedXRing,
    GroupedAIC,             // <---
    GroupedAICType1,
    GroupedAICType2,
    GroupedAICType3,
    // named wings and rings
    SWing,
    M2Wing,
    M3Wing,
    L1Wing,
    L2Wing,
    L3Wing,
    H1Wing,
    H2Wing,
    H3Wing,
    StrongWing,
    iWWing,
    DualWWing,
    iXYWing,
    iSWing,
    iM2Wing,
    iM3Wing,
    iL1Wing,
    iL2Wing,
    iL3Wing,
    iH1Wing,
    iH2Wing,
    iH3Wing,
    WRing,
    SRing,
    M2Ring,
    M3Ring,
    L1Ring,
    L2Ring,
    L3Ring,
    H1Ring,
    H2Ring,
    H3Ring,
    StrongRing,
    iWRing,
    DualWRing,
    iXYRing,
    iSRing,
    iM2Ring,
    iM3Ring,
    iL1Ring,
    iL2Ring,
    iL3Ring,
    iH1Ring,
    iH2Ring,
    iH3Ring,
    GroupedXYWing,
    GroupedWWing,
    GroupedSWing,
    GroupedM2Wing,
    GroupedM3Wing,
    GroupedL1Wing,
    GroupedL2Wing,
    GroupedL3Wing,
    GroupedH1Wing,
    GroupedH2Wing,
    GroupedH3Wing,
    GroupedStrongWing,
    GroupediWWing,
    GroupedDualWWing,
    GroupediXYWing,
    GroupediSWing,
    GroupediM2Wing,
    GroupediM3Wing,
    GroupediL1Wing,
    GroupediL2Wing,
    GroupediL3Wing,
    GroupediH1Wing,
    GroupediH2Wing,
    GroupediH3Wing,
    GroupedWRing,
    GroupedM2Ring,
    GroupedL1Ring,
    GroupedL2Ring,
    GroupedH2Ring,
    GroupedStrongRing,
    GroupediWRing,
    GroupediXYRing,
    GroupediSRing,
    GroupediM2Ring,
    GroupediM3Ring,
    GroupediL1Ring,
    GroupediL2Ring,
    GroupediL3Ring,
    GroupediH1Ring,
    GroupediH2Ring,
    GroupediH3Ring,
    // almost locked sets
    ALSXZ,                  // <---
    ALSXZSinglyLinked,
    ALSXZDoublyLinked,
    ALSXYWing,              // <---
    ALSXYRing,
    ALSChain,               // <---
    ALSRing,
    SueDeCoq,               // <---
    DeathBlossom,           // <---
    // forcing
    ForcingChain,           // <---
    DigitForcingChain,
    NishioForcingChain,
    CellForcingChain,
    UnitForcingChain,
    ForcingNet,             // <---
};
```

---

# Solver API

## sudorix_solver_count_solutions

```
int sudorix_solver_count_solutions(const char *in81)
```

Ricevas Sudokuon kiel ĉenon kaj liveras la nombron da solvoj.

| Liveraĵo | Signifo           |
| -------- | ----------------- |
| 0        | neniu solvo       |
| 1        | unu sola solvo    |
| >1       | pluraj solvoj     |
| -1       | eraro en la enigo |

---

## sudorix_solver_full

```
int sudorix_solver_full(const char *in81, char *out81)
```

Ricevas Sudokuon kiel ĉenon kaj solvas ĝin en unu paŝo.

---

## sudorix_solver_init_board

```
int sudorix_solver_init_board(const char *in81)
```

Ricevas Sudokuon kiel ĉenon kaj ŝargas ĝin en la interna stato de la solvilo.

---

## sudorix_solver_next_step

```
int sudorix_solver_next_step(uint32_t *out, uint32_t out_words)
```

Liveras unu solvopaŝon de la ŝargita Sudokuo kaj ĝisdatigas la internan staton.

---

## sudorix_solver_export_board

```
int sudorix_solver_export_board(uint8_t *values, uint16_t *cands)
```

Eksportas la staton de la ŝargita Sudokuo kun solvitaj ĉeloj kaj kandidatoj.
---

## sudorix_solver_hint

```
int sudorix_solver_hint(const uint8_t *values,
                        const uint16_t *cands,
                        uint32_t *out,
                        uint32_t out_words)
```

Ricevas Sudokuon kiel tabelojn enhavantajn kaj la jam solvitajn ĉelojn kaj la kandidatojn por ĉiu ĉelo, kaj liveras unu solvopaŝon **sen ĝisdatigo de la interna stato**.

---

## sudorix_solver_set_enabled_techniques

```
int sudorix_solver_set_enabled_techniques(const uint32_t *reasons, uint32_t count);
```

Ricevas liston de ŝaltendaj teknikoj por la solvado.

---

# Specifo de fontoj por teknikoj

Ĉi tiu sekcio difinas la entenon de la fontoj por ĉiu Sudoku-tekniko.

## Naked / Hidden Single

Neniu fonto.

---

## Bazaj teknikoj

La plej simplaj teknikoj necesas nur unu fonton:

* Ĉeloj kiuj konsistigas la aron + Ciferoj de la aro

Tio aplikiĝas al: **Naked Subsets**, **Hidden Subsets**, **Intersection Removal**.

---

## Fiŝoj

Kaze de fiŝoj, estas tiom da fontoj, kiom estas la bazaj aroj de la fiŝo:

* Ĉeloj de la unua baza aro + Cifero
* Ĉeloj de la dua baza aro + Cifero
* ...

Tio aplikiĝas al: **X-Wing** (du aroj), **Swordfish** (tri aroj), **Jellyfish** (kvar aroj).

Se la fiŝo enhavas naĝilojn, necesas aldoni pluajn fontojn:

* Disigilo
* Ĉeloj entenantaj la naĝilon + Cifero

Tio aplikiĝas al: **Finned X-Wing**, **Finned Swordfish**, **Finned Jellyfish**.

---

## Rektanguloj

La rektanguloj estas prezentitaj de du fontoj:

* Ĉeloj de la unua vico + Ciferoj
* Ĉeloj de la dua vico + Ciferoj

En kelkaj kazoj, pluaj fontoj estas prezentitaj:

* Disigilo
* Ĉeloj de la plua subaro + Ciferoj

Tio aplikiĝas al: **Unique Rectangle**, **Hidden Rectangle**, **Avoidable Rectangle**.

---

## Flugiloj

En flugiloj, la fontoj estas la sekvaj:

* Ĉelo de la unua flugilo + Ciferoj
* Ĉelo de la pivoto + Ciferoj
* Ĉelo de la dua flugilo + Ciferoj
* Disigilo
* Ĉelo de la unua flugilo + Cifero Z
* Ĉelo de la pivoto + Cifero Z (se aplikebla)
* Ĉelo de la dua flugilo + Cifero Z

Tio aplikiĝas al: **XY-Wing**, **XYZ-Wing**, **W-Wing**.

---

## Kolorigado

En kolorigaj teknikoj, la fontoj estas disigitaj en grupo laŭ koloro:

* Ĉelo en unua koloro + Cifero
* Alia ĉelo en unua koloro + Cifero
* ...
* Disigilo
* Ĉelo en dua koloro + Cifero
* Alia ĉelo en dua koloro + Cifero
* ...
* Disigilo
* Malplenigita ĉelo + Ciferoj (nur en 3D Medusa, se aplikebla)

Nur en Remote Pair la fontoj havas la du ciferojn entenantajn la foran duopon.

Tio aplikiĝas al: **Remote Pair**, **Simple Coloring**, **3D Medusa**.

---

## Ĉenoj

En ĉenoj, estas tiom da fontoj kiom estas la nodoj en la ĉeno. Ekzemple, kaze de ĉielskrapanto (la plej simpla ĉeno), estos kvar fontoj:

* Ĉelo de la unua nodo + Cifero
* Ĉelo de la dua nodo + Cifero
* Ĉelo de la tria nodo + Cifero
* Ĉelo de la kvara nodo + Cifero

Unu fonto povas enteni pli ol unu ĉelon se ĝi prezentas nodon kun grupigitaj ĉeloj.

Tio aplikiĝas al: **Skyscraper**, **Two-String Kite**, **Crane**, **Empty Rectangle**, **X-Chain**, **XY-Chain**, **Alternating Inference Chain**, **Grouped Alternating Inference Chain**.

Kaze de devigantaj ĉenoj, pli ol unu ĉeno aperos kun disigilo inter ili. La ĉeno ĉiam komenciĝas per vera hipotezo, escepte de ciferaj devigantaj ĉenoj: tie la unua ĉeno komenciĝas per vera hipotezo, kaj la dua per malvera hipotezo.

Tio aplikiĝas al: **Forcing Chain**.

---

## ALS

Ĉenoj konstruitaj per ALS-oj (Preskaŭ Blokitaj Aroj) havas tiujn ĉi fontojn:

* Ĉeloj de la unua aro + Ciferoj
* Ĉeloj de la dua aro + Ciferoj
* ...
* Disigilo
* Ĉeloj de la aro entenanta la unuan RCC-on + Cifero RCC
* Ĉeloj de la aro entenanta la duan RCC-on + Cifero RCC
* ...
* Disigilo
* Ĉeloj en la unua aro entenantaj ciferon Z + Cifero Z (se aplikebla)
* Ĉeloj en la lasta aro entenantaj ciferon Z + Cifero Z (se aplikebla)
* ... (ripetite se ekzistas pli ol unu cifero Z)

Tio aplikiĝas al: **ALS-XZ**, **ALS-XY-Wing**, **ALS Chain**.

---

## BUG+1

En BUG+1, la fontoj estas la sekvaj:

* Ĉelo kiu entenas la BUG-on + Cifero
* Ĉeloj samunuaj de la BUG-o + Cifero

---

## Sue-de-Coq

La Sue-de-Coq havas du aŭ tri fontojn:

* Ĉeloj de la linio + Ciferoj kiuj apartenas nur al la linio
* Ĉeloj de la bloko + Ciferoj kiuj apartenas nur al la bloko
* Ĉeloj de la intersekco + Ciferoj kiuj apartenas nur al la intersekco (se aplikebla)

---

## Death Blossom

En Death Blossom oni havas unu fonton por la tigo kaj unu fonton por ĉiu petalo:

* Ĉeloj de la tigo + Ciferoj
* Ĉeloj de la unua petalo + Ciferoj
* Ĉeloj de la dua petalo + Ciferoj
* ...

---

# Rimarko

La solvilo liveras nur **strukturitajn datumojn**.

La reta interfaco interpretas la fontojn por produkti legeblajn klarigojn kaj kolorigojn en la krado.
