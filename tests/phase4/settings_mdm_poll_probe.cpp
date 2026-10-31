#include "settings/persistence/MdmPoll.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  using namespace batbox::settings::persistence;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array(),current=nullptr;
    std::function<void()> tick;
    MdmPollServices services;
    services.getMdmSettings=[&](){trace.push_back({"initialMdm"});
      const auto& value=test["initial"]["mdm"];
      return batbox::settings::mdm::MdmResult{value["settings"],
        value["errors"]};};
    services.getHkcuSettings=[&](){trace.push_back({"initialHkcu"});
      const auto& value=test["initial"]["hkcu"];
      return batbox::settings::mdm::MdmResult{value["settings"],
        value["errors"]};};
    services.refreshMdmSettings=[&](auto done){
      trace.push_back({"refresh"});
      if(current.contains("error"))
        done(std::nullopt,current["error"].get<std::string>());
      else done(MdmValues{
        {current["mdm"]["settings"],current["mdm"]["errors"]},
        {current["hkcu"]["settings"],current["hkcu"]["errors"]}},
        std::nullopt);
    };
    services.setMdmSettingsStash=[&](const MdmValues& value){
      trace.push_back({"cache",
        Json{{"settings",value.mdm.settings},{"errors",value.mdm.errors}},
        Json{{"settings",value.hkcu.settings},{"errors",value.hkcu.errors}}});};
    services.setInterval=[&](double delay,std::function<void()> callback,
        bool unref){
      trace.push_back({"interval",delay});tick=std::move(callback);
      if(unref)trace.push_back({"unref"});
      return [](){};
    };
    services.debug=[&](const std::string& text){trace.push_back({"log",text});};
    services.fanOutPolicySettings=[&](){
      trace.push_back({"emit","policySettings"});};
    services.pollIntervalMs=17;
    MdmPoll poll(std::move(services));poll.start();
    for(const auto& item:test["ticks"]){current=item;tick();}
    std::cout<<value::stringify(trace)<<'\n';
  }
}
