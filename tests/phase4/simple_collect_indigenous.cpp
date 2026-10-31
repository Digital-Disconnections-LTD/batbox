#include "query/SimpleQueriesBinding.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  auto first=std::make_shared<Json>(Json{{"type","assistant"},{"message",{{"content",Json::array()}}}});
  auto last=std::make_shared<Json>(*first);
  bool finished=false,done=false;
  auto cursor=query::makeProducerModelCursor(loop,[&](const auto& yield,std::stop_token){
    yield(first);
    yield(std::make_shared<Json>(Json{{"type","stream_event"}}));
    yield(last);
    (*last)["message"]["usage"]=Json{{"output_tokens",17}};
    finished=true;
  });
  auto result=query::collectLastAssistantMessage(loop,std::move(cursor));
  result.then([&](const model::MessageRef* message,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    assert(finished&&*message==last);
    assert((**message)["message"]["usage"]["output_tokens"]==17);
    done=true;
  });
  loop.run();assert(done);
  for(bool aborted:{false,true}){
    std::stop_source stop;if(aborted)stop.request_stop();
    auto empty=query::makeProducerModelCursor(loop,[](const auto&,std::stop_token){});
    bool rejected=false;
    query::collectLastAssistantMessage(loop,std::move(empty),stop.get_token()).then(
      [&](const model::MessageRef*,const std::exception_ptr& error){
        assert(error);
        try{std::rethrow_exception(error);}
        catch(const model::UserAbortError&){assert(aborted);}
        catch(const std::runtime_error& e){assert(!aborted);assert(std::string(e.what())=="No assistant message found");}
        rejected=true;
      });
    loop.run();assert(rejected);
  }
  std::cout<<"native simple query collector drains final state and retains assistant identity\n";
}
