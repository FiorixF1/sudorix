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
  PointingPair,
  PointingTriple,
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

  EventType type;
  ReasonId reason;

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
