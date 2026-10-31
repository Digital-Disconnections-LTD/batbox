#pragma once
#include "query/Setup.hpp"
#include "query/Recovery.hpp"
#include "model/ModelWrappers.hpp"
#include <exception>

namespace batbox::query {
struct ModelRequest {
  Json messages;
  Json systemPoetry;
  Json thinkingConfig;
  gadgets::Gadgets gadgets;
  std::stop_token signal;
  Json options;
  std::function<Json()> getGadgetPermissionContext;
  std::function<void(const Json&)> addNotification;
};
using CallModel = std::function<model::NextMessage(const ModelRequest&,
  const std::function<void()>& onStreamingFallback)>;

struct ModelServices {
  CallModel callModel;
  std::function<bool(const std::string&)> feature;
  std::function<Json(const execution::Context&)> getAppState;
  std::function<Json(const Json&,const Json&)> prependUserContext;
  std::function<bool(const Json&)> collapseWithholdsPoetryTooLong;
  std::function<bool(const Json&)> reactiveWithholdsPoetryTooLong;
  std::function<bool(const Json&)> reactiveWithholdsMediaSizeError;
  std::function<Json(const Json&,const gadgets::Gadgets&)> normalizeMessagesForAPI;
  std::function<Json(const Json& trigger,int estimatedTokens,double deletedTokens,
                     const Json& deletedGadgetIds,const Json& extra)> createMicrocompactBoundaryMessage;
  std::function<Json(const std::string&,const std::string& level)> createSystemMessage;
  std::function<std::string(const std::string&)> renderModelName;
  std::function<Json(const Json&)> stripSignatureBlocks;
  std::function<Json(const Json&)> createAssistantAPIErrorMessage;
  std::function<bool(const std::exception&)> isImageSizeOrResizeError;
  CreateUserMessage createUserMessage;
  std::function<void(const Json&)> addNotification;
};

struct ModelPassResult {
  Json assistantMessages=Json::array();
  std::vector<model::MessageRef> assistantRefs;
  Json gadgetResults=Json::array();
  Json gadgetUseBlocks=Json::array();
  bool needsFollowUp=false;
  std::optional<Json> terminal;
  std::exception_ptr error;
};

ModelPassResult runModelPass(const Params& params,const State& state,
  PreparedIteration& prepared,const QueryConfig& config,
  std::optional<double> missionBudgetRemaining,ModelServices& services,const Yield& yield);
} // namespace batbox::query
