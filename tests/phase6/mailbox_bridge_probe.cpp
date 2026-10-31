#include "mailbox/Bridge.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace mb=batbox::mailbox;
namespace {
Json run(const char* name,bool loading,std::vector<std::string> ids,
    bool falseReturn=false){
  event_loop::Loop loop;mb::Mailbox box(loop);Json trace=Json::array();
  for(const auto& id:ids)
    box.send(std::make_shared<mb::Message>(
      mb::Message{id,"user",id,"now",{}, {}}));
  mb::runMailboxBridgeEffect(box,loading,[&](const std::string& content){
    trace.push_back(content);return !falseReturn;});
  const auto length=box.length(),revision=box.revision();
  const auto remaining=box.poll();
  return Json{{"name",name},{"trace",trace},{"length",length},
    {"revision",revision},
    {"remaining",remaining?Json(remaining->id):Json(nullptr)}};
}
}
int main(){
  std::cout<<Json::array({run("loading",true,{"a","b"}),
    run("empty",false,{}),run("one",false,{"a","b"}),
    run("falseReturn",false,{"a"},true)}).dump()<<'\n';
}
