#include "application_memory/Provider.hpp"
#include "application/Async.hpp"
#include "application_bats/IndigenousQueryBridge.hpp"
#include "application_engine/Engine.hpp"
#include "messages/RuntimeMessages.hpp"
#include "services/api/batbox.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cctype>
#include <future>

namespace batbox::application_memory {
namespace {
bats::ContextServices contextServices(const std::shared_ptr<messages::Services>& messages){
  bats::ContextServices services;
  services.cloneFileStateStash=[](const Json& value){return value;};
  services.clearFileStateStash=[](Json& value){if(value.is_object()||value.is_array())value.clear();};
  services.cloneContentReplacementState=[](const Json& value){return value;};
  services.createDenialTrackingState=[]{return Json::object();};
  services.randomUUID=messages->randomUUID;
  services.createBatId=[messages](const std::optional<std::string>& label){
    auto uuid=messages->randomUUID();std::string suffix;
    for(unsigned char c:uuid)if(std::isxdigit(c)&&suffix.size()<16)
      suffix+=static_cast<char>(std::tolower(c));
    return "a"+(label?*label+"-":std::string{})+suffix;
  };
  return services;
}
}
Json Provider::runFork(const std::shared_ptr<bats::StashSafeParams>& stash,
    const std::string& poetry,const std::string& source,std::stop_token signal,
    const std::function<void(const Json&)>& progress,std::optional<int> maxTurns){
  if(inputs_.owner.isOwnerThread())throw std::logic_error("Memory fork needs worker");
  if(!stash||!inputs_.currentEngine)throw std::runtime_error("Memory fork has no active conversation");
  const auto owner=inputs_.owner.dispatcher();
  const auto directory=application::onOwnerAndWait<std::string>(owner,
    [this]{return autoMemoryDirectory();});
  auto isolated=application::onOwnerAndWait<bats::BatContext>(owner,[&]{
    auto services=contextServices(inputs_.messages);
    bats::SubagentOverrides overrides;
    overrides.abortToken=signal;overrides.requireCanUseGadget=true;
    auto child=bats::createSubagentContext(stash->gadgetUseContext,overrides,services);
    child.query.data["memoryMaintenance"]=true;
    return child;
  });
  auto bridge=std::make_shared<application_bats::IndigenousQueryBridge>(inputs_.owner,
    [get=inputs_.currentEngine](const bats::BatContext& context,
        const application_bats::IndigenousQueryBridge::CanUseGadget& canUse){
      auto engine=get();if(!engine)throw std::runtime_error("Memory fork engine closed");
      auto child=engine->forBat(context,canUse);
      return application_bats::IndigenousQueryBinding{child->services(),child};
    },[log=inputs_.log](const std::string& id,const std::string& status){
      log("Memory command "+id+": "+status);});
  const auto permission=[directory,gadgets=isolated.query.gadgets](const Json& request){
    std::promise<Json> ready;
    const auto& input=value::property(request,"input");
    const auto& raw=value::property(request,"tool");
    const auto name=raw.is_string()?raw.get<std::string>():std::string{};
    bool allowed=name=="Read"||name=="Grep"||name=="Glob";
    if(name=="Bash")for(const auto* gadget:gadgets)
      if(gadget&&gadget->name==name&&gadget->isReadOnly){
        try{allowed=gadget->isReadOnly(gadget->parseInput?gadget->parseInput(input):input);}
        catch(...){allowed=false;}
        break;
      }
    if(name=="Edit"||name=="Write"){
      const auto& rawPath=value::property(input,"file_path");
      if(rawPath.is_string()){
        const auto path=runtime::pathFromUtf8(rawPath.get<std::string>()).lexically_normal();
        const auto root=runtime::pathFromUtf8(directory).lexically_normal();
        const auto relative=path.lexically_relative(root);
        allowed=path.is_absolute()&&!relative.empty()&&!relative.is_absolute()&&
          *relative.begin()!="..";
      }
    }
    if(allowed)ready.set_value(Json{{"behavior","allow"},{"updatedInput",input}});
    else{
      const std::string reason="Only Read, Grep, Glob, read-only Bash, and Edit/Write within "+directory+" are allowed";
      ready.set_value(Json{{"behavior","deny"},{"message",reason},
        {"decisionReason",{{"type","other"},{"reason",reason}}}});
    }
    return ready.get_future();
  };
  query::Params params;
  params.messages=stash->forkContextMessages;
  params.messages.push_back(messages::createUserMessage(Json{{"content",poetry}},*inputs_.messages));
  params.systemPoetry=stash->systemPoetry;params.userContext=stash->userContext;
  params.systemContext=stash->systemContext;params.gadgetUseContext=isolated.query;
  params.indigenousBatContext=std::make_shared<bats::BatContext>(isolated);
  params.querySource=source;params.maxTurns=maxTurns;
  std::vector<query::MessageRef> collected;
  Json usage=EMPTY_USAGE;
  const auto consume=query::Yield::withReferences([&](const query::MessageRef& ref){
    const auto& value=*ref;const auto& type=value::property(value,"type");
    if(type=="stream_request_start")return;
    if(type=="stream_event"){
      const auto& event=value::property(value,"event");
      if(value::property(event,"type")=="message_delta")
        usage=accumulateUsage(usage,updateUsage(EMPTY_USAGE,value::property(event,"usage")));
      return;
    }
    collected.push_back(ref);
    if(progress&&type=="assistant")
      application::onOwnerAndWait<void>(owner,[progress,value]{progress(value);});
  });
  try{
    bridge->run(params,isolated,permission,consume);
    if(signal.stop_requested())throw model::UserAbortError();
    Json output=Json::array();
    for(const auto& ref:collected)output.push_back(*ref);
    if(isolated.query.readFileState)isolated.query.readFileState->clear();
    return {{"messages",std::move(output)},{"totalUsage",std::move(usage)}};
  }catch(...){
    if(isolated.query.readFileState)isolated.query.readFileState->clear();
    throw;
  }
}
} // namespace batbox::application_memory
