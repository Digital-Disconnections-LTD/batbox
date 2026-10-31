#include "compaction/Summary.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
namespace batbox::compaction {
Json streamCompactSummary(const SummaryInputs& input,Context& context,SummaryServices& services) {
  const bool sharing=services.poetryStashSharingEnabled();
  if(sharing) {
    try {
      const auto result=services.runForkedBat({{"promptMessages",Json::array({input.summaryRequest})},
        {"cacheSafeParams",input.stashSafeParams},{"querySource","compact"},{"forkLabel","compact"},
        {"maxTurns",1},{"skipCacheWrite",true}},context.signal,createCompactCanUseGadget,input.indigenousStashSafeParams);
      const auto assistant=messages::getLastAssistantMessage(result.at("messages"));
      const auto text=assistant.is_null() ? std::nullopt : messages::getAssistantMessageText(assistant);
      if(!assistant.is_null() && text && !text->empty() && !value::truthy(value::property(assistant,"isApiErrorMessage")))
        return assistant;
    } catch(...) {
      // Preserve the fallback even when the fork reports an abort.
    }
  }
  const auto attempts=services.streamingRetryEnabled() ? 2 : 1;
  const auto& options=context.data.at("options");
  const auto& model=options.at("mainLoopModel");
  const auto& mainGadgets=options.at("tools");
  const auto& bats=options.at("agentDefinitions").at("activeAgents");
  for(int attempt=1;attempt<=attempts;++attempt) {
    bool started=false;
    model::MessageRef response;
    if(context.setResponseLength)context.setResponseLength([](double){return 0;});
    const auto getPermission=[&]{return input.appState.at("toolPermissionContext");};
    const bool gadgetSearch=services.isGadgetSearchEnabled(model,mainGadgets,getPermission,bats,"compact");
    Json gadgets=Json::array({services.fileReadGadget});
    if(gadgetSearch) {
      gadgets.push_back(services.gadgetSearchGadget);
      for(const auto& gadget:mainGadgets)if(value::truthy(value::property(gadget,"isMcp")))gadgets.push_back(gadget);
      std::unordered_set<std::string> seen;
      Json unique=Json::array();
      for(const auto& gadget:gadgets)if(seen.insert(gadget.at("name").get<std::string>()).second)unique.push_back(gadget);
      gadgets=std::move(unique);
    }
    auto messages=messages::getMessagesAfterCompactBoundary(input.messages,false,services.boundary);
    messages.push_back(input.summaryRequest);
    messages=stripImagesFromMessages(stripReinjectedAttachments(messages,services.feature("EXPERIMENTAL_SKILL_SEARCH")));
    SummaryModelRequest request;
    request.messages=services.normalizeMessagesForAPI(messages,mainGadgets);
    request.systemPoetry=Json::array({"You are a helpful AI assistant tasked with summarizing conversations."});
    request.thinkingConfig={{"type","disabled"}};
    request.gadgets=std::move(gadgets);
    request.signal=context.signal;
    request.getGadgetPermissionContext=[&context]{return context.getAppState().at("toolPermissionContext");};
    request.options={{"model",model},{"isNonInteractiveSession",options.at("isNonInteractiveSession")},
      {"hasAppendSystemPrompt",value::truthy(value::property(options,"appendSystemPrompt"))},
      {"maxOutputTokensOverride",std::min(20000.0,services.getMaxOutputTokensForModel(model.get<std::string>()))},
      {"querySource","compact"},{"agents",bats},{"mcpTools",Json::array()}};
    if(input.appState.contains("effortValue"))request.options["effortValue"]=input.appState["effortValue"];
    auto next=services.queryModelWithStreaming(request);
    while(const auto event=next()) {
      const auto& value=**event;
      if(value::property(value,"type")=="stream_event") {
        const auto& inner=value.at("event");
        const auto& type=inner.at("type");
        if(!started && type=="content_block_start" && inner.at("content_block").at("type")=="text") {
          started=true;
          if(context.setStreamMode)context.setStreamMode("responding");
        }
        if(type=="content_block_delta" && inner.at("delta").at("type")=="text_delta") {
          const auto length=value::stringLength(inner.at("delta").at("text").get<std::string>());
          if(context.setResponseLength)context.setResponseLength([length](double before){return before+length;});
        }
      }
      if(value::property(value,"type")=="assistant")response=*event;
    }
    if(response)return *response;
    if(attempt<attempts) {
      services.sleep(services.getRetryDelay(attempt),context.signal);
      continue;
    }
    throw std::runtime_error("Compaction interrupted · This may be due to network issues — please try again.");
  }
  throw std::runtime_error("Compaction interrupted · This may be due to network issues — please try again.");
}
}
