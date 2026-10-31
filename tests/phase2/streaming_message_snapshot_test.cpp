#include "runtime/MessageSnapshot.hpp"
#include "streaming/Reducer.hpp"
#include <atomic>
#include <cassert>
#include <exception>
#include <iostream>
#include <thread>

int main(){
  using namespace batbox;
  streaming::ReducerConfig config;
  config.model="model";config.resolvedModel="model";
  config.nowMillis=[](){return 10;};
  config.randomUUID=[](){return "message-1";};
  config.timestamp=[](){return "now";};
  config.calculateUSDCost=[](const std::string&,const Json&){return 0.;};
  config.addToTotalFlightCost=[](double,const Json&,const std::string&){return 0.;};
  config.getErrorMessageIfRefusal=[](const Json&,const std::string&)->std::optional<Json>{return std::nullopt;};
  config.createAssistantAPIErrorMessage=[](const std::string&,const std::string&,const std::string&){return Json::object();};
  streaming::Reducer reducer(config);
  reducer.consume(Json{{"type","message_start"},{"message",{{"content",Json::array()},
    {"usage",{{"input_tokens",1},{"output_tokens",0}}}}}});
  reducer.consume(Json{{"type","content_block_start"},{"index",0},
    {"content_block",{{"type","text"},{"text","hello"}}}});
  const auto emissions=reducer.consume(Json{{"type","content_block_stop"},{"index",0}});
  const auto original=emissions.front();
  assert(original==reducer.messages().front());
  auto addedFieldClone=std::make_shared<Json>(runtime::snapshotMessage(original));
  (*addedFieldClone)["message"]["content"][0]["extra"]="backfilled";
  std::atomic<bool> done=false;
  std::exception_ptr readerError;
  std::thread reader([&](){
    try{
      while(!done.load(std::memory_order_acquire)){
        const auto snapshot=runtime::snapshotMessage(original);
        const auto& body=snapshot.at("message");
        if(body.contains("stop_reason")){
          const int reason=std::stoi(body.at("stop_reason").get<std::string>());
          assert(body.at("usage").at("output_tokens")==reason);
        }
      }
    }catch(...){readerError=std::current_exception();}
  });
  for(int i=1;i<=2000;++i){
    reducer.consume(Json{{"type","message_delta"},
      {"usage",{{"output_tokens",i}}},
      {"delta",{{"stop_reason",std::to_string(i)}}}});
  }
  done.store(true,std::memory_order_release);
  reader.join();
  if(readerError)std::rethrow_exception(readerError);
  const auto terminal=runtime::snapshotMessage(original);
  assert(terminal.at("message").at("usage").at("output_tokens")==2000);
  assert(terminal.at("message").at("stop_reason")=="2000");
  assert(!addedFieldClone->at("message").contains("stop_reason"));
  assert(addedFieldClone->at("message").at("content")[0].at("extra")=="backfilled");
  std::cout<<"stream message snapshot locking and message clone semantics passed\n";
}
