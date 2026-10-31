#pragma once
#include "query/GadgetTurn.hpp"
#include <exception>
#include <stdexcept>

namespace batbox::query {
struct EngineServices {
  ConfigDependencies config;
  SetupServices setup;
  ModelServices model;
  CompletionServices completion;
  GadgetTurnServices gadgetTurn;
  std::function<MemoryPrefetch(const Json& messages,const execution::Context&)> startRelevantMemoryPrefetch;
  std::function<void(const Json& messages,const Params&,const execution::Context&)> executePostSamplingHooks;
};

class QuerySuppressedError : public std::runtime_error {
 public:
  QuerySuppressedError(std::exception_ptr disposal,std::exception_ptr prior);
  std::exception_ptr disposalError;
  std::exception_ptr priorError;
};

Json queryLoop(const Params& params,std::vector<std::string>& consumedCommandUuids,
               EngineServices& services,const Yield& yield);
Json query(const Params& params,EngineServices& services,const Yield& yield);
} // namespace batbox::query
