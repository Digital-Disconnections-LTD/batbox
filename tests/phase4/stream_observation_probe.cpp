#include "model/StreamObservation.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  Json output=Json::array();
  for(const auto& item:cases){
    std::size_t clockIndex=0;
    Json logs=Json::array();
    batbox::model::StreamObservation observation(
      [&]{return item["times"][clockIndex++].get<std::int64_t>();},
      [&](const std::string& message,const std::string& level){
        logs.push_back(Json::array({level,message}));});
    for(const auto& _:item["times"]){(void)_;observation.onEvent();}
    const bool started=item["hasMessageStart"].get<bool>();
    const auto count=item["completedMessages"].get<std::size_t>();
    const bool stopped=item["hasStopReason"].get<bool>();
    observation.beforeFinish(started,count,stopped);
    Json error=nullptr;
    if(!started||(count==0&&!stopped))error="Stream ended without receiving any events";
    output.push_back({{"stallCount",observation.stallCount()},
      {"totalStallTime",observation.totalStallTimeMs()},
      {"error",error},{"logs",logs}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
