#include "instructions/hooks/runtime/CombinedAbort.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;std::stop_source a,b;
  if(item.value("preA",false))a.request_stop();
  if(item.value("preB",false))b.request_stop();
  std::optional<double> timeout;
  if(item.contains("timeout"))timeout=item.at("timeout").get<double>();
  auto combined=hook::CombinedAbort::create(loop,a.get_token(),b.get_token(),timeout);
  const bool before=combined->aborted();
  if(item.value("cleanupFirst",false))combined->cleanup();
  if(item.value("laterA",false))a.request_stop();
  if(item.value("laterB",false))b.request_stop();
  if(item.value("fireTimer",false)){
    auto hold=loop.hold();
    loop.setTimeout(5,[&]{hold.release();});
    loop.run();
  }else loop.runReady();
  const bool after=combined->aborted();
  combined->cleanup();
  return {{"name",item.at("name")},{"before",before},{"after",after},
    {"afterCleanup",combined->aborted()}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  {
    event_loop::Loop loop;
    auto combined=hook::CombinedAbort::create(loop,std::nullopt,std::nullopt,1000);
    loop.run();
    if(combined->aborted())throw std::runtime_error("unreferenced timer kept loop alive");
    combined->cleanup();
  }
  std::cout<<rows.dump()<<'\n';
}
