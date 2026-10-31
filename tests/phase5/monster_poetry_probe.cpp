#include "monster/Poetry.hpp"
#include <iostream>
#include <iterator>
using namespace batbox;
Json run(const Json& item){
 Json calls=Json::array();monster::IntroServices s;
 s.feature=[&](const std::string& key){calls.push_back(Json::array({"feature",key}));return item.at("enabled").get<bool>();};
 s.getMonster=[&]()->std::optional<Json>{calls.push_back(Json::array({"getMonster"}));if(!item.contains("monster")||item["monster"].is_null())return {};return item["monster"];};
 s.getGlobalConfig=[&]{calls.push_back(Json::array({"config"}));return item.value("config",Json::object());};
 std::optional<Json> messages;if(item.contains("messages")&&!item["messages"].is_null())messages=item["messages"];
 const auto attachments=monster::getMonsterIntroAttachment(messages,s);
 const monster::LocalDate date{item["date"][0],item["date"][1],item["date"][2]};
 const auto teaser=monster::isMonsterTeaserWindow(date),live=monster::isMonsterLive(date);
 const auto enabled=s.feature("MONSTER");
 return Json{{"text",monster::monsterIntroText(item["name"],item["species"])},{"attachments",attachments},
  {"teaser",teaser},{"live",live},{"triggers",monster::findMonsterTriggerPositions(item["text"],enabled)},{"calls",calls}};
}
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();for(const auto& item:input)out.push_back(run(item));std::cout<<value::stringify(out)<<'\n';}
