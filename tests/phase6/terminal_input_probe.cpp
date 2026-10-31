#include "terminal_events/InputEvent.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
namespace te=batbox::terminal_events;
namespace {
bool flag(const Json& value,const char* key){
  return value.contains(key)&&value.at(key).get<bool>();
}
std::optional<std::string> stringField(const Json& value,const char* key){
  if(!value.contains(key)||value.at(key).is_null())return {};
  return value.at(key).get<std::string>();
}
std::shared_ptr<te::ParsedKey> parsedKey(const Json& value){
  auto result=std::make_shared<te::ParsedKey>();
  result->fn=flag(value,"fn");result->ctrl=flag(value,"ctrl");
  result->meta=flag(value,"meta");result->shift=flag(value,"shift");
  result->option=flag(value,"option");result->super=flag(value,"super");
  result->isPasted=flag(value,"isPasted");
  result->name=stringField(value,"name");
  result->sequence=stringField(value,"sequence");
  result->raw=stringField(value,"raw");
  result->code=stringField(value,"code");
  return result;
}
Json keyJson(const te::Key& key){
  return Json{{"upArrow",key.upArrow},{"downArrow",key.downArrow},
    {"leftArrow",key.leftArrow},{"rightArrow",key.rightArrow},
    {"pageDown",key.pageDown},{"pageUp",key.pageUp},
    {"wheelUp",key.wheelUp},{"wheelDown",key.wheelDown},
    {"home",key.home},{"end",key.end},{"return",key.returnKey},
    {"escape",key.escape},{"fn",key.fn},{"ctrl",key.ctrl},
    {"shift",key.shift},{"tab",key.tab},{"backspace",key.backspace},
    {"delete",key.deleteKey},{"meta",key.meta},{"super",key.super}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);Json cases;stream>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    const auto parsed=parsedKey(item.at("keypress"));
    te::InputEvent event(parsed);
    rows.push_back(Json{{"name",item.at("name")},{"input",event.input},
      {"key",keyJson(event.key)},{"keypressSame",event.keypress==parsed},
      {"nonAlphanumeric",parsed->name&&
        te::isNonAlphanumericKey(*parsed->name)}});
  }
  std::cout<<rows.dump()<<'\n';
}
