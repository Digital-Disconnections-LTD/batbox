#include "mcp/ProcessResult.hpp"
#include "mcp/StdioClient.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>

namespace {
void require(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
class OwnerExecutor {
 public:
  OwnerExecutor():worker_([this]{run();}){}
  ~OwnerExecutor(){
    {std::lock_guard lock(mutex_);closing_=true;}
    ready_.notify_all();worker_.join();
  }
  void post(std::function<void()> callback){
    {std::lock_guard lock(mutex_);jobs_.push_back(std::move(callback));}
    ready_.notify_one();
  }
  std::thread::id threadId() const{return worker_.get_id();}
 private:
  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<std::function<void()>> jobs_;
  bool closing_=false;
  std::thread worker_;
  void run(){
    while(true){
      std::function<void()> callback;
      {std::unique_lock lock(mutex_);
       ready_.wait(lock,[&]{return closing_||!jobs_.empty();});
       if(jobs_.empty()&&closing_)return;
       callback=std::move(jobs_.front());jobs_.pop_front();}
      callback();
    }
  }
};
}
int main(int argc,char** argv){
  require(argc==2,"expected stdio fixture path");
  auto client=std::make_shared<batbox::mcp::StdioClient>(
    std::vector<std::string>{"python3",argv[1]},"fixture",std::chrono::seconds(3));
  auto connection=batbox::mcp::makeStdioConnection(client,"fixture");
  OwnerExecutor owner;
  connection->postToOwner=[&](std::function<void()> callback){owner.post(std::move(callback));};
  batbox::mcp::Services services;
  services.wire.searchHintMetaKey="test/searchHint";
  services.wire.alwaysLoadMetaKey="test/alwaysLoad";
  services.nowMs=[](){return std::int64_t{1000};};
  services.isOutputLineTruncated=[](const std::string&){return false;};
  services.renderGadgetUseMessage=[](const batbox::Json&,const batbox::Json&){
    return batbox::Json();};
  services.renderGadgetResultMessage=[](const batbox::Json&,const batbox::Json&,
      const batbox::Json&){return batbox::Json();};
  services.renderGadgetUseProgressMessage=[](const batbox::Json&,const batbox::Json&){
    return batbox::Json();};
  batbox::mcp::ProcessResultServices processor;
  processor.result.nowMs=[](){return std::int64_t{1000};};
  processor.result.randomSuffix=[](){return "fixture";};
  processor.validation.configuredMaxOutputTokens=25000;
  batbox::mcp::bindIndigenousProcessResult(services,processor);
  const auto gadgets=batbox::mcp::fetchGadgetsForClient(connection,services);
  require(gadgets.size()==3,"factory should expose three tools");
  const auto& echo=gadgets[0];
  require(echo.name=="mcp__fixture__echo","qualified name");
  require(echo.isMcp&&*echo.isMcp,"MCP identity");
  require(echo.searchHint&&*echo.searchHint=="line one","search hint whitespace");
  require(echo.alwaysLoad&&*echo.alwaysLoad,"always-load metadata");
  require(echo.inputJSONSchema["required"][0]=="text","native input schema");
  require(echo.poetry({}).get()=="Echo the given text","description prompt");
  require(echo.isReadOnly({})&&echo.isConcurrencySafe({}),"readonly annotation");
  require(echo.userFacingName({})=="fixture - Echo (MCP)","user-facing title");
  require(echo.checkPermissions({},{}).get()["behavior"]=="passthrough",
    "MCP permission must pass through");
  const auto parent=batbox::Json{{"message",{{"content",batbox::Json::array(
    {{{"type","tool_use"},{"id","test-tool-use"}}})}}}};
  const auto noPermission=[](const batbox::Json&){
    std::promise<batbox::Json> ready;ready.set_value(batbox::Json());return ready.get_future();};
  std::vector<batbox::gadgets::GadgetProgress> progress;
  std::mutex progressMutex;std::condition_variable progressReady;
  bool allOnOwner=true;
  const auto report=[&](const batbox::gadgets::GadgetProgress& value){
    std::lock_guard lock(progressMutex);
    allOnOwner&=std::this_thread::get_id()==owner.threadId();
    progress.push_back(value);progressReady.notify_all();};
  const auto outcome=echo.call({{"text","hello"}}, {},noPermission,parent,report).get();
  {std::unique_lock lock(progressMutex);
   require(progressReady.wait_for(lock,std::chrono::seconds(2),[&]{return progress.size()==3;}),
     "progress owner dispatch timeout");}
  require(outcome.data=="{\"echo\":\"hello\"}","structured content precedence");
  require(outcome.mcpMeta["_meta"]["origin"]=="fixture","result metadata");
  require(outcome.mcpMeta["structuredContent"]["echo"]=="hello",
    "structured content metadata");
  require(progress.size()==3,"started, progress, completed");
  require(progress[0].gadgetUseID=="test-tool-use"&&
    progress[0].data["status"]=="started","start progress");
  require(progress[1].data["status"]=="progress"&&
    progress[1].data["progressMessage"]=="halfway","transport progress");
  require(progress[2].data["status"]=="completed","completed progress");
  require(allOnOwner,"progress must run on owner strand");
  const auto& errorGadget=gadgets[2];
  std::vector<batbox::gadgets::GadgetProgress> failedProgress;
  bool failed=false;
  try{(void)errorGadget.call(batbox::Json::object(),{},noPermission,parent,
    [&](const auto& value){
      std::lock_guard lock(progressMutex);failedProgress.push_back(value);
      progressReady.notify_all();}).get();}
  catch(const batbox::mcp::GadgetCallError& error){
    failed=std::string(error.what())=="tool failure"&&error.meta["reason"]=="fixture";}
  {std::unique_lock lock(progressMutex);
   require(progressReady.wait_for(lock,std::chrono::seconds(2),[&]{return failedProgress.size()==3;}),
     "failed progress owner dispatch timeout");}
  require(failed,"MCP tool error with metadata");
  require(failedProgress.size()==3&&failedProgress.back().data["status"]=="failed",
    "failed progress");
  const auto& slow=gadgets[1];
  std::stop_source stop;
  batbox::gadgets::GadgetCallContext context;context.stopToken=stop.get_token();
  auto pending=slow.call({{"text","cancelled"},{"delay",0.3}},context,
    noPermission,parent,{});
  std::this_thread::sleep_for(std::chrono::milliseconds(50));stop.request_stop();
  require(pending.get().data.is_null(),"aborted call should return undefined content");
  require(echo.call({{"text","after cancel"}},{},noPermission,parent,{}).get().data==
    "{\"echo\":\"after cancel\"}","transport survives cancellation");
  std::cout<<"native MCP factory, schema, permissions, progress, metadata, errors and cancellation passed\n";
}
