#include "interactive/lifecycle/MessageState.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  auto initial=std::make_shared<TurnMessages>(TurnMessages{
    std::make_shared<Json>(Json{{"type","system"},{"id",1}}),
    std::make_shared<Json>(Json{{"type","assistant"},{"id",2}})});
  TurnMessagesRef raw;
  std::optional<std::string> rawInput;
  MessageServices imports;
  imports.publishMessages=[&](const TurnMessagesRef& value){raw=value;};
  imports.publishProcessingInput=[&](const auto& value){rawInput=value;};
  MessageState state(initial,std::move(imports));
  Json result=Json::array();
  auto capture=[&]{
    Json value={{"length",state.messages()->size()},
      {"baseline",state.baseline()},{"pending",state.userMessagePending()},
      {"processing",rawInput?Json(*rawInput):Json()},
      {"rawLength",raw?Json(raw->size()):Json()},
      {"lastType",state.messages()->empty()?Json():
        value::property(*state.messages()->back(),"type")}};
    result.push_back(std::move(value));
  };
  auto appended=[](const TurnMessagesRef& old,Json item){
    auto next=std::make_shared<TurnMessages>(*old);
    next->push_back(std::make_shared<Json>(std::move(item)));
    return next;
  };
  state.setUserInputOnProcessing("prompt");capture();
  state.setMessages([&](const auto& old){return appended(old,
    Json{{"type","system"},{"id",3}});});capture();
  state.setMessages([&](const auto& old){return appended(old,
    Json{{"type","user"},{"id",4}});});capture();
  state.setMessages([&](const auto& old){return appended(old,
    Json{{"type","assistant"},{"id",5}});});capture();
  state.setMessages(std::make_shared<TurnMessages>(TurnMessages{
    std::make_shared<Json>(Json{{"type","system"},{"id",6}})}));capture();
  state.setUserInputOnProcessing({});capture();
  state.setUserInputOnProcessing("second");capture();
  state.setMessages([](const auto& old){
    auto next=std::make_shared<TurnMessages>();
    next->push_back(std::make_shared<Json>(Json{{"type","system"},{"id",7}}));
    next->insert(next->end(),old->begin(),old->end());return next;
  });capture();
  state.setMessages([&](const auto& old){return appended(old,
    Json{{"type","user"},{"toolUseResult",nullptr},{"id",8}});});capture();
  state.setMessages([&](const auto& old){return appended(old,
    Json{{"type","user"},{"isMeta",false},{"id",9}});});capture();
  std::cout<<result.dump()<<'\n';
}
