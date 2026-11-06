#include "utils/tokens.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
void require(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
Json user(const std::string& id,const std::string& text){
  return Json{{"type","user"},{"uuid",id},{"message",{{"role","user"},{"content",text}}}};
}
Json assistant(const std::string& id,const std::string& text,const Json& usage){
  return Json{{"type","assistant"},{"uuid",id},{"message",{{"id","msg-"+id},{"role","assistant"},
    {"model","local-model"},{"content",Json::array({Json{{"type","text"},{"text",text}}})},
    {"usage",usage}}}};
}
Json attachment(const std::string& id){
  return Json{{"type","attachment"},{"uuid",id},{"attachment",{{"type","critical_system_reminder"},
    {"content","reminder"}}}};
}
}
int main(){
  int normalized=0;
  const AttachmentNormalizer normalize=[&](const Json&){
    ++normalized;
    return Json::array({Json{{"message",{{"content",std::string(400,'r')}}}}});
  };
  const Json empty={{"input_tokens",0},{"output_tokens",0}};
  Json messages=Json::array({user("u1",std::string(4000,'a')),attachment("a1"),
    assistant("r1",std::string(800,'b'),empty),user("u2",std::string(400,'c'))});
  const auto whole=roughTokenCountEstimationForMessages(messages,normalize);
  normalized=0;
  require(tokenCountWithEstimation(messages,normalize)==whole&&whole>1000,
    "A response without usage hid the conversation from the estimate");
  require(normalized==1,"The first estimate did not read every message");

  messages.push_back(attachment("a2"));
  messages.push_back(user("u3",std::string(4000,'d')));
  const auto expected=roughTokenCountEstimationForMessages(messages,normalize);
  normalized=0;
  const auto grown=tokenCountWithEstimation(messages,normalize);
  require(grown==expected&&normalized==1,"The estimate was not extended from the previous one");
  normalized=0;
  tokenCountWithEstimation(messages,normalize);
  require(normalized==0,"An unchanged conversation was estimated again");

  Json other=Json::array({user("v1","short"),user("v2",std::string(400,'e'))});
  require(tokenCountWithEstimation(other,normalize)==
    roughTokenCountEstimationForMessages(other,normalize),"A second conversation reused another's estimate");
  Json rewritten=messages;rewritten[3]=user("u2-edited","x");
  require(tokenCountWithEstimation(rewritten,normalize)==
    roughTokenCountEstimationForMessages(rewritten,normalize),"A changed history reused a stale estimate");

  const Json usage={{"input_tokens",50000},{"output_tokens",200},{"cache_read_input_tokens",1000}};
  Json reported=Json::array({user("w1",std::string(4000,'a')),assistant("w2","ok",usage),
    user("w3",std::string(400,'f'))});
  require(tokenCountWithEstimation(reported,normalize)==51200+
    roughTokenCountEstimationForMessage(reported[2],normalize),"Reported usage was not used");
  std::cout<<"Token estimates ignore empty usage and grow with the conversation\n";
}
