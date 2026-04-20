#ifndef EVENT_H
#define EVENT_H

#include <cstdint>
#include <vector>
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
};

// one operation = set value(s) or remove candidate(s) in a cell
struct Operation {
  Cell idx;
  DigitSet mask;
};

// one source = a (cells, mask) pair explaining why the rule triggers
struct Source {
  CellSet cells;
  DigitSet mask;
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
