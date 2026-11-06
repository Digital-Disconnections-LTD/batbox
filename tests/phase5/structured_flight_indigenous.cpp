#include "cli/structured/StructuredFlight.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;
namespace {
void require(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
}
int main(){
  event_loop::Loop loop;
  std::vector<Json> writes;
  int chunks=0;
  cli::structured::StructuredFlightServices services;
  services.nextChunk=[&]{
    event_loop::Deferred<std::optional<std::string>> result(loop);
    if(chunks++==0)result.resolve(std::string(
      "{\"type\":\"user\",\"message\":{\"role\":\"user\","
      "\"content\":\"next\"}}\n"));
    else result.resolve({});
    return result;
  };
  services.writeOutput=[&](const Json& value){
    writes.push_back(value);
    event_loop::Deferred<event_loop::Unit> result(loop);result.resolve({});
    return result;
  };
  services.processLine.notifyCommandLifecycle=[](const auto&,const auto&){};
  services.processLine.logForDebugging=[](const auto&,const auto&){};
  services.processLine.fatalInput=[](const auto& message){
    throw std::runtime_error(message);
  };
  cli::structured::StructuredFlight flight(loop,std::move(services));
  flight.prependUserMessage("before");
  std::vector<Json> input;
  auto takeInput=[&]{
    flight.nextInput().then([&](const auto* item,const auto& failure){
      if(failure)std::rethrow_exception(failure);
      if(item&&!item->done&&item->value)input.push_back(*item->value);
    });
    loop.runReady();
  };
  takeInput();takeInput();
  require(input.size()==2,"two input messages expected");
  require(value::property(value::property(input[0],"message"),"content")==
    "before","prepended turn must lead");
  require(value::property(value::property(input[1],"message"),"content")==
    "next","streamed turn must follow");
  bool written=false;
  flight.write(Json{{"type","result"}}).then([&](const auto*,const auto& failure){
    if(failure)std::rethrow_exception(failure);
    written=true;
  });
  loop.runReady();
  require(written&&writes.size()==1&&value::property(writes[0],"type")=="result",
    "output must reach the writer");
}
