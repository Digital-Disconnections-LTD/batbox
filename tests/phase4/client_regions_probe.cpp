#include "client_factory/Regions.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json reads=Json::array();
    client_factory::RegionServices services;
    services.vertexRegionOverrides={
      {"claude-haiku-4-5","VERTEX_REGION_CLAUDE_HAIKU_4_5"},
      {"claude-3-5-haiku","VERTEX_REGION_CLAUDE_3_5_HAIKU"},
      {"claude-3-5-sonnet","VERTEX_REGION_CLAUDE_3_5_SONNET"},
      {"claude-3-7-sonnet","VERTEX_REGION_CLAUDE_3_7_SONNET"},
      {"claude-opus-4-1","VERTEX_REGION_CLAUDE_4_1_OPUS"},
      {"claude-opus-4","VERTEX_REGION_CLAUDE_4_0_OPUS"},
      {"claude-sonnet-4-6","VERTEX_REGION_CLAUDE_4_6_SONNET"},
      {"claude-sonnet-4-5","VERTEX_REGION_CLAUDE_4_5_SONNET"},
      {"claude-sonnet-4","VERTEX_REGION_CLAUDE_4_0_SONNET"}};
    services.environment=[&](const std::string& key)->
        std::optional<std::string>{
      reads.push_back(key);
      const auto env=test.value("env",Json::object());
      if(!env.contains(key)||env[key].is_null())return std::nullopt;
      return env[key].get<std::string>();
    };
    const auto op=test.value("op",std::string("vertex"));
    std::string value;
    if(op=="aws")value=client_factory::getAwsRegion(services);
    else if(op=="default")value=
      client_factory::getDefaultVertexRegion(services);
    else{
      std::optional<std::string> model;
      if(test.contains("model")&&!test["model"].is_null())
        model=test["model"].get<std::string>();
      value=client_factory::getVertexRegionForModel(model,services);
    }
    std::cout<<value::stringify(Json{{"value",value},
      {"reads",reads}})<<'\n';
  }
}
