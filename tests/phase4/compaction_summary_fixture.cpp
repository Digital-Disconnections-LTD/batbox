#include "compaction/Summary.hpp"
#include <memory>
#include <stdexcept>
using namespace batbox;
using namespace batbox::compaction;
Json runCompactionSummaryFixture(const Json& row,Json& trace) {
  Context context;
  context.data={{"options",{{"tools",row.value("tools",Json::array())},{"mainLoopModel","local-model"},
    {"isNonInteractiveSession",true},{"appendSystemPrompt",row.value("append","")},
    {"agentDefinitions",{{"activeAgents",Json::array()}}}}}};
  double length=0;
  context.setResponseLength=[&](const auto& change){length=change(length);trace.push_back(Json::array({"length",length}));};
  context.setStreamMode=[&](const std::string& value){trace.push_back(Json::array({"mode",value}));};
  context.getAppState=[&]{trace.push_back("appState");return Json{{"toolPermissionContext",{{"mode","default"}}}};};
  SummaryInputs input{row.value("messages",Json::array()),{{"type","user"},{"message",{{"content","summarize"}}}},
    {{"toolPermissionContext",{{"mode","default"}}},{"effortValue","low"}},row.value("cache",Json::object()),123};
  SummaryServices services;
  services.poetryStashSharingEnabled=[&]{return row.value("sharing",true);};
  services.streamingRetryEnabled=[&]{return row.value("retry",false);};
  services.runForkedBat=[&](const Json& params,std::stop_token signal,const auto& canUse,const auto&){
    trace.push_back(Json::array({"fork",params,canUse(),signal.stop_requested()}));
    if(row.value("forkThrows",false))throw std::runtime_error("fork failed");
    return Json{{"messages",row.value("forkMessages",Json::array())}};
  };
  services.isGadgetSearchEnabled=[&](const Json& model,const Json& gadgets,const auto& permission,const Json& bats,const std::string& source){
    trace.push_back(Json::array({"toolSearch",model,gadgets,permission(),bats,source}));return row.value("toolSearch",false);
  };
  services.fileReadGadget={{"name","Read"}};services.gadgetSearchGadget={{"name","ToolSearch"}};
  services.feature=[&](const std::string& name){return value::property(row.value("features",Json::object()),name)==true;};
  services.boundary.feature=services.feature;
  services.boundary.projectSnippedView=[](const Json& messages){Json result=Json::array();for(const auto& m:messages)if(!m.value("snipped",false))result.push_back(m);return result;};
  services.normalizeMessagesForAPI=[&](const Json& messages,const Json& gadgets){trace.push_back(Json::array({"normalize",messages,gadgets}));return messages;};
  services.getMaxOutputTokensForModel=[&](const std::string& model){trace.push_back(Json::array({"maxOutput",model}));return row.value("maxOutput",50000.0);};
  std::size_t attempt=0;
  services.queryModelWithStreaming=[&](const SummaryModelRequest& request) -> model::NextMessage {
    trace.push_back(Json::array({"model",{{"messages",request.messages},{"systemPrompt",request.systemPoetry},
      {"thinkingConfig",request.thinkingConfig},{"tools",request.gadgets},{"options",request.options}},request.getGadgetPermissionContext()}));
    const auto streams=row.value("streams",Json::array({Json::array()}));
    auto events=std::make_shared<std::vector<model::MessageRef>>();
    for(const auto& event:streams[std::min(attempt++,streams.size()-1)])events->push_back(std::make_shared<Json>(event));
    return [&,events,index=std::size_t{0}]()mutable -> std::optional<model::MessageRef>{
      if(index<events->size())return (*events)[index++];
      if(row.value("mutateAfterYield",false))for(const auto& event:*events)
        if(value::property(*event,"type")=="assistant")(*event)["message"]["usage"]={{"output_tokens",77}};
      if(row.value("streamThrows",false))throw std::runtime_error("stream failed");
      return std::nullopt;
    };
  };
  services.getRetryDelay=[&](double attempt){trace.push_back(Json::array({"delay",attempt}));return 25.0;};
  services.sleep=[&](double delay,std::stop_token signal){trace.push_back(Json::array({"sleep",delay,signal.stop_requested()}));};
  return streamCompactSummary(input,context,services);
}
