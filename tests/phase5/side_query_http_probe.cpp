#include "side_query/IndigenousBinding.hpp"
#include "memory_prefetch/Bindings.hpp"
#include "query/MemoryAttachmentFilter.hpp"
#include "model_config/ModelConfig.hpp"
#include "transport/AsyncMessages.hpp"
#include "transport/Errors.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=3)return 2;
  const std::string endpoint=argv[1];
  const std::filesystem::path directory=argv[2];
  std::filesystem::create_directories(directory);
  {std::ofstream note(directory/"notes.md");
    note<<"---\ndescription: local setup\n---\n\nUse the native tool binding.\n";}
  event_loop::Loop loop;
  client_factory::Services factory;
  factory.environment=[](const std::string&)->Json{return nullptr;};
  factory.getUserBat=[](){return "batbox-test/1";};
  factory.checkAndRefreshAuth=[&]{
    event_loop::Deferred<event_loop::Unit> result(loop);result.resolve({});return result;
  };
  factory.isSubscriber=[](){return false;};
  factory.isNonInteractive=[](){return false;};
  factory.apiKeyHelper=[&](bool){
    event_loop::Deferred<std::optional<std::string>> result(loop);
    result.resolve(std::nullopt);return result;
  };
  factory.getProxyFetchOptions=[](){return client_factory::NetworkOptions{};};
  factory.getApiKey=[]()->std::optional<std::string>{return "fixture-key";};
  factory.debugToStderr=[](){return false;};
  factory.directBaseUrl=endpoint;
  side_query::Services side;
  side_query::bindIndigenousClientFactory(loop,side,factory);
  side.getModelBetas=[](const std::string&){return std::vector<std::string>{"memory-beta"};};
  side.supportsStructuredOutputs=[](const std::string&){return false;};
  side.systemPoetryPrefix=[](const Json&){return "Batbox";};
  side.normalizeModel=model_config::normalizeModelStringForAPI;
  side.nowMillis=[](){return 1700000000000.;};
  int completions=0;
  side.setLastApiCompletionTimestamp=[&](double){++completions;};
  instructions::MemoryPathStash paths;
  memory_prefetch::Services memory{loop};
  memory.pathStash=&paths;memory.cwd=directory.string();
  memory.paths.projectRoot=directory.string();memory.paths.configHomeDir=directory.string();
  memory.paths.environment={{memory.paths.keys.fullPathOverride,directory.string()}};
  memory.searchFeatureEnabled=[](){return true;};
  memory.defaultStandardModel=[](){return "local[1m]";};
  memory.logWarning=[](const std::string& message){throw std::runtime_error(message);};
  memory.logError=[](std::exception_ptr error){std::rethrow_exception(error);};
  memory.isAbortError=[](const std::exception& error){
    return dynamic_cast<const transport::APIUserAbortError*>(&error)!=nullptr;
  };
  memory.nowMillis=side.nowMillis;
  memory_prefetch::bindSideQuery(memory,side);
  query::AsyncEngineServices engine;query::AsyncGadgetTurnServices gadgetTurn;
  memory_prefetch::bindQueryPrefetch(engine,gadgetTurn,memory);
  execution::Context context;
  context.readFileState=file_state_stash::createFileStateStashWithSizeLimit(100);
  context.data={{"options",{{"agentDefinitions",{{"activeAgents",Json::array()}}}}}};
  auto prefetch=engine.startRelevantMemoryPrefetch(Json::array({
    Json{{"type","user"},{"message",{{"content","Find local setup memory"}}}}}),context);
  bool heartbeat=false;
  auto timer=loop.setTimeout(1,[&]{heartbeat=true;});
  for(int i=0;i<10000&&!prefetch.isSettled();++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(prefetch.isSettled()&&heartbeat&&completions==1);
  const auto note=(directory/"notes.md").string();
  assert(!context.readFileState->has(note));
  Json attachments;
  auto read=gadgetTurn.readMemoryPrefetch(prefetch);
  read.then([&](const Json* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);attachments=*value;
  });
  loop.runReady();assert(attachments.size()==1);
  assert(attachments[0]["memories"][0]["content"].get<std::string>().find(
    "native tool binding")!=std::string::npos);
  auto accepted=query::filterDuplicateMemoryAttachments(attachments,*context.readFileState);
  assert(accepted.size()==1&&context.readFileState->has(note));
  assert(query::filterDuplicateMemoryAttachments(attachments,*context.readFileState).empty());
  prefetch.dispose();
  transport::ClientConfig config;config.baseUrl=endpoint;
  auto client=std::make_shared<transport::Client>(config);
  auto check=[&](const std::string& name,int retries,bool expectSuccess,
      bool cancel=false){
    std::stop_source abort;bool done=false,ok=false,aborted=false;
    auto pending=transport::createMessage(loop,client,Json{{"case",name}},retries,
      abort.get_token());
    auto cancelTimer=cancel?loop.setTimeout(50,[&]{abort.request_stop();}):
      event_loop::Loop::TimerHandle{};
    pending.then([&](const transport::Response* response,const std::exception_ptr& error){
      if(error){try{std::rethrow_exception(error);}
        catch(const transport::APIUserAbortError&){aborted=true;}
        catch(const std::exception&){} }
      else ok=response->body.value("ok",false);
      done=true;
    });
    loop.run();assert(done&&ok==expectSuccess&&aborted==cancel);
  };
  check("permanent",2,false);
  check("exhaust",2,false);
  check("force",2,true);
  check("malformed",2,false);
  check("cancel",2,false,true);
  std::cout<<Json{{"memory",true},{"heartbeat",heartbeat},{"completionCount",completions},
    {"retryCases",5}}<<'\n';
}
