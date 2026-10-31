#include "termio/Querier.hpp"
#include "termio/Csi.hpp"
#include "termio/OscLink.hpp"
namespace batbox::termio::query {
namespace {
bool type(const ResponseRef& response,const std::string& kind){
  const auto& value=value::property(*response,"type");return value.is_string()&&value.get<std::string>()==kind;
}
bool number(const ResponseRef& response,const std::string& key,double expected){
  const auto& value=value::property(*response,key);return value.is_number()&&value.get<double>()==expected;
}
}
TerminalQuery decrqm(double mode){
  return {csi(Json::array({"?"+value::numberToString(mode)+"$p"})),[mode](const ResponseRef& r){return type(r,"decrpm")&&number(r,"mode",mode);}};
}
TerminalQuery da1(){return {csi(Json::array({"c"})),[](const ResponseRef& r){return type(r,"da1");}};}
TerminalQuery da2(){return {csi(Json::array({">c"})),[](const ResponseRef& r){return type(r,"da2");}};}
TerminalQuery kittyKeyboard(){return {csi(Json::array({"?u"})),[](const ResponseRef& r){return type(r,"kittyKeyboard");}};}
TerminalQuery cursorPosition(){return {csi(Json::array({"?6n"})),[](const ResponseRef& r){return type(r,"cursorPosition");}};}
TerminalQuery oscColor(double code,const std::string& terminal){
  return {osc(Json::array({code,"?"}),terminal),[code](const ResponseRef& r){return type(r,"osc")&&number(r,"code",code);}};
}
TerminalQuery xtversion(){return {csi(Json::array({">0q"})),[](const ResponseRef& r){return type(r,"xtversion");}};}
}
