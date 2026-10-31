#include "flights/Transcript.hpp"
#include <cassert>
using namespace batbox;
int main(){
  runtime::OrderedStringSet set{"z","b","a","10","2"};
  set.insert("b");set.insert("0");assert(Json(set)==Json::array({"z","b","a","10","2","0"}));
  auto copy=set;copy.erase("b");copy.insert("b");assert(Json(copy)==Json::array({"z","a","10","2","0","b"}));
  assert(set.contains("b"));auto iterator=set.begin();set.insert("tail");assert(*iterator=="z");
  flights::MessageMap messages;
  for(int i=0;i<20000;++i){const auto uuid=std::to_string(i);messages.set(uuid,Json{{"uuid",uuid},{"type","user"},{"parentUuid",i?Json(std::to_string(i-1)):Json()}});}
  auto snapshot=messages;for(int i=0;i<15000;++i)messages.erase(std::to_string(i));
  assert(messages.values().size()==5000&&snapshot.values().size()==20000);
  assert(messages.values().front().at("uuid")=="15000");assert(snapshot.get("0")&&!messages.get("0"));
  // Map keys remain independent of fields on their values, including copies.
  messages.set("key",Json{{"uuid","different"},{"value",1}});auto copied=messages;
  assert(copied.get("key")->at("uuid")=="different");copied.erase("key");assert(messages.get("key"));
  assert(messages.keys().back()=="key");
  auto assigned=snapshot;assigned=messages;assigned.erase("15000");assert(messages.get("15000"));
  auto moved=std::move(assigned);assert(moved.get("15001"));
}
