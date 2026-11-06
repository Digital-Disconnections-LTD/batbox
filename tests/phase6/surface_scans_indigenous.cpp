// Per-frame scans over the transcript list: the unseen-divider prefix match
// and the latest shell output answer as before without copying every row.
#include "interactive/message_list_view/Surface.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
namespace mlv=batbox::interactive::message_list_view;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
MessageRef user(const std::string& id,const std::string& text){
  return std::make_shared<Json>(Json{{"type","user"},{"uuid",id},
    {"message",{{"role","user"},{"content",Json::array({{{"type","text"},{"text",text}}})}}}});
}
}

int main(){
  try{
    for(const std::string text:{std::string("0f9c2a1e-6b1d-4c7e-9d2f-3a4b5c6d7e8f-conversation"),
        std::string("short"),std::string("雪0f9c2a1e-6b1d-4c7e-9d2f-3a4b5c6d"),std::string(""),
        std::string("0f9c2a1e-6b1d-4c7e-9d2f-3a")}){
      for(const std::string other:{text,std::string("0f9c2a1e-6b1d-4c7e-9d2f-3a4b5c6d"),
          std::string("雪0f9c2a1e-6b1d-4c7e-9d2f-3a4b5c6d7e8f")}){
        const auto prefix=value::slice(other,0,24);
        check(mlv::hasUuidPrefix(text,prefix)==(value::slice(text,0,24)==prefix),
          "prefix match differs from slice for "+text+" / "+other);
      }
    }
    const MessageRefs messages{user("a","<bash-stdout>ok</bash-stdout>"),
      user("b",std::string(100000,'x')),user("c","plain")};
    check(mlv::latestBashOutputUUID(messages)=="a","latest shell output found past long text");
    check(mlv::latestBashOutputUUID({user("d","none")}).is_null(),"no shell output");
    std::cout<<"transcript scans match without per-row copies\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
