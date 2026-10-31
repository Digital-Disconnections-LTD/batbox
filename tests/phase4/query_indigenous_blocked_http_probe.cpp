#include "query/IndigenousModelCursor.hpp"
#include "query_async_e2e_model_fixture.hpp"
#include <chrono>
#include <iostream>

using namespace batbox;
namespace {
std::optional<model::MessageRef> await(event_loop::Loop& loop,
    event_loop::Deferred<std::optional<model::MessageRef>> pending){
  std::optional<std::optional<model::MessageRef>> result;
  std::exception_ptr error;
  pending.then([&](const std::optional<model::MessageRef>* value,
      const std::exception_ptr& rejected){
    error=rejected;if(value)result=*value;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);
  if(!result)throw std::logic_error("blocked HTTP pull did not settle");
  return *result;
}
}
int main(int argc,char** argv){
  if(argc<2)return 2;
  event_loop::Loop loop;
  QueryE2EModelFixture fixture;fixture.baseUrl=argv[1];
  model::PreflightInputs input;
  input.options={{"model","batbox-model"},{"querySource","sdk"}};
  input.thinkingConfig={{"type","disabled"}};
  std::stop_source stop;
  auto services=fixture.makeServices();
  services.signal=stop.get_token();
  services.environment["BATBOX_DISABLE_NONSTREAMING_FALLBACK"]="1";
  auto cursor=query::makeIndigenousQueryModelCursor(loop,std::move(input),
    std::move(services),[]{});
  const auto start=std::chrono::steady_clock::now();
  auto timer=loop.setTimeout(80,[&]{stop.request_stop();});
  Json emitted=Json::array();std::string error;
  try{
    for(int i=0;i<20;++i){
      const auto value=await(loop,cursor.next());
      if(!value)break;
      emitted.push_back(**value);
    }
  }catch(const std::exception& caught){error=caught.what();}
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now()-start).count();
  std::cout<<Json{{"emitted",emitted},{"error",error},
    {"elapsedMs",elapsed},{"stopRequested",stop.stop_requested()}}.dump()<<'\n';
}
