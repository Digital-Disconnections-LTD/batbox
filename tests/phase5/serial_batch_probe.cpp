#include "cli/remote/SerialBatchEventUploader.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
Json run(const Json& test){
  event_loop::Loop loop;
  Json batches=Json::array(),dropped=Json::array();
  int failures=test.value("failures",0);
  SerialBatchConfig config;
  config.maxBatchSize=test.value("maxBatchSize",2);
  config.maxQueueSize=test.value("maxQueueSize",100);
  if(test.contains("maxBatchBytes"))
    config.maxBatchBytes=test["maxBatchBytes"].get<std::size_t>();
  if(test.contains("maxConsecutiveFailures"))
    config.maxConsecutiveFailures=
      test["maxConsecutiveFailures"].get<std::size_t>();
  config.randomUnit=[]{return 0.0;};
  config.jsonStringify=[](const Json& value){return value::stringify(value);};
  config.onBatchDropped=[&](std::size_t size,std::size_t count){
    dropped.push_back(Json::array({size,count}));
  };
  config.send=[&](const std::vector<Json>& batch){
    batches.push_back(batch);
    event_loop::Deferred<event_loop::Unit> result(loop);
    if(failures-- >0)
      result.reject(std::make_exception_ptr(RetryableError("retry",0)));
    else result.resolve({});
    return result;
  };
  auto uploader=std::make_shared<SerialBatchEventUploader>(loop,config);
  std::vector<event_loop::Deferred<event_loop::Unit>> enqueues;
  for(const auto& items:test.at("enqueues"))
    enqueues.push_back(uploader->enqueue(items.get<std::vector<Json>>()));
  auto completion=uploader->flush();
  for(int turn=0;turn<1000&&!completion.settled();++turn){
    loop.runReady();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(!completion.settled())throw std::runtime_error("Uploader did not drain");
  const auto before=uploader->pendingCount();
  uploader->close();
  return Json{{"batches",batches},{"dropped",dropped},
    {"dropCount",uploader->droppedBatchCount()},
    {"beforeClose",before},{"afterClose",uploader->pendingCount()}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    std::cout<<value::stringify(run(value::parse(line)))<<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
