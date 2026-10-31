#include "cli/remote/WorkerStateUploader.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
Json run(const Json& input){
  if(input.value("kind",std::string())=="coalesce")
    return coalesceWorkerPatches(input.at("base"),input.at("overlay"));
  event_loop::Loop loop;
  Json sent=Json::array();
  int fail=input.value("fail",0);
  WorkerStateConfig config;
  config.randomUnit=[]{return 0.0;};
  config.send=[&](const Json& body){
    sent.push_back(body);
    event_loop::Deferred<bool> result(loop);
    result.resolve(fail--<=0);
    return result;
  };
  auto uploader=std::make_shared<WorkerStateUploader>(loop,config);
  for(const auto& patch:input.at("patches"))uploader->enqueue(patch);
  for(int i=0;i<30;++i){
    loop.runReady();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  uploader->close();
  return Json{{"sent",sent}};
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
