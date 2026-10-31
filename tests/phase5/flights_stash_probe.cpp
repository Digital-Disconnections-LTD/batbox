#include "flights/Stash.hpp"
#include <iostream>
using namespace batbox;
Json evaluate(const Json& t){
 Json calls=Json::array(),observations=Json::array();std::vector<std::shared_ptr<runtime::OrderedStringSet>> retained;
 flights::FlightMessageStash stash([&](const std::string& id){
  calls.push_back(id);const auto fail=t.value("fail",Json::array());if(std::find(fail.begin(),fail.end(),Json(id))!=fail.end())throw std::runtime_error("load failed");
  const auto input=t.value("messages",Json::object());
  flights::LoadedTranscript result;for(const auto& uuid:value::property(input,id))result.messages.set(uuid,Json{{"uuid",uuid}});return result;
 });
 for(const auto& step:t.value("steps",Json::array()))try {
  const auto op=step.at("op").get<std::string>();
  if(op=="clear")stash.clearFlightMessagesStash();
  else if(op=="exists")observations.push_back(stash.doesMessageExistInFlight(step.at("id"),step.at("uuid")));
  else if(op=="get"){const auto value=stash.getFlightMessages(step.at("id"));retained.push_back(value);observations.push_back(Json(*value));}
  else if(op=="add")stash.getFlightMessages(step.at("id"))->insert(step.at("uuid"));
  else if(op=="same")observations.push_back(retained.at(step.at("index").get<std::size_t>())==stash.getFlightMessages(step.at("id")));
  else if(op=="old")observations.push_back(Json(*retained.at(step.at("index").get<std::size_t>())));
 }catch(const std::exception& e){observations.push_back(Json{{"error",e.what()}});}
 return Json{{"calls",calls},{"observations",observations}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{const auto result=evaluate(value::parse(line));std::cout<<value::stringify(Json{{"result",result}})<<'\n';}catch(const std::exception& e){std::cout<<value::stringify(Json{{"error",e.what()}})<<'\n';}}
