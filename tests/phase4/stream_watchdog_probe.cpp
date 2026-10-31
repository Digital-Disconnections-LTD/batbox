#include "model/Watchdog.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>

int main(){
  using namespace batbox;
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  Json output=Json::array();
  for(const auto& input:cases){
    struct Timer{double delay;std::function<void()> callback;};
    std::map<int,Timer> timers;int nextId=1;
    Json trace=Json::array();
    auto scheduler=[&](double delay,std::function<void()> callback){
      const int id=nextId++;
      timers.emplace(id,Timer{delay,std::move(callback)});
      trace.push_back(Json::array({"set",id,delay}));
      return std::function<void()>([&,id]{
        trace.push_back(Json::array({"clear",id}));timers.erase(id);
      });
    };
    const double timeout=input["timeoutMs"].get<double>();
    batbox::model::WatchdogSettings settings;
    settings.enabled=input["enabled"].get<bool>();
    settings.timeoutMs=timeout;settings.warningMs=timeout/2;
    auto warning=[&]{
      trace.push_back(Json::array({"debug","warn",
        "Streaming idle warning: no chunks received for "+
          value::numberToString(settings.warningMs/1000)+"s"}));
      trace.push_back(Json::array({"diagnostic","warn","cli_streaming_idle_warning"}));
    };
    auto timedOut=[&]{
      trace.push_back(Json::array({"debug","error",
        "Streaming idle timeout: no chunks received for "+
          value::numberToString(settings.timeoutMs/1000)+"s, aborting stream"}));
      trace.push_back(Json::array({"diagnostic","error","cli_streaming_idle_timeout"}));
    };
    bool aborted=false;
    {
      batbox::model::StreamWatchdog watchdog(settings,[&]{trace.push_back(Json::array({"abort"}));},
        warning,timedOut,scheduler);
      for(const auto& value:input["actions"]){
        const auto action=value.get<std::string>();
        if(action=="reset")watchdog.reset();
        if(action=="clear")watchdog.clear();
        if(action=="warning"||action=="timeout"){
          const double delay=action=="warning"?settings.warningMs:settings.timeoutMs;
          for(auto it=timers.begin();it!=timers.end();++it)if(it->second.delay==delay){
            auto callback=std::move(it->second.callback);timers.erase(it);
            callback();break;
          }
        }
      }
      aborted=watchdog.aborted();
    }
    output.push_back({{"trace",trace},{"aborted",aborted}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
