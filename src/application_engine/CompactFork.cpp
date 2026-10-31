#include "application_engine/CompactFork.hpp"
#include "application/Async.hpp"
#include "application_bats/IndigenousQueryBridge.hpp"
#include "compaction/Workflow.hpp"
#include "services/api/batbox.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace batbox::application_engine {
namespace {
std::string createBatId(const std::shared_ptr<messages::Services>& messages,
    const std::optional<std::string>& label){
  std::string suffix;
  for(const char c:messages->randomUUID())
    if(std::isxdigit(static_cast<unsigned char>(c))){
      suffix.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
      if(suffix.size()==16)break;
    }
  if(suffix.size()!=16)throw std::runtime_error("UUID provider returned an invalid agent ID seed");
  return "a"+(label?*label+"-":std::string())+suffix;
}
bats::ContextServices makeContextServices(
    const std::shared_ptr<messages::Services>& messages){
  bats::ContextServices services;
  services.cloneFileStateStash=[](const Json& stash){return stash;};
  services.clearFileStateStash=[](Json& stash){
    if(stash.is_object()||stash.is_array())stash.clear();
  };
  services.cloneContentReplacementState=[](const Json& state){return state;};
  services.createDenialTrackingState=[](){return Json::object();};
  services.createBatId=[messages](const std::optional<std::string>& label){
    return createBatId(messages,label);
  };
  services.randomUUID=messages->randomUUID;
  return services;
}
void clearFileState(bats::BatContext& context,bats::ContextServices& services){
  if(context.query.readFileState){context.query.readFileState->clear();return;}
  Json& fileState=context.query.data["readFileState"];
  services.clearFileStateStash(fileState);
}
Json concatMessages(const Json& prefix,const Json& appended){
  if(!prefix.is_array()||!appended.is_array())
    throw std::invalid_argument("Compact fork messages must be arrays");
  Json result=prefix;
  for(const auto& message:appended)result.push_back(message);
  return result;
}
std::optional<std::string> uuidOf(const Json& message){
  const Json& value=value::property(message,"uuid");
  if(value.is_string())return value.get<std::string>();
  return std::nullopt;
}
void recordBat(const std::shared_ptr<application_flights::Flight>& flight,
    event_loop::Loop::Dispatcher owner,const std::string& bat,
    const Json& values,const std::optional<std::string>& parent){
  std::vector<flights::MessageRef> refs;
  if(values.is_array()){
    refs.reserve(values.size());
    for(const auto& value:values)refs.push_back(std::make_shared<Json>(value));
  }
  (void)application::awaitOnOwner<event_loop::Unit>(owner,
    [flight,bat,refs=std::move(refs),parent]{
      return flight->recordBat(bat,refs,parent);
    });
}
Json runCompactFork(const Json& request,std::stop_token signal,
    const std::shared_ptr<bats::StashSafeParams>& stash,
    const std::shared_ptr<application_bats::IndigenousQueryBridge>& bridge,
    const std::shared_ptr<application_flights::Flight>& flight,
    event_loop::Loop::Dispatcher owner,
    const std::shared_ptr<messages::Services>& messages){
  if(!stash)throw std::invalid_argument("Compact fork needs typed cache-safe context");
  if(!request.is_object()||value::property(request,"querySource")!="compact"||
      value::property(request,"maxTurns")!=1||
      value::property(request,"skipCacheWrite")!=true)
    throw std::invalid_argument("Compact fork must be one cache-safe summary turn");
  auto contextServices=makeContextServices(messages);
  bats::SubagentOverrides overrides;
  overrides.abortToken=signal;
  // The hook resolver guard forces even hook-allowed calls through
  // this fork's deny-all canUseGadget callback before Gadget.call.
  overrides.requireCanUseGadget=true;
  bats::BatContext isolated=bats::createSubagentContext(
    stash->gadgetUseContext,overrides,contextServices);
  Json initialMessages=concatMessages(stash->forkContextMessages,
    request.at("promptMessages"));
  const auto transcriptId=createBatId(messages,std::string("compact"));
  std::optional<std::string> lastUuid;
  try{
    try{recordBat(flight,owner,transcriptId,initialMessages,std::nullopt);}
    catch(...){/* Transcript errors do not fail the summary query. */}
    if(!initialMessages.empty())lastUuid=uuidOf(initialMessages.back());
    query::Params params;
    params.messages=initialMessages;
    params.systemPoetry=stash->systemPoetry;
    params.userContext=stash->userContext;
    params.systemContext=stash->systemContext;
    params.gadgetUseContext=isolated.query;
    params.querySource="compact";
    params.maxTurns=1;
    params.skipStashWrite=true;
    params.indigenousBatContext=std::make_shared<bats::BatContext>(isolated);
    // Do not override max output tokens: that changes thinking budget and
    // invalidates the original model request's stashed prefix.
    const application_bats::IndigenousQueryBridge::CanUseGadget deny=[](const Json&){
      std::promise<Json> result;auto future=result.get_future();
      result.set_value(compaction::createCompactCanUseGadget());
      return future;
    };
    Json output=Json::array();
    Json usage=EMPTY_USAGE;
    std::vector<std::shared_ptr<Json>> refs;
    const query::Yield consume=query::Yield::withReferences(
      [&](const query::MessageRef& ref){
        const Json& value=*ref;
        const Json& type=value::property(value,"type");
        if(type=="stream_event"){
          const Json& event=value::property(value,"event");
          if(value::property(event,"type")=="message_delta"){
            const Json& part=value::property(event,"usage");
            if(value::truthy(part))usage=accumulateUsage(usage,updateUsage(EMPTY_USAGE,part));
          }
          return;
        }
        if(type=="stream_request_start")return;
        output.push_back(value);
        refs.push_back(ref);
        if(type=="assistant"||type=="user"||type=="progress"){
          try{recordBat(flight,owner,transcriptId,Json::array({value}),lastUuid);}
          catch(...){/* Transcript errors do not fail the summary query. */}
          if(type!="progress")lastUuid=uuidOf(value);
        }
      });
    try{bridge->run(params,isolated,deny,consume);}
    catch(...){
      for(std::size_t i=0;i<refs.size();++i)output[i]=*refs[i];
      throw;
    }
    for(std::size_t i=0;i<refs.size();++i)output[i]=*refs[i];
    clearFileState(isolated,contextServices);
    initialMessages.clear();
    params.messages.clear();
    return {{"messages",std::move(output)},{"totalUsage",std::move(usage)}};
  }catch(...){
    clearFileState(isolated,contextServices);
    initialMessages.clear();
    throw;
  }
}
} // namespace

void bindCompactSummaryFork(compaction::SummaryServices& summary,
    CompactForkInputs inputs){
  if(!inputs.messages||!inputs.messages->randomUUID||!inputs.flight||
      !inputs.currentEngine||!inputs.notifyCommandLifecycle)
    throw std::invalid_argument("Compact fork owner, session, engine, and lifecycle required");
  const auto owner=inputs.owner.dispatcher();
  auto bridge=std::make_shared<application_bats::IndigenousQueryBridge>(
    inputs.owner,
    [owner,current=std::move(inputs.currentEngine)](
        const bats::BatContext& context,
        const application_bats::IndigenousQueryBridge::CanUseGadget& canUse){
      auto root=application::onOwnerAndWait<std::shared_ptr<Engine>>(owner,current);
      if(!root)throw std::logic_error("Root engine closed during compact fork");
      auto child=root->forBat(context,canUse);
      return application_bats::IndigenousQueryBinding{child->services(),child};
    },std::move(inputs.notifyCommandLifecycle));
  const auto enabled=inputs.poetryStashSharingEnabled;
  summary.poetryStashSharingEnabled=[enabled]{return enabled;};
  summary.runForkedBat=[bridge,flight=std::move(inputs.flight),
      messages=std::move(inputs.messages),owner](const Json& request,
      std::stop_token signal,const std::function<Json()>& getAppState,
      const std::shared_ptr<bats::StashSafeParams>& stash){
    (void)getAppState;
    return runCompactFork(request,signal,stash,bridge,flight,owner,messages);
  };
}
} // namespace batbox::application_engine
