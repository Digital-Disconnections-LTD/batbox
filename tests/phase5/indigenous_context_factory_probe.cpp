#include "interactive/context/ContextFactory.hpp"
#include "bats/ForkedBat.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace ctx=batbox::interactive::context;
static Json gadgetNames(const gadgets::Gadgets& available){
  Json names=Json::array();
  for(const auto* gadget:available)names.push_back(gadget->name);
  return names;
}
static app_state::AppStateRef stateFromJson(const Json& value){
  auto state=std::make_shared<app_state::AppState>();
  state->fields=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(value));
  state->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(value.at("toolPermissionContext")));
  return state;
}
int main(){
  event_loop::Loop owner;
  auto state=stateFromJson(Json{
    {"verbose",true},{"thinkingEnabled",false},
    {"toolPermissionContext",Json{{"mode","default"}}},
    {"mcp",Json{{"tools",Json::array({Json{{"name","mcp__z"}}})},
      {"clients",Json::array({Json{{"name","server"},{"tag","live"}}})},
      {"resources",Json{{"server",Json::array({Json{{"uri","one"}}})}}}}},
    {"agentDefinitions",Json{{"activeAgents",Json::array(
      {Json{{"agentType","general"}}})}}},
    {"fileHistory",Json{{"version",1}}},
    {"attribution",Json{{"version",1}}}});
  int notifications=0;
  auto store=std::make_shared<app_state::AppStateStore>(state,
    [&](const auto&){++notifications;});
  gadgets::Gadget beta,alpha,mcpZ,mcpA;
  beta.name="Beta";alpha.name="Alpha";
  mcpZ.name="mcp__z";mcpA.name="mcp__a";
  Json events=Json::array();
  double responseLength=10;
  auto ids=std::make_shared<runtime::OrderedStringSet>();ids->insert("old");
  auto messages=std::make_shared<interactive::TurnMessages>();
  messages->push_back(std::make_shared<Json>(Json{{"type","user"},
    {"message",Json{{"content","old"}}}}));
  auto commands=std::make_shared<Json>(Json::array({Json{{"name","config"}}}));
  auto stash=file_state_stash::createFileStateStashWithSizeLimit(5);
  auto loaded=std::make_shared<runtime::OrderedStringSet>();loaded->insert("memory");
  auto discovered=std::make_shared<runtime::OrderedStringSet>();
  discovered->insert("skill");
  auto replacement=gadget_results::createContentReplacementState();
  ctx::ContextBindings bindings;
  bindings.store=store;
  bindings.commands=commands;
  bindings.initialMcpClients=std::make_shared<Json>(Json::array(
    {Json{{"name","server"},{"tag","initial"}}}));
  bindings.thinkingConfig=std::make_shared<Json>(Json{{"type","enabled"}});
  bindings.combinedInitialGadgets={&beta};
  bindings.getMcpGadgets=[&](const app_state::AppStateRef& current){
    const auto snapshot=current->snapshot();
    const auto& name=value::property(value::property(snapshot,"mcp"),"tools")[0]["name"];
    return gadgets::Gadgets{name=="mcp__a"?&mcpA:&mcpZ};
  };
  bindings.assembleGadgets=[&](const Json&,const gadgets::Gadgets& mcp){
    gadgets::Gadgets result={&alpha};result.insert(result.end(),mcp.begin(),mcp.end());
    return result;
  };
  bindings.gadgetPool.localeCompare=[](const auto& a,const auto& b){return a.compare(b);};
  bindings.gadgetPool.feature=[](const auto&){return false;};
  bindings.feature=[](const auto& name){return name=="HOOK_PROMPTS";};
  bindings.allowedBatTypes=Json::array({"Explore"});
  bindings.debug=true;
  bindings.customSystemPoetry="custom";
  bindings.appendSystemPoetry="append";
  bindings.readFileState=stash;
  bindings.loadedNestedMemoryPaths=loaded;
  bindings.discoveredSkillNames=discovered;
  bindings.contentReplacementState=replacement;
  bindings.setMessages=[&](const interactive::TurnMessageUpdate& update){
    messages=update(messages);
  };
  bindings.showMessageSelector=[&]{events.push_back(Json::array({"selector",true}));};
  bindings.setResponseLength=[&](const std::function<double(double)>& update){
    responseLength=update(responseLength);};
  bindings.setInProgressGadgetUseIDs=[&](const auto& update){ids=update(ids);};
  bindings.spinner.primaryColor="claudeBlue_FOR_SYSTEM_SPINNER";
  bindings.spinner.shimmerColor="claudeBlueShimmer_FOR_SYSTEM_SPINNER";
  bindings.spinner.setColor=[&](std::optional<std::string> value){
    events.push_back(Json::array({"color",value?Json(*value):Json()}));};
  bindings.spinner.setShimmerColor=[&](std::optional<std::string> value){
    events.push_back(Json::array({"shimmer",value?Json(*value):Json()}));};
  bindings.spinner.setMessage=[&](std::optional<std::string> value){
    events.push_back(Json::array({"message",value?Json(*value):Json()}));};
  bindings.sendOSNotification=[&](const Json& options){
    events.push_back(Json::array({"os",options.at("message"),"terminal"}));};
  bindings.requestPoetry=[&](const std::string& title,
      const std::optional<std::string>& summary){
    return [&,title,summary](const Json& request){
      events.push_back(Json::array({"prompt",title,
        summary?Json(*summary):Json(),request.at("kind")}));
      event_loop::Deferred<Json> done(owner);done.resolve(Json{{"kind","ok"}});
      return done;
    };
  };
  ctx::ContextFactory factory(std::move(bindings));
  auto abort=std::make_shared<interactive::TurnAbort>();
  auto full=factory.make(messages,std::make_shared<interactive::TurnMessages>(),
    abort,"main");
  Json clientNames=Json::array();
  for(const auto& client:*full->options.mcpClients)
    clientNames.push_back(Json::array({client.at("name"),client.at("tag")}));
  Json initial={{"tools",gadgetNames(full->options.gadgets)},
    {"clients",clientNames},{"verbose",full->options.verbose},
    {"thinking",full->options.thinkingConfig->at("type")},
    {"allowed",full->options.batDefinitions->at("allowedAgentTypes")},
    {"commandsSame",full->options.commands==commands},
    {"cacheSame",full->readFileState==stash},
    {"abortSame",full->abortController==abort}};
  store->setState([](const app_state::AppStateRef& prior){
    auto next=std::make_shared<app_state::AppState>(prior->shallowCopy());
    next->fields->set("verbose",false);
    next->fields->set("thinkingEnabled",true);
    auto mcp=value::property(prior->snapshot(),"mcp");
    mcp["tools"]=Json::array({Json{{"name","mcp__a"}}});
    next->fields->set("mcp",std::move(mcp));
    return next;
  });
  notifications=0;
  const auto refreshed=gadgetNames(full->options.refreshGadgets());
  full->updateFileHistoryState([](const ctx::ValueRef& prior){return prior;});
  full->updateFileHistoryState([](const ctx::ValueRef& prior){
    auto next=std::make_shared<Json>(*prior);(*next)["version"]=2;return next;});
  full->updateAttributionState([](const ctx::ValueRef& prior){return prior;});
  full->updateAttributionState([](const ctx::ValueRef& prior){
    auto next=std::make_shared<Json>(*prior);(*next)["version"]=2;return next;});
  full->appendSystemMessage(std::make_shared<Json>(Json{{"type","system"},
    {"message",Json{{"content","notice"}}}}));
  full->openMessageSelector();
  full->setResponseLength([](double prior){return prior+3;});
  full->setInProgressGadgetUseIDs([](const ctx::GadgetUseContext::SetRef& prior){
    auto next=std::make_shared<runtime::OrderedStringSet>(*prior);
    next->insert("new");return next;});
  full->onCompactProgress(Json{{"type","hooks_start"},{"hookType","pre_compact"}});
  full->onCompactProgress(Json{{"type","compact_start"}});
  full->onCompactProgress(Json{{"type","compact_end"}});
  full->sendOSNotification(Json{{"message","hello"},
    {"notificationType","done"}});
  (void)full->requestPoetry("Title",std::string("summary"))(
    Json{{"kind","confirm"}});
  auto second=factory.make(messages,std::make_shared<interactive::TurnMessages>(),
    std::make_shared<interactive::TurnAbort>(),"main");
  interactive::TurnRequest request;
  request.newMessages=std::make_shared<interactive::TurnMessages>();
  request.abort=std::make_shared<interactive::TurnAbort>();
  request.mainLoopModel="main";
  auto projected=factory.makeQueryContext(messages,request);
  if(projected.context.readFileState!=stash||
     projected.batContext->getTypedAppState()!=store->getState()||
     projected.batContext->setAppState||
     projected.context.stop!=request.abort->controller.get_token()||
     projected.context.loadedNestedMemoryPaths!=loaded||
     projected.context.discoveredSkillNames!=discovered||
     !projected.context.nestedMemoryAttachmentTriggers||
     !projected.context.dynamicSkillDirTriggers||
     projected.context.nestedMemoryAttachmentTriggers==
       second->nestedMemoryAttachmentTriggers)
    throw std::runtime_error("query projection identity");
  const auto beforeNoop=store->getState();
  projected.batContext->setTypedAppState([](const auto& prior){return prior;});
  if(store->getState()!=beforeNoop)
    throw std::runtime_error("typed no-op state update");
  Json idValues=Json::array();for(const auto& id:*ids)idValues.push_back(id);
  const auto end=store->getState()->snapshot();
  std::cout<<Json{{"initial",initial},{"refreshed",refreshed},
    {"freshVerbose",value::property(end,"verbose")},
    {"notifications",notifications},
    {"history",value::property(value::property(end,"fileHistory"),"version")},
    {"attribution",value::property(value::property(end,"attribution"),"version")},
    {"messageCount",messages->size()},
    {"responseLength",static_cast<int>(responseLength)},
    {"ids",idValues},{"events",events},
    {"newNested",second->nestedMemoryAttachmentTriggers!=
      full->nestedMemoryAttachmentTriggers},
    {"newDynamic",second->dynamicSkillDirTriggers!=
      full->dynamicSkillDirTriggers},
    {"sharedLoaded",second->loadedNestedMemoryPaths==
      full->loadedNestedMemoryPaths},
    {"sharedDiscovered",second->discoveredSkillNames==
      full->discoveredSkillNames},
    {"sharedReplacement",second->contentReplacementState==
      full->contentReplacementState}}.dump()<<'\n';
}
