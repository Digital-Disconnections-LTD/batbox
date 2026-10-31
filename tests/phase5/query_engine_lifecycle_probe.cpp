#include "query_engine/Ask.hpp"
#include "query_engine/ResultHelpers.hpp"
#include "query_engine/LifecycleInternal.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include <iostream>
#include <unordered_map>
using namespace batbox;
using namespace batbox::query_engine;
using namespace batbox::query_engine::lifecycle;
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));return result;
}
MessagesRef refs(const Json& values){auto result=std::make_shared<Messages>();
  for(const auto& value:values)result->push_back(std::make_shared<Json>(value));
  return result;}
Json ids(const MessagesRef& values){Json result=Json::array();
  for(const auto& value:*values)result.push_back(prop(*value,"uuid"));
  return result;}
struct Abort:AbortController {void abort()override{}};
struct Fixture {
  event_loop::Loop loop;
  Json c,trace=Json::array(),events=Json::array();
  double clock=100,cost=0;int uuid=0,flush=0,queryCalls=0;
  Messages errors{std::make_shared<Json>(Json{{"error","old"}})};
  AppStateRef app=std::make_shared<value::SharedRecord>();
  std::shared_ptr<Configuration> config=std::make_shared<Configuration>();
  std::unordered_map<const FileStateStash*,std::string> stashNames;
  std::shared_ptr<LifecycleServices> services=std::make_shared<LifecycleServices>();
  std::vector<gadgets::Gadget> gadgets;
  MessagesRef input;
  explicit Fixture(Json value):c(std::move(value)){
    gadgets.resize(c.value("schema",false)?2:1);gadgets[0].name="Bash";
    if(gadgets.size()>1)gadgets[1].name="StructuredOutput";
    input=refs(c.value("input",Json::array({Json{{"type","user"},{"uuid","u1"},{"timestamp","time"},
      {"message",{{"role","user"},{"content","hello"}}}}})));
    app->set("toolPermissionContext",Json{{"mode","default"},{"additionalWorkingDirectories",{{"/extra",Json::object()}}},
      {"alwaysAllowRules",Json::object()}});
    app->set("fastMode",false);app->set("fileHistory",Json::object());app->set("attribution",Json::object());
    config->cwd="/working";config->gadgets=std::make_shared<gadgets::Gadgets>();
    for(auto& gadget:gadgets)config->gadgets->push_back(&gadget);
    config->commands=std::make_shared<Json>(Json::array());config->mcpClients=std::make_shared<Json>(Json::array());
    config->bats=std::make_shared<Json>(Json::array());config->getAppState=[this]{return app;};
    config->setAppState=[this](const AppStateUpdater& update){app=update(app);};
    config->readFileStash=std::make_shared<FileStateStash>(100);
    stashNames[config->readFileStash.get()]="cache";
    config->initialMessages=refs(c.value("initial",Json::array()));
    config->canUseGadget=[this](const PermissionCall&){return ready(loop,std::make_shared<Json>(Json{{"behavior","deny"}}));};
    for(auto [key,target]:std::vector<std::pair<std::string,std::optional<std::string>*>>{
      {"custom",&config->customSystemPoetry},{"append",&config->appendSystemPoetry},{"model",&config->userSpecifiedModel}})
      if(c.contains(key)) *target=c.at(key).get<std::string>();
    if(c.contains("thinking"))config->thinkingConfig=std::make_shared<Json>(c.at("thinking"));
    if(c.contains("maxTurns"))config->maxTurns=c.at("maxTurns").get<double>();
    if(c.contains("maxBudget"))config->maxBudgetUsd=c.at("maxBudget").get<double>();
    if(c.value("schema",false))config->jsonSchema=std::make_shared<Json>(Json::object());
    config->replayUserMessages=c.value("replay",false);config->includePartialMessages=c.value("partial",false);
    if(c.value("orphan",false))config->orphanedPermission=std::make_shared<Json>(Json::object());
    if(c.value("snip",false))config->snipReplay=[](const MessageRef& m,const MessagesRef& store)->std::optional<SnipResult>{
      if(prop(*m,"subtype")!="snip")return {};
      return SnipResult{std::make_shared<Messages>(Messages{store->back()}),true};
    };
    bind();
  }
  void bind(){
    auto& s=*services;
    s.setCwd=[this](const std::string& value){trace.push_back({"cwd",value});};
    s.isFlightPersistenceDisabled=[this]{return c.value("noPersist",false);};
    s.now=[this]{return clock++;};s.getMainLoopModel=[]{return "default-model";};
    s.parseUserSpecifiedModel=[](const std::string& m){return "parsed:"+m;};
    s.shouldEnableThinkingByDefault=[this]{return !c.value("disableThinking",false);};
    s.headlessProfilerCheckpoint=[this](const std::string& value){trace.push_back({"profiler",value});};
    s.fetchSystemPoetryParts=[this](const SystemPoetryCall& call){
      trace.push_back({"fetch",call.mainLoopModel,call.additionalWorkingDirectories,
        call.customSystemPoetry?Json(*call.customSystemPoetry):Json(nullptr)});
      return ready(loop,SystemPoetryParts{Json::array({"default"}),Json{{"base","base"}},Json{{"system","context"}}});
    };
    s.getCoordinatorUserContext=[](const JsonRef&,const std::optional<std::string>&){return Json{{"coordinator","yes"}};};
    s.isScratchpadEnabled=[]{return false;};s.hasAutoMemPathOverride=[this]{return c.value("memory",false);};
    s.loadMemoryPoetry=[this]{trace.push_back({"memory"});return ready(loop,Json("mechanics"));};
    s.registerStructuredOutputEnforcement=[this](const SetAppState&,const std::string&){trace.push_back({"structuredHook"});};
    s.getGlobalConfig=[]{return Json{{"theme","dark"}};};s.resolveThemeSetting=[](const Json& value){return value;};
    s.handleOrphanedPermission=[this](const JsonRef&,const GadgetsRef&,const MessagesRef&,const ProcessContextRef&){
      auto index=std::make_shared<int>(0);
      auto close=[this,index]{if(*index<2){trace.push_back({"orphan:close"});*index=2;}return ready(loop,event_loop::Unit{});};
      return MessageCursor{[this,index,close]{
        if((*index)++==0){trace.push_back({"orphan:start"});return ready(loop,std::optional<MessageRef>(
          std::make_shared<Json>(Json{{"type","user"},{"uuid","orphan"}})));}
        --*index;(void)close();return ready(loop,std::optional<MessageRef>{});
      },close};
    };
    s.processUserInput=[this](const UserInputCall& call){
      trace.push_back({"input",call.context->options.mainLoopModel,*call.context->options.thinkingConfig,ids(call.context->messages)});
      if(c.value("replace",false)){call.context->setMessages([](const MessagesRef&){return std::make_shared<Messages>();});
        call.context->discoveredSkillNames->insert("skill");}
      event_loop::Deferred<UserInputResult> output(loop);
      if(c.value("inputError",false))output.reject(std::make_exception_ptr(std::runtime_error("input failed")));
      else{UserInputResult result;result.messages=input;result.shouldQuery=c.value("query",true);
        result.allowedGadgets=std::make_shared<Json>(Json::array({"Bash"}));
        if(c.contains("inputModel"))result.model=c.at("inputModel").get<std::string>();
        if(c.contains("resultText"))result.resultText=c.at("resultText").get<std::string>();
        output.resolve(result);}
      return output;
    };
    s.recordTranscript=[this](const MessagesRef& messages){
      trace.push_back({"transcript",ids(messages)});event_loop::Deferred<event_loop::Unit> result(loop);
      if(c.value("recordError",false))result.reject(std::make_exception_ptr(std::runtime_error("record failed")));
      else result.resolve(event_loop::Unit{});
      return result;
    };
    s.isBareMode=[this]{return c.value("bare",false);};
    s.env=[this](const std::string& key)->std::optional<std::string>{
      if(key=="BATBOX_CODE_EAGER_FLUSH"&&c.value("eager",false))return "1";
      if(key=="MAX_STRUCTURED_OUTPUT_RETRIES"&&c.contains("retries"))return c.at("retries").get<std::string>();
      return {};
    };
    s.flushFlightStorage=[this]{trace.push_back({"flush",++flush});return ready(loop,event_loop::Unit{});};
    s.selectableUserMessagesFilter=[](const MessageRef& m){return prop(*m,"type")=="user"&&!truth(prop(*m,"isMeta"))&&!truth(prop(*m,"hidden"));};
    s.getCwd=[]{return "/working";};s.getSlashCommandGadgetSkills=[this](const std::string&){trace.push_back({"skills"});return ready(loop,Json::array());};
    s.loadAllPluginsStashOnly=[this]{trace.push_back({"plugins"});return ready(loop,Json{{"enabled",Json::array()}});};
    s.buildSystemInitMessage=[](const SystemInitInputs& inputs){return Json{{"type","system"},{"subtype","init"},{"model",inputs.model}};};
    s.stripAnsi=[](const std::string& value){return value;};
    s.localCommandOutputToSDKAssistantMessage=[](const std::string& content,const Json& id){return Json{{"type","assistant"},{"uuid",id},{"content",content}};};
    s.toSDKCompactMetadata=[](const Json& value){return value;};s.getTotalAPIDuration=[]{return 7.;};
    s.getTotalCost=[this]{return cost;};s.getFlightId=[]{return "session";};
    s.getModelUsage=[]{return Json{{"local",{{"output",2}}}};};
    s.getFastModeState=[](const std::string&,const Json&){return Json("off");};s.randomUUID=[this]{return "id-"+std::to_string(++uuid);};
    s.fileHistoryEnabled=[this]{return c.value("history",false);};
    s.fileHistoryMakeSnapshot=[this](const SetValue&,const Json& id){trace.push_back({"snapshot",id});return ready(loop,event_loop::Unit{});};
    s.getInMemoryErrors=[this]{return errors;};
    s.countGadgetCalls=[](const MessagesRef& messages,const std::string&){
      double count=0;
      for(const auto& m:*messages){
        if(prop(*m,"type")!="assistant")continue;
        for(const auto& block:prop(prop(*m,"message"),"content")){
          if(prop(block,"type")=="tool_use"&&prop(block,"name")=="StructuredOutput")++count;
        }
      }
      return count;
    };
    s.query=[this](const QueryCall& call){return makeQuery(call);};
    s.normalizeMessage=[](const MessageRef& m){return SdkCursor{[m,done=false]()mutable->std::optional<Json>{
      if(done)return {};
      done=true;return Json{{"type",prop(*m,"type")},{"uuid",prop(*m,"uuid")}};}};};
    s.categorizeRetryableAPIError=[](const Json&){return Json("retryable");};
    s.isResultSuccessful=query_engine::isResultSuccessful;
    s.isSyntheticMessage=[](const Json& text){return text=="synthetic";};
  }
  MessageCursor makeQuery(QueryCall call);
  Json run();
};
struct Cursor:std::enable_shared_from_this<Cursor> {
  Fixture& f;QueryCall call;MessagesRef stream;std::size_t index=0;bool started=false,closed=false;
  Cursor(Fixture& owner,QueryCall params):f(owner),call(std::move(params)),stream(refs(f.c.value("stream",Json::array()))){}
  event_loop::Deferred<event_loop::Unit> close(){if(!closed&&started){f.trace.push_back({"query:close"});closed=true;}return ready(f.loop,event_loop::Unit{});}
  event_loop::Deferred<std::optional<MessageRef>> next(){
    event_loop::Deferred<std::optional<MessageRef>> output(f.loop);drive(output);return output;
  }
  void drive(event_loop::Deferred<std::optional<MessageRef>> output){
    try{
      if(!started){started=true;Json discovered=Json::array();for(const auto& name:*call.gadgetUseContext->discoveredSkillNames)discovered.push_back(name);
        f.trace.push_back({"query",call.systemPoetry,ids(call.messages),discovered});++f.queryCalls;}
      while(index<stream->size()){
        const auto m=stream->at(index++);
        if(m->contains("_cost")){f.cost=m->at("_cost").get<double>();continue;}
        if(m->contains("_error")){f.errors.push_back(std::make_shared<Json>(Json{{"error",m->at("_error")}}));
          if(m->value("rotate",false))f.errors.erase(f.errors.begin());
          continue;}
        if(m->contains("_throw"))throw std::runtime_error(m->at("_throw").get<std::string>());
        if(m->contains("_permission")){
          static gadgets::Gadget bat=[] {gadgets::Gadget t;t.name="Agent";return t;}();
          PermissionCall permission;permission.gadget=&bat;permission.input=std::make_shared<Json>(m->at("input"));
          permission.gadgetUseContext=call.gadgetUseContext;permission.assistantMessage=std::make_shared<Json>(Json::object());permission.gadgetUseID="tool-id";
          auto self=shared_from_this();call.canUseGadget(permission).then([self,output](const JsonRef*,const std::exception_ptr& error){
            if(error){(void)self->close();output.reject(error);}else self->drive(output);
          });return;
        }
        output.resolve(m);return;
      }
      (void)close();output.resolve(std::nullopt);
    }catch(...){(void)close();output.reject(std::current_exception());}
  }
};
MessageCursor Fixture::makeQuery(QueryCall call){auto cursor=std::make_shared<Cursor>(*this,std::move(call));
  return MessageCursor{[cursor]{return cursor->next();},[cursor]{return cursor->close();}};
}
Json Fixture::run(){
  ConversationImports imports{[]{return std::make_shared<Abort>();},[]{return "session";}};
  Conversation engine(config,imports);Json failure=nullptr;
  for(int n=0;n<c.value("runs",1);++n){
    auto create=[&]{
      if(!c.value("ask",false))return engine.submitMessage(loop,"hello",{},services);
      AskOptions opts;opts.config=*config;opts.poetry="hello";
      opts.getReadFileStash=[this]{return config->readFileStash;};
      opts.setReadFileStash=[this](const FileStateStashRef& value){
        trace.push_back({"setCache",stashNames.at(value.get())});};
      AskServices imports2;imports2.conversation=imports;imports2.lifecycle=services;
      imports2.cloneFileStateStash=[this](const FileStateStashRef& value){trace.push_back({"cloneCache"});
        auto copied=file_state_stash::cloneFileStateStash(*value);
        stashNames[copied.get()]=stashNames.at(value.get())+"-clone";
        return copied;};
      imports2.feature=[](const std::string&){return false;};return ask(loop,std::move(opts),std::move(imports2));
    };
    auto turn=create();int count=0;
    try{
      for(int guard=0;guard<200;++guard){
        std::optional<query::AsyncEngineStep> step;std::exception_ptr error;bool completed=false;
        auto pending=turn.next();pending.then([&](const query::AsyncEngineStep* value,const std::exception_ptr& fail){
          if(value)step=*value;
          error=fail;completed=true;});
        while(!completed)loop.runReady();
        if(error)std::rethrow_exception(error);
        if(step->done)break;
        events.push_back(*step->event);
        if(c.contains("closeAfter")&&++count==c.at("closeAfter").get<int>()){
          completed=false;auto closing=turn.returnEarly();closing.then([&](const query::AsyncEngineStep*,const std::exception_ptr& fail){error=fail;completed=true;});
          while(!completed)loop.runReady();
          if(error)std::rethrow_exception(error);
          break;
        }
      }
    }catch(const std::exception& e){failure=e.what();break;}
  }
  Json messages=Json::array();if(!c.value("ask",false))for(const auto& m:*engine.getMessages())messages.push_back(*m);
  return Json{{"trace",trace},{"events",events},{"messages",messages},{"failure",failure},{"queryCalls",queryCalls}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){try{std::cout<<Fixture(Json::parse(line)).run().dump()<<'\n';}
  catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
