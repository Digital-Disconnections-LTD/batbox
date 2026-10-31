#include "settings/schema/Main.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Url.hpp"
#include <iostream>

using batbox::Json;
namespace value=batbox::value;
namespace schema=batbox::settings::schema;
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const Json test=value::parse(line);
    const auto& flags=test.at("flags");
    const auto has=[&](const char* name){
      for(const auto& value:flags)if(value==name)return true;
      return false;
    };
    schema::MainConfig config;
    config.xaa=has("XAA");config.transcriptClassifier=
      has("TRANSCRIPT_CLASSIFIER");
    config.lodestone=has("LODESTONE");config.proactive=has("PROACTIVE");
    config.kairos=has("KAIROS");config.voiceMode=has("VOICE_MODE");
    config.kairosBrief=has("KAIROS_BRIEF");config.internal=has("INTERNAL");
    schema::MainImports imports;
    const auto any=[](const Json& value){
      return batbox::settings::SchemaResult{true,value,Json::array()};};
    imports.hooks=any;imports.sandbox=any;imports.marketplaceSource=any;
    imports.canonicalUrl=batbox::runtime::canonicalUrl;
    const auto result=schema::settings(test.at("value"),config,imports);
    std::cout<<value::stringify(result.success?Json{{"success",true},
      {"data",result.data}}:Json{{"success",false},
      {"issues",result.issues}})<<'\n';
  }catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
