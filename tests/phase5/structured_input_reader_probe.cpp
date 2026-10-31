#include "cli/structured/InputReader.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array(),snapshots=Json::array();
    auto control=std::make_shared<cli::structured::ControlResponses>(
      [](const Json&){});
    auto outbound=std::make_shared<cli::structured::AsyncStream<Json>>(loop);
    auto producer=std::make_shared<cli::structured::RequestProducer>(loop,
      control,outbound,cli::structured::RequestProducerServices{});
    if(input.value("pending",false)){
      cli::structured::PendingRequest pending;
      pending.request=Json{{"type","control_request"},{"request_id","r"},
        {"request",Json{{"subtype","can_use_tool"},
          {"tool_use_id","t"}}}};
      pending.resolve=[](const Json&){};
      pending.reject=[&](const std::string& error){
        events.push_back(Json::array({"reject",error}));
      };
      control->addPendingRequest("r",std::move(pending));
    }
    const auto chunks=input.value("chunks",Json::array());
    std::size_t at=0;
    cli::structured::InputReaderServices services;
    services.nextChunk=[&]{
      event_loop::Deferred<std::optional<std::string>> result(loop);
      if(at<chunks.size())result.resolve(chunks.at(at++).get<std::string>());
      else result.resolve({});
      return result;
    };
    services.logParsedMessageType=[&](const std::string& type){
      events.push_back(Json::array({"parsed",type}));
    };
    services.processLine.logForDebugging=[](const auto&,const auto&){};
    auto reader=std::make_shared<cli::structured::InputReader>(loop,control,
      producer,services);
    for(const auto& step:input.at("steps")){
      const auto action=step.at(0).get<std::string>();
      if(action=="prepend")reader->prependUserMessage(
        step.at(1).get<std::string>());
      else if(action=="next"){
        std::optional<cli::structured::StreamItem<Json>> item;
        std::exception_ptr error;
        reader->next().then([&](const auto* result,const auto& failure){
          error=failure;if(result)item=*result;
        });
        loop.runReady();
        if(error)std::rethrow_exception(error);
        if(!item)throw std::runtime_error("Reader did not settle");
        snapshots.push_back(Json{{"done",item->done},
          {"value",item->value?*item->value:Json()},
          {"inputClosed",producer->inputClosed()},{"events",events}});
      }
    }
    std::cout<<value::stringify(Json{{"snapshots",snapshots},
      {"events",events}})<<'\n';
  }
}
