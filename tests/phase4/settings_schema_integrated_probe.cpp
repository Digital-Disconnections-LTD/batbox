#include "settings/schema/IndigenousBindings.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto test=batbox::value::parse(line);
    batbox::settings::schema::MainConfig config;
    for(const auto& flag:test.at("flags")){
      if(flag=="XAA")config.xaa=true;
      if(flag=="TRANSCRIPT_CLASSIFIER")config.transcriptClassifier=true;
      if(flag=="INTERNAL")config.internal=true;
    }
    const auto validator=batbox::settings::schema::bindIndigenousSettingsSchema(config,{});
    const auto result=validator(test.at("value"));
    std::cout<<batbox::value::stringify(result.success?
      batbox::Json{{"success",true},{"data",result.data}}:
      batbox::Json{{"success",false},{"issues",result.issues}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}}
