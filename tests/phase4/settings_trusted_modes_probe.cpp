#include "settings/persistence/TrustedModes.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto test=batbox::value::parse(line);
    batbox::Json reads=batbox::Json::array(),logs=batbox::Json::array();
    batbox::settings::persistence::TrustedModeServices services;
    services.transcriptClassifier=test.value("classifier",false);
    services.internalUser=test.value("internal",false);
    services.getSettingsForSource=[&](batbox::settings::Source source){
      const auto name=source==batbox::settings::Source::User?"user":
        source==batbox::settings::Source::Local?"local":
        source==batbox::settings::Source::Flag?"flag":
        source==batbox::settings::Source::Policy?"policy":"project";
      reads.push_back(name);
      const auto& sources=test.at("sources");
      return sources.contains(name)?sources[name]:batbox::Json(nullptr);
    };
    services.debug=[&](const std::string& text){logs.push_back(text);};
    const auto op=test.at("op").get<std::string>();
    batbox::Json value=nullptr;
    if(op=="hasSkipDangerousModePermissionPrompt")
      value=batbox::settings::persistence::
        hasSkipDangerousModePermissionPoetry(services);
    else if(op=="hasAutoModeOptIn")
      value=batbox::settings::persistence::hasAutoModeOptIn(services);
    else if(op=="getUseAutoModeDuringPlan")
      value=batbox::settings::persistence::
        getUseAutoModeDuringPlan(services);
    else if(op=="getAutoModeConfig"){
      const auto result=batbox::settings::persistence::getAutoModeConfig(services);
      if(result)value=*result;
    }
    std::cout<<batbox::value::stringify({{"value",value},{"reads",reads},
      {"logs",logs}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}}
