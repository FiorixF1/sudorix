#ifndef EVENT_H
#define EVENT_H

#include <cstdint>
#include <vector>
#include <algorithm>
#include "types.hpp"

enum class EventType : uint8_t {
  None = 0,
  SetValue = 1,
  RemoveCandidate = 2
};

enum class ReasonId : uint8_t {
  Solver = 0,
  // naked subsets
  FullHouse,
  NakedSingle,
  NakedPair,
  NakedTriple,
  NakedQuad,
  // hidden subsets
  HiddenSingle,
  HiddenPair,
  HiddenTriple,
  HiddenQuad,
  // intersections
  PointingSet,
  PointingPair,
  PointingTriple,
  BoxLineReduction,
  ClaimingPair,
  ClaimingTriple,
  // basic fish
  XWing,
  Swordfish,
  Jellyfish,
  // finned and sashimi fish
  FinnedXWing,
  FinnedSwordfish,
  FinnedJellyfish,
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
  SingleDigitPattern,
  Skyscraper,
  TwoStringKite,
  Crane,
  EmptyRectangle,
  // uniqueness
  UniqueRectangle,
  UniqueRectangleType1,
  UniqueRectangleType2,
  UniqueRectangleType3,
  UniqueRectangleType4,
  UniqueRectangleType5,
  UniqueRectangleType6,
  HiddenRectangle,
  AvoidableRectangle,
  BUGPlusOne,
  // wings
  XYWing,
  XYZWing,
  WXYZWing,
  WWing,
  // coloring
  SimpleColoring,
  SimpleColoringColorTrap,
  SimpleColoringColorWrap,
  _3DMedusa,
  _3DMedusaColorTrap,
  _3DMedusaColorWrap,
  _3DMedusaEmptiedCell,
  // chains
  RemotePair,
  XChain,
  XRing,
  XYChain,
  XYRing,
  AIC,
  AICType1,
  AICType2,
  AICType3,
  GroupedXChain,
  GroupedXRing,
  GroupedAIC,
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
  ALSXZ,
  ALSXZSinglyLinked,
  ALSXZDoublyLinked,
  ALSXYWing,
  ALSXYRing,
  ALSChain,
  ALSRing,
  SueDeCoq,
  DeathBlossom,
  // forcing
  ForcingChain,
  DigitForcingChain,
  NishioForcingChain,
  CellForcingChain,
  UnitForcingChain,
  ForcingNet,
  // exotic patterns
  Fireworks,
  TripleFireworks,
  QuadrupleFireworks,
};

inline const char *reasonIdToString(ReasonId reason) {
  switch (reason) {
    case ReasonId::Solver: return "Solver";
    case ReasonId::FullHouse: return "Full House";
    case ReasonId::NakedSingle: return "Naked Single";
    case ReasonId::NakedPair: return "Naked Pair";
    case ReasonId::NakedTriple: return "Naked Triple";
    case ReasonId::NakedQuad: return "Naked Quad";
    case ReasonId::HiddenSingle: return "Hidden Single";
    case ReasonId::HiddenPair: return "Hidden Pair";
    case ReasonId::HiddenTriple: return "Hidden Triple";
    case ReasonId::HiddenQuad: return "Hidden Quad";
    case ReasonId::PointingSet: return "Pointing Set";
    case ReasonId::PointingPair: return "Pointing Pair";
    case ReasonId::PointingTriple: return "Pointing Triple";
    case ReasonId::BoxLineReduction: return "Box/Line Reduction";
    case ReasonId::ClaimingPair: return "Claiming Pair";
    case ReasonId::ClaimingTriple: return "Claiming Triple";
    case ReasonId::XWing: return "X-Wing";
    case ReasonId::Swordfish: return "Swordfish";
    case ReasonId::Jellyfish: return "Jellyfish";
    case ReasonId::FinnedXWing: return "Finned X-Wing";
    case ReasonId::FinnedSwordfish: return "Finned Swordfish";
    case ReasonId::FinnedJellyfish: return "Finned Jellyfish";
    case ReasonId::SashimiXWing: return "Sashimi X-Wing";
    case ReasonId::SashimiSwordfish: return "Sashimi Swordfish";
    case ReasonId::SashimiJellyfish: return "Sashimi Jellyfish";
    case ReasonId::FrankenXWing: return "Franken X-Wing";
    case ReasonId::FrankenSwordfish: return "Franken Swordfish";
    case ReasonId::FrankenJellyfish: return "Franken Jellyfish";
    case ReasonId::FinnedFrankenXWing: return "Finned Franken X-Wing";
    case ReasonId::FinnedFrankenSwordfish: return "Finned Franken Swordfish";
    case ReasonId::FinnedFrankenJellyfish: return "Finned Franken Jellyfish";
    case ReasonId::MutantXWing: return "Mutant X-Wing";
    case ReasonId::MutantSwordfish: return "Mutant Swordfish";
    case ReasonId::MutantJellyfish: return "Mutant Jellyfish";
    case ReasonId::FinnedMutantXWing: return "Finned Mutant X-Wing";
    case ReasonId::FinnedMutantSwordfish: return "Finned Mutant Swordfish";
    case ReasonId::FinnedMutantJellyfish: return "Finned Mutant Jellyfish";
    case ReasonId::SiameseFish: return "Siamese Fish";
    case ReasonId::KrakenFish: return "Kraken Fish";
    case ReasonId::SingleDigitPattern: return "Single Digit Pattern";
    case ReasonId::Skyscraper: return "Skyscraper";
    case ReasonId::TwoStringKite: return "Two-String Kite";
    case ReasonId::Crane: return "Crane";
    case ReasonId::EmptyRectangle: return "Empty Rectangle";
    case ReasonId::UniqueRectangle: return "Unique Rectangle";
    case ReasonId::UniqueRectangleType1: return "Unique Rectangle (Type 1)";
    case ReasonId::UniqueRectangleType2: return "Unique Rectangle (Type 2)";
    case ReasonId::UniqueRectangleType3: return "Unique Rectangle (Type 3)";
    case ReasonId::UniqueRectangleType4: return "Unique Rectangle (Type 4)";
    case ReasonId::UniqueRectangleType5: return "Unique Rectangle (Type 5)";
    case ReasonId::UniqueRectangleType6: return "Unique Rectangle (Type 6)";
    case ReasonId::HiddenRectangle: return "Hidden Rectangle";
    case ReasonId::AvoidableRectangle: return "Avoidable Rectangle";
    case ReasonId::BUGPlusOne: return "BUG+1";
    case ReasonId::XYWing: return "XY-Wing";
    case ReasonId::XYZWing: return "XYZ-Wing";
    case ReasonId::WXYZWing: return "WXYZ-Wing";
    case ReasonId::WWing: return "W-Wing";
    case ReasonId::SimpleColoring: return "Simple Coloring";
    case ReasonId::SimpleColoringColorTrap: return "Simple Coloring (Color Trap)";
    case ReasonId::SimpleColoringColorWrap: return "Simple Coloring (Color Wrap)";
    case ReasonId::_3DMedusa: return "3D Medusa";
    case ReasonId::_3DMedusaColorTrap: return "3D Medusa (Color Trap)";
    case ReasonId::_3DMedusaColorWrap: return "3D Medusa (Color Wrap)";
    case ReasonId::_3DMedusaEmptiedCell: return "3D Medusa (Emptied Cell)";
    case ReasonId::RemotePair: return "Remote Pair";
    case ReasonId::XChain: return "X-Chain";
    case ReasonId::XRing: return "X-Ring";
    case ReasonId::XYChain: return "XY-Chain";
    case ReasonId::XYRing: return "XY-Ring";
    case ReasonId::AIC: return "AIC";
    case ReasonId::AICType1: return "AIC (Type 1)";
    case ReasonId::AICType2: return "AIC (Type 2)";
    case ReasonId::AICType3: return "AIC (Type 3)";
    case ReasonId::GroupedXChain: return "Grouped X-Chain";
    case ReasonId::GroupedXRing: return "Grouped X-Ring";
    case ReasonId::GroupedAIC: return "Grouped AIC";
    case ReasonId::GroupedAICType1: return "Grouped AIC (Type 1)";
    case ReasonId::GroupedAICType2: return "Grouped AIC (Type 2)";
    case ReasonId::GroupedAICType3: return "Grouped AIC (Type 3)";
    case ReasonId::SWing: return "S-Wing";
    case ReasonId::M2Wing: return "M(2)-Wing";
    case ReasonId::M3Wing: return "M(3)-Wing";
    case ReasonId::L1Wing: return "L(1)-Wing";
    case ReasonId::L2Wing: return "L(2)-Wing";
    case ReasonId::L3Wing: return "L(3)-Wing";
    case ReasonId::H1Wing: return "H(1)-Wing";
    case ReasonId::H2Wing: return "H(2)-Wing";
    case ReasonId::H3Wing: return "H(3)-Wing";
    case ReasonId::StrongWing: return "Strong Wing";
    case ReasonId::iWWing: return "Inverted W-Wing";
    case ReasonId::DualWWing: return "Dual W-Wing";
    case ReasonId::iXYWing: return "Inverted XY-Wing";
    case ReasonId::iSWing: return "Inverted S-Wing";
    case ReasonId::iM2Wing: return "Inverted M(2)-Wing";
    case ReasonId::iM3Wing: return "Inverted M(3)-Wing";
    case ReasonId::iL1Wing: return "Inverted L(1)-Wing";
    case ReasonId::iL2Wing: return "Inverted L(2)-Wing";
    case ReasonId::iL3Wing: return "Inverted L(3)-Wing";
    case ReasonId::iH1Wing: return "Inverted H(1)-Wing";
    case ReasonId::iH2Wing: return "Inverted H(2)-Wing";
    case ReasonId::iH3Wing: return "Inverted H(3)-Wing";
    case ReasonId::WRing: return "W-Ring";
    case ReasonId::SRing: return "S-Ring";
    case ReasonId::M2Ring: return "M(2)-Ring";
    case ReasonId::M3Ring: return "M(3)-Ring";
    case ReasonId::L1Ring: return "L(1)-Ring";
    case ReasonId::L2Ring: return "L(2)-Ring";
    case ReasonId::L3Ring: return "L(3)-Ring";
    case ReasonId::H1Ring: return "H(1)-Ring";
    case ReasonId::H2Ring: return "H(2)-Ring";
    case ReasonId::H3Ring: return "H(3)-Ring";
    case ReasonId::StrongRing: return "Inverted Strong Ring";
    case ReasonId::iWRing: return "Inverted W-Ring";
    case ReasonId::DualWRing: return "Inverted Dual W-Ring";
    case ReasonId::iXYRing: return "Inverted XY-Ring";
    case ReasonId::iSRing: return "Inverted S-Ring";
    case ReasonId::iM2Ring: return "Inverted M(2)-Ring";
    case ReasonId::iM3Ring: return "Inverted M(3)-Ring";
    case ReasonId::iL1Ring: return "Inverted L(1)-Ring";
    case ReasonId::iL2Ring: return "Inverted L(2)-Ring";
    case ReasonId::iL3Ring: return "Inverted L(3)-Ring";
    case ReasonId::iH1Ring: return "Inverted H(1)-Ring";
    case ReasonId::iH2Ring: return "Inverted H(2)-Ring";
    case ReasonId::iH3Ring: return "Inverted H(3)-Ring";
    case ReasonId::GroupedXYWing: return "Grouped XY-Wing";
    case ReasonId::GroupedWWing: return "Grouped W-Wing";
    case ReasonId::GroupedSWing: return "Grouped S-Wing";
    case ReasonId::GroupedM2Wing: return "Grouped M(2)-Wing";
    case ReasonId::GroupedM3Wing: return "Grouped M(3)-Wing";
    case ReasonId::GroupedL1Wing: return "Grouped L(1)-Wing";
    case ReasonId::GroupedL2Wing: return "Grouped L(2)-Wing";
    case ReasonId::GroupedL3Wing: return "Grouped L(3)-Wing";
    case ReasonId::GroupedH1Wing: return "Grouped H(1)-Wing";
    case ReasonId::GroupedH2Wing: return "Grouped H(2)-Wing";
    case ReasonId::GroupedH3Wing: return "Grouped H(3)-Wing";
    case ReasonId::GroupedStrongWing: return "Grouped Strong Wing";
    case ReasonId::GroupediWWing: return "Grouped Inverted W-Wing";
    case ReasonId::GroupedDualWWing: return "Grouped Dual W-Wing";
    case ReasonId::GroupediXYWing: return "Grouped Inverted XY-Wing";
    case ReasonId::GroupediSWing: return "Grouped Inverted S-Wing";
    case ReasonId::GroupediM2Wing: return "Grouped Inverted M(2)-Wing";
    case ReasonId::GroupediM3Wing: return "Grouped Inverted M(3)-Wing";
    case ReasonId::GroupediL1Wing: return "Grouped Inverted L(1)-Wing";
    case ReasonId::GroupediL2Wing: return "Grouped Inverted L(2)-Wing";
    case ReasonId::GroupediL3Wing: return "Grouped Inverted L(3)-Wing";
    case ReasonId::GroupediH1Wing: return "Grouped Inverted H(1)-Wing";
    case ReasonId::GroupediH2Wing: return "Grouped Inverted H(2)-Wing";
    case ReasonId::GroupediH3Wing: return "Grouped Inverted H(3)-Wing";
    case ReasonId::GroupedWRing: return "Grouped W-Ring";
    case ReasonId::GroupedM2Ring: return "Grouped M(2)-Ring";
    case ReasonId::GroupedL1Ring: return "Grouped L(1)-Ring";
    case ReasonId::GroupedL2Ring: return "Grouped L(2)-Ring";
    case ReasonId::GroupedH2Ring: return "Grouped H(2)-Ring";
    case ReasonId::GroupedStrongRing: return "Grouped Inverted Strong Ring";
    case ReasonId::GroupediWRing: return "Grouped Inverted W-Ring";
    case ReasonId::GroupediXYRing: return "Grouped Inverted XY-Ring";
    case ReasonId::GroupediSRing: return "Grouped Inverted S-Ring";
    case ReasonId::GroupediM2Ring: return "Grouped Inverted M(2)-Ring";
    case ReasonId::GroupediM3Ring: return "Grouped Inverted M(3)-Ring";
    case ReasonId::GroupediL1Ring: return "Grouped Inverted L(1)-Ring";
    case ReasonId::GroupediL2Ring: return "Grouped Inverted L(2)-Ring";
    case ReasonId::GroupediL3Ring: return "Grouped Inverted L(3)-Ring";
    case ReasonId::GroupediH1Ring: return "Grouped Inverted H(1)-Ring";
    case ReasonId::GroupediH2Ring: return "Grouped Inverted H(2)-Ring";
    case ReasonId::GroupediH3Ring: return "Grouped Inverted H(3)-Ring";
    case ReasonId::ALSXZ: return "ALS-XZ";
    case ReasonId::ALSXZSinglyLinked: return "ALS-XZ Singly Linked";
    case ReasonId::ALSXZDoublyLinked: return "ALS-XZ Doubly Linked";
    case ReasonId::ALSXYWing: return "ALS-XY-Wing";
    case ReasonId::ALSXYRing: return "ALS-XY-Ring";
    case ReasonId::ALSChain: return "ALS Chain";
    case ReasonId::ALSRing: return "ALS Ring";
    case ReasonId::SueDeCoq: return "Sue-De-Coq";
    case ReasonId::DeathBlossom: return "Death Blossom";
    case ReasonId::ForcingChain: return "Forcing Chain";
    case ReasonId::DigitForcingChain: return "Digit Forcing Chain";
    case ReasonId::NishioForcingChain: return "Nishio Forcing Chain";
    case ReasonId::CellForcingChain: return "Cell Forcing Chain";
    case ReasonId::UnitForcingChain: return "Unit Forcing Chain";
    case ReasonId::ForcingNet: return "Forcing Net";
    case ReasonId::Fireworks: return "Fireworks";
    case ReasonId::TripleFireworks: return "Triple Fireworks";
    case ReasonId::QuadrupleFireworks: return "Quadruple Fireworks";
  }
  return "Unknown Reason";
}

// one operation = set value(s) or remove candidate(s) in a cell
struct Operation {
  Cell idx;
  DigitSet mask;

  bool operator==(const Operation &other) const {
    return idx == other.idx && mask == other.mask;
  }
};

// one source = a (cells, mask) pair explaining why the rule triggers
struct Source {
  CellSet cells;
  DigitSet mask;

  bool operator==(const Source &other) const {
    return cells == other.cells && mask == other.mask;
  }
};

class Event
{
public:
  Event();
  Event(EventType type, ReasonId reason);
  Event(EventType type, ReasonId reason, ReasonId detailedReason);

  EventType type;
  ReasonId reason;
  ReasonId detailedReason;

  std::vector<Operation> &getOperations();
  size_t getNumberOfOperations() const;
  void addOperation(Cell idx, Digit digit);
  void addOperation(Cell idx, DigitSet mask);

  std::vector<Source> &getSources();
  size_t getNumberOfSources() const;
  void addSource(Cell idx, Digit digit);
  void addSource(Cell idx, DigitSet mask);
  void addSource(CellSet cells, Digit digit);
  void addSource(CellSet cells, DigitSet mask);
  void addDelimiter();

private:
  friend class EventQueue;
  // an event is a set of multiple operations
  std::vector<Operation> ops;
  // an event can be described by more sources
  std::vector<Source> sources;
};

#endif // EVENT_H
