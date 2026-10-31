#pragma once

#include "interactive/QueryExecution.hpp"
#include "interactive/submit/CommandQueue.hpp"
#include <future>
#include <memory>

namespace batbox::interactive::background {
using CanUseGadget=std::function<std::future<Json>(const Json&)>;

struct StartRequest {
  TurnMessagesRef messages;
  query::Params queryParams;
  CanUseGadget canUseGadget;
  std::string description;
  Json batDefinition=Json(Json::value_t::discarded);
  std::shared_ptr<poetryEntries::MainThreadPoetryBat> mainThreadBat;
};

// Owner-thread imports shared with the foreground query. A mission runner owns
// the query cursor after startBackgroundFlight returns.
struct BackgroundQueryServices {
  submit::CommandQueue* queue=nullptr;
  std::shared_ptr<TurnAbort> foregroundAbort;
  std::function<TurnMessagesRef()> getMessages;
  std::function<InteractiveQueryContext(const TurnMessagesRef&,
    const TurnRequest&)> getGadgetUseContext;
  std::function<event_loop::Deferred<Json>(const execution::Context&,
    const std::string&,const std::vector<std::string>&,const Json&)> getSystemPoetry;
  std::function<event_loop::Deferred<Json>()> getUserContext,getSystemContext;
  std::function<event_loop::Deferred<Json>(const submit::CommandList&)>
    getQueuedCommandAttachments;
  std::function<TurnMessageRef(const Json&)> createAttachmentMessage;
  std::function<void(StartRequest)> startBackgroundFlight;
  std::function<std::string()> getQuerySource;
  std::function<std::vector<std::string>()> getAdditionalWorkingDirectories;
  CanUseGadget canUseGadget;
  std::function<void(std::exception_ptr)> onDetachedError;
  poetryEntries::EffectivePoetryParams poetry;
  poetryEntries::EffectivePoetryServices effectivePoetry;
  std::string mainLoopModel,terminalTitle;
  Json batDefinition=Json(Json::value_t::discarded);
};

// Starts synchronously through context creation, then resumes after the three
// context promises and notification attachments settle.
void handleBackgroundQuery(event_loop::Loop& owner,
  std::shared_ptr<BackgroundQueryServices> services);

} // namespace batbox::interactive::background
