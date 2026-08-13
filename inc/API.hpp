#ifndef API_H
#define API_H

#include "nlohmann/json.hpp"

using json = nlohmann::json;

enum class ApiCommand {
  CountSolutions,
  FullSolve,
  InitBoard,
  NextStep,
  ExportBoard,
  Hint,
  AllPossibleSteps,
  SetEnabledTechniques
};

NLOHMANN_JSON_SERIALIZE_ENUM_STRICT( ApiCommand, {
  {ApiCommand::CountSolutions, "countSolutions"},
  {ApiCommand::FullSolve, "fullSolve"},
  {ApiCommand::InitBoard, "initBoard"},
  {ApiCommand::NextStep, "nextStep"},
  {ApiCommand::ExportBoard, "exportBoard"},
  {ApiCommand::Hint, "hint"},
  {ApiCommand::AllPossibleSteps, "allPossibleSteps"},
  {ApiCommand::SetEnabledTechniques, "setEnabledTechniques"},
})

enum class ApiError {
  InvalidCommand,
  InvalidRequest,
  InvalidPuzzle,
  InvalidBoard,
  InvalidTechnique,
  NoStep,
  InternalError
};

NLOHMANN_JSON_SERIALIZE_ENUM_STRICT( ApiError, {
  {ApiError::InvalidCommand, "INVALID_COMMAND"},
  {ApiError::InvalidRequest, "INVALID_REQUEST"},
  {ApiError::InvalidPuzzle, "INVALID_PUZZLE"},
  {ApiError::InvalidBoard, "INVALID_BOARD"},
  {ApiError::InvalidTechnique, "INVALID_TECHNIQUE"},
  {ApiError::NoStep, "NO_STEP"},
  {ApiError::InternalError, "INTERNAL_ERROR"},
})

json api_ok();
json api_error(ApiError code, const std::string &message);

#endif // API_H
