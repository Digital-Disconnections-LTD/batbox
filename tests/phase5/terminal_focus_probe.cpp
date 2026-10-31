#include "termio/Focus.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>
int main(){using namespace batbox;Json cases;std::cin>>cases;Json all=Json::array();for(const auto& item:cases){termio::FocusModule focus;Json log=Json::array(),results=Json::array();std::map<std::string,termio::FocusModule::Callback> listeners;std::map<std::string,std::function<void()>> removes;
 for(const auto& op:item.at("steps")){try{const auto kind=op.at("op").get<std::string>();
  if(kind=="subscribe"){const auto id=op.at("id").get<std::string>();if(!listeners.contains(id))listeners[id]=std::make_shared<bootstrap::Signal<>::Listener>([&,op,id]{log.push_back(Json::array({id,focus.getTerminalFocusState()}));if(op.value("throw",false))throw std::runtime_error("listener failed");if(op.contains("remove"))removes.at(op.at("remove"))();if(op.value("add",false)&&!listeners.contains("added")){listeners["added"]=std::make_shared<bootstrap::Signal<>::Listener>([&]{log.push_back(Json::array({"added",focus.getTerminalFocusState()}));});removes["added"]=focus.subscribeTerminalFocus(listeners["added"]);}});removes[op.value("key",id)]=focus.subscribeTerminalFocus(listeners.at(id));}
  else if(kind=="unsubscribe")removes.at(op.at("key"))();else if(kind=="focus")focus.setTerminalFocused(op.at("value"));else if(kind=="reset")focus.resetTerminalFocusState();
  results.push_back(Json{{"state",focus.getTerminalFocusState()},{"focused",focus.getTerminalFocused()}});
 }catch(const std::exception& e){results.push_back(Json{{"error",e.what()},{"state",focus.getTerminalFocusState()}});}}
 all.push_back(Json{{"results",results},{"log",log}});}std::cout<<value::stringify(all)<<'\n';}
