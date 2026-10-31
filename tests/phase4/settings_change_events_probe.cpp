#include "settings/persistence/ChangeEvents.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>

int main(){
  using namespace batbox;
  using namespace batbox::settings;
  using namespace batbox::settings::persistence;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    std::map<int,std::function<void()>> timers;
    int nextTimer=1;
    InternalWrites writes([](){return 0.0;});
    ChangeEventServices services;
    services.paths.getPathForSource=[](Source source)
      ->std::optional<std::string>{
      switch(source){
        case Source::User:return "/cfg/settings.json";
        case Source::Project:return "/project/settings.json";
        case Source::Local:return "/project/settings.local.json";
        case Source::Flag:return "/tmp/flag";
        case Source::Policy:return "/cfg/managed.json";
      }
      return std::nullopt;
    };
    services.paths.getManagedDropInDir=[](){return "/cfg/managed-settings.d";};
    services.internalWrites=&writes;
    services.debug=[&](const std::string& text){trace.push_back({"log",text});};
    services.executeConfigChangeHooks=[&](const std::string& source,
        const std::string& path,std::function<void(const std::vector<bool>&)> done){
      trace.push_back({"hook",source,path});
      const bool blocked=test.contains("blocked")&&
        test["blocked"].value(path,false);
      done(blocked?std::vector<bool>{true}:std::vector<bool>{});
    };
    services.setTimeout=[&](double delay,std::function<void()> callback){
      const int id=nextTimer++;trace.push_back({"timer",delay});
      timers[id]=std::move(callback);
      return [&,id](){timers.erase(id);trace.push_back({"cancel"});};
    };
    services.resetSettingsStash=[&](){trace.push_back({"reset"});};
    services.deletionGraceMs=17;
    ChangeEvents events(std::move(services));
    events.subscribe([&](Source source){trace.push_back({"emit",sourceKey(source)});});
    for(const auto& op:test.at("ops")){
      const auto kind=op.at("kind").get<std::string>();
      if(kind=="mark")writes.markInternalWrite(op.at("path"));
      else if(kind=="change")events.handleChange(op.at("path"));
      else if(kind=="add")events.handleAdd(op.at("path"));
      else if(kind=="delete")events.handleDelete(op.at("path"));
      else if(kind=="notify")events.notifyChange(sourceFromKey(
        op.at("source").get<std::string>()));
      else if(kind=="tick"){
        const auto pending=std::move(timers);timers.clear();
        for(const auto& [id,callback]:pending){(void)id;callback();}
      }
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
