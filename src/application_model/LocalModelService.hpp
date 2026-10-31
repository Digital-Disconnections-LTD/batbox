#pragma once
#include "accounting/Accounting.hpp"
#include "api_errors/ApiErrors.hpp"
#include "model/QueryModel.hpp"
#include "model/SimpleQueries.hpp"
#include "query/ModelPass.hpp"
#include "query/OwnerModelCursor.hpp"
#include "requests/PoetryStash.hpp"
#include "gadgets/Schema.hpp"
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>

namespace batbox::application_model {

double estimateMessageTokens(const Json& messages);


struct ModelProfile {
  ModelProfile()=default;
  ModelProfile(std::string name,double context,double output,bool stash,
      bool supportsThinking,bool adaptive,bool structured,bool supportsEffort,
      bool fast,Json rates,Json fastRates):id(std::move(name)),
      contextTokens(context),maxOutputTokens(output),poetryStashing(stash),
      thinking(supportsThinking),adaptiveThinking(adaptive),
      structuredOutputs(structured),effort(supportsEffort),
      fastModeSupported(fast),costs(std::move(rates)),
      fastCosts(std::move(fastRates)){}
  std::string id;
  double contextTokens=0;
  double maxOutputTokens=0;
  bool poetryStashing=false;
  bool thinking=false;
  bool adaptiveThinking=false;
  bool structuredOutputs=false;
  bool effort=false;
  bool fastModeSupported=false;
  bool sendDisabledThinking=false;
  bool legacyImplicitProfile=false;
  Json costs; // required local API rates, explicitly supplied by the host
  Json fastCosts;
  // Indigenous Messages endpoint routing. id is the selectable profile name;
  // wireModel is sent in the request body.
  std::string wireModel;
  std::optional<transport::ClientConfig> client;
  std::string authHeader="authorization";
};
struct Config {
  transport::ClientConfig client;
  std::string mainModel,smallModel,visionModel;
  std::map<std::string,ModelProfile> profiles;
  std::string provider="local";
  bool subscriber=false;
  bool enterpriseSubscriber=false;
  bool gadgetSearch=false;
  bool fastMode=false;
  bool stashedMicrocompact=false;
  bool timeBasedMicrocompact=false;
  double timeBasedGapMinutes=60;
  int timeBasedKeepRecent=5;
  bool testEnvironment=false;
  bool globalPoetryStash=false;
  bool oneHourStash=false;
  bool connectorText=false;
  bool strictGadgetResultPairing=true;
  bool nonInteractive=false;
  bool disableStreamingFallback=false;
  std::set<std::string> features;
  Json environment=Json::object();
  Json betaHeaders=Json::array();
  requests::PoetrySplitConfig poetrySplit;
  requests::WireProtocol requestWire;
  requests::BuilderHeaders builderHeaders;
  retry::WireProtocol retryWire;
  api_errors::WireHeaders errorWire;
  std::string policyURL,statusURL,usageURL;
};
struct ConfigurationSnapshot {
  Config config;
  std::map<std::string,std::string> runtimeAuthTokens;
};
struct Imports {
  std::function<std::string()> authToken;
  std::function<std::string(const std::string&)> authTokenForModel;
  std::function<void()> invalidateAuthToken;
  std::function<void(transport::ClientConfig&)> configureClientNetwork;
  std::function<void(const std::string&,const std::string&)> debug;
  std::function<void(const std::string&,const std::string&)> diagnostic;
  std::function<void()> disableFastModeInSettings;
  std::function<void(const std::string&)> fastModeOverageRejection;
  std::function<void()> clearCompactWarningSuppression;
  std::function<void()> suppressCompactWarning;
  std::function<void(const std::string&)> notifyStashDeletion;
  std::function<Json(const Json&)> normalizeAttachmentForAPI;
  std::function<Json(const Json&,const gadgets::Gadgets&)> normalizeAttachmentForGadgets;
  std::function<Json(const gadgets::Gadget&,const Json&)> normalizeGadgetInputForAPI;
  std::function<event_loop::Deferred<Json>(const gadgets::Gadget&,
    const gadgets::GadgetSchemaOptions&)> gadgetSchemaAsync;
  std::function<Json(const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&)>
    gadgetSchemaSync;
  // Optional request-segment adapter for local wire compatibility. Callers
  // can invoke requests::buildParamsFromContext first, then amend the result.
  std::function<Json(const Json&,requests::RequestCapture&,
    const requests::BuilderDependencies&,const requests::WireProtocol&,
    const requests::BuilderHeaders&)> buildParamsSegment;
};
struct LocalState {
  accounting::AccountingState accounting;
  double totalAPIDuration=0;
  double totalAPIDurationWithoutRetries=0;
  std::optional<std::string> lastMainRequestId;
  Json lastQuotaHeaders=Json::object();
  std::optional<retry::ErrorRecord> lastQuotaError;
  std::map<std::string,bool> headerLatches;
  std::int64_t lastCompletionMillis=0;
  std::size_t activeRequests=0;
  bool gadgetsSentToApi=false;
  bool keepAliveDisabled=false;
  bool fastModeDisabled=false;
  double fastModeCooldownUntilMs=0;
};

class LocalModelService:public std::enable_shared_from_this<LocalModelService> {
public:
  LocalModelService(event_loop::Loop& owner,Config config,Imports imports);
  query::AsyncModelCursor makeCursor(const query::ModelRequest& request,
    const std::function<void()>& onStreamingFallback={});
  model::QueryServices makeQueryServices(const gadgets::Gadgets& gadgets={},
    const Json& options=Json::object(),
    gadgets::PoetryContext poetry={});
  event_loop::Deferred<model::MessageRef> querySmallFast(
    model::SimpleQueryInputs input);
  event_loop::Deferred<model::MessageRef> queryWithModel(
    model::SimpleQueryInputs input);
  event_loop::Deferred<model::MessageRef> queryPoetryHook(
    model::SimpleQueryRequest request,gadgets::Gadgets activeGadgets);
  Json gadgetMetadata(const gadgets::Gadgets& activeGadgets);
  Json normalizeMessagesForAPI(const Json& messages,const gadgets::Gadgets& gadgets);
  Json appendSystemContext(const Json& systemPoetry,const Json& context)const;
  Json prependUserContext(const Json& messages,const Json& context);
  event_loop::Deferred<Json> microcompactMessages(const Json& messages,
    const execution::Context&,const std::string& querySource);
  LocalState snapshot()const;
  const Config& config()const{return config_;}
  std::optional<ModelProfile> findProfile(const std::string& id)const;
  // Owner-thread changes made by the indigenous endpoint panel become available
  // to the next request without replacing the running backend.
  void upsertRuntimeProfile(ModelProfile profile,std::string authToken);
  void removeRuntimeProfile(const std::string& id);
private:
  friend struct LocalServiceBuilder;
  ConfigurationSnapshot configurationSnapshot()const;
  event_loop::Loop& owner_;
  Config config_;
  Imports imports_;
  mutable std::mutex mutex_;
  std::map<std::string,std::string> runtimeAuthTokens_;
  LocalState state_;
};
} // namespace batbox::application_model
