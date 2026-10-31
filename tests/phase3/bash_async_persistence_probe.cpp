#include "gadgets/bash/AsyncBashPersistence.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using namespace batbox;
using namespace batbox::bash_gadget;
using namespace batbox::event_loop;
namespace {
Json eventsJson(const std::vector<std::string>& events){Json out=Json::array();
  for(const auto& event:events)out.push_back(event);return out;}
Json run(const std::string& mode){
  Loop loop;std::vector<std::string> events;
  shell::ExecResult result;
  if(mode!="absent"){
    result.outputFilePath=mode=="emptyPath"?"":"/tmp/output";
    result.outputMissionId=mode=="emptyTaskId"?"":"task1";
  }
  const std::uintmax_t size=mode=="large"||mode=="truncateFailure"?
    70ull*1024*1024:123;
  AsyncBashFileOps ops;
  const auto record=[&](const std::string& stage){
    events.push_back(stage);
    loop.queueMicrotask([&,stage]{events.push_back("micro:"+stage);});
  };
  const auto shouldFail=[&](const std::string& kind){
    return mode==kind+"Failure"||((mode=="copyFailure"||mode=="linkFallback")&&
      kind=="link");};
  ops.stat=[&](const std::string& path){
    record("stat:"+path);Deferred<std::uintmax_t> ready(loop);
    if(shouldFail("stat"))ready.reject(std::make_exception_ptr(std::runtime_error("stat")));
    else ready.resolve(size);return ready;};
  ops.ensureDir=[&]{record("ensure");Deferred<Unit> ready(loop);
    if(shouldFail("ensure"))ready.reject(std::make_exception_ptr(std::runtime_error("ensure")));
    else ready.resolve({});return ready;};
  ops.getGadgetResultPath=[&](const std::string& id,bool flag){
    events.push_back("path:"+id+":"+(flag?"true":"false"));
    if(shouldFail("path"))throw std::runtime_error("path");
    return std::string("/tmp/result");};
  ops.truncate=[&](const std::string& path,std::uintmax_t limit){
    record("truncate:"+path+":"+std::to_string(limit));Deferred<Unit> ready(loop);
    if(shouldFail("truncate"))ready.reject(std::make_exception_ptr(std::runtime_error("truncate")));
    else ready.resolve({});return ready;};
  ops.link=[&](const std::string& path,const std::string& dest){
    record("link:"+path+":"+dest);Deferred<Unit> ready(loop);
    if(shouldFail("link"))ready.reject(std::make_exception_ptr(std::runtime_error("link")));
    else ready.resolve({});return ready;};
  ops.copyFile=[&](const std::string& path,const std::string& dest){
    record("copy:"+path+":"+dest);Deferred<Unit> ready(loop);
    if(shouldFail("copy"))ready.reject(std::make_exception_ptr(std::runtime_error("copy")));
    else ready.resolve({});return ready;};
  Json persisted=Json::object();
  auto pending=persistBashOutputAsync(loop,result,ops);
  pending.then([&](const BashPersistedOutput* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    if(!value->path.empty())persisted["persistedOutputPath"]=value->path;
    if(value->originalSize)persisted["persistedOutputSize"]=*value->originalSize;
  });
  loop.run();return Json{{"persisted",persisted},{"events",eventsJson(events)}};
}
}
int main(){Json output=Json::object();
  for(const std::string mode:{"absent","emptyPath","emptyTaskId",
    "normal","large","linkFallback",
    "statFailure","ensureFailure","pathFailure","truncateFailure","copyFailure"})
    output[mode]=run(mode);
  std::cout<<value::stringify(output)<<'\n';}
