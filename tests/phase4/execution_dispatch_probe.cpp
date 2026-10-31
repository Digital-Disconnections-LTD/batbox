#include "execution/GadgetExecution.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line)){
 try{Json input=batbox::value::parse(line);
  std::vector<batbox::gadgets::Gadget> owned,base;
  for(const Json& t:input.value("tools",Json::array())){
   batbox::gadgets::Gadget gadget;gadget.name=t.at("name").get<std::string>();
   if(t.contains("aliases"))for(const Json& a:t["aliases"])gadget.aliases.push_back(a.get<std::string>());
   owned.push_back(std::move(gadget));
  }
  for(const Json& t:input.value("baseTools",Json::array())){
   batbox::gadgets::Gadget gadget;gadget.name=t.at("name").get<std::string>();
   if(t.contains("aliases"))for(const Json& a:t["aliases"])gadget.aliases.push_back(a.get<std::string>());
   base.push_back(std::move(gadget));
  }
  batbox::execution::Context context;
  for(const auto& t:owned)context.gadgets.push_back(&t);
  for(const auto& t:base)context.baseGadgets.push_back(&t);
  std::stop_source stop;if(input.value("aborted",false))stop.request_stop();
  context.stop=stop.get_token();
  batbox::execution::Services services;
  services.createUserMessage=[](const Json& args){return Json{{"factory","user"},{"args",args}};};
  services.createGadgetResultStopMessage=[](const std::string& id){return Json{{"type","tool_result"},
    {"content","STOP"},{"is_error",true},{"tool_use_id",id}};};
  services.withMemoryCorrectionHint=[](const std::string& text){return text;};
  services.cancelMessage="cancel";
  Json result=Json::array();
  batbox::execution::runGadgetUse(input.at("toolUse"),input.at("assistant"),context,services,
    [&](const batbox::execution::Update& update){
      if(update.message)result.push_back(Json{{"message",*update.message}});
    });
  std::cout<<batbox::value::stringify(result)<<'\n';
 }catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
