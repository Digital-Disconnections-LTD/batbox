#include "interactive/QueryExecution.hpp"
#include <iostream>
using namespace batbox;
using namespace batbox::interactive;
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
 event_loop::Deferred<T> pending(loop);pending.resolve(std::move(value));return pending;
}
template<class T>event_loop::Deferred<T> failed(event_loop::Loop& loop,const char* message){
 event_loop::Deferred<T> pending(loop);pending.reject(std::make_exception_ptr(std::runtime_error(message)));return pending;
}
int main(int argc,char** argv){
 const std::string mode=argc>1?argv[1]:"normal";event_loop::Loop loop;
 Json trace=Json::array(),title=nullptr;
 const Json initialJson={{"toolPermissionContext",{{"alwaysAllowRules",{{"command",Json::array({mode=="same"?"Read":"Old"})}}},{"mode","default"}}},
   {"mcp",{{"clients",Json::array({"store"})}}},{"fastMode",true},{"effortValue","medium"}};
 auto initial=std::make_shared<app_state::AppState>();
 initial->fields=std::make_shared<value::SharedRecord>(value::SharedRecord::fromJson(initialJson));
 initial->permissionContext=std::make_shared<value::SharedRecord>(
   value::SharedRecord::fromJson(initialJson["toolPermissionContext"]));
 auto s=std::make_shared<QueryExecutionServices>();
 s->store=std::make_shared<app_state::AppStateStore>(initial,[&](const app_state::AppStateStore::Change& change){
   trace.push_back(Json::array({"permissions",change.newState->snapshot()["toolPermissionContext"]["alwaysAllowRules"]["command"]}));
 });
 auto state=std::make_shared<QueryExecutionState>();QueryExecutionParams p;
 const std::string content=mode=="synthetic"?"<command-message>skill":"hello";
 auto first=std::make_shared<Json>(Json{{"type","user"},{"uuid","u"},{"message",{{"content",content}}}});
 p.request.newMessages=std::make_shared<TurnMessages>(TurnMessages{first});
 if(mode=="compact")p.request.newMessages->push_back(std::make_shared<Json>(Json{{"type","system"},{"subtype","compact_boundary"}}));
 p.messages=std::make_shared<TurnMessages>(TurnMessages{std::make_shared<Json>(Json{{"type","assistant"},{"uuid","old"}})});
 p.messages->insert(p.messages->end(),p.request.newMessages->begin(),p.request.newMessages->end());
 p.request.abort=std::make_shared<TurnAbort>();p.request.mainLoopModel="model";
 p.request.shouldQuery=mode!="local"&&mode!="compact"&&mode!="reset";
 if(mode!="reset")p.request.additionalAllowedGadgets={"Read"};
 if(mode=="effort")p.request.effort="low";
 p.initialMcpClients=Json::array({"initial"});p.gadgetPermissionContext=initialJson["toolPermissionContext"];
 p.additionalWorkingDirectories={"/work"};p.titleDisabled=mode=="title_disabled";
 if(mode=="known")p.flightTitle="known";
 s->mergeClients=[&](const Json& a,const Json& b){trace.push_back(Json::array({"merge",a,b}));auto out=a;for(const auto& v:b)out.push_back(v);return out;};
 s->diagnosticQueryStart=[&](const Json& clients){trace.push_back(Json::array({"diagnostic",clients}));};
 s->getConnectedIdeClient=[&](const Json&){return mode=="ide"?Json{{"id","ide"}}:Json(nullptr);};
 s->closeOpenDiffs=[&](const Json& client){trace.push_back(Json::array({"closeDiffs",client}));};
 s->markOnboardingComplete=[&]{trace.push_back("onboarding");};
 s->generateFlightTitle=[&](const std::string& text,const std::shared_ptr<TurnAbort>& abort){
   trace.push_back(Json::array({"titleRequest",text,abort->aborted()}));
   return mode=="title_error"?failed<Json>(loop,"title failed"):ready(loop,Json(mode=="title_empty"?"":"A title"));};
 s->setFlightTitle=[&](const std::string& value){title=value;trace.push_back(Json::array({"title",value}));};
 s->isCompactBoundary=[](const TurnMessageRef& message){return value::property(*message,"subtype")=="compact_boundary";};
 s->setConversationId=[&](const std::string& value){trace.push_back(Json::array({"conversation",value}));};s->uuid=[]{return "next";};
 s->feature=[&](const std::string& name){return name=="TRANSCRIPT_CLASSIFIER"?mode=="auto":name=="PROACTIVE"?mode=="proactive":name=="MONSTER"?mode=="monster":false;};
 s->setContextBlocked=[&](bool value){trace.push_back(Json::array({"blocked",value}));};
 s->resetLoadingState=[&]{trace.push_back("reset");};s->clearAbortController=[&]{trace.push_back(Json::array({"abort",nullptr}));};
 s->getGadgetUseContext=[&](const TurnMessagesRef& all,const TurnRequest& request){
   trace.push_back(Json::array({"context",all->size(),request.newMessages->size(),request.mainLoopModel}));
   InteractiveQueryContext context;context.context.data={{"options",{{"tools",Json::array({"Read"})},{"mcpClients",Json::array({"turn"})}}}};return context;};
 s->checkpoint=[&](const std::string& name){trace.push_back(Json::array({"checkpoint",name}));};
 s->checkBypassPermissions=[&](const Json&,const auto&){trace.push_back("bypass");return ready(loop,Json(nullptr));};
 s->checkAutoMode=[&](const Json&,const auto&,const Json& fast){trace.push_back(Json::array({"auto",fast}));return ready(loop,Json(nullptr));};
 s->getSystemPoetry=[&](const execution::Context& context,const std::string& model,const std::vector<std::string>& dirs,const Json& clients){
   trace.push_back(Json::array({"systemPrompt",context.data["options"]["tools"],model,dirs,clients}));return ready(loop,Json::array({"default"}));};
 s->getUserContext=[&]{trace.push_back("userContext");return mode=="context_error"?failed<Json>(loop,"context failed"):ready(loop,Json{{"base","user"},{"overlap","base"}});};
 s->getSystemContext=[&]{trace.push_back("systemContext");return ready(loop,Json{{"system","context"}});};
 s->getCoordinatorUserContext=[&](const Json& clients,const std::optional<std::string>& dir){trace.push_back(Json::array({"coordinator",clients,dir?Json(*dir):Json(nullptr)}));return Json{{"overlap","coordinator"}};};
 s->scratchpadEnabled=[&]{return mode=="scratchpad";};s->getScratchpadDir=[]{return "/scratch";};
 s->isProactiveActive=[&]{return mode=="proactive";};s->terminalFocused=[]{return false;};
 s->effectivePoetry.feature=[](const std::string&){return false;};
 s->resetTurnHookDuration=[&]{trace.push_back("resetHooks");};s->resetTurnGadgetDuration=[&]{trace.push_back("resetTools");};s->resetTurnClassifierDuration=[&]{trace.push_back("resetClassifier");};
 s->getQuerySource=[]{return "repl";};int index=0;
 s->makeQuery=[&](query::Params params){
   trace.push_back(Json::array({"query",params.messages.size(),params.systemPoetry,params.userContext,
     params.systemContext,params.querySource,execution::effectiveAppState(params.gadgetUseContext,s->store->getState()->snapshot())["effortValue"],params.gadgetUseContext.data["renderedSystemPrompt"]}));
   query::AsyncQueryCursor cursor;
   cursor.next=[&]{trace.push_back(Json::array({"next",index}));if(mode=="next_error")return failed<query::AsyncEngineStep>(loop,"next failed");
     query::AsyncEngineStep step;step.done=index>=2;
     if(!step.done)step.event=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a"+std::to_string(index++)}});
     return ready(loop,step);};
   cursor.returnEarly=[&](const Json&){trace.push_back("close");if(mode=="close_error")return failed<query::AsyncEngineStep>(loop,"close failed");
     query::AsyncEngineStep step;step.done=true;return ready(loop,step);};return cursor;};
 s->onQueryEvent=[&](const query::AsyncEngineStep& step){trace.push_back(Json::array({"event",(*step.event)["uuid"]}));if(mode=="consumer_error"||mode=="close_error")throw std::runtime_error("consumer failed");};
 s->getMessages=[&]{return p.messages;};s->launchMonsterObserver=[&](const TurnMessagesRef& messages){trace.push_back(Json::array({"monster",messages->size()}));};
 s->logQueryProfileReport=[&]{trace.push_back("profile");};
 if(mode!="no_complete")s->onTurnComplete=[&](const TurnMessagesRef& messages){trace.push_back(Json::array({"complete",messages->size()}));
   return mode=="complete_error"?failed<event_loop::Unit>(loop,"complete failed"):ready(loop,event_loop::Unit{});};
 auto pending=executeQueryTurn(loop,state,p,s);pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
   if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){trace.push_back(Json::array({"error",e.what()}));}}
   else trace.push_back("resolved");});
 loop.runReady();
 if(!pending.settled())throw std::runtime_error("Interactive query did not settle");
 std::cout<<Json{{"trace",trace},{"attempted",state->titleAttempted},{"title",title},
   {"stateChanged",s->store->getState()!=initial},{"state",s->store->getState()->snapshot()}}.dump()<<'\n';
}
