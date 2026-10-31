#include "bootstrap/InvokedSkills.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
Json writeMap(const batbox::bootstrap::InvokedSkillMap& entries){
  Json out=Json::array();
  for(const auto& [key,value]:entries){
    const Json skill={{"skillName",value->skillName},
      {"skillPath",value->skillPath},{"content",value->content},
      {"invokedAt",value->invokedAt},
      {"agentId",value->batId?Json(*value->batId):Json(nullptr)}};
    out.push_back(Json::array({key,skill}));
  }
  return out;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::InvokedSkillsState state;
    const auto original=bootstrap::getInvokedSkills(state);
    std::optional<bootstrap::InvokedSkillMap> captured;
    auto snap=[&](){return Json{
      {"all",writeMap(*bootstrap::getInvokedSkills(state))},
      {"sameMap",bootstrap::getInvokedSkills(state)==original},
      {"main",writeMap(bootstrap::getInvokedSkillsForBat(state,
        std::nullopt))},
      {"a",writeMap(bootstrap::getInvokedSkillsForBat(state,
        std::string("a")))},
      {"b",writeMap(bootstrap::getInvokedSkillsForBat(state,
        std::string("b")))},
      {"captured",captured?writeMap(*captured):Json(nullptr)}};};
    Json trace=Json::array();trace.push_back(snap());
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="add"){
        std::optional<std::string> bat;
        if(op.contains("agent")&&!op["agent"].is_null())
          bat=op["agent"].get<std::string>();
        const double now=op["now"].get<double>();
        bootstrap::addInvokedSkill(state,op["name"].get<std::string>(),
          op["path"].get<std::string>(),op["content"].get<std::string>(),
          bat,[now]{return now;});
      }else if(kind=="capture"){
        std::optional<std::string> bat;
        if(op.contains("agent")&&!op["agent"].is_null())
          bat=op["agent"].get<std::string>();
        captured=bootstrap::getInvokedSkillsForBat(state,bat);
      }else if(kind=="clear"){
        std::optional<std::vector<std::string>> preserve;
        if(op.contains("preserve")&&!op["preserve"].is_null()){
          preserve=std::vector<std::string>();
          for(const auto& bat:op["preserve"])
            preserve->push_back(bat.get<std::string>());
        }
        bootstrap::clearInvokedSkills(state,preserve);
      }else if(kind=="clearAgent")
        bootstrap::clearInvokedSkillsForBat(state,
          op["agent"].get<std::string>());
      else throw std::runtime_error("Unknown invoked-skill operation");
      trace.push_back(snap());
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
