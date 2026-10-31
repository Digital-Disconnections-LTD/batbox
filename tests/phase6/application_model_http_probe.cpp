#include "application_model/LocalModelService.hpp"
#include "execution/GadgetExecution.hpp"
#include "messages/Messages.hpp"
#include "query/SimpleQueriesBinding.hpp"
#include "runtime/Platform.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("Base URL required");
  event_loop::Loop loop;
  application_model::Config config;
  config.client.baseUrl=argv[1];
  config.client.messagesPath="/normal";
  config.client.requestTimeout=std::chrono::seconds(8);
  config.client.streamIdleTimeout=std::chrono::seconds(6);
  config.mainModel=config.smallModel="batbox-model";
  config.profiles.emplace("batbox-model",application_model::ModelProfile{
    "batbox-model",200000,50000,false,false,false,false,false,false,
    Json{{"inputTokens",0},{"outputTokens",0},
      {"promptCacheReadTokens",0},{"promptCacheWriteTokens",0},
      {"webSearchRequests",0}},Json()});
  config.policyURL="https://batbox.local/policy";
  config.statusURL="https://batbox.local/status";
  config.usageURL="https://batbox.local/usage";
  config.retryWire.retryAfter="retry-after";
  config.retryWire.shouldRetry="x-should-retry";
  application_model::Imports imports;
  std::atomic<int> schemaPoetryEntries{0};
  imports.authToken=[]{return std::string("test-local-token");};
  imports.invalidateAuthToken=[]{};
  imports.debug=[](const auto&,const auto&){};
  imports.diagnostic=[](const auto&,const auto&){};
  imports.normalizeAttachmentForAPI=[](const Json&)->Json{
    throw std::logic_error("Unexpected attachment");
  };
  imports.normalizeGadgetInputForAPI=[](const gadgets::Gadget&,
      const Json&)->Json{throw std::logic_error("Unexpected tool input");};
  imports.gadgetSchemaAsync=[&loop,&schemaPoetryEntries](const gadgets::Gadget& gadget,
      const gadgets::GadgetSchemaOptions& settings){
    event_loop::Deferred<Json> result(loop);
    try{
      if(gadget.name!="Read"||settings.poetry.gadgets.size()!=1||
         settings.poetry.gadgets.at(0)!=&gadget||
         !settings.poetry.getGadgetPermissionContext||
         settings.poetry.getGadgetPermissionContext().get().at("mode")!="allow"||
         settings.poetry.bats.at(0)!="agent-one")
        throw std::logic_error("Tool prompt context was not retained");
      ++schemaPoetryEntries;
      result.resolve(Json{{"name","Read"},{"description","read"},
        {"input_schema",{{"type","object"}}}});
    }catch(...){result.reject(std::current_exception());}
    return result;
  };
  imports.gadgetSchemaSync=[](const gadgets::Gadget&,
      const gadgets::GadgetSchemaOptions&)->Json{
    throw std::logic_error("Unexpected tool schema");
  };
  auto timedConfig=config;
  timedConfig.timeBasedMicrocompact=true;
  timedConfig.timeBasedKeepRecent=1;
  auto timedImports=imports;
  int clearedWarning=0,suppressedWarning=0;
  timedImports.clearCompactWarningSuppression=[&]{++clearedWarning;};
  timedImports.suppressCompactWarning=[&]{++suppressedWarning;};
  auto service=std::make_shared<application_model::LocalModelService>(
    loop,std::move(config),std::move(imports));
  const Json context={{"project","Batbox"},{"branch","main"}};
  if(service->appendSystemContext(Json::array({"base"}),context)!=
      Json::array({"base","project: Batbox\nbranch: main"}))
    throw std::logic_error("System context formatting changed");
  const Json original=Json::array({Json{{"type","user"},
    {"message",{{"role","user"},{"content","hello"}}}}});
  const auto prefixed=service->prependUserContext(original,context);
  if(prefixed.size()!=2||prefixed[1]!=original[0]||
      prefixed[0].at("isMeta")!=true||
      prefixed[0].at("message").at("content").get<std::string>().find(
        "# project\nBatbox\n# branch\nmain")==std::string::npos)
    throw std::logic_error("User context formatting changed");
  if(!service->normalizeMessagesForAPI(original,{}).is_array())
    throw std::logic_error("Message normalization unavailable");
  const Json oldMessages=Json::array({
    Json{{"type","assistant"},{"timestamp","2000-01-01T00:00:00.000Z"},
      {"message",{{"content",Json::array({
        Json{{"type","tool_use"},{"name","Read"},{"id","old"}},
        Json{{"type","tool_use"},{"name","Read"},{"id","new"}}})}}}},
    Json{{"type","user"},{"message",{{"content",Json::array({
      Json{{"type","tool_result"},{"tool_use_id","old"},
        {"content","long old result"}},
      Json{{"type","tool_result"},{"tool_use_id","new"},
        {"content","recent result"}}})}}}}
  });
  if(application_model::estimateMessageTokens(oldMessages)<=0)
    throw std::logic_error("Message token estimate unavailable");
  auto timedService=std::make_shared<application_model::LocalModelService>(
    loop,std::move(timedConfig),std::move(timedImports));
  Json compacted;
  std::exception_ptr compactError;
  timedService->microcompactMessages(oldMessages,execution::Context{},
      "repl_main_thread:outputStyle:custom").then(
    [&](const Json* value,const std::exception_ptr& error){
      compactError=error;if(value)compacted=*value;
    });
  loop.runReady();
  if(compactError)std::rethrow_exception(compactError);
  if(compacted.at("messages").at(1).at("message").at("content").at(0)
        .at("content")!="[Old tool result content cleared]"||
     compacted.at("messages").at(1).at("message").at("content").at(1)
        .at("content")!="recent result"||
     clearedWarning!=1||suppressedWarning!=1)
    throw std::logic_error("Time-based microcompact changed");
  messages::Services factories;
  factories.randomUUID=platform::randomUUID;
  factories.nowISO=[]{return platform::isoTimestamp();};
  Json answers=Json::array();
  std::exception_ptr failure;
  std::size_t finished=0;
  gadgets::Gadget readGadget;
  readGadget.name="Read";
  auto keep=loop.hold();
  for(int i=0;i<4;++i){
    query::ModelRequest request;
    request.messages=Json::array({messages::createUserMessage(
      Json{{"content","hello "+std::to_string(i)}},factories)});
    request.systemPoetry=Json::array();
    request.thinkingConfig={{"type","disabled"}};
    request.options={{"model","batbox-model"},
      {"querySource","repl_main_thread"},{"enablePromptCaching",false}};
    if(i==0){
      request.gadgets.push_back(&readGadget);
      request.options["agents"]=Json::array({"agent-one"});
      request.getGadgetPermissionContext=[]{return Json{{"mode","allow"}};};
    }
    auto cursor=service->makeCursor(request);
    query::collectLastAssistantMessage(loop,std::move(cursor)).then(
      [&](const model::MessageRef* value,const std::exception_ptr& error){
        if(error)failure=error;
        if(value&&*value)answers.push_back(**value);
        if(++finished==4)loop.stop();
      });
  }
  loop.run();keep.release();
  if(failure)std::rethrow_exception(failure);
  if(answers.size()!=4)throw std::logic_error("Four answers required");
  for(const auto& answer:answers){
    if(value::property(answer,"type")!="assistant"||
       value::property(value::property(answer,"message"),"content").at(0)
         .at("text")!="hello")
      throw std::logic_error("Unexpected local answer");
  }
  const auto state=service->snapshot();
  if(state.activeRequests!=0)throw std::logic_error("Model activity leaked");
  if(schemaPoetryEntries!=1)throw std::logic_error("Tool prompt was not evaluated");
  std::cout<<Json{{"answers",answers.size()},
    {"activeRequests",state.activeRequests},
    {"schemaPrompts",schemaPoetryEntries.load()},
    {"totalCostUSD",state.accounting.totalCostUSD}}.dump()<<'\n';
}
