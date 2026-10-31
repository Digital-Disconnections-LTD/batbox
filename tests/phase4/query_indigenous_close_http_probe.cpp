#include "query/IndigenousModelCursor.hpp"
#include "query_async_e2e_model_fixture.hpp"
#include <chrono>
#include <iostream>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* item,const std::exception_ptr& rejected){
    error=rejected;if(item)value=*item;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::logic_error("HTTP close did not settle");
  return *value;
}
}
int main(int argc,char** argv){
  if(argc<2)return 2;
  event_loop::Loop loop;
  QueryE2EModelFixture fixture;fixture.baseUrl=argv[1];
  model::PreflightInputs input;
  input.options={{"model","batbox-model"},{"querySource","sdk"}};
  input.thinkingConfig={{"type","disabled"}};
  auto services=fixture.makeServices();
  services.environment["BATBOX_DISABLE_NONSTREAMING_FALLBACK"]="1";
  auto cursor=query::makeIndigenousQueryModelCursor(loop,std::move(input),
    std::move(services),[]{});
  const auto first=await(loop,cursor.next());
  if(!first)throw std::logic_error("HTTP stream emitted nothing before close");
  const auto start=std::chrono::steady_clock::now();
  if(!cursor.close)throw std::logic_error("Native model close was not bound");
  (void)await(loop,cursor.close());
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now()-start).count();
  std::cout<<Json{{"first",**first},{"closeMs",elapsed}}.dump()<<'\n';
}
